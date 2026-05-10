//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2026, Cinesite VFX Ltd. All rights reserved.
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

#include "GafferScene/SceneStats.h"

#include "GafferScene/SceneAlgo.h"

#include "Gaffer/CompoundNumericPlug.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/PlugAlgo.h"
#include "Gaffer/PlugType.h"
#include "Gaffer/TypedObjectPlug.h"
#include "Gaffer/TypedPlug.h"

#include "IECore/CompoundData.h"
#include "IECore/SimpleTypedData.h"

#include "tbb/enumerable_thread_specific.h"

#include <atomic>
#include <vector>

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

//////////////////////////////////////////////////////////////////////////
// Internal utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

// If `plug` is a supported type, downcasts it to its true type and
// calls `functor( plug, args )`. Otherwise, does nothing.
/// \todo This is copied from Collect.cpp (but with only a subset of
/// the supported types). Should we consolidate them somewhere?
template<typename F>
void dispatchPlugFunction( const ValuePlug *plug, F &&functor )
{
	switch( (int)plug->typeId() )
	{
		case BoolPlugTypeId :
			functor( static_cast<const BoolPlug *>( plug ) );
			break;
		case IntPlugTypeId :
			functor( static_cast<const IntPlug *>( plug ) );
			break;
		case FloatPlugTypeId :
			functor( static_cast<const FloatPlug *>( plug ) );
			break;
		case V2iPlugTypeId :
			functor( static_cast<const V2iPlug *>( plug ) );
			break;
		case V3iPlugTypeId :
			functor( static_cast<const V3iPlug *>( plug ) );
			break;
		case V2fPlugTypeId :
			functor( static_cast<const V2fPlug *>( plug ) );
			break;
		case V3fPlugTypeId :
			functor( static_cast<const V3fPlug *>( plug ) );
			break;
		// case Color3fPlugTypeId :
		// 	functor( static_cast<const Color3fPlug *>( plug ) );
		// 	break;
		// case Color4fPlugTypeId :
		// 	functor( static_cast<const Color4fPlug *>( plug ) );
		// 	break;
		default :
			break;
	}
}

template<typename T>
struct StatsTraits
{
	using SumDataType = TypedData<T>;
	//using AverageType = float;
};

template<>
struct StatsTraits<bool>
{
	using SumDataType = IntData;
	//using AverageType = float;
};

template<typename T>
struct StatsTraits<Vec2<T>>
{
	using SumDataType = GeometricTypedData<Vec2<T>>;
	//using AverageType = Imath::V2f;
};

template<typename T>
struct StatsTraits<Vec3<T>>
{
	using SumDataType = GeometricTypedData<Vec3<T>>;
	//using AverageType = Imath::V3f;
};

/// TODO : USE RECURSIVE RANGE
void addLeafPlugs( const Gaffer::Plug *plug, Gaffer::DependencyNode::AffectedPlugsContainer &outputs )
{
	if( plug->children().empty() )
	{
		outputs.push_back( plug );
	}
	else
	{
		for( const Gaffer::PlugPtr &child : Gaffer::Plug::OutputRange( *plug ) )
		{
			addLeafPlugs( child.get(), outputs );
		}
	}
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// SceneStats
//////////////////////////////////////////////////////////////////////////

GAFFER_NODE_DEFINE_TYPE( SceneStats )

size_t SceneStats::g_firstPlugIndex = 0;

SceneStats::SceneStats( const std::string &name )
	:	ComputeNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new ScenePlug( "scene" ) );
	addChild( new FilterPlug( "filter" ) );
	addChild( new ValuePlug( "queries", Plug::In ) );
	addChild( new ValuePlug( "out", Plug::Out ) );
	addChild( new AtomicCompoundDataPlug( "__internalData", Plug::Out, new CompoundData ) );
}

SceneStats::~SceneStats()
{
}

GafferScene::ScenePlug *SceneStats::scenePlug()
{
	return getChild<ScenePlug>( g_firstPlugIndex );
}

const GafferScene::ScenePlug *SceneStats::scenePlug() const
{
	return getChild<ScenePlug>( g_firstPlugIndex );
}

GafferScene::FilterPlug *SceneStats::filterPlug()
{
	return getChild<FilterPlug>( g_firstPlugIndex + 1 );
}

const GafferScene::FilterPlug *SceneStats::filterPlug() const
{
	return getChild<FilterPlug>( g_firstPlugIndex + 1 );
}

Gaffer::ValuePlug *SceneStats::queriesPlug()
{
	return getChild<ValuePlug>( g_firstPlugIndex + 2 );
}

const Gaffer::ValuePlug *SceneStats::queriesPlug() const
{
	return getChild<ValuePlug>( g_firstPlugIndex + 2 );
}

Gaffer::ValuePlug *SceneStats::outPlug()
{
	return getChild<ValuePlug>( g_firstPlugIndex + 3 );
}

const Gaffer::ValuePlug *SceneStats::outPlug() const
{
	return getChild<ValuePlug>( g_firstPlugIndex + 3 );
}

Gaffer::AtomicCompoundDataPlug *SceneStats::internalDataPlug()
{
	return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 4 );
}

const Gaffer::AtomicCompoundDataPlug *SceneStats::internalDataPlug() const
{
	return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 4 );
}

Gaffer::ValuePlug *SceneStats::addQuery( const Gaffer::ValuePlug *plug, const std::string &name )
{
	const std::string actualName = name.empty() ? plug->getName().string() : name;
	PlugPtr queryChild = plug->createCounterpart( actualName, Plug::In );
	queryChild->setFlags( Plug::Dynamic, true );

	PlugPtr outChild;
	dispatchPlugFunction(
		plug, [&] ( auto *plug ) {
			using InputPlugType = remove_const_t<remove_pointer_t<decltype( plug )>>;
			using SumType = typename StatsTraits<typename InputPlugType::ValueType>::SumDataType::ValueType;
			using SumPlugType = typename PlugType<SumType>::Type;
			outChild = new SumPlugType( actualName, Plug::Out );
		}
	);

	queriesPlug()->addChild( queryChild );
	outPlug()->addChild( outChild );

	return static_cast<ValuePlug *>( queryChild.get() );
}

void SceneStats::removeQuery( Gaffer::ValuePlug *plug )
{
	ValuePlug *out = outPlug()->getChild<ValuePlug>( plug->getName() );
	queriesPlug()->removeChild( plug );
	outPlug()->removeChild( out );
}

const Gaffer::ValuePlug *SceneStats::outPlugFromQuery( const Gaffer::ValuePlug *queryPlug ) const
{
	return outPlug()->getChild<ValuePlug>( queryPlug->getName() );
}

const Gaffer::ValuePlug *SceneStats::queryPlug( const Gaffer::ValuePlug *outputPlug ) const
{
	const ValuePlug *p = outputPlug;
	while( p->parent<Plug>() != outPlug() )
	{
		p = p->parent<ValuePlug>();
	}
	return queriesPlug()->getChild<ValuePlug>( p->getName() );
}

void SceneStats::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	ComputeNode::affects( input, outputs );

	if( input->parent<Plug>() == scenePlug() )
	{
		filterPlug()->sceneAffects( input, outputs );
		outputs.push_back( internalDataPlug() );
	}

	if( input == filterPlug() )
	{
		outputs.push_back( internalDataPlug() );
	}

	if( queriesPlug()->isAncestorOf( input ) )
	{
		outputs.push_back( internalDataPlug() );
	}

	if( input == internalDataPlug() )
	{
		addLeafPlugs( outPlug(), outputs );
	}
}

void SceneStats::hash( const Gaffer::ValuePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( output == internalDataPlug() )
	{
		ComputeNode::hash( output, context, h );

		std::atomic<uint64_t> h1( 0 ), h2( 0 );
		auto functor = [&]( const ScenePlug *scene, const ScenePlug::ScenePath &path ) -> bool
		{
			IECore::MurmurHash locationHash;
			for( const ValuePlugPtr &child : ValuePlug::Range( *queriesPlug() ) )
			{
				child->hash( locationHash );
			}
			h1 += locationHash.h1();
			h2 += locationHash.h2();
			return true;
		};
		SceneAlgo::filteredParallelTraverse( scenePlug(), filterPlug(), functor );
		h.append( IECore::MurmurHash( h1.load(), h2.load() ) );
	}
	else if( outPlug()->isAncestorOf( output ) )
	{
		ComputeNode::hash( output, context, h );

		const ValuePlug *topOutput = output;
		while( topOutput->parent<Plug>() != outPlug() )
		{
			topOutput = topOutput->parent<ValuePlug>();
		}

		internalDataPlug()->hash( h );
		h.append( topOutput->getName() );
		if( output != topOutput )
		{
			h.append( output->getName() );
		}
	}
}

void SceneStats::compute( Gaffer::ValuePlug *output, const Gaffer::Context *context ) const
{
	if( output == internalDataPlug() )
	{
		using Accumulators = vector<DataPtr>;
		tbb::enumerable_thread_specific<Accumulators> threadAccumulators(
			[this]()
			{
				Accumulators acc;
				for( const auto &queryPlug : ValuePlug::Range( *queriesPlug() ) )
				{
					dispatchPlugFunction(
						queryPlug.get(), [&] ( auto *plug ) {
							using InputPlugType = remove_const_t<remove_pointer_t<decltype( plug )>>;
							using SumDataType = typename StatsTraits<typename InputPlugType::ValueType>::SumDataType;
							acc.push_back( new SumDataType( typename SumDataType::ValueType( 0 ) ) );
						}
					);
				}
				return acc;
			}
		);

		auto functor = [&]( const ScenePlug *scene, const ScenePlug::ScenePath &path ) -> bool
		{
			Accumulators &acc = threadAccumulators.local();
			size_t i = 0;
			for( const auto &queryPlug : ValuePlug::Range( *queriesPlug() ) )
			{
				dispatchPlugFunction(
					queryPlug.get(), [&] ( auto *plug ) {
						using InputPlugType = remove_const_t<remove_pointer_t<decltype( plug )>>;
						using SumDataType = typename StatsTraits<typename InputPlugType::ValueType>::SumDataType;
						static_cast<SumDataType *>( acc[i++].get() )->writable() += plug->getValue();
					}
				);
			}
			return true;
		};
		SceneAlgo::filteredParallelTraverse( scenePlug(), filterPlug(), functor );

		CompoundDataPtr result = new CompoundData;

		size_t i = 0;
		for( const auto &queryPlug : ValuePlug::Range( *queriesPlug() ) )
		{
			dispatchPlugFunction(
				queryPlug.get(), [&] ( auto *plug ) {
					using InputPlugType = remove_const_t<remove_pointer_t<decltype( plug )>>;
					using SumDataType = typename StatsTraits<typename InputPlugType::ValueType>::SumDataType;
					typename SumDataType::Ptr data = new SumDataType( typename SumDataType::ValueType( 0 ) );
					threadAccumulators.combine_each( [&] ( const Accumulators &acc ) {
						data->writable() += static_cast<SumDataType *>( acc[i].get() )->readable();
					} );
					result->writable()[ plug->getName() ] = data;
				}
			);
			++i;
		}

		static_cast<AtomicCompoundDataPlug *>( output )->setValue( result );
		return;
	}
	else if( outPlug()->isAncestorOf( output ) )
	{
		const ValuePlug *topOutput = output;
		while( topOutput->parent<Plug>() != outPlug() )
		{
			topOutput = topOutput->parent<ValuePlug>();
		}

		ConstCompoundDataPtr data = internalDataPlug()->getValue();
		const auto it = data->readable().find( topOutput->getName() );
		assert( it != data->readable().end() );
		PlugAlgo::setValueFromData( topOutput, output, it->second.get() );
		return;
	}

	ComputeNode::compute( output, context );
}
