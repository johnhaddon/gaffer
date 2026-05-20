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

#include "GafferScene/PointInstancer.h"

// #include "Gaffer/Context.h"
#include "Gaffer/StringPlug.h"
// #include "Gaffer/Private/IECorePreview/LRUCache.h"

#include "IECoreScene/PointInstancer.h"

// TODO : REMOVE WHAT WE DON'T NEED

#include "IECore/DataAlgo.h"
#include "IECore/MessageHandler.h"
#include "IECore/ObjectVector.h"
#include "IECore/NullObject.h"
#include "IECore/VectorTypedData.h"

#include "boost/lexical_cast.hpp"
#include "boost/unordered_set.hpp"

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/spin_mutex.h"

#include "fmt/format.h"

#include <functional>
#include <unordered_map>

// TODO : REMOVE WHAT WE DON'T NEED
using namespace std;
using namespace std::placeholders;
using namespace tbb;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

// namespace
// {

// const PrimitiveVariable *findVertexVariable( const IECoreScene::Primitive* primitive, const InternedString &name )
// {
// 	PrimitiveVariableMap::const_iterator it = primitive->variables.find( name );
// 	if( it == primitive->variables.end() )
// 	{
// 		return nullptr;
// 	}

// 	if(
// 		it->second.interpolation == IECoreScene::PrimitiveVariable::Vertex ||
// 		(
// 			it->second.interpolation == IECoreScene::PrimitiveVariable::Varying &&
// 			primitive->variableSize( PrimitiveVariable::Vertex ) == primitive->variableSize( PrimitiveVariable::Varying )
// 		)
// 	)
// 	{
// 		return &it->second;
// 	}

// 	return nullptr;

// }

// // We need to able to quantize all our basic numeric values, so we have a set of templates for this, with
// // a special exception if you try to use a non-zero quantize on a type that can't be quantize ( ie. a string ).
// //
// // We quantize by forcing a value to the closest value that is a multiple of quantize.  For vector types,
// // this is done independently for each axis.
// class QuantizeException {};

// template <class T>
// inline T quantize( const T &v, float q )
// {
// 	if( q != 0.0f )
// 	{
// 		throw QuantizeException();
// 	}
// 	return v;
// }

// template <>
// inline float quantize( const float &v, float q )
// {
// 	if( q == 0.0f )
// 	{
// 		return v;
// 	}
// 	// \todo : Higher performance round
// 	float r = q * round( v / q );

// 	// Letting negative zeros slip through is confusing because they hash to different values
// 	if( r == 0 )
// 	{
// 		r = 0;
// 	}
// 	return r;
// }

// template <>
// inline int quantize( const int &v, float q )
// {
// 	if( q == 0.0f )
// 	{
// 		return v;
// 	}
// 	int intQuantize = round( q );
// 	if( intQuantize == 0 )
// 	{
// 		return v;
// 	}
// 	int halfQuantize = intQuantize / 2;
// 	return intQuantize * ( ( v + halfQuantize ) / intQuantize );
// }

// template <class T>
// inline Vec2<T> quantize( const Vec2<T> &v, float q )
// {
// 	return Vec2<T>( quantize( v[0], q ), quantize( v[1], q ) );
// }

// template <class T>
// inline Vec3<T> quantize( const Vec3<T> &v, float q )
// {
// 	return Vec3<T>( quantize( v[0], q ), quantize( v[1], q ), quantize( v[2], q ) );
// }

// template <>
// inline Color3f quantize( const Color3f &v, float q )
// {
// 	return Color3f( quantize( v[0], q ), quantize( v[1], q ), quantize( v[2], q ) );
// }

// template <>
// inline Color4f quantize( const Color4f &v, float q )
// {
// 	return Color4f( quantize( v[0], q ), quantize( v[1], q ), quantize( v[2], q ), quantize( v[3], q ) );
// }

// // An internal struct for storing everything we need to know about a context modification we're making
// // when accessing the prototypes scene
// struct PrototypeContextVariable
// {
// 	InternedString name;               // Name of context variable
// 	const PrimitiveVariable *primVar;  // Primitive variable that drives it
// 	float quantize;                    // The interval we quantize to
// 	bool offsetMode;                   // Special mode for adding to existing variable instead of replacing
// 	bool seedMode;                     // Special mode for seed context which is driven from the id
// 	int numSeeds;                      // When in seedMode, the number of distinct seeds to output
// 	int seedScramble;                  // A random seed that affects how seeds are generated
// };


// // A functor for use with IECore::dispatch that sets a variable in a context, based on a PrototypeContextVariable
// // struct
// struct AccessPrototypeContextVariable
// {
// 	template< class T>
// 	void operator()( const TypedData<vector<T>> *data, const PrototypeContextVariable &v, size_t index, Context::EditableScope &scope )
// 	{
// 		T raw = PrimitiveVariable::IndexedView<T>( *v.primVar )[index];
// 		T value = quantize( raw, v.quantize );
// 		scope.setAllocated( v.name, value );
// 	}

// 	void operator()( const TypedData<vector<float>> *data, const PrototypeContextVariable &v, size_t index, Context::EditableScope &scope )
// 	{
// 		float raw = PrimitiveVariable::IndexedView<float>( *v.primVar )[index];
// 		float value = quantize( raw, v.quantize );

// 		if( v.offsetMode )
// 		{
// 			scope.setAllocated( v.name, value + scope.context()->get<float>( v.name ) );
// 		}
// 		else
// 		{
// 			scope.setAllocated( v.name, value );
// 		}
// 	}

// 	void operator()( const TypedData<vector<int>> *data, const PrototypeContextVariable &v, size_t index, Context::EditableScope &scope )
// 	{
// 		int raw = PrimitiveVariable::IndexedView<int>( *v.primVar )[index];
// 		int value = quantize( raw, v.quantize );

// 		if( v.offsetMode )
// 		{
// 			scope.setAllocated( v.name, float(value) + scope.context()->get<float>( v.name ) );
// 		}
// 		else
// 		{
// 			scope.setAllocated( v.name, value );
// 		}
// 	}

// 	void operator()( const Data *data, const PrototypeContextVariable &v, size_t index, Context::EditableScope &scope )
// 	{
// 		throw IECore::Exception( "Context variable prim vars must contain vector data" );
// 	}
// };

// // A functor for use with IECore::dispatch that adds to a hash, based on a PrototypeContextVariable
// // struct.  This is only used to count the number of unique hashes, so we can take some shortcuts, for
// // example, we ignore the offsetMode, because adding the offsets to a different global time doesn't change
// // the number of unique offsets.  We also ignore the name of the context variable, since we always process
// // the same PrototypeContextVariables in the same order
// struct UniqueHashPrototypeContextVariable
// {
// 	template< class T>
// 	void operator()( const TypedData<vector<T>> *data, const PrototypeContextVariable &v, size_t index, MurmurHash &contextHash )

// 	{
// 		T raw = PrimitiveVariable::IndexedView<T>( *v.primVar )[index];
// 		T value = quantize( raw, v.quantize );
// 		contextHash.append( value );
// 	}

// 	void operator()( const Data *data, const PrototypeContextVariable &v, int index, MurmurHash &contextHash )
// 	{
// 		throw IECore::Exception( "Context variable prim vars must contain vector data" );
// 	}
// };

// InternedString g_prototypeRootName( "root" );
// ConstInternedStringVectorDataPtr g_emptyNames = new InternedStringVectorData();

// struct IdData
// {
// 	IdData() :
// 		intElements( nullptr ), int64Elements( nullptr )
// 	{
// 	}

// 	void initialize( const Primitive *primitive, const std::string &name )
// 	{
// 		if( const IntVectorData *intData = primitive->variableData<IntVectorData>( name ) )
// 		{
// 			intElements = &intData->readable();
// 		}
// 		else if( const Int64VectorData *int64Data = primitive->variableData<Int64VectorData>( name ) )
// 		{
// 			int64Elements = &int64Data->readable();
// 		}

// 	}

// 	size_t size() const
// 	{
// 		if( intElements )
// 		{
// 			return intElements->size();
// 		}
// 		else if( int64Elements )
// 		{
// 			return int64Elements->size();
// 		}
// 		else
// 		{
// 			return 0;
// 		}
// 	}

// 	int64_t element( size_t i ) const
// 	{
// 		if( intElements )
// 		{
// 			return (*intElements)[i];
// 		}
// 		else
// 		{
// 			return (*int64Elements)[i];
// 		}
// 	}

// 	const std::vector<int> *intElements;
// 	const std::vector<int64_t> *int64Elements;

// };

// // We create a seed integer that corresponds to the id by hashing the id and then modulo'ing to
// // numSeeds, to create seeds in the range 0 .. numSeeds-1 that persistently correspond to the ids,
// // with a grouping pattern that can be changed with seedScramble
// int seedForPoint( size_t index, const IdData &idData, int numSeeds, int seedScramble )
// {
// 	int64_t id = index;
// 	if( idData.size() )
// 	{
// 		id = idData.element( index );
// 	}

// 	// numSeeds is set to 0 when we're just passing through the id
// 	if( numSeeds != 0 )
// 	{
// 		// The method used for random generation of seeds is actually rather important.
// 		// We need a random access RNG which allows evaluating any input id independently,
// 		// and should not create lattice artifacts if interpreted as a spacial attribute
// 		// such as size.  This is actually a somewhat demanding set of criteria - many
// 		// easy to seed RNGs with a small state space could create lattice artifacts.
// 		//
// 		// Using MurmurHash doesn't seem conceptually perfect, but it uses code we already
// 		// have around, should perform fairly well ( might help if the constructor was inlined ),
// 		// and I've tested for lattice artifacts by generating 200 000 points with Y set to
// 		// seedId, and X set to point index.  These points looked good, with even distribution
// 		// and no latticing, so this is probably a reasonable approach to stick with

// 		IECore::MurmurHash seedHash;
// 		seedHash.append( seedScramble );

// 		if( id <= INT32_MAX && id >= INT_MIN )
// 		{
// 			// This branch shouldn't be needed, we'd like to just treat ids as 64 bit now ...
// 			// but if we just took the branch below, that would changing the seeding of existing
// 			// scenes.
// 			seedHash.append( (int)id );
// 		}
// 		else
// 		{
// 			seedHash.append( id );
// 		}

// 		id = int( ( double( seedHash.h1() ) / double( UINT64_MAX ) ) * double( numSeeds ) );
// 		id = id % numSeeds;  // For the rare case h1 / max == 1.0, make sure we stay in range
// 	}
// 	return id;
// }

// std::atomic<int> g_instancerCount( 0 );

// bool checkEnvFlag( const char *envVar, bool def )
// {
// 	const char *value = getenv( envVar );
// 	if( value )
// 	{
// 		return std::string( value ) != "0";
// 	}
// 	else
// 	{
// 		return def;
// 	}
// }

// }

GAFFER_NODE_DEFINE_TYPE( PointInstancer );

size_t PointInstancer::g_firstPlugIndex = 0;

PointInstancer::PointInstancer( const std::string &name )
	:	BranchCreator( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	// addChild( new StringPlug( "name", Plug::In, "instances" ) );
	// addChild( new ScenePlug( "prototypes" ) );
	// addChild( new IntPlug( "prototypeMode", Plug::In, (int)PrototypeMode::IndexedRootsList, /* min */ (int)PrototypeMode::IndexedRootsList, /* max */ (int)PrototypeMode::RootPerVertex ) );
	// addChild( new StringPlug( "prototypeIndex", Plug::In, "instanceIndex" ) );
	// addChild( new StringPlug( "prototypeRoots", Plug::In, "prototypeRoots" ) );
	// addChild( new StringVectorDataPlug( "prototypeRootsList", Plug::In, new StringVectorData ) );
	// addChild( new StringPlug( "id", Plug::In, "instanceId" ) );
	// addChild( new BoolPlug( "omitDuplicateIds", Plug::In, true ) );
	// addChild( new StringPlug( "position", Plug::In, "P" ) );
	// addChild( new StringPlug( "orientation", Plug::In ) );
	// addChild( new StringPlug( "scale", Plug::In ) );
	// addChild( new StringPlug( "inactiveIds", Plug::In, "" ) );
	// addChild( new StringPlug( "attributes", Plug::In ) );
	// addChild( new StringPlug( "attributePrefix", Plug::In ) );
	// addChild( new BoolPlug( "encapsulate", Plug::In ) );
	// addChild( new BoolPlug( "seedEnabled", Plug::In ) );
	// addChild( new StringPlug( "seedVariable", Plug::In, "seed" ) );
	// addChild( new IntPlug( "seeds", Plug::In, 10, 1 ) );
	// addChild( new IntPlug( "seedPermutation", Plug::In ) );
	// addChild( new BoolPlug( "rawSeed", Plug::In ) );
	// addChild( new ValuePlug( "contextVariables", Plug::In ) );
	// addChild( new ContextVariablePlug( "timeOffset", Plug::In, false, Plug::Flags::Default ) );
	// addChild( new AtomicCompoundDataPlug( "variations", Plug::Out, new CompoundData() ) );
	// addChild( new ObjectPlug( "__engine", Plug::Out, NullObject::defaultNullObject() ) );
	// addChild( new ObjectPlug( "__engineSplitPrototypes", Plug::Out, NullObject::defaultNullObject() ) );
	// addChild( new ScenePlug( "__capsuleScene", Plug::Out ) );
	// addChild( new PathMatcherDataPlug( "__setCollaborate", Plug::Out, new IECore::PathMatcherData() ) );
	// addChild( new Int64VectorDataPlug( "__capsuleComputedHash", Plug::Out ) );

	// \todo : This should be a const member var, not a plug, but that would break ABI. Remove this plug
	// and add the member next time we break ABI.
	// addChild( new IntPlug( "__nodeIdPlug", Plug::In, g_instancerCount.fetch_add( 1 ) ) );

	// Hide `destination` plug until we resolve issues surrounding `processesRootObject()`.
	// See `BranchCreator::computeObject()`.
	destinationPlug()->setName( "__destination" );
	copySourceAttributesPlug()->setName( "__copySourceAttributes" );
}

PointInstancer::~PointInstancer()
{
}

// Gaffer::StringPlug *Instancer::namePlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex );
// }

// const Gaffer::StringPlug *Instancer::namePlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex );
// }

// ScenePlug *Instancer::prototypesPlug()
// {
// 	return getChild<ScenePlug>( g_firstPlugIndex + 1 );
// }

// const ScenePlug *Instancer::prototypesPlug() const
// {
// 	return getChild<ScenePlug>( g_firstPlugIndex + 1 );
// }

// Gaffer::IntPlug *Instancer::prototypeModePlug()
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 2 );
// }

// const Gaffer::IntPlug *Instancer::prototypeModePlug() const
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 2 );
// }

// Gaffer::StringPlug *Instancer::prototypeIndexPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 3 );
// }

// const Gaffer::StringPlug *Instancer::prototypeIndexPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 3 );
// }

// Gaffer::StringPlug *Instancer::prototypeRootsPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 4 );
// }

// const Gaffer::StringPlug *Instancer::prototypeRootsPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 4 );
// }

// Gaffer::StringVectorDataPlug *Instancer::prototypeRootsListPlug()
// {
// 	return getChild<StringVectorDataPlug>( g_firstPlugIndex + 5 );
// }

// const Gaffer::StringVectorDataPlug *Instancer::prototypeRootsListPlug() const
// {
// 	return getChild<StringVectorDataPlug>( g_firstPlugIndex + 5 );
// }

// Gaffer::StringPlug *Instancer::idPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 6 );
// }

// const Gaffer::StringPlug *Instancer::idPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 6 );
// }

// Gaffer::BoolPlug *Instancer::omitDuplicateIdsPlug()
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 7 );
// }

// const Gaffer::BoolPlug *Instancer::omitDuplicateIdsPlug() const
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 7 );
// }

// Gaffer::StringPlug *Instancer::positionPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 8 );
// }

// const Gaffer::StringPlug *Instancer::positionPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 8 );
// }

// Gaffer::StringPlug *Instancer::orientationPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 9 );
// }

// const Gaffer::StringPlug *Instancer::orientationPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 9 );
// }

// Gaffer::StringPlug *Instancer::scalePlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 10 );
// }

// const Gaffer::StringPlug *Instancer::scalePlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 10 );
// }

// Gaffer::StringPlug *Instancer::inactiveIdsPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 11 );
// }

// const Gaffer::StringPlug *Instancer::inactiveIdsPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 11 );
// }

// Gaffer::StringPlug *Instancer::attributesPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 12 );
// }

// const Gaffer::StringPlug *Instancer::attributesPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 12 );
// }

// Gaffer::StringPlug *Instancer::attributePrefixPlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 13 );
// }

// const Gaffer::StringPlug *Instancer::attributePrefixPlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 13 );
// }

// Gaffer::BoolPlug *Instancer::encapsulatePlug()
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 14 );
// }

// const Gaffer::BoolPlug *Instancer::encapsulatePlug() const
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 14 );
// }

// Gaffer::BoolPlug *Instancer::seedEnabledPlug()
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 15 );
// }

// const Gaffer::BoolPlug *Instancer::seedEnabledPlug() const
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 15 );
// }

// Gaffer::StringPlug *Instancer::seedVariablePlug()
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 16 );
// }

// const Gaffer::StringPlug *Instancer::seedVariablePlug() const
// {
// 	return getChild<StringPlug>( g_firstPlugIndex + 16 );
// }

// Gaffer::IntPlug *Instancer::seedsPlug()
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 17 );
// }

// const Gaffer::IntPlug *Instancer::seedsPlug() const
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 17 );
// }

// Gaffer::IntPlug *Instancer::seedPermutationPlug()
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 18 );
// }

// const Gaffer::IntPlug *Instancer::seedPermutationPlug() const
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 18 );
// }

// Gaffer::BoolPlug *Instancer::rawSeedPlug()
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 19 );
// }

// const Gaffer::BoolPlug *Instancer::rawSeedPlug() const
// {
// 	return getChild<BoolPlug>( g_firstPlugIndex + 19 );
// }

// Gaffer::ValuePlug *Instancer::contextVariablesPlug()
// {
// 	return getChild<ValuePlug>( g_firstPlugIndex + 20 );
// }

// const Gaffer::ValuePlug *Instancer::contextVariablesPlug() const
// {
// 	return getChild<ValuePlug>( g_firstPlugIndex + 20 );
// }

// GafferScene::Instancer::ContextVariablePlug *Instancer::timeOffsetPlug()
// {
// 	return getChild<ContextVariablePlug>( g_firstPlugIndex + 21 );
// }

// const GafferScene::Instancer::ContextVariablePlug *Instancer::timeOffsetPlug() const
// {
// 	return getChild<ContextVariablePlug>( g_firstPlugIndex + 21 );
// }

// Gaffer::AtomicCompoundDataPlug *Instancer::variationsPlug()
// {
// 	return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 22 );
// }

// const Gaffer::AtomicCompoundDataPlug *Instancer::variationsPlug() const
// {
// 	return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 22 );
// }

// Gaffer::ObjectPlug *Instancer::enginePlug()
// {
// 	return getChild<ObjectPlug>( g_firstPlugIndex + 23 );
// }

// const Gaffer::ObjectPlug *Instancer::enginePlug() const
// {
// 	return getChild<ObjectPlug>( g_firstPlugIndex + 23 );
// }

// Gaffer::ObjectPlug *Instancer::engineSplitPrototypesPlug()
// {
// 	return getChild<ObjectPlug>( g_firstPlugIndex + 24 );
// }

// const Gaffer::ObjectPlug *Instancer::engineSplitPrototypesPlug() const
// {
// 	return getChild<ObjectPlug>( g_firstPlugIndex + 24 );
// }

// GafferScene::ScenePlug *Instancer::capsuleScenePlug()
// {
// 	return getChild<ScenePlug>( g_firstPlugIndex + 25 );
// }

// const GafferScene::ScenePlug *Instancer::capsuleScenePlug() const
// {
// 	return getChild<ScenePlug>( g_firstPlugIndex + 25 );
// }

// Gaffer::PathMatcherDataPlug *Instancer::setCollaboratePlug()
// {
// 	return getChild<PathMatcherDataPlug>( g_firstPlugIndex + 26 );
// }

// const Gaffer::PathMatcherDataPlug *Instancer::setCollaboratePlug() const
// {
// 	return getChild<PathMatcherDataPlug>( g_firstPlugIndex + 26 );
// }

// Gaffer::Int64VectorDataPlug *Instancer::capsuleComputedHashPlug()
// {
// 	return getChild<Int64VectorDataPlug>( g_firstPlugIndex + 27 );
// }

// const Gaffer::Int64VectorDataPlug *Instancer::capsuleComputedHashPlug() const
// {
// 	return getChild<Int64VectorDataPlug>( g_firstPlugIndex + 27 );
// }

// Gaffer::IntPlug *Instancer::nodeIdPlug()
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 28 );
// }

// const Gaffer::IntPlug *Instancer::nodeIdPlug() const
// {
// 	return getChild<IntPlug>( g_firstPlugIndex + 28 );
// }

bool PointInstancer::affectsBranchBound( const Gaffer::Plug *input ) const
{
	return input == outPlug()->childBoundsPlug();
	// 	input == engineSplitPrototypesPlug() ||
	// 	input == namePlug() ||
	// 	input == prototypesPlug()->boundPlug() ||
	// 	input == prototypesPlug()->transformPlug() ||
	// 	input == outPlug()->childBoundsPlug()
	// ;
}

void PointInstancer::hashBranchBound( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() < 2 )
	{
		h = outPlug()->childBoundsPlug()->hash();
	}
	// else if( branchPath.size() == 2 )
	// {
	// 	// "/instances/<prototypeName>"
	// 	BranchCreator::hashBranchBound( sourcePath, branchPath, context, h );

	// 	engineHash( sourcePath, context, h );
	// 	h.append( branchPath.back() );

	// 	{
	// 		PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );

	// 		prototypesPlug()->transformPlug()->hash( h );
	// 		prototypesPlug()->boundPlug()->hash( h );
	// 	}
	// }
	// else
	// {
	// 	// "/instances/<prototypeName>/<id>/..."
	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	h = prototypesPlug()->boundPlug()->hash();
	// }
}

Imath::Box3f PointInstancer::computeBranchBound( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	//if( branchPath.size() < 2 )
	{
		// "/" or "/instances"
		return outPlug()->childBoundsPlug()->getValue();
	}
	// else if( branchPath.size() == 2 )
	// {
	// 	// "/instances/<prototypeName>"
	// 	//
	// 	// We need to return the union of all the transformed children, but
	// 	// because we have direct access to the engine, we can implement this
	// 	// more efficiently than `ScenePlug::childBounds()`.

	// 	ConstEngineSplitPrototypesDataPtr esp = engineSplitPrototypes( sourcePath, context );
	// 	const EngineData *e = esp->engine();

	// 	M44f childTransform;
	// 	Box3f childBound;
	// 	{
	// 		PrototypeScope scope( esp->engine(), context, &sourcePath, &branchPath );
	// 		childTransform = prototypesPlug()->transformPlug()->getValue();
	// 		childBound = prototypesPlug()->boundPlug()->getValue();
	// 	}

	// 	const std::vector<size_t> &pointIndicesForPrototype = esp->pointIndicesForPrototype( branchPath.back() );

	// 	// TODO - might be worth using a looser approximation - expand point cloud bound by largest diagonal of
	// 	// prototype bound x largest scale. Especially since this isn't fully accurate anyway: we are getting a
	// 	// single bound for the prototype with no context variables set, which may have nothing to do with actual
	// 	// prototype we get once the context variables are set.
	// 	task_group_context taskGroupContext( task_group_context::isolated );
	// 	return parallel_reduce(
	// 		tbb::blocked_range<size_t>( 0, pointIndicesForPrototype.size() ),
	// 		Box3f(),
	// 		[ pointIndicesForPrototype, &e, &childBound, &childTransform ] ( const tbb::blocked_range<size_t> &r, Box3f u ) {
	// 			for( size_t i = r.begin(); i != r.end(); ++i )
	// 			{
	// 				const size_t pointIndex = pointIndicesForPrototype[i];
	// 				const M44f m = childTransform * e->instanceTransform( pointIndex );
	// 				const Box3f b = transform( childBound, m );
	// 				u.extendBy( b );
	// 			}
	// 			return u;
	// 		},
	// 		// Union
	// 		[] ( const Box3f &b0, const Box3f &b1 ) {
	// 			Box3f u( b0 );
	// 			u.extendBy( b1 );
	// 			return u;
	// 		},
	// 		tbb::auto_partitioner(),
	// 		// Prevents outer tasks silently cancelling our tasks
	// 		taskGroupContext
	// 	);
	// }
	// else
	// {
	// 	// "/instances/<prototypeName>/<id>/..."
	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	return prototypesPlug()->boundPlug()->getValue();
	// }
}

bool PointInstancer::affectsBranchTransform( const Gaffer::Plug *input ) const
{
	return false;
}

void PointInstancer::hashBranchTransform( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<prototypeName>" // TODO
		BranchCreator::hashBranchTransform( sourcePath, branchPath, context, h );
	}
}

Imath::M44f PointInstancer::computeBranchTransform( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	//if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<prototypeName>" // TODO
		return M44f();
	}
}

bool PointInstancer::affectsBranchAttributes( const Gaffer::Plug *input ) const
{
	return false;
}

void PointInstancer::hashBranchAttributes( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<prototypeName>"
		h = outPlug()->attributesPlug()->defaultValue()->Object::hash();
	}
}

IECore::ConstCompoundObjectPtr PointInstancer::computeBranchAttributes( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	//if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<prototypeName>"
		return outPlug()->attributesPlug()->defaultValue();
	}
}

bool PointInstancer::processesRootObject() const
{
	return true;
}

bool PointInstancer::affectsBranchObject( const Gaffer::Plug *input ) const
{
	return input == inPlug()->objectPlug();
	// 	input == prototypesPlug()->objectPlug() ||
	// 	input == enginePlug()
	// ;
}

void PointInstancer::hashBranchObject( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() == 0 )
	{
		BranchCreator::hashBranchObject( sourcePath, branchPath, context, h );
		inPlug()->objectPlug()->hash( h );
	}
	else if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<prototypeName>"
		h = outPlug()->objectPlug()->defaultValue()->Object::hash();
	}
	// else
	// {
	// 	// "/instances/<prototypeName>/<id>/...
	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	h = prototypesPlug()->objectPlug()->hash();
	// }
}

IECore::ConstObjectPtr PointInstancer::computeBranchObject( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	if( branchPath.size() == 0 )
	{
		ConstObjectPtr inputObject = inPlug()->objectPlug()->getValue();
		const auto primitive = runTimeCast<const IECoreScene::Primitive>( inputObject.get() );
		if( !primitive )
		{
			return inputObject.get();
		}

		IECoreScene::PointInstancerPtr result = new IECoreScene::PointInstancer(
			primitive->variableSize( IECoreScene::PrimitiveVariable::Vertex )
		);

		for( const auto &[name, primitiveVariable ] : primitive->variables )
		{
			if( primitiveVariable.interpolation == IECoreScene::PrimitiveVariable::Constant )
			{
				result->variables[name] = primitiveVariable;
			}
			else if( primitive->variableSize( primitiveVariable.interpolation ) == result->getNumPoints() )
			{
				result->variables[name] = IECoreScene::PrimitiveVariable(
					IECoreScene::PrimitiveVariable::Vertex, primitiveVariable.data, primitiveVariable.indices
				);
			}
		}
		return result;
	}
	//if( branchPath.size() <= 2 )
	else
	{
		// "/" or "/instances" or "/instances/<prototypeName>"
		return outPlug()->objectPlug()->defaultValue();
	}
	// else
	// {
	// 	// "/instances/<prototypeName>/<id>/...
	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	return prototypesPlug()->objectPlug()->getValue();
	// }
}

bool PointInstancer::affectsBranchChildNames( const Gaffer::Plug *input ) const
{
	return false;
}

void PointInstancer::hashBranchChildNames( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	h = outPlug()->childNamesPlug()->defaultHash();
	// if( branchPath.size() == 0 )
	// {
	// 	// "/"
	// 	BranchCreator::hashBranchChildNames( sourcePath, branchPath, context, h );
	// 	namePlug()->hash( h );
	// }
	// else if( branchPath.size() == 1 )
	// {
	// 	// "/instances"
	// 	BranchCreator::hashBranchChildNames( sourcePath, branchPath, context, h );
	// 	engineHash( sourcePath, context, h );
	// }
	// else if( branchPath.size() == 2 )
	// {
	// 	// "/instances/<prototypeName>"
	// 	BranchCreator::hashBranchChildNames( sourcePath, branchPath, context, h );
	// 	engineSplitPrototypesHash( sourcePath, context, h );
	// 	h.append( branchPath.back() );

	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	h.append( prototypesPlug()->existsPlug()->hash() );
	// }
	// else
	// {
	// 	// "/instances/<prototypeName>/<id>/..."
	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	h = prototypesPlug()->childNamesPlug()->hash();
	// }
}

IECore::ConstInternedStringVectorDataPtr PointInstancer::computeBranchChildNames( const ScenePath &sourcePath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	return outPlug()->childNamesPlug()->defaultValue();
	// if( branchPath.size() == 0 )
	// {
	// 	// "/"
	// 	std::string name = namePlug()->getValue();
	// 	if( name.empty() )
	// 	{
	// 		return outPlug()->childNamesPlug()->defaultValue();
	// 	}
	// 	InternedStringVectorDataPtr result = new InternedStringVectorData();
	// 	result->writable().push_back( name );
	// 	return result;
	// }
	// else if( branchPath.size() == 1 )
	// {
	// 	// "/instances"
	// 	return engine( sourcePath, context )->prototypeNames();
	// }
	// else if( branchPath.size() == 2 )
	// {
	// 	// "/instances/<prototypeName>"

	// 	ConstEngineSplitPrototypesDataPtr esp = engineSplitPrototypes( sourcePath, context );

	// 	const std::vector<size_t> &pointIndicesForPrototype = esp->pointIndicesForPrototype( branchPath.back() );

	// 	// The children of the prototypeName are all the instances which use this prototype,
	// 	// which we can query from the engine - however the names we output under use
	// 	// the ids, not the point indices, and must be sorted. So we need to allocate a
	// 	// temp buffer of integer ids, before converting to strings.

	// 	std::vector<int64_t> ids;
	// 	ids.reserve( pointIndicesForPrototype.size() );

	// 	const EngineData *engineData = esp->engine();

	// 	PrototypeScope scope( engineData, context, &sourcePath, &branchPath );

	// 	if( !prototypesPlug()->existsPlug()->getValue() )
	// 	{
	// 		throw IECore::Exception(
	// 			fmt::format(
	// 				"Prototype root \"{}\" does not exist in the `prototypes` scene",
	// 				ScenePlug::pathToString( scope.context()->get<ScenePath>( ScenePlug::scenePathContextName ) )
	// 			)
	// 		);
	// 	}

	// 	for( size_t q : pointIndicesForPrototype )
	// 	{
	// 		ids.push_back( engineData->instanceId( q ) );
	// 	}

	// 	// Sort ids before converting to string ( they have already been uniquified but not sorted by
	// 	// the EngineData which uses a hash table )
	// 	std::sort( ids.begin(), ids.end() );

	// 	InternedStringVectorDataPtr childNamesData = new InternedStringVectorData;
	// 	std::vector<InternedString> &childNames = childNamesData->writable();
	// 	childNames.reserve( ids.size() );
	// 	for( int64_t id : ids )
	// 	{
	// 		childNames.emplace_back( id );
	// 	}

	// 	return childNamesData;
	// }
	// else
	// {
	// 	// "/instances/<prototypeName>/<id>/..."
	// 	PrototypeScope scope( enginePlug(), context, &sourcePath, &branchPath );
	// 	return prototypesPlug()->childNamesPlug()->getValue();
	// }
}

bool PointInstancer::affectsBranchSetNames( const Gaffer::Plug *input ) const
{
	return false;
}

void PointInstancer::hashBranchSetNames( const ScenePath &sourcePath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	//assert( sourcePath.size() == 0 ); // Expectation driven by `constantBranchSetNames() == true` TODO
	h = outPlug()->setNamesPlug()->defaultHash();
}

IECore::ConstInternedStringVectorDataPtr PointInstancer::computeBranchSetNames( const ScenePath &sourcePath, const Gaffer::Context *context ) const
{
	//assert( sourcePath.size() == 0 ); // Expectation driven by `constantBranchSetNames() == true` TODO
	return outPlug()->setNamesPlug()->defaultValue();
}

bool PointInstancer::affectsBranchSet( const Gaffer::Plug *input ) const
{
	return false;
	// 	input == enginePlug() ||
	// 	input == engineSplitPrototypesPlug() ||
	// 	input == prototypesPlug()->setPlug() ||
	// 	input == namePlug() ||
	// 	input == setCollaboratePlug()
	// ;
}

void PointInstancer::hashBranchSet( const ScenePath &sourcePath, const IECore::InternedString &setName, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	BranchCreator::hashBranchSet( sourcePath, setName, context, h );

	// // If we have context variables, we need to do a much more expensive evaluation of the prototype set
	// // plug in every instance context.  We allow task collaboration on this expensive evaluation by redirecting
	// // to an internal plug when we have context variables.  We could request hasContextVariables off the engine,
	// // but we don't need to evaluate the engine here, so instead we make a conservative hasContextVariables
	// // based on whether the source plugs have been touched
	// bool hasContextVariables =
	// 	( !timeOffsetPlug()->enabledPlug()->isSetToDefault() && !timeOffsetPlug()->namePlug()->isSetToDefault() ) ||
	// 	!seedEnabledPlug()->isSetToDefault();

	// for( ContextVariablePlug::Iterator it( contextVariablesPlug() ); !it.done() && !hasContextVariables; ++it )
	// {
	// 	const ContextVariablePlug *plug = it->get();

	// 	hasContextVariables |=
	// 		( plug->enabledPlug()->getInput() || plug->enabledPlug()->getValue() ) &&
	// 		!plug->namePlug()->isSetToDefault();
	// }

	// if( hasContextVariables )
	// {
	// 	Context::EditableScope scope( context );
	// 	scope.set( ScenePlug::scenePathContextName, &sourcePath );
	// 	setCollaboratePlug()->hash( h );
	// }
	// else
	// {
	// 	engineHash( sourcePath, context, h );
	// 	engineSplitPrototypesHash( sourcePath, context, h );
	// 	prototypesPlug()->setPlug()->hash( h );
	// 	namePlug()->hash( h );
	// }
}

IECore::ConstPathMatcherDataPtr PointInstancer::computeBranchSet( const ScenePath &sourcePath, const IECore::InternedString &setName, const Gaffer::Context *context ) const
{
	return new PathMatcherData;
	// ConstEngineDataPtr engine = this->engine( sourcePath, context );

	// if( engine->hasContextVariables() )
	// {
	// 	// When doing the much expensive work required when we have context variables, we try to share the
	// 	// work between multiple threads using an internal PathMatcher plug with a TaskCollaborate policy.
	// 	// The setCollaborate plug does all the heavy work.  It is evaluated with the sourcePath in the
	// 	// context's scenePath, and it returns a PathMatcher for the set contents of one branch.
	// 	Context::EditableScope scope( context );
	// 	scope.set( ScenePlug::scenePathContextName, &sourcePath );
	// 	return setCollaboratePlug()->getValue();
	// }

	// ConstEngineSplitPrototypesDataPtr esp = engineSplitPrototypes( sourcePath, context );

	// ConstPathMatcherDataPtr inputSet = prototypesPlug()->setPlug()->getValue();

	// PathMatcherDataPtr outputSetData = new PathMatcherData;
	// PathMatcher &outputSet = outputSetData->writable();

	// vector<InternedString> branchPath( { namePlug()->getValue(), InternedString(), InternedString() } );

	// for( const auto &prototypeName : engine->prototypeNames()->readable() )
	// {
	// 	const std::vector<size_t> &pointIndicesForPrototype = esp->pointIndicesForPrototype( prototypeName );

	// 	ScenePlug::ScenePath prototypeRootStorage;
	// 	PathMatcher instanceSet = inputSet->readable().subTree( *engine->prototypeRoot( prototypeName, sourcePath, prototypeRootStorage ) );
	// 	branchPath[1] = prototypeName;

	// 	for( const size_t &index : pointIndicesForPrototype )
	// 	{
	// 		branchPath[2] = engine->instanceId( index );
	// 		outputSet.addPaths( instanceSet, branchPath );
	// 	}
	// }

	// return outputSetData;
}

// Instancer::PrototypeScope::PrototypeScope( const Gaffer::ObjectPlug *enginePlug, const Gaffer::Context *context, const ScenePath *sourcePath, const ScenePath *branchPath )
// 	:	Gaffer::Context::EditableScope( context )
// {
// 	set( ScenePlug::scenePathContextName, sourcePath );

// 	// Must hold a smart pointer to engine so it can't be freed during the lifespan of this scope
// 	m_engine = boost::static_pointer_cast<const EngineData>( enginePlug->getValue() );

// 	setPrototype( m_engine.get(), sourcePath, branchPath );
// }

// Instancer::PrototypeScope::PrototypeScope( const EngineData *engine, const Gaffer::Context *context, const ScenePath *sourcePath, const ScenePath *branchPath )
// 	:	Gaffer::Context::EditableScope( context )
// {
// 	setPrototype( engine, sourcePath, branchPath );
// }

// void Instancer::PrototypeScope::setPrototype( const EngineData *engine, const ScenePath *sourcePath, const ScenePath *branchPath )
// {
// 	assert( branchPath->size() >= 2 );

// 	// We pass in m_prototypePath as the storage to prototypeRoot() - it may or may not be set,
// 	// becaues prototypeRoot can sometimes just return a pointer without needing to do any allocation.
// 	m_prototypePath.resize( 0 );
// 	const ScenePlug::ScenePath *prototypeRoot = engine->prototypeRoot( (*branchPath)[1], *sourcePath, m_prototypePath );

// 	if( branchPath->size() >= 3 && engine->hasContextVariables() )
// 	{
// 		const size_t pointIndex = engine->pointIndex( (*branchPath)[2] );
// 		engine->setPrototypeContextVariables( pointIndex, *this );
// 	}

// 	if( branchPath->size() > 3 )
// 	{
// 		if( !m_prototypePath.size() )
// 		{
// 			// If prototypeRoot didn't need to do an allocation, we have to do it now so we can modify
// 			// m_prototypePath.
// 			m_prototypePath = *prototypeRoot;
// 		}
// 		m_prototypePath.reserve( prototypeRoot->size() + branchPath->size() - 3 );
// 		m_prototypePath.insert( m_prototypePath.end(), branchPath->begin() + 3, branchPath->end() );
// 		set( ScenePlug::scenePathContextName, &m_prototypePath );
// 	}
// 	else
// 	{
// 		set( ScenePlug::scenePathContextName, prototypeRoot );
// 	}
// }
