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

#include "GafferScene/PrimitiveVariablePaint.h"

#include "IECoreScene/Primitive.h"

#include "IECore/DataAlgo.h"

#include "IECore/TypeTraits.h"
#include <unordered_set>

using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

GAFFER_NODE_DEFINE_TYPE( PrimitiveVariablePaint );

size_t PrimitiveVariablePaint::g_firstPlugIndex = 0;

PrimitiveVariablePaint::PrimitiveVariablePaint( const std::string &name )
	:	Deformer( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new AtomicCompoundDataPlug( "paint", Plug::In ) );
}

PrimitiveVariablePaint::~PrimitiveVariablePaint()
{
}

Gaffer::AtomicCompoundDataPlug *PrimitiveVariablePaint::paintPlug()
{
	return getChild<Gaffer::AtomicCompoundDataPlug>( g_firstPlugIndex + 0 );
}

const Gaffer::AtomicCompoundDataPlug *PrimitiveVariablePaint::paintPlug() const
{
	return getChild<Gaffer::AtomicCompoundDataPlug>( g_firstPlugIndex + 0 );
}

bool PrimitiveVariablePaint::affectsProcessedObject( const Gaffer::Plug *input ) const
{
	return
		Deformer::affectsProcessedObject( input ) ||
		input == paintPlug()
	;
}

void PrimitiveVariablePaint::hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	Deformer::hashProcessedObject( path, context, h );
	// TODO - not really a fan of any approach to this function ... we can either modify the hash for
	// every location in the scene, or we can evaluate the paint plug and do string munging in the hash ...
	// neither seems great.
	//
	// Should we be computing a filter from the stored data?
	/*ConstCompoundDataPtr paint = paintPlug()->getValue();

	std::string pathString = ScenePlug::pathToString( path );

	const CompoundData *locPaint = paint->member<CompoundData>( pathString );

	if( locPaint )
	{
		Deformer::hashProcessedObject( path, context, h );
		locPaint->hash( h );
	}
	else
	{
		h = inPlug()->objectPlug()->hash();
	}*/
	paintPlug()->hash( h );
}

IECore::ConstObjectPtr PrimitiveVariablePaint::computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, const IECore::Object *inputObject ) const
{
	const Primitive *inputPrimitive = runTimeCast<const Primitive>( inputObject );
	if( !inputPrimitive )
	{
		return inputObject;
	}

	if( !inputPrimitive->arePrimitiveVariablesValid() )
	{
		throw IECore::Exception( "Cannot paint primitive with invalid primitive variables" );
	}

	ConstCompoundDataPtr locPaint = paintPlug()->getValue();

	// TODO - now only needed for errors
	std::string pathString = ScenePlug::pathToString( path );

	PrimitivePtr result = inputPrimitive->copy();

	static const InternedString valueString( "value" );
	static const InternedString opacityString( "opacity" );
	static const InternedString interpolationString( "interpolation" );
	for( auto &var : locPaint->readable() )
	{
		const CompoundData *varCompound = IECore::runTimeCast<CompoundData>( var.second.get() );

		if( !varCompound )
		{
			throw IECore::Exception( fmt::format( "Invalid paint for variable {} with no compound at location {}", var.first.string(), pathString ) );
		}

		const Data *value = varCompound->member<Data>( valueString );
		if( !value )
		{
			throw IECore::Exception( fmt::format( "Invalid paint for variable {} with no value at location {}", var.first.string(), pathString ) );
		}


		const IntData *interpolationData = varCompound->member<IntData>( interpolationString );

		PrimitiveVariable::Interpolation interp =
			interpolationData ?
			(PrimitiveVariable::Interpolation) interpolationData->readable() :
			PrimitiveVariable::Vertex;

		if( IECore::size( value ) != result->variableSize( interp ) )
		{
			// TODO - should we support some sort of reprojection for loading out of date paint? This
			// would require storing a reference P in the paint file
			throw IECore::Exception( fmt::format( "Invalid paint for variable {} at location {} size {} does not match {}", var.first.string(), pathString, IECore::size( value ), result->variableSize( interp ) ) );

		}

		const FloatVectorData *opacityData = varCompound->member<FloatVectorData>( opacityString );

		auto existingVar = result->variables.find( var.first );
		if( existingVar != result->variables.end() && existingVar->second.interpolation != interp && opacityData )
		{
			IECore::msg( IECore::Msg::Warning, "PrimitiveVariablePaint", fmt::format( "Interpolation mismatch for variable {} at location {}, overwriting instead of compositing.", var.first.string(), pathString ) );
			existingVar = result->variables.end();

		}

		if( existingVar == result->variables.end() || !opacityData )
		{
			// TODO - I think this const_cast is safe because the result is treated as const
			result->variables[var.first] = PrimitiveVariable( interp, const_cast<Data*>( value ) );
			continue;
		}


		const std::vector<float> &opacity = opacityData->readable();
		if( opacity.size() != IECore::size( value ) )
		{
			throw IECore::Exception( "Corrupt paint : Opacity size different from value size." );
		}

		IECore::dispatch( value,
			[&var, &existingVar, &opacity, &result]( auto *typedValueData )
			{
				using SourceType = typename std::remove_const_t< std::remove_pointer_t<decltype( typedValueData )> >;

				// This check should be unnecessary, but IsNumericBasedVectorTypedData fails to compile for
				// non-vector types.
				if constexpr( TypeTraits::IsVectorTypedData< SourceType >::value )
				{
					if constexpr( std::is_same_v< typename SourceType::BaseType, float > )
					//if constexpr( TypeTraits::IsNumericBasedVectorTypedData< SourceType >::value )
					{

                        using ValueType = typename SourceType::ValueType::value_type;

                        // There a few types that are numeric based, but don't support weighting
                        //if constexpr( std::is_same_v< ValueType::BaseType, float > )
                        //if constexpr( !TypeTraits::IsMatrix<ValueType>::value && !TypeTraits::IsQuat<ValueType>::value && !TypeTraits::IsBox<ValueType>::value )
                        //if constexpr( !TypeTraits::IsQuat<ValueType>::value && !TypeTraits::IsBox<ValueType>::value )
                        //if constexpr( !TypeTraits::IsBox<ValueType>::value && !TypeTraits::IsMatrix<ValueType>::value )

                        if constexpr( !TypeTraits::IsBox<ValueType>::value )
						{
							typename SourceType::Ptr resultData = IECore::runTimeCast<SourceType>( existingVar->second.expandedData() );
							auto &resultVec = resultData->writable();
							auto &typedValue = typedValueData->readable();

							for( size_t i = 0; i < resultVec.size(); i++ )
							{
								resultVec[i] = ( 1 - opacity[i] ) * resultVec[i] + typedValue[i];
							}

							result->variables[var.first] = PrimitiveVariable( existingVar->second.interpolation, resultData );
							return;
						}
					}
				}

				throw IECore::Exception( fmt::format(
					"PrimitiveVariablePaint : Cannot apply type \"{}\"", typedValueData->typeName()
				) );
			}
		);

	}

	/*

	*/

	return result;
}

bool PrimitiveVariablePaint::adjustBounds() const
{
	if( !Deformer::adjustBounds() )
	{
		return false;
	}

	// TODO - if we want to support painting P, we would need to query every possible input
	// to see if there is paint on P?
	/*static const InternedString pString( "P" );

	ConstCompoundDataPtr paint = paintPlug()->getValue();
	for( const auto &loc : paint->readable() )
	{
		const CompoundData *locCompound = IECore::runTimeCast<CompoundData>( loc.second.get() );
		if( locCompound->member<Data>( pString ) )
		{
			return true;
		}
	}*/

	return false;
}
