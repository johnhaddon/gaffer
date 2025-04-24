//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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

#include "GafferScene/Shader.h"

#include "GafferScene/ShaderTweakProxy.h"

#include "Gaffer/Metadata.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/OptionalValuePlug.h"
#include "Gaffer/PlugAlgo.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/SplinePlug.h"
#include "Gaffer/TypedPlug.h"

#include "IECoreScene/ShaderNetwork.h"

#include "IECore/VectorTypedData.h"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind/bind.hpp"
#include "boost/lexical_cast.hpp"

#include "fmt/compile.h"
#include "fmt/format.h"

#include <unordered_map>
#include <unordered_set>

using namespace std;
using namespace boost::placeholders;
using namespace Imath;
using namespace GafferScene;
using namespace Gaffer;

//////////////////////////////////////////////////////////////////////////
// Internal utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

using ShaderAndHash = std::pair<const Shader *, IECore::MurmurHash>;

} // namespace

template <>
struct std::hash<ShaderAndHash>
{
	std::size_t operator()( const ShaderAndHash &x ) const
	{
		size_t result = 0;
		boost::hash_combine( result, x.first );
		boost::hash_combine( result, x.second );
		return result;
	}
};

namespace
{

bool isOutputParameter( const Gaffer::Plug *parameterPlug )
{
	const Shader *shaderNode = IECore::runTimeCast<const Shader>( parameterPlug->node() );
	if( !shaderNode )
	{
		return false;
	}

	const Plug *shaderNodeOutPlug = shaderNode->outPlug();
	if( !shaderNodeOutPlug )
	{
		return false;
	}

	return parameterPlug == shaderNodeOutPlug || shaderNodeOutPlug->isAncestorOf( parameterPlug );
}

bool isInputParameter( const Gaffer::Plug *parameterPlug )
{
	const Shader *shaderNode = IECore::runTimeCast<const Shader>( parameterPlug->node() );
	if( !shaderNode )
	{
		return false;
	}

	return shaderNode->parametersPlug()->isAncestorOf( parameterPlug );
}

bool isParameter( const Gaffer::Plug *parameterPlug )
{
	return isInputParameter( parameterPlug ) || isOutputParameter( parameterPlug );
}

bool isCompoundNumericPlug( const Gaffer::Plug *plug )
{
	switch( (Gaffer::TypeId)plug->typeId() )
	{
		case V2iPlugTypeId :
		case V2fPlugTypeId :
		case V3iPlugTypeId :
		case V3fPlugTypeId :
		case Color3fPlugTypeId :
		case Color4fPlugTypeId :
			return true;
		default :
			return false;
	}
}

struct CycleDetector
{

	// ShaderAndHash is used to store a Shader and the hash of the context it is
	// being visited in.
	using DownstreamShaders = std::unordered_set<ShaderAndHash>;

	CycleDetector( DownstreamShaders &downstreamShaders, const ShaderAndHash &shaderAndContext )
		:	m_downstreamShaders( downstreamShaders ),
			m_shaderAndContext( shaderAndContext )
	{
		if( !m_downstreamShaders.insert( m_shaderAndContext ).second )
		{
			throw IECore::Exception(
				fmt::format(
					"Shader \"{}\" is involved in a dependency cycle.",
					shaderAndContext.first->relativeName( shaderAndContext.first->ancestor<ScriptNode>() )
				)
			);
		}
	}

	~CycleDetector()
	{
		m_downstreamShaders.erase( m_shaderAndContext );
	}

	private :

		DownstreamShaders &m_downstreamShaders;
		const ShaderAndHash m_shaderAndContext;

};

const IECore::InternedString g_outPlugName( "out" );
const IECore::InternedString g_label( "label" );
const IECore::InternedString g_gafferNodeName( "gaffer:nodeName" );
const IECore::InternedString g_gafferNodeColor( "gaffer:nodeColor" );

struct OptionalScopedContext
{

	ConstContextPtr context;
	std::optional<Context::Scope> scope;

};

} // namespace

//////////////////////////////////////////////////////////////////////////
// Shader::NetworkBuilder implementation
//////////////////////////////////////////////////////////////////////////

class Shader::NetworkBuilder
{

	public :

		NetworkBuilder( const Gaffer::Plug *output )
			:	m_output( output ), m_hasProxyNodes( false )
		{
		}

		IECore::MurmurHash networkHash()
		{
			OptionalScopedContext outputContext;
			if( const Gaffer::Plug *p = connectionSource( m_output, outputContext ) )
			{
				IECore::MurmurHash result;
				parameterHashForPlug( p, result );
				return result;
			}

			return IECore::MurmurHash();
		}

		IECoreScene::ConstShaderNetworkPtr network()
		{
			static IECore::InternedString hasProxyNodesIdentifier( "__hasProxyNodes" );

			if( !m_network )
			{
				m_network = new IECoreScene::ShaderNetwork;
				OptionalScopedContext outputContext;
				if( const Gaffer::Plug *p = connectionSource( m_output, outputContext ) )
				{
					m_network->setOutput( outputParameterForPlug( p ) );
				}
			}

			if( m_hasProxyNodes )
			{
				m_network->blindData()->writable()[hasProxyNodesIdentifier] = new IECore::BoolData( true );
			}

			return m_network;
		}

		const ValuePlug *parameterSource( const IECoreScene::ShaderNetwork::Parameter &parameter )
		{
			for( auto &[key, handle] : m_shaders )
			{
				if( handle == parameter.shader )
				{
					return key.first->parametersPlug()->descendant<ValuePlug>( parameter.name );
				}
			}
			return nullptr;
		}

	private :

		// Returns the shader output plug that is the source for
		// `parameterPlug`, taking into account pass-throughs for disabled
		// shaders, and intermediate Switches and ContextProcessors. Returns
		// `nullptr` if no such source exists.
		const Gaffer::Plug *connectionSource( const Gaffer::Plug *parameterPlug, OptionalScopedContext &parameterContext ) const
		{
			while( true )
			{
				if( !parameterPlug )
				{
					return nullptr;
				}

				if( isOutputParameter( parameterPlug ) )
				{
					const Shader *shaderNode = static_cast<const Shader *>( parameterPlug->node() );
					if( shaderNode->enabledPlug()->getValue() )
					{
						return parameterPlug;
					}
					else
					{
						// Follow pass-through, ready for next iteration.
						parameterPlug = shaderNode->correspondingInput( parameterPlug );
					}
				}
				else
				{
					assert( isInputParameter( parameterPlug ) );
					// Traverse through switches etc that are embedded in the
					// middle of the shader network.
					auto [source, sourceContext] = PlugAlgo::contextSensitiveSource( parameterPlug );

					if( source == parameterPlug || !isParameter( source ) )
					{
						return nullptr;
					}
					else
					{
						// Follow connection, ready for next iteration.
						if( sourceContext != Context::current() )
						{
							parameterContext.context = sourceContext;
							parameterContext.scope.emplace( parameterContext.context.get() );
						}
						parameterPlug = source;
					}
				}
			}
		}

		IECoreScene::ShaderNetwork::Parameter outputParameterForPlug( const Plug *parameter )
		{
			assert( isOutputParameter( parameter ) );

			const Shader *shader = static_cast<const Shader *>( parameter->node() );
			const Plug *outPlug = shader->outPlug();

			IECore::InternedString outputName;
			if( outPlug->typeId() == Plug::staticTypeId() && parameter != outPlug )
			{
				// Standard case where `out` is just a container, and individual
				// outputs are parented under it.
				outputName = parameter->relativeName( outPlug );
			}
			else
			{
				// Legacy case for subclasses which use `outPlug()` as the sole
				// output.
				/// \todo Enforce that `outPlug()` is always a container, and
				/// all outputs are represented as `out.name` children, even if
				/// there is only one output.
				outputName = parameter->relativeName( shader );
			}

			return { this->handle( shader ), outputName };
		}

		void parameterHashForPlug( const Plug *parameter, IECore::MurmurHash &h )
		{
			const Shader *shader = static_cast<const Shader *>( parameter->node() );
			h.append( shaderHash( shader ) );
			if( shader->outPlug()->isAncestorOf( parameter ) )
			{
				h.append( parameter->relativeName( shader->outPlug() ) );
			}
		}

		void checkNoShaderInput( const Gaffer::Plug *parameterPlug )
		{
			OptionalScopedContext parameterContext;
			if( connectionSource( parameterPlug, parameterContext ) )
			{
				throw IECore::Exception( fmt::format( "Shader connections to {} are not supported.", parameterPlug->fullName() ) );
			}
		}

		IECore::MurmurHash shaderHash( const Shader *shaderNode )
		{
			assert( shaderNode );
			assert( shaderNode->enabledPlug()->getValue() );

			const ShaderAndHash shaderContext = { shaderNode, Context::current()->hash() } ;
			CycleDetector cycleDetector( m_downstreamShaders, shaderContext );

			auto [it, inserted] = m_shaderHashes.insert( { shaderContext, IECore::MurmurHash() } );
			if( inserted )
			{
				IECore::MurmurHash &h = it->second;
				h.append( shaderNode->typeId() );
				shaderNode->namePlug()->hash( h );
				shaderNode->typePlug()->hash( h );

				shaderNode->nodeNamePlug()->hash( h );
				shaderNode->nodeColorPlug()->hash( h );

				hashParameterWalk( shaderNode->parametersPlug(), h, false, false );
			}

			return it->second;
		}

		IECore::InternedString handle( const Shader *shaderNode )
		{
			assert( shaderNode );
			assert( shaderNode->enabledPlug()->getValue() );

			IECore::InternedString &handle = m_shaders[{shaderNode, shaderHash( shaderNode )}];
			if( !handle.string().empty() )
			{
				return handle;
			}

			IECoreScene::ShaderPtr shader = new IECoreScene::Shader(
				shaderNode->namePlug()->getValue(), shaderNode->typePlug()->getValue()
			);
			if(
				!ShaderTweakProxy::isProxy( shader.get() ) &&
				shaderNode != m_output->node() && !boost::ends_with( shader->getType(), "shader" )
			)
			{
				// Some renderers (Arnold for one) allow surface shaders to be connected
				// as inputs to other shaders, so we may need to change the shader type to
				// convert it into a standard shader. We must take care to preserve any
				// renderer specific prefix when doing this.
				size_t i = shader->getType().find_first_of( ":" );
				if( i != std::string::npos )
				{
					shader->setType( shader->getType().substr( 0, i + 1 ) + "shader" );
				}
				else
				{
					shader->setType( "shader" );
				}
			}
			m_hasProxyNodes |= ShaderTweakProxy::isProxy( shader.get() );

			const std::string nodeName = shaderNode->nodeNamePlug()->getValue();
			shader->blindData()->writable()[g_label] = new IECore::StringData( nodeName );
			// \todo: deprecated, stop storing gaffer:nodeName after a grace period
			shader->blindData()->writable()[g_gafferNodeName] = new IECore::StringData( nodeName );
			shader->blindData()->writable()[g_gafferNodeColor] = new IECore::Color3fData( shaderNode->nodeColorPlug()->getValue() );

			vector<IECoreScene::ShaderNetwork::Connection> inputConnections;
			addParameterWalk( shaderNode->parametersPlug(), IECore::InternedString(), shader.get(), inputConnections, false, false );

			handle = m_network->addShader( nodeName, std::move( shader ) );
			for( const auto &c : inputConnections )
			{
				m_network->addConnection( { c.source, { handle, c.destination.name } } );
			}

			return handle;
		}

		void hashParameterWalk( const Gaffer::Plug *parameter, IECore::MurmurHash &h, bool foundValue, bool foundConnection )
		{
			if( !foundValue )
			{
				const IECore::MurmurHash hc = h;
				static_cast<const Shader *>( parameter->node() )->parameterHash( parameter, h );
				foundValue = h != hc;
			}

			if( !foundConnection )
			{
				OptionalScopedContext sourceContext;
				if( auto source = this->connectionSource( parameter, sourceContext ) )
				{
					parameterHashForPlug( source, h );
					foundConnection = true;
				}
			}

			if( foundValue && foundConnection )
			{
				return;
			}

			if( auto splineFF = IECore::runTimeCast<const SplineffPlug>( parameter ) )
			{
				hashSplineParameterWalk( splineFF, h );
			}
			else if( auto splineFColor3f = IECore::runTimeCast<const SplinefColor3fPlug>( parameter ) )
			{
				hashSplineParameterWalk( splineFColor3f, h );
			}
			else if( auto splineFColor4f = IECore::runTimeCast<const SplinefColor4fPlug>( parameter ) )
			{
				hashSplineParameterWalk( splineFColor4f, h );
			}
			else
			{
				for( const auto &childParameter : Plug::InputRange( *parameter ) )
				{
					const Plug *valuePlug = childParameter.get();
					if( auto optionalPlug = IECore::runTimeCast<const OptionalValuePlug>( valuePlug ) )
					{
						if( !optionalPlug->enabledPlug()->getValue() )
						{
							continue;
						}
						valuePlug = optionalPlug->valuePlug();
					}

					hashParameterWalk( valuePlug, h, foundValue, foundConnection );
				}
			}
		}

		void addParameterWalk( const Gaffer::Plug *parameter, const IECore::InternedString &parameterName, IECoreScene::Shader *shader, vector<IECoreScene::ShaderNetwork::Connection> &connections, bool foundValue, bool foundConnection )
		{
			// Store the value of the parameter whether or not we have a
			// connection, so parameter type information is always available
			// from the ShaderNetwork.
			if( !foundValue )
			{
				if( IECore::DataPtr value = static_cast<const Shader *>( parameter->node() )->parameterValue( parameter ) )
				{
					shader->parameters()[parameterName] = value;
					foundValue = true;
				}
			}

			if( !foundConnection )
			{
				OptionalScopedContext sourceContext;
				if( auto source = this->connectionSource( parameter, sourceContext ) )
				{
					connections.push_back( {
						outputParameterForPlug( source ),
						{ IECore::InternedString(), parameterName }
					} );
					foundConnection = true;
				}
			}

			if( foundValue && foundConnection )
			{
				return;
			}

			// Recurse to handle children
			// ==========================
			//
			// These might be the individual fields of a struct, elements of an ArrayPlug,
			// components of a CompoundNumericPlug, or the points of a SplinePlug.

			if( auto splineFF = IECore::runTimeCast<const SplineffPlug>( parameter ) )
			{
				addSplineParameterWalk( splineFF, parameterName, connections );
			}
			else if( auto splineFColor3f = IECore::runTimeCast<const SplinefColor3fPlug>( parameter ) )
			{
				addSplineParameterWalk( splineFColor3f, parameterName, connections );
			}
			else if( auto splineFColor4f = IECore::runTimeCast<const SplinefColor4fPlug>( parameter ) )
			{
				addSplineParameterWalk( splineFColor4f, parameterName, connections );
			}
			else
			{
				int arrayIndex = IECore::runTimeCast<const ArrayPlug>( parameter ) ? 0 : -1;
				for( const auto &childParameter : Plug::InputRange( *parameter ) )
				{
					const Plug *valuePlug = childParameter.get();
					if( auto optionalPlug = IECore::runTimeCast<const OptionalValuePlug>( valuePlug ) )
					{
						if( !optionalPlug->enabledPlug()->getValue() )
						{
							continue;
						}
						valuePlug = optionalPlug->valuePlug();
					}

					IECore::InternedString childParameterName;
					if( arrayIndex != -1 )
					{
						childParameterName = parameterName.string() + "[" + std::to_string( arrayIndex++ ) + "]";
					}
					else
					{
						if( parameterName.string().size() )
						{
							childParameterName = parameterName.string() + "." + childParameter->getName().string();
						}
						else
						{
							childParameterName = childParameter->getName();
						}
					}
					addParameterWalk( valuePlug, childParameterName, shader, connections, foundValue, foundConnection );
				}
			}
		}

		template<typename T>
		void hashSplineParameterWalk( const T *parameter, IECore::MurmurHash &h )
		{
			checkNoShaderInput( parameter->interpolationPlug() );

			bool hasInput = false;
			for( unsigned int i = 0; i < parameter->numPoints(); i++ )
			{
				checkNoShaderInput( parameter->pointPlug( i ) );
				checkNoShaderInput( parameter->pointXPlug( i ) );

				const auto* yPlug = parameter->pointYPlug( i );
				OptionalScopedContext sourceContext;
				if( auto source = connectionSource( yPlug, sourceContext ) )
				{
					hasInput = true;
					parameterHashForPlug( source, h );
					h.append( i );
				}
				else if( isCompoundNumericPlug( yPlug ) )
				{
					for( Plug::InputIterator it( yPlug ); !it.done(); ++it )
					{
						sourceContext.scope.reset();
						if( auto sourceComponent = connectionSource( it->get(), sourceContext ) )
						{
							hasInput = true;
							parameterHashForPlug( sourceComponent, h );
							h.append( i );
							h.append( (*it)->getName() );
						}
					}
				}
			}

			if( hasInput )
			{
				for( unsigned int i = 0; i < parameter->numPoints(); i++ )
				{
					parameter->pointXPlug( i )->hash( h );
				}
			}
		}

		template<typename T>
		void addSplineParameterWalk( const T *parameter, const IECore::InternedString &parameterName, vector<IECoreScene::ShaderNetwork::Connection> &connections )
		{
			const int n = parameter->numPoints();
			std::vector< std::tuple<int, std::string, IECoreScene::ShaderNetwork::Parameter> > inputs;

			for( int i = 0; i < n; i++ )
			{
				const auto* yPlug = parameter->pointYPlug( i );
				OptionalScopedContext sourceContext;
				if( auto source = connectionSource( yPlug, sourceContext ) )
				{
					inputs.push_back( std::make_tuple( i, "", outputParameterForPlug( source ) ) );
				}
				else if( isCompoundNumericPlug( yPlug ) )
				{
					for( Plug::InputIterator it( yPlug ); !it.done(); ++it )
					{
						sourceContext.scope.reset();
						if( auto sourceComponent = connectionSource( it->get(), sourceContext ) )
						{
							inputs.push_back( std::make_tuple( i, "." + (*it)->getName().string(), outputParameterForPlug( sourceComponent ) ) );
						}
					}
				}
			}

			if( !inputs.size() )
			{
				return;
			}

			std::vector< int > applySort( n );

			{
				std::vector< std::pair< float, unsigned int > > ordering;
				ordering.reserve( n );
				for( int i = 0; i < n; i++ )
				{
					ordering.push_back( std::make_pair( parameter->pointXPlug( i )->getValue(), i ) );
				}
				std::sort( ordering.begin(), ordering.end() );

				for( int i = 0; i < n; i++ )
				{
					applySort[ ordering[i].second ] = i;
				}
			}

			SplineDefinitionInterpolation interp = (SplineDefinitionInterpolation)parameter->interpolationPlug()->getValue();
			int endPointDupes = 0;
			// \todo : Need to duplicate the logic from SplineDefinition::endPointMultiplicity
			// John requested an explicit notice that we are displeased by this duplication.
			// Possible alternatives to this would be storing SplineDefinitionData instead of SplineData
			// in the ShaderNetwork, or moving the handling of endpoint multiplicity inside Splineff
			if( interp == SplineDefinitionInterpolationCatmullRom )
			{
				endPointDupes = 1;
			}
			else if( interp == SplineDefinitionInterpolationBSpline )
			{
				endPointDupes = 2;
			}
			else if( interp == SplineDefinitionInterpolationMonotoneCubic )
			{
				throw IECore::Exception(
					"Cannot support monotone cubic interpolation for splines with inputs, for plug " + parameter->fullName()
				);
			}


			for( const auto &[ origIndex, componentSuffix, sourceParameter ] : inputs )
			{
				int index = applySort[ origIndex ];
				int outIndexMin, outIndexMax;
				if( index == 0 )
				{
					outIndexMin = 0;
					outIndexMax = endPointDupes;
				}
				else if( index == n - 1 )
				{
					outIndexMin = endPointDupes + n - 1;
					outIndexMax = endPointDupes + n - 1 + endPointDupes;
				}
				else
				{
					outIndexMin = outIndexMax = index + endPointDupes;
				}

				for( int i = outIndexMin; i <= outIndexMax; i++ )
				{
					IECore::InternedString inputName = fmt::format(
						FMT_COMPILE( "{}[{}].y{}" ),
						parameterName.string(), i, componentSuffix
					);
					connections.push_back( {
						sourceParameter,
						{ IECore::InternedString(), inputName }
					} );
				}
			}
		}

		const Plug *m_output;
		IECoreScene::ShaderNetworkPtr m_network;

		// Maps from `{ node, contextHash }` to a hash which uniquely identifies
		// the shader produced by the node in that context.
		using ShaderHashMap = std::unordered_map<ShaderAndHash, IECore::MurmurHash>;
		ShaderHashMap m_shaderHashes;

		// Maps from the hashes above to the shaders uniquely identified by each hash.
		// We include the node in the key to avoid sharing shaders between different nodes.
		using ShaderMap = std::unordered_map<ShaderAndHash, IECore::InternedString>;
		ShaderMap m_shaders;

		CycleDetector::DownstreamShaders m_downstreamShaders;

		bool m_hasProxyNodes;

};

//////////////////////////////////////////////////////////////////////////
// Shader implementation
//////////////////////////////////////////////////////////////////////////

static IECore::InternedString g_nodeColorMetadataName( "nodeGadget:color" );

GAFFER_NODE_DEFINE_TYPE( Shader );

size_t Shader::g_firstPlugIndex = 0;
const IECore::InternedString Shader::g_outputParameterContextName( "scene:shader:outputParameter" );

Shader::Shader( const std::string &name )
	:	ComputeNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "name", Gaffer::Plug::In, "", Plug::Default & ~Plug::Serialisable ) );
	addChild( new StringPlug( "type", Gaffer::Plug::In, "", Plug::Default & ~Plug::Serialisable ) );
	addChild( new StringPlug( "attributeSuffix", Gaffer::Plug::In, "" ) );
	addChild( new Plug( "parameters", Plug::In, Plug::Default & ~Plug::AcceptsInputs ) );
	addChild( new BoolPlug( "enabled", Gaffer::Plug::In, true ) );
	addChild( new StringPlug( "__nodeName", Gaffer::Plug::In, name, Plug::Default & ~(Plug::Serialisable | Plug::AcceptsInputs), IECore::StringAlgo::NoSubstitutions ) );
	addChild( new Color3fPlug( "__nodeColor", Gaffer::Plug::In, Color3f( 0.0f ) ) );
	nodeColorPlug()->setFlags( Plug::Serialisable | Plug::AcceptsInputs, false );
	addChild( new CompoundObjectPlug( "__outAttributes", Plug::Out, new IECore::CompoundObject ) );

	Metadata::nodeValueChangedSignal( this ).connect( boost::bind( &Shader::nodeMetadataChanged, this, ::_2 ) );
}

Shader::~Shader()
{
}

Gaffer::StringPlug *Shader::namePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *Shader::namePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

Gaffer::StringPlug *Shader::typePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::StringPlug *Shader::typePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

Gaffer::StringPlug *Shader::attributeSuffixPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 2 );
}

const Gaffer::StringPlug *Shader::attributeSuffixPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 2 );
}

Gaffer::Plug *Shader::parametersPlug()
{
	return getChild<Plug>( g_firstPlugIndex + 3 );
}

const Gaffer::Plug *Shader::parametersPlug() const
{
	return getChild<Plug>( g_firstPlugIndex + 3 );
}

Gaffer::Plug *Shader::outPlug()
{
	// not getting by index, because it is created by the
	// derived classes in loadShader().
	return getChild<Plug>( g_outPlugName );
}

const Gaffer::Plug *Shader::outPlug() const
{
	return getChild<Plug>( g_outPlugName );
}

Gaffer::BoolPlug *Shader::enabledPlug()
{
	return getChild<BoolPlug>( g_firstPlugIndex + 4 );
}

const Gaffer::BoolPlug *Shader::enabledPlug() const
{
	return getChild<BoolPlug>( g_firstPlugIndex + 4 );
}

Gaffer::StringPlug *Shader::nodeNamePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 5 );
}

const Gaffer::StringPlug *Shader::nodeNamePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 5 );
}

Gaffer::Color3fPlug *Shader::nodeColorPlug()
{
	return getChild<Color3fPlug>( g_firstPlugIndex + 6 );
}

const Gaffer::Color3fPlug *Shader::nodeColorPlug() const
{
	return getChild<Color3fPlug>( g_firstPlugIndex + 6 );
}

Gaffer::CompoundObjectPlug *Shader::outAttributesPlug()
{
	return getChild<CompoundObjectPlug>( g_firstPlugIndex + 7 );
}

const Gaffer::CompoundObjectPlug *Shader::outAttributesPlug() const
{
	return getChild<CompoundObjectPlug>( g_firstPlugIndex + 7 );
}

IECore::MurmurHash Shader::attributesHash() const
{
	return outAttributesPlug()->hash();
}

void Shader::attributesHash( IECore::MurmurHash &h ) const
{
	outAttributesPlug()->hash( h );
}

IECore::ConstCompoundObjectPtr Shader::attributes() const
{
	return outAttributesPlug()->getValue();
}

bool Shader::affectsAttributes( const Gaffer::Plug *input ) const
{
	return
		parametersPlug()->isAncestorOf( input ) ||
		input == enabledPlug() ||
		input == nodeNamePlug() ||
		input == namePlug() ||
		input == typePlug() ||
		input->parent<Plug>() == nodeColorPlug() ||
		input == attributeSuffixPlug()
	;
}

void Shader::attributesHash( const Gaffer::Plug *output, IECore::MurmurHash &h ) const
{
	attributeSuffixPlug()->hash( h );

	NetworkBuilder networkBuilder( output );
	h.append( networkBuilder.networkHash() );
}

IECore::ConstCompoundObjectPtr Shader::attributes( const Gaffer::Plug *output ) const
{
	IECore::CompoundObjectPtr result = new IECore::CompoundObject;
	NetworkBuilder networkBuilder( output );
	if( networkBuilder.network()->size() )
	{
		std::string attr = typePlug()->getValue();
		std::string postfix = attributeSuffixPlug()->getValue();
		if( postfix != "" )
		{
			attr += ":" + postfix;
		}
		result->members()[attr] = boost::const_pointer_cast<IECoreScene::ShaderNetwork>( networkBuilder.network() );
	}
	return result;
}

void Shader::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	ComputeNode::affects( input, outputs );

	if( affectsAttributes( input ) )
	{
		outputs.push_back( outAttributesPlug() );
	}

	if( input == outAttributesPlug() )
	{
		// Our `outPlug()` is the one that actually gets connected into
		// the ShaderPlug on ShaderAssignment etc. But `ShaderPlug::attributes()`
		// pulls on `outAttributesPlug()`, so when that is dirtied, we should
		// also dirty `outPlug()` to propagate dirtiness to ShaderAssignments.

		if( const Plug *out = outPlug() )
		{
			if( !out->children().empty() )
			{
				for( Plug::RecursiveIterator it( out ); !it.done(); it++ )
				{
					if( (*it)->children().empty() )
					{
						outputs.push_back( it->get() );
					}
				}
			}
			else
			{
				outputs.push_back( out );
			}
		}
	}
}

void Shader::loadShader( const std::string &shaderName, bool keepExistingValues )
{
	// A base shader doesn't know anything about what sort of parameters you might want to load.
	//
	// The only reason why this isn't pure virtual is because it is occasionally useful to
	// manually create a shader type which doesn't actually correspond to any real shader on disk.
	// IERendering using this to create a generic mesh light shader which is later translated into
	// the correct shader type for whichever renderer you are using.  Similarly, ArnoldDisplacement
	// doesn't need a loadShader override because it's not really a shader.
}

void Shader::reloadShader()
{
	// Sub-classes should take care of any necessary cache clearing before calling this

	loadShader( namePlug()->getValue(), true );
}

void Shader::hash( const Gaffer::ValuePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( output == outAttributesPlug() )
	{
		ComputeNode::hash( output, context, h );
		const Plug *outputParameter = outPlug();
		std::optional<Context::EditableScope> cleanContext;
		if( const std::string *name = context->getIfExists< std::string >( g_outputParameterContextName ) )
		{
			outputParameter = outputParameter->descendant<Plug>( *name );
			cleanContext.emplace( context );
			cleanContext->remove( g_outputParameterContextName );
		}
		attributesHash( outputParameter, h );
		return;
	}
	else if( const Plug *o = outPlug() )
	{
		if( output == o || o->isAncestorOf( output ) )
		{
			if( !enabledPlug()->getValue() )
			{
				if( auto input = IECore::runTimeCast<const ValuePlug>( correspondingInput( output ) ) )
				{
					ComputeNode::hash( output, context, h );
					input->hash( h );
					// Account for potential type conversions.
					h.append( input->typeId() );
					h.append( output->typeId() );
					return;
				}
			}
			h = output->defaultHash();
			return;
		}
	}

	ComputeNode::hash( output, context, h );
}

void Shader::compute( Gaffer::ValuePlug *output, const Gaffer::Context *context ) const
{
	if( output == outAttributesPlug() )
	{
		const Plug *outputParameter = outPlug();
		std::optional<Context::EditableScope> cleanContext;
		if( const std::string *name = context->getIfExists< std::string >( g_outputParameterContextName ) )
		{
			outputParameter = outputParameter->descendant<Plug>( *name );
			cleanContext.emplace( context );
			cleanContext->remove( g_outputParameterContextName );
		}
		static_cast<CompoundObjectPlug *>( output )->setValue( attributes( outputParameter ) );
		return;
	}
	else if( const Plug *o = outPlug() )
	{
		if( output == o || o->isAncestorOf( output ) )
		{
			if( !enabledPlug()->getValue() )
			{
				if( auto input = IECore::runTimeCast<const ValuePlug>( correspondingInput( output ) ) )
				{
					output->setFrom( input );
					return;
				}
			}
			output->setToDefault();
			return;
		}
	}

	ComputeNode::compute( output, context );
}

void Shader::parameterHash( const Gaffer::Plug *parameterPlug, IECore::MurmurHash &h ) const
{
	if( auto valuePlug = IECore::runTimeCast<const ValuePlug>( parameterPlug ) )
	{
		valuePlug->hash( h );
	}
}

IECore::DataPtr Shader::parameterValue( const Gaffer::Plug *parameterPlug ) const
{
	if( auto valuePlug = IECore::runTimeCast<const Gaffer::ValuePlug>( parameterPlug ) )
	{
		return Gaffer::PlugAlgo::getValueAsData( valuePlug );
	}

	return nullptr;
}

void Shader::nameChanged( IECore::InternedString oldName )
{
	nodeNamePlug()->setValue( getName() );
}

void Shader::nodeMetadataChanged( IECore::InternedString key )
{
	if( key == g_nodeColorMetadataName )
	{
		IECore::ConstColor3fDataPtr d = Metadata::value<const IECore::Color3fData>( this, g_nodeColorMetadataName );
		nodeColorPlug()->setValue( d ? d->readable() : Color3f( 0.0f ) );
	}
}

const ValuePlug *Shader::parameterSource( const Plug *output, const IECoreScene::ShaderNetwork::Parameter &parameter ) const
{
	Context::EditableScope cleanContext( Context::current() );
	cleanContext.remove( g_outputParameterContextName );

	NetworkBuilder networkBuilder( output );
	if( networkBuilder.network()->size() )
	{
		return networkBuilder.parameterSource( parameter );
	}

	return nullptr;
}
