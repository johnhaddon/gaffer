//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2013, John Haddon. All rights reserved.
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

#include "GafferScene/MapProjection.h"

#include "Gaffer/StringPlug.h"

#include "IECoreScene/Camera.h"
#include "IECoreScene/Primitive.h"

#include "IECore/AngleConversion.h"

#include "OpenEXR/ImathFun.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

GAFFER_GRAPHCOMPONENT_DEFINE_TYPE( MapProjection );

size_t MapProjection::g_firstPlugIndex = 0;

MapProjection::MapProjection( IECore::InternedString name )
	:	ObjectProcessor( name, PathMatcher::EveryMatch )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "camera" ) );
	addChild( new StringPlug( "uvSet", Plug::In, "uv" ) );
}

MapProjection::~MapProjection()
{
}

Gaffer::StringPlug *MapProjection::cameraPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *MapProjection::cameraPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

Gaffer::StringPlug *MapProjection::uvSetPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::StringPlug *MapProjection::uvSetPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

bool MapProjection::affectsProcessedObject( const Gaffer::Plug *input ) const
{
	return
		ObjectProcessor::affectsProcessedObject( input ) ||
		input == cameraPlug() ||
		input == uvSetPlug() ||
		input == inPlug()->transformPlug()
	;
}

void MapProjection::hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ObjectProcessor::hashProcessedObject( path, context, h );

	ScenePath cameraPath;
	ScenePlug::stringToPath( cameraPlug()->getValue(), cameraPath );

	h.append( inPlug()->objectHash( cameraPath ) );
	h.append( inPlug()->transformHash( cameraPath ) );

	inPlug()->transformPlug()->hash( h );
	uvSetPlug()->hash( h );
}

IECore::ConstObjectPtr MapProjection::computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, const IECore::Object *inputObject ) const
{
	// early out if it's not a primitive with a "P" variable
	const Primitive *inputPrimitive = runTimeCast<const Primitive>( inputObject );
	if( !inputPrimitive )
	{
		return inputObject;
	}

	const V3fVectorData *pData = inputPrimitive->variableData<V3fVectorData>( "P" );
	if( !pData )
	{
		return inputObject;
	}

	// early out if the uv set name hasn't been provided

	const string uvSet = uvSetPlug()->getValue();

	if( uvSet == "" )
	{
		return inputObject;
	}

	// get the camera and early out if we can't find one

	ScenePath cameraPath;
	ScenePlug::stringToPath( cameraPlug()->getValue(), cameraPath );

	ConstCameraPtr constCamera = runTimeCast<const Camera>( inPlug()->object( cameraPath ) );
	if( !constCamera )
	{
		return inputObject;
	}

	M44f cameraMatrix = inPlug()->fullTransform( cameraPath );
	M44f objectMatrix = inPlug()->fullTransform( path );
	M44f objectToCamera = objectMatrix * cameraMatrix.inverse();

	bool perspective = constCamera->getProjection() == "perspective";

	Box2f normalizedScreenWindow;
	if( constCamera->hasResolution() )
	{
		normalizedScreenWindow = constCamera->frustum();
	}
	else
	{
		// We don't know what resolution the camera is meant to render with, so take the whole aperture
		// as the screen window
		normalizedScreenWindow = constCamera->frustum( Camera::Distort );
	}

	// do the work

	PrimitivePtr result = inputPrimitive->copy();

	V2fVectorDataPtr uvData = new V2fVectorData();
	uvData->setInterpretation( GeometricData::UV );

	result->variables[uvSet] = PrimitiveVariable( PrimitiveVariable::Vertex, uvData );

	const vector<V3f> &p = pData->readable();
	vector<V2f> &uv = uvData->writable();
	uv.reserve( p.size() );

	for( size_t i = 0, e = p.size(); i < e; ++i )
	{
		V3f pCamera = p[i] * objectToCamera;
		V2f pScreen = V2f( pCamera.x, pCamera.y );
		if( perspective )
		{
			pScreen /= -pCamera.z;
		}
		uv.push_back(
			V2f(
				lerpfactor( pScreen.x, normalizedScreenWindow.min.x, normalizedScreenWindow.max.x ),
				lerpfactor( pScreen.y, normalizedScreenWindow.min.y, normalizedScreenWindow.max.y )
			)
		);
	}

	return result;
}
