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

#include "GafferScene/TemporalFilter.h"

#include "Gaffer/Context.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/StringPlug.h"

#include "IECoreScene/Primitive.h"

#include "IECore/DataAlgo.h"
#include "IECore/StringAlgo.h"
#include "IECore/TypeTraits.h"

#include "tbb/blocked_range.h"
#include "tbb/parallel_reduce.h"

#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

using namespace std;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

namespace
{

template<typename DataType>
constexpr bool isFilterable()
{
	if( TypeTraits::IsTypedData<DataType>::value )
	{
		if(	std::is_same_v<typename DataType::BaseType, float> )
		{
			return true;
		}
	}
	return false;
}

template<typename T>
void accumulateValues( const T *sourceBegin, T *destBegin, T *destEnd, TemporalFilter::Filter filter, float weight )
{
	switch( filter )
	{
		case TemporalFilter::Filter::Min :
			while( destBegin != destEnd )
			{
				*destBegin = std::min( sourceBegin ? *sourceBegin++ : 0, *destBegin );
				destBegin++;
			}
			break;
		case TemporalFilter::Filter::Max :
			while( destBegin != destEnd )
			{
				*destBegin = std::max( sourceBegin ? *sourceBegin++ : 0, *destBegin );
				destBegin++;
			}
			break;
		default :
			if( sourceBegin && weight != 0.0f )
			{
				while( destBegin != destEnd )
				{
					*destBegin += *sourceBegin++ * weight;
					destBegin++;
				}
			}
			break;
	}
}

template<typename DataType>
void accumulateTyped( const std::string &name, const PrimitiveVariable &sourcePrimitiveVariable, TemporalFilter::Filter filter, float weight, Primitive *destPrimitive )
{
	using BaseType = typename DataType::BaseType;
	const DataType *sourceData = static_cast<const DataType *>( sourcePrimitiveVariable.data.get() );
	const size_t baseSize = sourceData->baseSize();

	auto it = destPrimitive->variables.find( name );
	if( it != destPrimitive->variables.end() )
	{
		// Accumulate into PrimitiveVariable we initialised earlier.
		PrimitiveVariable &destPrimitiveVariable = it->second;
		if(
			!sourcePrimitiveVariable.data->isInstanceOf( destPrimitiveVariable.data->typeId() ) ||
			sourcePrimitiveVariable.interpolation != destPrimitiveVariable.interpolation ||
			(bool)sourcePrimitiveVariable.indices != (bool)destPrimitiveVariable.indices ||
			(sourcePrimitiveVariable.indices && !sourcePrimitiveVariable.indices->isEqualTo( destPrimitiveVariable.indices.get() ))
		)
		{
			// Not compatible
			return;
		}

		DataType *destData = static_cast<DataType *>( destPrimitiveVariable.data.get() );
		assert( destData->baseSize() == numBaseValues );

		accumulateValues(
			sourceData->baseReadable(),
			destData->baseWritable(), destData->baseWritable() + baseSize,
			filter, weight
		);
	}
	else
	{
		// Initialise new PrimitiveVariable.
		typename DataType::Ptr destData;
		destData = sourceData->copy();
		if( filter != TemporalFilter::Filter::Min && filter != TemporalFilter::Filter::Max )
		{
			for( auto p = destData->baseWritable(), e = destData->baseWritable() + baseSize; p != e; ++p )
			{
				*p = *p * weight;
			}
		}
		destPrimitive->variables[name] = PrimitiveVariable(
			sourcePrimitiveVariable.interpolation, destData,
			sourcePrimitiveVariable.indices ? sourcePrimitiveVariable.indices->copy() : nullptr
		);
	}
}

void accumulate( const std::string &name, const PrimitiveVariable &sourcePrimitiveVariable, TemporalFilter::Filter filter, float weight, Primitive *destPrimitive )
{
	if( !destPrimitive->isPrimitiveVariableValid( sourcePrimitiveVariable ) )
	{
		return;
	}

	IECore::dispatch(

		sourcePrimitiveVariable.data.get(),

		[&]( auto *typedData ) -> void
		{
			using DataType = std::remove_pointer_t<decltype( typedData )>;
			if constexpr( isFilterable<DataType>() )
			{
				accumulateTyped<DataType>( name, sourcePrimitiveVariable, filter, weight, destPrimitive );
			}
		}
	);
}

} // namespace

GAFFER_NODE_DEFINE_TYPE( TemporalFilter );

size_t TemporalFilter::g_firstPlugIndex = 0;

TemporalFilter::TemporalFilter( const std::string &name )
	:	Deformer( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new StringPlug( "primitiveVariables", Plug::In, "" ) );

	ValuePlugPtr startPlug = new ValuePlug( "start", Plug::In );
	startPlug->addChild( new IntPlug( "mode", Plug::In, (int)FrameMode::Relative, (int)FrameMode::Relative, (int)FrameMode::Absolute ) );
	startPlug->addChild( new FloatPlug( "frame", Plug::In, -2 ) );
	addChild( startPlug );

	ValuePlugPtr endPlug = new ValuePlug( "end", Plug::In );
	endPlug->addChild( new IntPlug( "mode", Plug::In, (int)FrameMode::Relative, (int)FrameMode::Relative, (int)FrameMode::Absolute ) );
	endPlug->addChild( new FloatPlug( "frame", Plug::In, 2 ) );
	addChild( endPlug );

	addChild( new IntPlug( "samplingMode", Plug::In, (int)SamplingMode::Variable, (int)SamplingMode::Variable, (int)SamplingMode::Fixed ) );
	addChild( new FloatPlug( "step", Plug::In, 1, 1e-6 ) );
	addChild( new IntPlug( "samples", Plug::In, 10, 2 ) );

	addChild( new IntPlug( "filterType", Plug::In, (int)Filter::Gaussian, (int)Filter::Box, (int)Filter::Ramp ) );
	addChild( new RampffPlug( "ramp", Plug::In, IECore::Rampff( { { 0.0f, 0.0f }, { 1.0f, 1.0f } }, IECore::RampInterpolation::CatmullRom ) ) );
}

TemporalFilter::~TemporalFilter()
{
}

StringPlug *TemporalFilter::primitiveVariablesPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const StringPlug *TemporalFilter::primitiveVariablesPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

IntPlug *TemporalFilter::startModePlug()
{
	return getChild<ValuePlug>( g_firstPlugIndex + 1 )->getChild<IntPlug>( 0 );
}

const IntPlug *TemporalFilter::startModePlug() const
{
	return getChild<ValuePlug>( g_firstPlugIndex + 1 )->getChild<IntPlug>( 0 );
}

FloatPlug *TemporalFilter::startFramePlug()
{
	return getChild<ValuePlug>( g_firstPlugIndex + 1 )->getChild<FloatPlug>( 1 );
}

const FloatPlug *TemporalFilter::startFramePlug() const
{
	return getChild<ValuePlug>( g_firstPlugIndex + 1 )->getChild<FloatPlug>( 1 );
}

IntPlug *TemporalFilter::endModePlug()
{
	return getChild<ValuePlug>( g_firstPlugIndex + 2 )->getChild<IntPlug>( 0 );
}

const IntPlug *TemporalFilter::endModePlug() const
{
	return getChild<ValuePlug>( g_firstPlugIndex + 2 )->getChild<IntPlug>( 0 );
}

FloatPlug *TemporalFilter::endFramePlug()
{
	return getChild<ValuePlug>( g_firstPlugIndex + 2 )->getChild<FloatPlug>( 1 );
}

const FloatPlug *TemporalFilter::endFramePlug() const
{
	return getChild<ValuePlug>( g_firstPlugIndex + 2 )->getChild<FloatPlug>( 1 );
}

IntPlug *TemporalFilter::samplingModePlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 3 );
}

const IntPlug *TemporalFilter::samplingModePlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 3 );
}

FloatPlug *TemporalFilter::stepPlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 4 );
}

const FloatPlug *TemporalFilter::stepPlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 4 );
}

IntPlug *TemporalFilter::samplesPlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 5 );
}

const IntPlug *TemporalFilter::samplesPlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 5 );
}

IntPlug *TemporalFilter::filterTypePlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 6 );
}

const IntPlug *TemporalFilter::filterTypePlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 6 );
}

RampffPlug *TemporalFilter::rampPlug()
{
	return getChild<RampffPlug>( g_firstPlugIndex + 7 );
}

const RampffPlug *TemporalFilter::rampPlug() const
{
	return getChild<RampffPlug>( g_firstPlugIndex + 7 );
}

bool TemporalFilter::adjustBounds() const
{
	if( !Deformer::adjustBounds() )
	{
		return false;
	}
	return StringAlgo::matchMultiple( "P", primitiveVariablesPlug()->getValue() );
}

bool TemporalFilter::affectsProcessedObject( const Gaffer::Plug *input ) const
{
	return
		Deformer::affectsProcessedObject( input ) ||
		input == primitiveVariablesPlug() ||
		input == startModePlug() ||
		input == startFramePlug() ||
		input == endModePlug() ||
		input == endFramePlug() ||
		input == samplingModePlug() ||
		input == stepPlug() ||
		input == samplesPlug() ||
		input == filterTypePlug() ||
		rampPlug()->isAncestorOf( input )
	;
}

vector<float> TemporalFilter::sampleFrames( const Context *context ) const
{
	// This is the same logic used by MotionPath.

	const float start = ( (FrameMode)startModePlug()->getValue() == FrameMode::Absolute ) ? startFramePlug()->getValue() : context->getFrame() + startFramePlug()->getValue();
	const float end = ( (FrameMode)endModePlug()->getValue() == FrameMode::Absolute ) ? endFramePlug()->getValue() : context->getFrame() + endFramePlug()->getValue();
	if( start >= end )
	{
		return {};
	}

	float step;
	int samples;
	if( (SamplingMode)samplingModePlug()->getValue() == SamplingMode::Variable )
	{
		step = stepPlug()->getValue();
		samples = (int)ceil( ( end - start ) / step - 1e-6f ) + 1;
	}
	else
	{
		samples = samplesPlug()->getValue();
		step = ( end - start ) / ( samples - 1 );
	}

	vector<float> frames;
	frames.reserve( samples );
	for( int i = 0; i < samples - 1; ++i )
	{
		frames.push_back( start + step * i );
	}
	frames.push_back( end );
	return frames;
}

vector<float> TemporalFilter::sampleWeights( const vector<float> &frames ) const
{
	vector<float> weights( frames.size() );

	const Filter filter = (Filter)filterTypePlug()->getValue();
	switch( filter )
	{
		case Filter::Box :
		case Filter::Min :
		case Filter::Max :
			fill( weights.begin(), weights.end(), 1.0f );
			break;
		case Filter::Gaussian :
		{
			for( size_t i = 0; i < frames.size(); ++i )
			{
				float x = ( frames[i] - frames.front() ) / ( frames.back() - frames.front() ); // 0 to 1
				x = 2.0f * ( x - 0.5f ); // -1 to 1
				const float w = exp( -2.0f * x * x );
				weights[i] = w;
			}
			break;
		}
		case Filter::Ramp :
		{
			auto evaluator = rampPlug()->getValue().evaluator();
			for( size_t i = 0; i < frames.size(); ++i )
			{
				const float x = ( frames[i] - frames.front() ) / ( frames.back() - frames.front() );
				weights[i] = evaluator( x );
			}
			break;
		}
	}

	// Normalise

	const float sum = std::accumulate( weights.begin(), weights.end(), 0.0f );
	if( sum > 0.0f )
	{
		for( auto &weight : weights )
		{
			weight /= sum;
		}
	}

	return weights;
}

void TemporalFilter::hashProcessedObject( const ScenePath &path, const Context *context, MurmurHash &h ) const
{
	Deformer::hashProcessedObject( path, context, h );

	const vector<float> frames = sampleFrames( context );
	if( frames.empty() )
	{
		h = inPlug()->objectPlug()->hash();
		return;
	}

	primitiveVariablesPlug()->hash( h );

	filterTypePlug()->hash( h ); // Needed to distinguish Min and Max mode, which have the same weights.
	const vector<float> weights = sampleWeights( frames );

	/// \todo We to use `parallel_deterministic_reduce()` here too,
	/// but there's no `ObjectProcessor::processedObjectHashCachePolicy()`
	/// we can override to use TaskCollaboration.

	Context::EditableScope scope( context );
	for( size_t i = 0; i < frames.size(); ++i )
	{
		scope.setFrame( frames[i] );
		inPlug()->objectPlug()->hash( h );
		h.append( weights[i] );
	}
}

IECore::ConstObjectPtr TemporalFilter::computeProcessedObject( const ScenePath &path, const Context *context, const IECore::Object *inputObject ) const
{
	const Primitive *inputPrimitive = runTimeCast<const Primitive>( inputObject );
	if( !inputPrimitive )
	{
		return inputObject;
	}

	const vector<float> frames = sampleFrames( context );
	if( frames.empty() )
	{
		return inputObject;
	}

	const string primitiveVariables = primitiveVariablesPlug()->getValue();
	if( primitiveVariables.empty() )
	{
		return inputObject;
	}

	const Filter filter = (Filter)filterTypePlug()->getValue();
	const vector<float> weights = sampleWeights( frames );

	// Start with a copy of the input primitive with all variables
	// removed.

	PrimitivePtr filteredPrimitive = inputPrimitive->copy();
	filteredPrimitive->variables.clear();

	// Process each input frame in parallel, accumulating matching primitive
	// variables into our result.

	tbb::task_group_context taskGroupContext( tbb::task_group_context::isolated );
	const ThreadState &threadState = ThreadState::current();

	filteredPrimitive = tbb::parallel_deterministic_reduce(

		tbb::blocked_range<size_t>( 0, frames.size() ),

		filteredPrimitive,

		[&] ( const tbb::blocked_range<size_t> &range, ConstPrimitivePtr init ) -> PrimitivePtr {

			PrimitivePtr result = init->copy();
			Context::EditableScope frameScope( threadState );
			for( size_t i = range.begin(); i != range.end(); ++i )
			{
				frameScope.setFrame( frames[i] );
				ConstPrimitivePtr primitive = runTimeCast<const Primitive>( inPlug()->objectPlug()->getValue() );
				if( !primitive )
				{
					continue;
				}

				for( const auto &[name, primitiveVariable] : primitive->variables )
				{
					if( StringAlgo::matchMultiple( name, primitiveVariables ) )
					{
						accumulate( name, primitiveVariable, filter, weights[i], result.get() );
					}
				}

				return result;
			}

			return nullptr;

		},

		[&] ( ConstPrimitivePtr x, ConstPrimitivePtr y ) -> PrimitivePtr {

			PrimitivePtr result = x->copy();
			for( const auto &[name, primitiveVariable] : y->variables )
			{
				// Filter weight is 1 because the inputs have been weighted already.
				accumulate( name, primitiveVariable, filter, 1.0f, result.get() );
			}

			return result;
		},

		taskGroupContext

	);

	// Add back any unfiltered variables from the input.
	filteredPrimitive->variables.insert( inputPrimitive->variables.begin(), inputPrimitive->variables.end() );

	return filteredPrimitive;
}

Gaffer::ValuePlug::CachePolicy TemporalFilter::processedObjectComputeCachePolicy() const
{
	return ValuePlug::CachePolicy::TaskCollaboration;
}
