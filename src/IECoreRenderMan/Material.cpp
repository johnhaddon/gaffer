//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2024, Cinesite VFX Ltd. All rights reserved.
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

#include "Material.h"

#include "ParamListAlgo.h"

#include "IECore/LRUCache.h"
#include "IECore/SearchPath.h"

#include "boost/property_tree/xml_parser.hpp"

#include "fmt/format.h"

#include <unordered_set>

using namespace std;
using namespace IECore;
using namespace IECoreScene;
using namespace IECoreRenderMan;

//////////////////////////////////////////////////////////////////////////
// Internal implementation
//////////////////////////////////////////////////////////////////////////

namespace
{

using ParameterTypeMap = std::unordered_map<InternedString, pxrcore::DataType>;
using ParameterTypeMapPtr = shared_ptr<ParameterTypeMap>;
using ParameterTypeCache = IECore::LRUCache<string, ParameterTypeMapPtr>;

void loadParameterTypes( const boost::property_tree::ptree &tree, ParameterTypeMap &typeMap )
{
	for( const auto &child : tree )
	{
		if( child.first == "param" )
		{
			const string name = child.second.get<string>( "<xmlattr>.name" );
			const string type = child.second.get<string>( "<xmlattr>.type" );
			if( type == "float" )
			{
				typeMap[name] = pxrcore::DataType::k_float;
			}
			else if( type == "int" )
			{
				typeMap[name] = pxrcore::DataType::k_integer;
			}
			else if( type == "point" )
			{
				typeMap[name] = pxrcore::DataType::k_point;
			}
			else if( type == "vector" )
			{
				typeMap[name] = pxrcore::DataType::k_vector;
			}
			else if( type == "normal" )
			{
				typeMap[name] = pxrcore::DataType::k_normal;
			}
			else if( type == "color" )
			{
				typeMap[name] = pxrcore::DataType::k_color;
			}
			else if( type == "string" )
			{
				typeMap[name] = pxrcore::DataType::k_string;
			}
			else if( type == "struct" )
			{
				typeMap[name] = pxrcore::DataType::k_struct;
			}
			else
			{
				IECore::msg( IECore::Msg::Warning, "IECoreRenderMan::Renderer", fmt::format( "Unknown type {} for parameter \"{}\".", type, name ) );
			}
		}
		else if( child.first == "page" )
		{
			loadParameterTypes( child.second, typeMap );
		}
	}
}

ParameterTypeCache g_parameterTypeCache(

	[]( const std::string &shaderName, size_t &cost ) {

		const char *pluginPath = getenv( "RMAN_RIXPLUGINPATH" );
		SearchPath searchPath( pluginPath ? pluginPath : "" );

		boost::filesystem::path argsFilename = searchPath.find( "Args/" + shaderName + ".args" );
		if( argsFilename.empty() )
		{
			throw IECore::Exception(
				fmt::format( "Unable to find shader \"{}\" on RMAN_RIXPLUGINPATH", shaderName )
			);
		}

		std::ifstream argsStream( argsFilename.string() );

		boost::property_tree::ptree tree;
		boost::property_tree::read_xml( argsStream, tree );

		auto parameterTypes = make_shared<ParameterTypeMap>();
		loadParameterTypes( tree.get_child( "args" ), *parameterTypes );

		cost = 1;
		return parameterTypes;
	},
	/* maxCost = */ 10000

);

const pxrcore::DataType *parameterType( const Shader *shader, IECore::InternedString parameterName )
{
	ParameterTypeMapPtr p = g_parameterTypeCache.get( shader->getName() );
	auto it = p->find( parameterName );
	if( it != p->end() )
	{
		return &it->second;
	}
	return nullptr;
}

using HandleSet = std::unordered_set<InternedString>;

void convertConnection( const IECoreScene::ShaderNetwork::Connection &connection, const IECoreScene::Shader *shader, RtParamList &paramList )
{
	const pxrcore::DataType *type = parameterType( shader, connection.destination.name );
	if( !type )
	{
		return;
	}

	std::string reference = connection.source.shader;
	if( !connection.source.name.string().empty() )
	{
		reference += ":" + connection.source.name.string();
	}

	const RtUString referenceU( reference.c_str() );

	RtParamList::ParamInfo const info = {
		RtUString( connection.destination.name.c_str() ),
		*type,
		pxrcore::DetailType::k_reference,
		1,
		false,
		false,
		false
	};

	paramList.SetParam( info, &referenceU );
}

void convertShaderNetworkWalk( const ShaderNetwork::Parameter &outputParameter, const IECoreScene::ShaderNetwork *shaderNetwork, vector<riley::ShadingNode> &shadingNodes, HandleSet &visited )
{
	if( !visited.insert( outputParameter.shader ).second )
	{
		return;
	}

	const IECoreScene::Shader *shader = shaderNetwork->getShader( outputParameter.shader );
	riley::ShadingNode node = {
		riley::ShadingNode::Type::k_Pattern,
		RtUString( shader->getName().c_str() ),
		RtUString( outputParameter.shader.c_str() ),
		RtParamList()
	};

	if( shader->getType() == "light" || shader->getType() == "renderman:light" )
	{
		node.type = riley::ShadingNode::Type::k_Light;
	}
	else if( shader->getType() == "surface" || shader->getType() == "renderman:bxdf" )
	{
		node.type = riley::ShadingNode::Type::k_Bxdf;
	}

	ParamListAlgo::convertParameters( shader->parameters(), node.params );

	for( const auto &connection : shaderNetwork->inputConnections( outputParameter.shader ) )
	{
		convertShaderNetworkWalk( connection.source, shaderNetwork, shadingNodes, visited );
		convertConnection( connection, shader, node.params );
	}

	shadingNodes.push_back( node );
}

riley::MaterialId convertShaderNetwork( const ShaderNetwork *network, riley::Riley *riley )
{
	vector<riley::ShadingNode> shadingNodes;
	shadingNodes.reserve( network->size() );

	HandleSet visited;
	convertShaderNetworkWalk( network->getOutput(), network, shadingNodes, visited );

	return riley->CreateMaterial( riley::UserId(), { (uint32_t)shadingNodes.size(), shadingNodes.data() }, RtParamList() );
}

riley::MaterialId defaultMaterial( riley::Riley *riley )
{
	vector<riley::ShadingNode> shaders;

	shaders.push_back( { riley::ShadingNode::Type::k_Pattern, RtUString( "PxrFacingRatio" ), RtUString( "facingRatio" ), RtParamList() } );

	RtParamList toFloat3ParamList;
	toFloat3ParamList.SetFloatReference( RtUString( "input" ), RtUString( "facingRatio:resultF" ) );
	shaders.push_back( { riley::ShadingNode::Type::k_Pattern, RtUString( "PxrToFloat3" ), RtUString( "toFloat3" ), toFloat3ParamList } );

	RtParamList constantParamList;
	constantParamList.SetColorReference( RtUString( "emitColor" ), RtUString( "toFloat3:resultRGB" ) );
	shaders.push_back( { riley::ShadingNode::Type::k_Bxdf, RtUString( "PxrConstant" ), RtUString( "constant" ), constantParamList } );

	return riley->CreateMaterial( riley::UserId(), { (uint32_t)shaders.size(), shaders.data() }, RtParamList() );
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Material
//////////////////////////////////////////////////////////////////////////

Material::Material( const IECoreScene::ShaderNetwork *network, const ConstSessionPtr &session )
	:	m_session( session )
{
	if( network )
	{
		m_id = convertShaderNetwork( network, m_session->riley );
	}
	else
	{
		m_id = defaultMaterial( m_session->riley );
	}
}

Material::~Material()
{
	if( m_session->renderType == IECoreScenePreview::Renderer::Interactive )
	{
		m_session->riley->DeleteMaterial( m_id );
	}
}

const riley::MaterialId &Material::id() const
{
	return m_id;
}

riley::LightShaderId IECoreRenderMan::convertLightShaderNetwork( const IECoreScene::ShaderNetwork *network, riley::Riley *riley )
{
	vector<riley::ShadingNode> shadingNodes;
	shadingNodes.reserve( network->size() );

	HandleSet visited;
	convertShaderNetworkWalk( network->getOutput(), network, shadingNodes, visited );

	return riley->CreateLightShader(
		riley::UserId(), { (uint32_t)shadingNodes.size(), shadingNodes.data() },
		/* lightFilter = */ { 0, nullptr }
	);
}

//////////////////////////////////////////////////////////////////////////
// MaterialCache
//////////////////////////////////////////////////////////////////////////

MaterialCache::MaterialCache( const ConstSessionPtr &session )
	:	m_session( session )
{
}

// Can be called concurrently with other calls to `get()`
ConstMaterialPtr MaterialCache::get( const IECoreScene::ShaderNetwork *network )
{
	Cache::accessor a;
	m_cache.insert( a, network ? network->Object::hash() : IECore::MurmurHash() );
	if( !a->second )
	{
		a->second = new Material( network, m_session );
	}
	return a->second;
}

// Must not be called concurrently with anything.
void MaterialCache::clearUnused()
{
	vector<IECore::MurmurHash> toErase;
	for( const auto &m : m_cache )
	{
		if( m.second->refCount() == 1 )
		{
			// Only one reference - this is ours, so
			// nothing outside of the cache is using the
			// shader.
			toErase.push_back( m.first );
		}
	}
	for( const auto &e : toErase )
	{
		m_cache.erase( e );
	}
}
