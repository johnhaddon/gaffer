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

#include "GafferScene/CurvesTangents.h"

#include "Gaffer/ThreadState.h"

#include "IECoreScene/CurvesPrimitive.h"
#include "IECoreScene/CurvesPrimitiveEvaluator.h"

#include "IECore/Canceller.h"
#include "IECore/VectorTypedData.h"

#include "tbb/blocked_range.h"
#include "tbb/enumerable_thread_specific.h"
#include "tbb/parallel_for.h"

using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

GAFFER_NODE_DEFINE_TYPE( CurvesTangents );

size_t CurvesTangents::g_firstPlugIndex = 0;

CurvesTangents::CurvesTangents( const std::string &name )
	:	ObjectProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "position", Plug::In, "P" ) );
	addChild( new StringPlug( "tangent", Plug::In, "tangent" ) );
}

CurvesTangents::~CurvesTangents()
{
}

Gaffer::StringPlug *CurvesTangents::positionPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *CurvesTangents::positionPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

Gaffer::StringPlug *CurvesTangents::tangentPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::StringPlug *CurvesTangents::tangentPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

Gaffer::ValuePlug::CachePolicy CurvesTangents::processedObjectComputeCachePolicy() const
{
	return ValuePlug::CachePolicy::TaskCollaboration;
}

bool CurvesTangents::affectsProcessedObject( const Gaffer::Plug *input ) const
{
	return
		ObjectProcessor::affectsProcessedObject( input ) ||
		input == positionPlug() ||
		input == tangentPlug()
	;
}

void CurvesTangents::hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ObjectProcessor::hashProcessedObject( path, context, h );
	positionPlug()->hash( h );
	tangentPlug()->hash( h );
}

IECore::ConstObjectPtr CurvesTangents::computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, const IECore::Object *inputObject ) const
{
	const CurvesPrimitive *curves = runTimeCast<const CurvesPrimitive>( inputObject );
	if( !curves )
	{
		return inputObject;
	}

	const std::string position = positionPlug()->getValue();
	const std::string tangentName = tangentPlug()->getValue();

	ConstCurvesPrimitivePtr evalCurves = curves;
	if( position != "P" )
	{
		auto it = curves->variables.find( position );
		if( it == curves->variables.end() )
		{
			return inputObject;
		}
		CurvesPrimitivePtr copy = runTimeCast<CurvesPrimitive>( curves->copy() );
		copy->variables["P"] = it->second;
		evalCurves = copy;
	}

	CurvesPrimitiveEvaluator evaluator( evalCurves );

	const bool periodic = curves->wrap() == CurvesPrimitive::Wrap::Periodic;
	const bool isLinear = curves->basis() == CubicBasisf::linear();
	const size_t numCurves = curves->verticesPerCurve()->readable().size();

	// Pre-compute per-curve offsets into the flat output array.
	std::vector<int> vertexOffsets( numCurves );
	int totalVerts = 0;
	for( size_t i = 0; i < numCurves; ++i )
	{
		vertexOffsets[i] = totalVerts;
		totalVerts += curves->variableSize( PrimitiveVariable::Interpolation::Vertex, i );
	}

	V3fVectorDataPtr tangentData = new V3fVectorData();
	tangentData->setInterpretation( GeometricData::Interpretation::Vector );
	auto &tangents = tangentData->writable();
	tangents.resize( totalVerts );

	const IECore::Canceller *canceller = context->canceller();
	const ThreadState &threadState = ThreadState::current();

	tbb::enumerable_thread_specific<PrimitiveEvaluator::ResultPtr> threadLocalResult(
		[&evaluator](){ return evaluator.createResult(); }
	);

	tbb::parallel_for(
		tbb::blocked_range<size_t>( 0, numCurves ),
		[&]( const tbb::blocked_range<size_t> &range )
		{
			ThreadState::Scope threadStateScope( threadState );
			PrimitiveEvaluator::ResultPtr &result = threadLocalResult.local();

			for( size_t curveIndex = range.begin(); curveIndex != range.end(); ++curveIndex )
			{
				IECore::Canceller::check( canceller );
				const int numVertex = curves->variableSize( PrimitiveVariable::Interpolation::Vertex, curveIndex );
				const int numVarying = curves->variableSize( PrimitiveVariable::Interpolation::Varying, curveIndex );
				const int vertexOffset = vertexOffsets[curveIndex];
				// For open cubic curves, numVertex == numVarying + 2, with the first
				// and last CVs being phantom hull points not on the curve.
				const bool hasPhantoms = numVertex != numVarying;

				for( int j = 0; j < numVarying; ++j )
				{
					// For periodic cubic curves, the evaluator's v=0 sits at CV 1
					// rather than CV 0 (Cortex's segment-k starts at CV k+1). Apply
					// a -1 shift so that vertex j receives the tangent at its own
					// curve position. Linear curves have no such offset.
					float v;
					if( periodic )
					{
						const int jShifted = isLinear ? j : ( j - 1 + numVarying ) % numVarying;
						v = float(jShifted) / float(numVarying);
					}
					else
					{
						v = float(j) / float(numVarying - 1);
					}
					evaluator.pointAtV( curveIndex, v, result.get() );
					const Imath::V3f t = result->vTangent();

					tangents[vertexOffset + j + hasPhantoms] = t;
					if( hasPhantoms )
					{
						if( j == 0 ) tangents[vertexOffset] = t;
						if( j == numVarying - 1 ) tangents[vertexOffset + numVertex - 1] = t;
					}
				}
			}
		}
	);

	CurvesPrimitivePtr output = runTimeCast<CurvesPrimitive>( curves->copy() );
	output->variables[tangentName] = PrimitiveVariable(
		PrimitiveVariable::Interpolation::Vertex,
		tangentData
	);

	return output;
}
