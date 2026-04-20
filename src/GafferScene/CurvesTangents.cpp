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

#include "IECoreScene/CurvesPrimitive.h"
#include "IECoreScene/CurvesPrimitiveEvaluator.h"

#include "IECore/VectorTypedData.h"

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
	PrimitiveEvaluator::ResultPtr result = evaluator.createResult();

	const bool periodic = curves->wrap() == CurvesPrimitive::Wrap::Periodic;
	const std::vector<int> &verticesPerCurve = curves->verticesPerCurve()->readable();

	V3fVectorDataPtr tangentData = new V3fVectorData();
	tangentData->setInterpretation( GeometricData::Interpretation::Vector );
	auto &tangents = tangentData->writable();
	tangents.reserve( curves->variableSize( PrimitiveVariable::Interpolation::Vertex ) );

	for( size_t curveIndex = 0; curveIndex < verticesPerCurve.size(); ++curveIndex )
	{
		const int numVerts = verticesPerCurve[curveIndex];
		for( int vertIndex = 0; vertIndex < numVerts; ++vertIndex )
		{
			float v;
			if( periodic )
			{
				v = float(vertIndex) / float(numVerts);
			}
			else
			{
				v = numVerts > 1 ? float(vertIndex) / float(numVerts - 1) : 0.0f;
			}
			evaluator.pointAtV( curveIndex, v, result.get() );
			tangents.push_back( result->vTangent() );
		}
	}

	CurvesPrimitivePtr output = runTimeCast<CurvesPrimitive>( curves->copy() );
	output->variables[tangentName] = PrimitiveVariable(
		PrimitiveVariable::Interpolation::Vertex,
		tangentData
	);

	return output;
}
