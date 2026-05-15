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
#include "IECore/DataAlgo.h"
#include "IECore/NullObject.h"
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
		case Color3fPlugTypeId :
			functor( static_cast<const Color3fPlug *>( plug ) );
			break;
		case Color4fPlugTypeId :
			functor( static_cast<const Color4fPlug *>( plug ) );
			break;
		default :
			break;
	}
}

template<typename InputPlugType>
struct StatsTraits
{
	using SumDataType = TypedData<typename InputPlugType::ValueType>;
	using MinMaxDataType = TypedData<typename InputPlugType::ValueType>;
};

template<>
struct StatsTraits<BoolPlug>
{
	using SumDataType = IntData;
	using MinMaxDataType = BoolData;
};

template<typename T>
struct StatsTraits<CompoundNumericPlug<Vec2<T>>>
{
	using SumDataType = GeometricTypedData<Vec2<T>>;
	using MinMaxDataType = GeometricTypedData<Vec2<T>>;
};

template<typename T>
struct StatsTraits<CompoundNumericPlug<Vec3<T>>>
{
	using SumDataType = GeometricTypedData<Vec3<T>>;
	using MinMaxDataType = GeometricTypedData<Vec3<T>>;
};

/// \todo This and `vectorAwareMax()` are borrowed from `TweakPlug.cpp`. Is there
/// anywhere we could share them?
template<typename T>
T vectorAwareMin( const T &v1, const T &v2 )
{
	if constexpr( IECore::TypeTraits::IsVec<T>::value || IECore::TypeTraits::IsColor<T>::value )
	{
		T result;
		for( size_t i = 0; i < T::dimensions(); ++i )
		{
			result[i] = std::min( v1[i], v2[i] );
		}
		return result;
	}
	else
	{
		return std::min( v1, v2 );
	}
}

template<typename T>
T vectorAwareMax( const T &v1, const T &v2 )
{
	if constexpr( IECore::TypeTraits::IsVec<T>::value || IECore::TypeTraits::IsColor<T>::value )
	{
		T result;
		for( size_t i = 0; i < T::dimensions(); ++i )
		{
			result[i] = std::max( v1[i], v2[i] );
		}
		return result;
	}
	else
	{
		return std::max( v1, v2 );
	}
}

// Utility class for accumulating statistics from repeated
// evaluations of a plug.
struct PlugStats
{

	template<typename InputPlugType>
	void update( const InputPlugType *inputPlug )
	{
		using SumDataType = typename StatsTraits<InputPlugType>::SumDataType;
		using MinMaxDataType = typename StatsTraits<InputPlugType>::MinMaxDataType;
		if( !sum )
		{
			sum = new SumDataType( inputPlug->getValue() );
			min = new MinMaxDataType( inputPlug->getValue() );
			max = new MinMaxDataType( inputPlug->getValue() );
		}
		else
		{
			static_cast<SumDataType *>( sum.get() )->writable() += inputPlug->getValue();
			auto typedMin = static_cast<MinMaxDataType *>( min.get() );
			typedMin->writable() = vectorAwareMin( typedMin->readable(), inputPlug->getValue() );
			auto typedMax = static_cast<MinMaxDataType *>( max.get() );
			typedMax->writable() = vectorAwareMax( typedMax->readable(), inputPlug->getValue() );
		}
		count++;
	}

	template<typename InputPlugType>
	void update( const InputPlugType *inputPlug, const PlugStats &other )
	{
		if( !sum )
		{
			*this = other;
			return;
		}
		else
		{
			using SumDataType = typename StatsTraits<InputPlugType>::SumDataType;
			using MinMaxDataType = typename StatsTraits<InputPlugType>::MinMaxDataType;
			static_cast<SumDataType *>( sum.get() )->writable() += static_cast<const SumDataType *>( other.sum.get() )->readable();
			static_cast<MinMaxDataType *>( min.get() )->writable() = vectorAwareMin(
				static_cast<const MinMaxDataType *>( min.get() )->readable(),
				static_cast<const MinMaxDataType *>( other.min.get() )->readable()
			);
			static_cast<MinMaxDataType *>( max.get() )->writable() = vectorAwareMax(
				static_cast<const MinMaxDataType *>( max.get() )->readable(),
				static_cast<const MinMaxDataType *>( other.max.get() )->readable()
			);
			count += other.count;
		}
	}

	IECore::DataPtr sum;
	IECore::DataPtr min;
	IECore::DataPtr max;
	size_t count = 0;

};

// Holds a map from name to PlugStats in a form suitable for storage
// on a plug. // TODO RENAME INTERNAL PLUG AND MATCH NAMES>
struct StatsData : public IECore::Data
{

	void update( const ValuePlug *queriesPlug )
	{
		for( const auto &queryPlug : OptionalValuePlug::Range( *queriesPlug ) )
		{
			if( queryPlug->enabledPlug()->getValue() )
			{
				dispatchPlugFunction(
					queryPlug->valuePlug(), [&] ( auto *plug ) { map[queryPlug->getName()].update( plug ); }
				);
			}
		}
	}

	void update( const ValuePlug *queriesPlug, const StatsData &other )
	{
		for( const auto &queryPlug : OptionalValuePlug::Range( *queriesPlug ) )
		{
			dispatchPlugFunction(
				queryPlug->valuePlug(), [&] ( auto *plug ) {
					auto it = other.map.find( queryPlug->getName() );
					if( it != other.map.end() )
					{
						map[queryPlug->getName()].update( plug, it->second );
					}
				}
			);
		}
	}

	IE_CORE_DECLAREMEMBERPTR( StatsData )
	using Map = std::unordered_map<InternedString, PlugStats>;
	Map map;

};

const InternedString g_count( "count" );
const InternedString g_sum( "sum" );
const InternedString g_min( "min" );
const InternedString g_max( "max" );

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
	addChild( new ObjectPlug( "__internalData", Plug::Out, NullObject::defaultNullObject() ) );
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

Gaffer::ObjectPlug *SceneStats::internalDataPlug()
{
	return getChild<ObjectPlug>( g_firstPlugIndex + 4 );
}

const Gaffer::ObjectPlug *SceneStats::internalDataPlug() const
{
	return getChild<ObjectPlug>( g_firstPlugIndex + 4 );
}

Gaffer::OptionalValuePlug *SceneStats::addQuery( const Gaffer::ValuePlug *plug, const std::string &name )
{
	const std::string actualName = name.empty() ? plug->getName().string() : name;
	ValuePlugPtr valuePlug = boost::static_pointer_cast<ValuePlug>( plug->createCounterpart( actualName, Plug::In ) );
	OptionalValuePlugPtr inChild = new OptionalValuePlug( actualName, valuePlug, true );

	PlugPtr outChild = new ValuePlug( actualName, Plug::Out );
	dispatchPlugFunction(
		plug, [&] ( auto *plug ) {
			using InputPlugType = remove_const_t<remove_pointer_t<decltype( plug )>>;
			outChild->addChild( new IntPlug( g_count, Plug::Out ) );
			using SumType = typename StatsTraits<InputPlugType>::SumDataType::ValueType;
			using SumPlugType = typename PlugType<SumType>::Type;
			outChild->addChild( new SumPlugType( g_sum, Plug::Out ) );
			using MinMaxType = typename StatsTraits<InputPlugType>::MinMaxDataType::ValueType;
			using MinMaxPlugType = typename PlugType<MinMaxType>::Type;
			outChild->addChild( new MinMaxPlugType( g_min, Plug::Out ) );
			outChild->addChild( new MinMaxPlugType( g_max, Plug::Out ) );
		}
	);

	queriesPlug()->addChild( inChild );
	outPlug()->addChild( outChild );

	return inChild.get();
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

const Gaffer::OptionalValuePlug *SceneStats::queryPlug( const Gaffer::ValuePlug *outputPlug ) const
{
	const ValuePlug *p = outputPlug;
	while( p->parent<Plug>() != outPlug() )
	{
		p = p->parent<ValuePlug>();
	}
	return queriesPlug()->getChild<OptionalValuePlug>( p->getName() );
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
		for( const auto &output : ValuePlug::RecursiveRange( *outPlug() ) )
		{
			if( output->children().empty() )
			{
				outputs.push_back( output.get() );
			}
		}
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
			/// TODO : SHOULD WE MOVE THE HASH INTO STATSDATA?
			IECore::MurmurHash locationHash;
			for( const OptionalValuePlugPtr &child : OptionalValuePlug::Range( *queriesPlug() ) )
			{
				locationHash.append( child->getName() );
				if( child->enabledPlug()->getValue() )
				{
					child->hash( locationHash );
				}
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
		internalDataPlug()->hash( h );
		// We also use the plug's name in the compute, but ComputeNode
		// includes that in the hash for us anyway.
	}
	else
	{
		ComputeNode::hash( output, context, h );
	}
}

void SceneStats::compute( Gaffer::ValuePlug *output, const Gaffer::Context *context ) const
{
	if( output == internalDataPlug() )
	{
		tbb::enumerable_thread_specific<StatsData> threadStats;

		auto functor = [&]( const ScenePlug *scene, const ScenePlug::ScenePath &path ) -> bool
		{
			threadStats.local().update( queriesPlug() );
			return true;
		};
		SceneAlgo::filteredParallelTraverse( scenePlug(), filterPlug(), functor );

		StatsData::Ptr result = new StatsData;
		for( const auto &statsData : threadStats )
		{
			result->update( queriesPlug(), statsData );
		}

		static_cast<ObjectPlug *>( output )->setValue( result );
	}
	else if( outPlug()->isAncestorOf( output ) )
	{
		auto [topOutput, statOutput] = outPlugAncestors( output );

		StatsData::ConstPtr data = boost::static_pointer_cast<const StatsData>( internalDataPlug()->getValue() );
		auto it = data->map.find( topOutput->getName() );
		if( it == data->map.end() )
		{
			// We didn't collect anything.
			output->setToDefault();
		}
		else
		{
			const auto &stats = it->second;
			if( statOutput->getName() == g_count )
			{
				static_cast<IntPlug *>( output )->setValue( stats.count );
			}
			else if( statOutput->getName() == g_sum )
			{
				PlugAlgo::setValueFromData( statOutput, output, stats.sum.get() );
			}
			else if( statOutput->getName() == g_min )
			{
				PlugAlgo::setValueFromData( statOutput, output, stats.min.get() );
			}
			else if( statOutput->getName() == g_max )
			{
				PlugAlgo::setValueFromData( statOutput, output, stats.max.get() );
			}
		}
	}
	else
	{
		ComputeNode::compute( output, context );
	}
}

std::tuple<const ValuePlug *, const ValuePlug *> SceneStats::outPlugAncestors( const Gaffer::ValuePlug *output ) const
{
	const ValuePlug *parentPlug = output->parent<ValuePlug>();
	while( parentPlug->parent() != outPlug() )
	{
		output = parentPlug;
		parentPlug = output->parent<ValuePlug>();
	}

	return { parentPlug, output };
}
