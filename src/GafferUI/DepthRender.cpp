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
		glDeleteRenderbuffers( 1, &m_depthBuffer );
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

void DepthRender::startRendering( const M44f &projectionMatrix ) //, const ViewportGadget* itemSource, Gadget::Layer filterLayer )
{
	glViewport(	0, 0, m_resolution.x, m_resolution.y );

    glMatrixMode( GL_PROJECTION );
    glLoadMatrixf( projectionMatrix.getValue() );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();


	glClearColor( 0.26f, 0.26f, 0.26f, 0.0f );
	glClearDepth( 1.0f );
	glClear( GL_DEPTH_BUFFER_BIT );

	/*if( !itemSource->m_renderItems.size() )
	{
		itemSource->getRenderItems( itemSource, M44f(), itemSource->style(), itemSource->m_renderItems );
	}*/

	/*M44f viewTransform;
	glGetFloatv( GL_MODELVIEW_MATRIX, viewTransform.getValue() );
	M44f projectionTransform;
	glGetFloatv( GL_PROJECTION_MATRIX, projectionTransform.getValue() );

	M44f combinedInverse = projectionTransform.inverse() * viewTransform.inverse();
	Box3f bound = transform( Box3f( V3f( -1 ), V3f( 1 ) ), combinedInverse );
	*/

	GLint outputFramebuffer = -1;
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &outputFramebuffer );

	glEnable( GL_BLEND );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, acquireFramebuffer() );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glEnable( GL_MULTISAMPLE );
	glClearDepth( 1.0f );
	glClear( GL_DEPTH_BUFFER_BIT );
}

/*
	for( int layerIndex = std::log2( (int)Gadget::Layer::Back ); layerIndex <= std::log2( (int)Gadget::Layer::Front ); ++layerIndex )
	{
		Gadget::Layer layer = Gadget::Layer( (int)Gadget::Layer::Back << layerIndex );
		//if( filterLayer != Gadget::Layer::None && layer != filterLayer )
		//{
		//	continue;
		//}

		itemSource->renderLayerInternal( Gadget::RenderReason::Draw, layer, viewTransform, bound, nullptr );
	}
*/

const float* DepthRender::finishRendering()
{

	glBindFramebuffer( GL_READ_FRAMEBUFFER, m_framebuffer );
	//glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_downsampledFramebuffer );
	m_depthMap.resize( m_resolution.x * m_resolution.y );
	glReadPixels( 0, 0, m_resolution.x, m_resolution.y, GL_DEPTH_COMPONENT, GL_FLOAT, &m_depthMap[0] );

	return &m_depthMap[0];
}

GLuint DepthRender::acquireFramebuffer() const
{
	if( m_framebuffer && m_framebufferSize == m_resolution )
	{
		// Reuse existing buffer.
		return m_framebuffer;
	}

	if( !m_depthBuffer )
	{
		glGenRenderbuffers( 1, &m_depthBuffer );
	}

	if( m_framebuffer )
	{
		// There is contradictory information out there about whether a
		// framebuffer can handle the attached textures and render buffers being
		// resized - sounds like it depends on the driver. Safer to just
		// recreate it.
		glDeleteFramebuffers( 1, &m_framebuffer );
	}

	// Create framebuffer
	glGenFramebuffers( 1, &m_framebuffer );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_framebuffer );

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

	m_framebufferSize = m_resolution;
	return m_framebuffer;
}
