//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2026, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "GafferUI/DepthRender.h"

#include "GafferUI/Style.h"
#include "GafferUI/Pointer.h"
#include "GafferUI/ViewportGadget.h"

#include "IECoreGL/Buffer.h"
#include "IECoreGL/Camera.h"
#include "IECoreGL/Selector.h"
#include "IECoreGL/ShaderLoader.h"
#include "IECoreGL/State.h"
#include "IECoreGL/ToGLCameraConverter.h"

#include "IECore/AngleConversion.h"
#include "IECore/MessageHandler.h"
#include "IECore/NullObject.h"
#include "IECore/SimpleTypedData.h"

#include "Imath/ImathBoxAlgo.h"
#include "Imath/ImathMatrixAlgo.h"

#include "boost/bind/bind.hpp"
#include "boost/bind/placeholders.hpp"

#include "fmt/format.h"

#include <chrono>
#include <cmath>
#include <regex>

using namespace std;
using namespace boost::placeholders;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferUI;

//////////////////////////////////////////////////////////////////////////
// Utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

bool checkGLArbTextureFloat()
{
	bool supported = std::regex_match( std::string( (const char*)glGetString( GL_EXTENSIONS ) ), std::regex( R"(.*GL_ARB_texture_float( |\n).*)" ) );
	if( !supported )
	{
		IECore::msg(
			IECore::Msg::Warning, "DepthRender",
			"Could not find supported floating point texture format in OpenGL. Viewport "
			"display is likely to show banding - please resolve graphics driver issue."
		);
	}
	return supported;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// DepthRender
//////////////////////////////////////////////////////////////////////////

DepthRender::DepthRender( V2i resolution )
	: m_resolution( resolution )
{
}

DepthRender::~DepthRender()
{
	// We should technically ensure that the right GL context is current when
	// making these calls, but this seems to work without, because all our GL
	// contexts are sharing the same resources.
	if( m_framebuffer )
	{
		glDeleteFramebuffers( 1, &m_framebuffer );
		glDeleteRenderbuffers( 1, &m_colorBuffer );
		glDeleteRenderbuffers( 1, &m_depthBuffer );
		glDeleteFramebuffers( 1, &m_downsampledFramebuffer );
		glDeleteTextures( 1, &m_downsampledFramebufferTexture );
	}
}

void DepthRender::setResolution( const V2i &resolution )
{
	m_resolution = resolution;
}

const V2i &DepthRender::getResolution() const
{
	return m_resolution;
}

const float* DepthRender::render( const M44f &projectionMatrix, const ViewportGadget* itemSource, Gadget::Layer filterLayer )
{
	glViewport(	0, 0, m_resolution.x, m_resolution.y );

    glMatrixMode( GL_PROJECTION );
    glLoadMatrixf( projectionMatrix.getValue() );
    /*glLoadIdentity();
	glFrustum( -1, 1,
		-1, 1,
		1, 10000
	);*/


    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();


	glClearColor( 0.26f, 0.26f, 0.26f, 0.0f );
	glClearDepth( 1.0f );
	//glClearDepth( 0.5f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// Set up the camera to world matrix in gl_TextureMatrix[0] so that we can
	// reference world space positions in shaders
	// This should be more appropriately named in a uniform buffer, but the
	// easiest time to get this right is probably when we switch everything
	// away from using fixed function stuff
	// TODO - is there any reason to need this during a depth render
	/*glActiveTexture( GL_TEXTURE0 );
	glMatrixMode( GL_TEXTURE );
	glLoadIdentity();
	glMultMatrixf( camera->getTransform().getValue() );
	glMatrixMode( GL_MODELVIEW );*/

	// TODO
	//bound(); // Updates layout if necessary

	if( !itemSource->m_renderItems.size() )
	{
		itemSource->getRenderItems( itemSource, M44f(), itemSource->style(), itemSource->m_renderItems );
	}

	M44f viewTransform;
	glGetFloatv( GL_MODELVIEW_MATRIX, viewTransform.getValue() );
	M44f projectionTransform;
	glGetFloatv( GL_PROJECTION_MATRIX, projectionTransform.getValue() );

	M44f combinedInverse = projectionTransform.inverse() * viewTransform.inverse();
	Box3f bound = transform( Box3f( V3f( -1 ), V3f( 1 ) ), combinedInverse );

	GLint outputFramebuffer = -1;
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &outputFramebuffer );

	glEnable( GL_BLEND );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, acquireFramebuffer() );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	glEnable( GL_MULTISAMPLE );
	glClearDepth( 1.0f );
	glClear( GL_DEPTH_BUFFER_BIT );

	for( int layerIndex = std::log2( (int)Gadget::Layer::Back ); layerIndex <= std::log2( (int)Gadget::Layer::Front ); ++layerIndex )
	{
		Gadget::Layer layer = Gadget::Layer( (int)Gadget::Layer::Back << layerIndex );
		/*if( filterLayer != Gadget::Layer::None && layer != filterLayer )
		{
			continue;
		}*/

		itemSource->renderLayerInternal( Gadget::RenderReason::Draw, layer, viewTransform, bound, nullptr );
	}

	glBindFramebuffer( GL_READ_FRAMEBUFFER, m_framebuffer );
	//glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_downsampledFramebuffer );
	m_depthMap.resize( m_resolution.x * m_resolution.y );
	glReadPixels( 0, 0, m_resolution.x, m_resolution.y, GL_DEPTH_COMPONENT, GL_FLOAT, &m_depthMap[0] );

	return &m_depthMap[0];
}

/*void ViewportGadget::renderLayerInternal( RenderReason reason, Gadget::Layer layer, const M44f &viewTransform, const Box3f &bound, IECoreGL::Selector *selector ) const
{
	const Style *currentStyle = nullptr;
	for( unsigned int i = 0; i < m_renderItems.size(); i++ )
	{
		const RenderItem &renderItem = m_renderItems[i];
		if( !( renderItem.layerMask & ( (unsigned int)layer ) ) )
		{
			continue;
		}

		if( !renderItem.bound.intersects( bound ) )
		{
			continue;
		}

		glLoadMatrixf( viewTransform.getValue() );
		glMultMatrixf( renderItem.transform.getValue() );
		if( selector )
		{
			// 0 is a reserved name for when nothing is selected, so start at 1
			selector->loadName( i + 1 );
		}

		if( renderItem.style != currentStyle )
		{
			renderItem.style->bind( currentStyle );
			currentStyle = renderItem.style;
		}

		renderItem.gadget->renderLayer( layer, currentStyle, reason );
	}
}*/

GLuint DepthRender::acquireFramebuffer() const
{
	if( m_framebuffer && m_framebufferSize == m_resolution )
	{
		// Reuse existing buffer.
		return m_framebuffer;
	}

	if( !m_colorBuffer )
	{
		glGenRenderbuffers( 1, &m_colorBuffer );
		glGenRenderbuffers( 1, &m_depthBuffer );

		glGenTextures( 1, &m_downsampledFramebufferTexture );
		glBindTexture( GL_TEXTURE_2D, m_downsampledFramebufferTexture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}

	if( m_framebuffer )
	{
		// There is contradictory information out there about whether a
		// framebuffer can handle the attached textures and render buffers being
		// resized - sounds like it depends on the driver. Safer to just
		// recreate it.
		glDeleteFramebuffers( 1, &m_framebuffer );
		glDeleteFramebuffers( 1, &m_downsampledFramebuffer );
	}

	// Create framebuffer
	glGenFramebuffers( 1, &m_framebuffer );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_framebuffer );

	// Resize color buffer and attach to framebuffer
	static const GLint colorFormat = checkGLArbTextureFloat() ? GL_RGBA16F : GL_RGBA8;

	glBindRenderbuffer( GL_RENDERBUFFER, m_colorBuffer );
	glRenderbufferStorage( GL_RENDERBUFFER, colorFormat, m_resolution.x, m_resolution.y );
	glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorBuffer );

	// Resize depth buffer and attach to framebuffer
	glBindRenderbuffer( GL_RENDERBUFFER, m_depthBuffer );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, m_resolution.x, m_resolution.y );
	glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer );

	// Validate framebuffer
	GLenum framebufferStatus = glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER );
	if( framebufferStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		IECore::msg( IECore::Msg::Warning, "GafferUI::DepthRender", "Multisampled framebuffer error : " + std::to_string( framebufferStatus ) );
	}

	// Create downsampled framebuffer
	/*glGenFramebuffers( 1, &m_downsampledFramebuffer );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_downsampledFramebuffer );

	// Resize color texture and attach to downsampled framebuffer
	glBindTexture( GL_TEXTURE_2D, m_downsampledFramebufferTexture );
	glTexImage2D( GL_TEXTURE_2D, 0, colorFormat, size.x, size.y, 0, GL_RGBA, GL_FLOAT, nullptr );
	glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_downsampledFramebufferTexture, 0 );

	// Validate downsampled framebuffer
	framebufferStatus = glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER );
	if( framebufferStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		IECore::msg( IECore::Msg::Warning, "GafferUI::ViewportGadget", "Downsampled framebuffer error : " + std::to_string( framebufferStatus ) );
	}*/

	m_framebufferSize = m_resolution;
	return m_framebuffer;
}
