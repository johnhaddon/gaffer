//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2017, Image Engine Design Inc. All rights reserved.
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

#include "GafferOSL/OSLLight.h"

#include "GafferOSL/OSLShader.h"

#include "GafferScene/Private/IECoreScenePreview/Geometry.h"
#include "GafferScene/Shader.h"

#include "Gaffer/StringPlug.h"

#include "IECoreScene/DiskPrimitive.h"
#include "IECoreScene/SpherePrimitive.h"

#include "IECore/NullObject.h"

using namespace Imath;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;
using namespace GafferOSL;

GAFFER_NODE_DEFINE_TYPE( OSLLight );

size_t OSLLight::g_firstPlugIndex = 0;

OSLLight::OSLLight( const std::string &name )
	:	GafferScene::Light( new OSLShader(), name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new IntPlug( "shape", Plug::In, Disk, Disk, Geometry ) );
	addChild( new FloatPlug( "radius", Plug::In, 0.01, 0 ) );
	addChild( new StringPlug( "geometryType" ) );
	addChild( new Box3fPlug( "geometryBound", Plug::In, Box3f( V3f( -1 ), V3f( 1 ) ) ) );
	addChild( new CompoundDataPlug( "geometryParameters" ) );
}

OSLLight::~OSLLight()
{
}

Gaffer::IntPlug *OSLLight::shapePlug()
{
	return getChild<IntPlug>( g_firstPlugIndex );
}

const Gaffer::IntPlug *OSLLight::shapePlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex );
}

Gaffer::FloatPlug *OSLLight::radiusPlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::FloatPlug *OSLLight::radiusPlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 1 );
}

Gaffer::StringPlug *OSLLight::geometryTypePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 2 );
}

const Gaffer::StringPlug *OSLLight::geometryTypePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 2 );
}

Gaffer::Box3fPlug *OSLLight::geometryBoundPlug()
{
	return getChild<Box3fPlug>( g_firstPlugIndex + 3 );
}

const Gaffer::Box3fPlug *OSLLight::geometryBoundPlug() const
{
	return getChild<Box3fPlug>( g_firstPlugIndex + 3 );
}

Gaffer::CompoundDataPlug *OSLLight::geometryParametersPlug()
{
	return getChild<CompoundDataPlug>( g_firstPlugIndex + 4 );
}

const Gaffer::CompoundDataPlug *OSLLight::geometryParametersPlug() const
{
	return getChild<CompoundDataPlug>( g_firstPlugIndex + 4 );
}

void OSLLight::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	Light::affects( input, outputs );

	if(
		input == shapePlug() ||
		input == radiusPlug() ||
		input == geometryTypePlug() ||
		geometryBoundPlug()->isAncestorOf( input ) ||
		geometryParametersPlug()->isAncestorOf( input )
	)
	{
		outputs.push_back( sourcePlug() );
	}
}

void OSLLight::hashSource( const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	shapePlug()->hash( h );
	radiusPlug()->hash( h );
	geometryTypePlug()->hash( h );
	geometryBoundPlug()->hash( h );
	geometryParametersPlug()->hash( h );
}

IECore::ConstObjectPtr OSLLight::computeSource( const Gaffer::Context *context ) const
{
	switch( shapePlug()->getValue() )
	{
		case Disk :
			return new DiskPrimitive( radiusPlug()->getValue() );
		case Sphere :
			return new SpherePrimitive( radiusPlug()->getValue() );
		case Geometry :
		{
			CompoundDataPtr parameters = new CompoundData;
			geometryParametersPlug()->fillCompoundData( parameters->writable() );
			return new IECoreScenePreview::Geometry(
				geometryTypePlug()->getValue(),
				geometryBoundPlug()->getValue(),
				parameters
			);
		}
	}
	return NullObject::defaultNullObject();
}
