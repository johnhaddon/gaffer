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

#include "GafferScene/ShaderTweakProxy.h"

#include "Gaffer/StringPlug.h"
#include "Gaffer/PlugAlgo.h"

using namespace Gaffer;
using namespace GafferScene;

GAFFER_NODE_DEFINE_TYPE( ShaderTweakProxy );

size_t ShaderTweakProxy::g_firstPlugIndex;

ShaderTweakProxy::ShaderTweakProxy( const std::string &sourceNode, const IECore::StringVectorData *outputNames, const IECore::ObjectVector *outputTypes, const std::string &name )
	:	Shader( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new Plug( "out", Plug::Out ) );
	namePlug()->setValue( shaderTweakProxyIdentifier() );
	typePlug()->setValue( shaderTweakProxyIdentifier() );

	parametersPlug()->addChild( new StringPlug( "shaderTweakProxySourceNode", Plug::Direction::In, sourceNode, Plug::Flags::Default | Plug::Flags::Dynamic ) );

	if( !outputNames || !outputTypes || outputNames->readable().size() != outputTypes->members().size() )
	{
		throw IECore::Exception( "ShaderTweakProxy must be constructed with matching outputNames and outputTypes" );
	}

	for( unsigned int i = 0; i < outputNames->readable().size(); i++ )
	{
		outPlug()->addChild(
			PlugAlgo::createPlugFromData( outputNames->readable()[i], Plug::Direction::Out, Plug::Flags::Default | Plug::Flags::Dynamic, IECore::runTimeCast<IECore::Data>( outputTypes->members()[i].get() ) )
		);
	}
}

ShaderTweakProxy::ShaderTweakProxy( const std::string &name )
	:	Shader( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new Plug( "out", Plug::Out ) );
	namePlug()->setValue( shaderTweakProxyIdentifier() );
	typePlug()->setValue( shaderTweakProxyIdentifier() );
}

ShaderTweakProxy::~ShaderTweakProxy()
{
}

void ShaderTweakProxy::loadShader( const std::string &shaderName, bool keepExistingValues )
{
}

const std::string& ShaderTweakProxy::shaderTweakProxyIdentifier()
{
	static const std::string id = "__SHADER_TWEAK_PROXY";
	return id;
}
