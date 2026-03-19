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

#pragma once

#include "GafferUI/Gadget.h"

#include "IECoreGL/Selector.h"
#include "IECoreGL/Shader.h"

#include "IECoreScene/Camera.h"

#include <array>
#include <chrono>

namespace GafferUI
{

class GAFFERUI_API DepthRender
{

	public :

		DepthRender( Imath::V2i resolution );
		~DepthRender();

		void setResolution( const Imath::V2i &resolution );
		const Imath::V2i &getResolution() const;
		//const float* render( const Imath::M44f &projectionMatrix, const ViewportGadget* itemSource, GafferUI::Gadget::Layer filterLayer );
		void startRendering( const Imath::M44f &projectionMatrix );
		const float* finishRendering();

	private :

		//void renderInternal( RenderReason reason, Layer filterLayer = Layer::None ) const;
		//void renderLayerInternal( RenderReason reason, Layer layer, const Imath::M44f &viewTransform, const Imath::Box3f &bound, IECoreGL::Selector *selector ) const;
		GLuint acquireFramebuffer() const;

		Imath::V2i m_resolution;
		std::vector<float> m_depthMap;

		mutable GLuint m_framebuffer;
		mutable Imath::V2i m_framebufferSize;
		//mutable GLuint m_colorBuffer;
		mutable GLuint m_depthBuffer;
		//mutable GLuint m_downsampledFramebuffer;
		//mutable GLuint m_downsampledFramebufferTexture;

};

} // namespace GafferUI
