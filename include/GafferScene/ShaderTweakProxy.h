//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2024, Image Engine Design Inc. All rights reserved.
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

#include "GafferScene/Shader.h"

#include "IECoreScene/ShaderNetwork.h"
#include "IECore/ObjectVector.h"
#include "IECore/VectorTypedData.h"

#include <unordered_map>

namespace GafferScene
{

class GAFFERSCENE_API ShaderTweakProxy : public Shader
{

	public :

		// Create a ShaderTweakProxy, set up to proxy a particular node in the network being tweaked.
		ShaderTweakProxy( const std::string &sourceNode, const IECore::StringVectorData *outputNames, const IECore::ObjectVector *outputTypes, const std::string &name=defaultName<ShaderTweakProxy>());

		// Should only be called by serializer, to construct ShaderTweakProxies that already have their plugs
		// set up ( It might be slightly cleaner to make the plugs not dynamic, and instead use a custom
		// serializer that calls the constructor above, but that seems like more work ).
		ShaderTweakProxy( const std::string &name );

		~ShaderTweakProxy() override;

		GAFFER_NODE_DECLARE_TYPE( GafferScene::ShaderTweakProxy, ShaderTweakProxyTypeId, Shader );

		// This is implemented to do nothing, because ShaderTweakProxy isn't really a shader, it just
		// acts like one to store connections before they get replumbed to their actual targets.
		void loadShader( const std::string &shaderName, bool keepExistingValues=false ) override;

		static const std::string &shaderTweakProxyIdentifier();

	private :

		static size_t g_firstPlugIndex;
};

IE_CORE_DECLAREPTR( ShaderTweakProxy )

} // namespace GafferScene
