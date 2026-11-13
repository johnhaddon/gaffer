//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2018, John Haddon. All rights reserved.
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

#include "boost/python.hpp"

#include "GafferRenderMan/BXDFPlug.h"
#include "GafferRenderMan/RenderManAttributes.h"
#include "GafferRenderMan/RenderManDisplayFilter.h"
#include "GafferRenderMan/RenderManIntegrator.h"
#include "GafferRenderMan/RenderManLight.h"
#include "GafferRenderMan/RenderManLightFilter.h"
#include "GafferRenderMan/RenderManMeshLight.h"
#include "GafferRenderMan/RenderManOptions.h"
#include "GafferRenderMan/RenderManSampleFilter.h"
#include "GafferRenderMan/RenderManShader.h"

#include "GafferBindings/DependencyNodeBinding.h"
#include "GafferBindings/PlugBinding.h"

#include "IECoreScene/ShaderNetworkAlgo.h"

#include "IECore/MessageHandler.h"

using namespace boost::python;
using namespace GafferRenderMan;
using namespace IECoreScene;

namespace
{

// Shaders that use the `trace()` shadeop.
const boost::container::flat_set<std::string> g_badShaders = { "PxrDirt", "TextureCameraProjection", "TextureSphericalProjection" };

using ShaderSet = std::unordered_set<IECore::InternedString>;

void findDirtUpstream( const ShaderNetwork *network, IECore::InternedString shaderHandle, IECore::InternedString badParameter, ShaderSet &visited )
{
	if( !visited.insert( shaderHandle ).second )
	{
		return;
	}

	const Shader *shader = network->getShader( shaderHandle );
	if( g_badShaders.contains( shader->getName() ) )
	{
		IECore::msg( IECore::Msg::Level::Warning, "Dirt Detector", shader->getName() + " found upstream of " + badParameter.string() );
	}

	for( const auto &connection : network->inputConnections( shaderHandle ) )
	{
		findDirtUpstream( network, connection.source.shader, badParameter, visited );
	}
}

const boost::container::flat_set<IECore::InternedString> g_badParameters = { "presence", "dispAmount", "dispScalar", "dispVector", "modelDispVector" };

void pxrDirtDetector( IECoreScene::ShaderNetwork *shaderNetwork, IECore::InternedString attributeName, const IECore::CompoundObject *attributes )
{
	for( const auto &[handle, shader] : shaderNetwork->shaders() )
	{
		for( const auto &connection : shaderNetwork->inputConnections( handle ) )
		{
			auto it = g_badParameters.find( connection.destination.name );
			if( it != g_badParameters.end() )
			{
				ShaderSet visited;
				findDirtUpstream( shaderNetwork, handle, *it, visited );
			}
		}
	}
}

void pxrDirtDetectorHash( const IECoreScene::ShaderNetwork *shaderNetwork, IECore::InternedString attributeName, const IECore::CompoundObject *attributes, IECore::MurmurHash &hash )
{
	hash.append( "pxrDirtDetector" );
}

const bool g_registration = []{ IECoreScene::ShaderNetworkAlgo::registerRenderAdaptor( "pxrDirtDetector", pxrDirtDetectorHash, pxrDirtDetector ); return true; } ();

} // namespace

BOOST_PYTHON_MODULE( _GafferRenderMan )
{
	GafferBindings::PlugClass<BXDFPlug>()
		.def( init<const std::string &, Gaffer::Plug::Direction, unsigned>(
				(
					arg( "name" ) = Gaffer::GraphComponent::defaultName<BXDFPlug>(),
					arg( "direction" ) = Gaffer::Plug::In,
					arg( "flags" ) = Gaffer::Plug::Default
				)
			)
		)
	;


	GafferBindings::DependencyNodeClass<RenderManLight>();
	GafferBindings::DependencyNodeClass<RenderManLightFilter>();
	GafferBindings::DependencyNodeClass<RenderManAttributes>();
	GafferBindings::DependencyNodeClass<RenderManOptions>();
	GafferBindings::DependencyNodeClass<RenderManShader>();
	GafferBindings::DependencyNodeClass<RenderManMeshLight>();
	GafferBindings::DependencyNodeClass<RenderManIntegrator>();

	{
		scope s = GafferBindings::DependencyNodeClass<RenderManOutputFilter>( nullptr, no_init );
		enum_<RenderManOutputFilter::Mode>( "Mode" )
			.value( "Replace", RenderManOutputFilter::Mode::Replace )
			.value( "InsertFirst", RenderManOutputFilter::Mode::InsertFirst )
			.value( "InsertLast", RenderManOutputFilter::Mode::InsertLast )
		;
	}

	GafferBindings::DependencyNodeClass<RenderManSampleFilter>();
	GafferBindings::DependencyNodeClass<RenderManDisplayFilter>();
}
