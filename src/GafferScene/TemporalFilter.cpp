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

#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

using namespace std;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

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

	// for( size_t i = 0; i < frames.size(); ++i )
	// {
	// 	fmt::print( "{} : {} : {}\n", i, frames[i], weights[i] );
	// }

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

	// TODO : PARALLEL_FOR

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

	// Fetch the primitive at each sample time.
	// TODO : DON'T STORE EVERYTHING! FILTER ON THE FLY!!! FFS.
	// TODO : PARALLEL_FOR, OR GATHER.
	vector<ConstPrimitivePtr> samplePrims;
	samplePrims.reserve( frames.size() );
	Context::EditableScope scope( context );
	for( float f : frames )
	{
		scope.setFrame( f );
		ConstObjectPtr obj = inPlug()->objectPlug()->getValue();
		samplePrims.push_back( runTimeCast<const Primitive>( obj.get() ) );
	}

	PrimitivePtr result = inputPrimitive->copy();

	for( auto &[name, primitiveVariable] : result->variables )
	{
		if( !StringAlgo::matchMultiple( name, primitiveVariables ) )
		{
			continue;
		}

		IECore::dispatch( primitiveVariable.data.get(),
			[&]( auto *typedData )
			{
				using DataType = std::remove_pointer_t<decltype( typedData )>;

				if constexpr( TypeTraits::IsTypedData<DataType>::value )
				if constexpr( std::is_arithmetic_v<typename DataType::BaseType> )
				{
				{
					using BaseType = typename DataType::BaseType;
					const size_t numBaseValues = typedData->baseSize();

					// TODO : MUST BE MORE COMPACT WAY OF EXPRESSING THIS - MOVE TO A FUNCTION AT LEAST

					if( filter == Filter::Min || filter == Filter::Max )
					{
						const BaseType initVal = ( filter == Filter::Min )
							? std::numeric_limits<BaseType>::max()
							: std::numeric_limits<BaseType>::lowest();

						BaseType *dest = typedData->baseWritable();
						fill( dest, dest + numBaseValues, initVal );

						for( const auto &samplePrim : samplePrims )
						{
							const BaseType *src = nullptr;
							if( samplePrim )
							{
								auto it = samplePrim->variables.find( name );
								if(
									it != samplePrim->variables.end() &&
									it->second.data->typeId() == typedData->typeId()
								)
								{
									const auto *st = static_cast<const DataType *>( it->second.data.get() );
									if( st->baseSize() == numBaseValues )
									{
										src = st->baseReadable();
									}
								}
							}

							if( src )
							{
								if( filter == Filter::Min )
								{
									for( size_t j = 0; j < numBaseValues; ++j )
									{
										dest[j] = std::min( dest[j], src[j] );
									}
								}
								else
								{
									for( size_t j = 0; j < numBaseValues; ++j )
									{
										dest[j] = std::max( dest[j], src[j] );
									}
								}
							}
							else
							{
								// Missing sample contributes zero.
								const BaseType zero{};
								if( filter == Filter::Min )
								{
									for( size_t j = 0; j < numBaseValues; ++j )
									{
										dest[j] = std::min( dest[j], zero );
									}
								}
								else
								{
									for( size_t j = 0; j < numBaseValues; ++j )
									{
										dest[j] = std::max( dest[j], zero );
									}
								}
							}
						}
					}
					else
					{
						// Weighted sum (Box, Gaussian, Ramp).
						vector<float> accumulated( numBaseValues, 0.0f );

						for( size_t si = 0; si < samplePrims.size(); ++si )
						{
							const float w = weights[si];
							if( w == 0 )
							{
								continue;
							}

							const Primitive *samplePrim = samplePrims[si].get();
							if( !samplePrim )
							{
								// Missing sample contributes zero - no-op.
								continue;
							}

							auto it = samplePrim->variables.find( name );
							if(
								it == samplePrim->variables.end() ||
								it->second.data->typeId() != typedData->typeId()
							)
							{
								// Missing or incompatible sample contributes zero - no-op.
								continue;
							}

							const auto *st = static_cast<const DataType *>( it->second.data.get() );
							if( st->baseSize() != numBaseValues )
							{
								continue;
							}

							const BaseType *src = st->baseReadable();
							for( size_t j = 0; j < numBaseValues; ++j )
							{
								accumulated[j] += w * static_cast<float>( src[j] );
							}
						}

						BaseType *dest = typedData->baseWritable();
						for( size_t j = 0; j < numBaseValues; ++j )
						{
							dest[j] = static_cast<BaseType>( accumulated[j] );
						}
					}
				}
				} // IsVectorTypedData || IsSimpleTypedData
			}
		);
	}

	return result;
}
