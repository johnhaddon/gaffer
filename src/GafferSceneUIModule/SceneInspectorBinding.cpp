//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2025, Cinesite VFX Ltd. All rights reserved.
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

#include "boost/python.hpp"

#include "SceneInspectorBinding.h"

#include "GafferSceneUI/Private/AttributeInspector.h"
#include "GafferSceneUI/Private/BasicInspector.h"
#include "GafferSceneUI/Private/InspectorColumn.h"
#include "GafferSceneUI/Private/OptionInspector.h"
#include "GafferSceneUI/TypeIds.h"

#include "GafferBindings/PathBinding.h"

#include "Gaffer/Context.h"
#include "Gaffer/ParallelAlgo.h"
#include "Gaffer/Path.h"
#include "Gaffer/PathFilter.h"
#include "Gaffer/Private/IECorePreview/LRUCache.h"

#include "IECoreScene/Camera.h"
#include "IECoreScene/CurvesPrimitive.h"
#include "IECoreScene/ExternalProcedural.h"
#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/Output.h"
#include "IECoreScene/Primitive.h"

#include "Imath/ImathMatrixAlgo.h"

#include "boost/algorithm/string/predicate.hpp"

#include <map>

using namespace std;
using namespace Imath;
using namespace boost::python;
using namespace IECore;
using namespace IECoreScene;
using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;
using namespace GafferScene;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Registry Shenanigans TODO : BETTER NAME PLEASE!
//////////////////////////////////////////////////////////////////////////

namespace
{

using Inspections = map<vector<InternedString>, GafferSceneUI::Private::ConstInspectorPtr>;
using InspectionProvider = std::function<Inspections ( ScenePlug *scene, const Gaffer::PlugPtr &editScope  )>;

Inspections transformInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;
	for( auto full : { false, true } )
	{
		vector<InternedString> path = { full ? "World" : "Local", "" };
		for( auto component : { 'm', 't', 'r', 's', 'h' } )
		{
			switch( component )
			{
				case 'm' : path[1] = "Matrix"; break;
				case 't' : path[1] = "Translate"; break;
				case 'r' : path[1] = "Rotate"; break;
				case 's' : path[1] = "Scale"; break;
				case 'h' : path[1] = "Shear"; break;
			}
			result[path] = new GafferSceneUI::Private::BasicInspector(
				scene->transformPlug(), editScope,
				[full, component] ( const SceneAlgo::History *history ) -> ConstDataPtr {
					const M44f matrix =
						full ?
							history->scene->fullTransform( history->context->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName ) )
						:
							history->scene->transformPlug()->getValue()
					;
					if( component == 'm' )
					{
						return new M44fData( matrix );
					}

					V3f s, h, r, t;
					extractSHRT( matrix, s, h, r, t );
					switch( component )
					{
						case 't' : return new V3fData( t );
						case 'r' : return new V3fData( r );
						case 's' : return new V3fData( s );
						default : return new V3fData( h );
					}
				}
			);
		}
	}
	return result;
}

Inspections boundInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;
	result[{"Local"}] = new GafferSceneUI::Private::BasicInspector(
		scene->boundPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			return new Box3fData( history->scene->boundPlug()->getValue() );
		}
	);
	result[{"World"}] = new GafferSceneUI::Private::BasicInspector(
		scene->boundPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			const Imath::Box3f bound = Imath::transform(
				history->scene->boundPlug()->getValue(),
				history->scene->fullTransform( history->context->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName ) )
			);
			return new Box3fData( bound );
		}
	);
	return result;
}

const boost::container::flat_map<string, InternedString> g_attributeCategories = {
	{ "ai:*", "Arnold" },
	{ "dl:*", "3Delight" },
	{ "cycles:*", "Cycles" },
	{ "ri:*", "RenderMan" },
	{ "gl:*", "OpenGL" },
	{ "usd:*", "USD" },
	{ "user:*", "User" },
	{
		"scene:visible doubleSided render:* gaffer:* " \
		"linkedLights shadowedLights filteredLights",
		"Standard"
	}
};

const InternedString g_other( "Other" );

Inspections attributeInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	ConstCompoundObjectPtr attributes = scene->fullAttributes( Context::current()->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName ) );
	Inspections result;
	for( const auto &[name, value] : attributes->members() )
	{
		InternedString category = g_other;
		for( const auto &[pattern, matchingCategory] : g_attributeCategories )
		{
			if( StringAlgo::matchMultiple( name, pattern ) )
			{
				category = matchingCategory;
				break;
			}
		}
		result.insert( { { category, name }, new GafferSceneUI::Private::AttributeInspector( scene, editScope, name ) } );
	}
	return result;
}

const CompoundData *objectParameters( const Object *object )
{
	if( auto camera = runTimeCast<const Camera>( object ) )
	{
		return camera->parametersData();
	}
	else if( auto externalProcedural = runTimeCast<const ExternalProcedural>( object ) )
	{
		return externalProcedural->parameters();
	}
	return nullptr;
}

const vector<pair<PrimitiveVariable::Interpolation, const char *>> g_primitiveVariableInterpolations = {
	{ PrimitiveVariable::Constant, "Constant" },
	{ PrimitiveVariable::Uniform, "Uniform" },
	{ PrimitiveVariable::Vertex, "Vertex" },
	{ PrimitiveVariable::Varying, "Varying" },
	{ PrimitiveVariable::FaceVarying, "FaceVarying" }
};

Inspections primitiveTopologyInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( runTimeCast<const Primitive>( object.get() ) )
	{
		for( const auto &[interpolation, interpolationName] : g_primitiveVariableInterpolations )
		{
			result[{ interpolationName }] = new GafferSceneUI::Private::BasicInspector(
				scene->objectPlug(), editScope,
				[interpolation] ( const SceneAlgo::History *history ) -> ConstDataPtr {
					ConstObjectPtr object = history->scene->objectPlug()->getValue();
					if( auto primitive = runTimeCast<const Primitive>( object.get() ) )
					{
						return new IntData( primitive->variableSize( interpolation ) );
					}
					return nullptr;
				}
			);
		}
	}
	return result;
}


Inspections meshTopologyInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( runTimeCast<const MeshPrimitive>( object.get() ) )
	{
		result[{ "Vertices" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return new IntData( mesh->variableSize( PrimitiveVariable::Vertex ) );
				}
				return nullptr;
			}
		);
		result[{ "Faces" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return new IntData( mesh->numFaces() );
				}
				return nullptr;
			}
		);
		result[{ "Vertices Per Face" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return mesh->verticesPerFace();
				}
				return nullptr;
			}
		);
		result[{ "Vertex Ids" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return mesh->vertexIds();
				}
				return nullptr;
			}
		);
	}
	return result;
}

Inspections curvesTopologyInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( runTimeCast<const CurvesPrimitive>( object.get() ) )
	{
		result[{ "Vertices" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto curves = runTimeCast<const CurvesPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return new IntData( curves->variableSize( PrimitiveVariable::Vertex ) );
				}
				return nullptr;
			}
		);
		result[{ "Curves" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto curves = runTimeCast<const CurvesPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return new IntData( curves->numCurves() );
				}
				return nullptr;
			}
		);
		result[{ "Vertices Per Curve" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto curves = runTimeCast<const CurvesPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return curves->verticesPerCurve();
				}
				return nullptr;
			}
		);
		result[{ "Periodic" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto curves = runTimeCast<const CurvesPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					return new BoolData( curves->periodic() );
				}
				return nullptr;
			}
		);
		result[{ "Basis" }] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstDataPtr {
				if( auto curves = runTimeCast<const CurvesPrimitive>( history->scene->objectPlug()->getValue() ) )
				{
					switch( curves->basis().standardBasis() )
					{
						case StandardCubicBasis::Linear : return new StringData( "Linear" );
						case StandardCubicBasis::Bezier : return new StringData( "Bezier" );
						case StandardCubicBasis::BSpline : return new StringData( "BSpline" );
						case StandardCubicBasis::CatmullRom : return new StringData( "CatmullRom" );
						case StandardCubicBasis::Constant : return new StringData( "Constant" );
						default : return nullptr;
					}
				}
				return nullptr;
			}
		);
	}

	return result;
}

Inspections objectParametersInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( auto parameters = objectParameters( object.get() ) )
	{
		for( const auto &[name, value] : parameters->readable() )
		{
			result[{ name }] = new GafferSceneUI::Private::BasicInspector(
				scene->objectPlug(), editScope,
				[name] ( const SceneAlgo::History *history ) -> ConstDataPtr {
					ConstObjectPtr object = history->scene->objectPlug()->getValue();
					if( auto parameters = objectParameters( object.get() ) )
					{
						return parameters->member( name );
					}
					return nullptr;
				}
			);
		}
	}
	return result;
}

Inspections objectTypeInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;
	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( object->typeId() != NullObjectTypeId )
	{
		result[{"Type"}] = new GafferSceneUI::Private::BasicInspector(
			scene->objectPlug(), editScope,
			[] ( const SceneAlgo::History *history ) -> ConstStringDataPtr {
				ConstObjectPtr object = history->scene->objectPlug()->getValue();
				if( object->typeId() == NullObjectTypeId )
				{
					return nullptr;
				}
				return new StringData( object->typeName() );
			}
		);
	}

	return result;
}

ConstStringDataPtr g_invalidStringData = new StringData( "Invalid" );
ConstStringDataPtr g_constantStringData = new StringData( "Constant" );
ConstStringDataPtr g_uniformStringData = new StringData( "Uniform" );
ConstStringDataPtr g_vertexStringData = new StringData( "Vertex" );
ConstStringDataPtr g_varyingStringData = new StringData( "Varying" );
ConstStringDataPtr g_faceVaryingStringData = new StringData( "FaceVarying" );

const PrimitiveVariable *primitiveVariable( const Object *object, const std::string &name )
{
	auto primitive = runTimeCast<const Primitive>( object );
	if( !primitive )
	{
		return nullptr;
	}

	auto it = primitive->variables.find( name );
	return it != primitive->variables.end() ? &it->second : nullptr;
}

ConstStringDataPtr primitiveVariableInterpolation( const std::string &name, const SceneAlgo::History *history )
{
	ConstObjectPtr object = history->scene->objectPlug()->getValue();
	auto variable = primitiveVariable( object.get(), name );
	if( !variable )
	{
		return nullptr;
	}

	switch( variable->interpolation )
	{
		case PrimitiveVariable::Invalid : return g_invalidStringData;
		case PrimitiveVariable::Constant : return g_constantStringData;
		case PrimitiveVariable::Uniform : return g_uniformStringData;
		case PrimitiveVariable::Vertex : return g_vertexStringData;
		case PrimitiveVariable::Varying : return g_varyingStringData;
		case PrimitiveVariable::FaceVarying : return g_faceVaryingStringData;
		default : return nullptr;
	}
}

ConstStringDataPtr primitiveVariableType( const std::string &name, const SceneAlgo::History *history )
{
	ConstObjectPtr object = history->scene->objectPlug()->getValue();
	auto variable = primitiveVariable( object.get(), name );
	if( !variable || !variable->data )
	{
		return nullptr;
	}

	return new StringData( variable->data->typeName() );
}

ConstDataPtr primitiveVariableData( const std::string &name, const SceneAlgo::History *history )
{
	ConstObjectPtr object = history->scene->objectPlug()->getValue();
	auto variable = primitiveVariable( object.get(), name );
	if( !variable )
	{
		return nullptr;
	}

	return variable->data;
}

ConstDataPtr primitiveVariableIndices( const std::string &name, const SceneAlgo::History *history )
{
	ConstObjectPtr object = history->scene->objectPlug()->getValue();
	auto variable = primitiveVariable( object.get(), name );
	if( !variable )
	{
		return nullptr;
	}

	return variable->indices;
}

Inspections primitiveVariablesInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	ConstObjectPtr object = scene->objectPlug()->getValue();
	Inspections result;
	if( auto primitive = runTimeCast<const Primitive>( object.get() ) )
	{
		for( const auto &[name, variable] : primitive->variables )
		{
			result[{ name, "Interpolation" }] = new GafferSceneUI::Private::BasicInspector(
				scene->objectPlug(), editScope,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableInterpolation( name, history );
				}
			);
			result[{ name, "Type" }] = new GafferSceneUI::Private::BasicInspector(
				scene->objectPlug(), editScope,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableType( name, history );
				}
			);
			result[{ name, "Data" }] = new GafferSceneUI::Private::BasicInspector(
				scene->objectPlug(), editScope,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableData( name, history );
				}
			);
			result[{ name, "Indices" }] = new GafferSceneUI::Private::BasicInspector(
				scene->objectPlug(), editScope,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableIndices( name, history );
				}
			);
		}
	}
	return result;
}

Inspections subdivisionInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	auto mesh = runTimeCast<const MeshPrimitive>( object.get() );
	if( !mesh )
	{
		return result;
	}

	result[{"Interpolation"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->interpolation() ) : nullptr;
		}
	);

	result[{"Corners"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new UInt64Data( mesh->cornerIds()->readable().size() ) : nullptr;
		}
	);

	result[{"Corners","Indices"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->cornerIds() : nullptr;
		}
	);

	result[{"Corners","Sharpnesses"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->cornerSharpnesses() : nullptr;
		}
	);

	result[{"Creases"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new UInt64Data( mesh->creaseLengths()->readable().size() ) : nullptr;
		}
	);

	result[{"Creases","Lengths"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->creaseLengths() : nullptr;
		}
	);

	result[{"Creases","Ids"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->creaseIds() : nullptr;
		}
	);

	result[{"Creases","Sharpnesses"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->creaseSharpnesses() : nullptr;
		}
	);

	result[{"Interpolate Boundary"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->getInterpolateBoundary() ) : nullptr;
		}
	);

	result[{"FaceVarying Linear Interpolation"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->getFaceVaryingLinearInterpolation() ) : nullptr;
		}
	);

	result[{"Triangle Subdivision Rule"}] = new GafferSceneUI::Private::BasicInspector(
		scene->objectPlug(), editScope,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->getTriangleSubdivisionRule() ) : nullptr;
		}
	);
	return result;
}

const boost::container::flat_map<string, InternedString> g_optionCategories = {
	{ "ai:*", "Arnold" },
	{ "dl:*", "3Delight" },
	{ "cycles:*", "Cycles" },
	{ "ri:*", "RenderMan" },
	{ "gl:*", "OpenGL" },
	{ "usd:*", "USD" },
	{ "user:*", "User" },
	{ "render:* sampleMotion", "Standard" },
};

const std::string g_optionPrefix( "option:" );
const std::string g_attributePrefix( "attribute:" );

Inspections optionInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	ConstCompoundObjectPtr globals = scene->globalsPlug()->getValue();
	Inspections result;
	for( const auto &[name, value] : globals->members() )
	{
		if( !boost::starts_with( name.string(), g_optionPrefix ) )
		{
			continue;
		}

		string optionName = name.string().substr( g_optionPrefix.size() );
		InternedString category = g_other;
		for( const auto &[pattern, matchingCategory] : g_optionCategories )
		{
			if( StringAlgo::matchMultiple( optionName, pattern ) )
			{
				category = matchingCategory;
				break;
			}
		}
		result.insert( { { category, optionName }, new GafferSceneUI::Private::OptionInspector( scene, editScope, optionName ) } );
	}
	return result;
}

Inspections globalAttributesInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	ConstCompoundObjectPtr globals = scene->globalsPlug()->getValue();
	Inspections result;
	for( const auto &[name, value] : globals->members() )
	{
		if( !boost::starts_with( name.string(), g_attributePrefix ) )
		{
			continue;
		}

		string optionName = name.string().substr( g_attributePrefix.size() );
		InternedString category = g_other;
		for( const auto &[pattern, matchingCategory] : g_attributeCategories )
		{
			if( StringAlgo::matchMultiple( optionName, pattern ) )
			{
				category = matchingCategory;
				break;
			}
		}
		result.insert( {
			{ category, optionName },
			new GafferSceneUI::Private::BasicInspector(
				scene->globalsPlug(), editScope,
				[name] ( const SceneAlgo::History *history ) {
					ConstCompoundObjectPtr globals = history->scene->globalsPlug()->getValue();
					return globals->member( name );
				}
			)
		} );
	}
	return result;
}

const std::string g_outputPrefix( "output:" );

Inspections outputsInspectionProvider( ScenePlug *scene, const Gaffer::PlugPtr &editScope )
{
	ConstCompoundObjectPtr globals = scene->globalsPlug()->getValue();
	Inspections result;
	for( const auto &[name, value] : globals->members() )
	{
		if( !boost::starts_with( name.string(), g_outputPrefix ) )
		{
			continue;
		}

		auto output = runTimeCast<const Output>( value.get() );
		if( !output )
		{
			continue;
		}

		vector<InternedString> path = ScenePlug::stringToPath( name.string().substr( g_outputPrefix.size( ) ) );
		path.push_back( "File Name" );
		result.insert( {
			path,
			new GafferSceneUI::Private::BasicInspector(
				scene->globalsPlug(), editScope,
				[ name ] ( const SceneAlgo::History *history ) {
					ConstOutputPtr output = history->scene->globalsPlug()->getValue()->member<Output>( name );
					return output ? new StringData( output->getName() ) : nullptr;
				}
			)
		} );

		path.back() = "Type";
		result.insert( {
			path,
			new GafferSceneUI::Private::BasicInspector(
				scene->globalsPlug(), editScope,
				[ name ] ( const SceneAlgo::History *history ) {
					ConstOutputPtr output = history->scene->globalsPlug()->getValue()->member<Output>( name );
					return output ? new StringData( output->getType() ) : nullptr;
				}
			)
		} );

		path.back() = "Data";
		result.insert( {
			path,
			new GafferSceneUI::Private::BasicInspector(
				scene->globalsPlug(), editScope,
				[ name ] ( const SceneAlgo::History *history ) {
					ConstOutputPtr output = history->scene->globalsPlug()->getValue()->member<Output>( name );
					return output ? new StringData( output->getData() ) : nullptr;
				}
			)
		} );

		path.back() = "Parameters"; path.resize( path.size() + 1 );
		for( const auto &[parameterName, parameterValue] : output->parameters() )
		{
			path.back() = parameterName;
			result.insert( {
				path,
				new GafferSceneUI::Private::BasicInspector(
					scene->globalsPlug(), editScope,
					[ name, parameterName ] ( const SceneAlgo::History *history ) {
						ConstOutputPtr output = history->scene->globalsPlug()->getValue()->member<Output>( name );
						return output ? output->parametersData()->member( parameterName ) : nullptr;
					}
				)
			} );
		}
	}
	return result;
}

using InspectionProviders = multimap<vector<InternedString>, InspectionProvider>;
InspectionProviders *g_inspectionProviders = new InspectionProviders( {
	{ { "Selection", "Bound" }, boundInspectionProvider },
	{ { "Selection", "Transform" }, transformInspectionProvider },
	{ { "Selection", "Attributes" }, attributeInspectionProvider },
	{ { "Selection", "Object" }, objectTypeInspectionProvider },
	{ { "Selection", "Object", "Topology" }, primitiveTopologyInspectionProvider },
	{ { "Selection", "Object", "Mesh Topology" }, meshTopologyInspectionProvider },
	{ { "Selection", "Object", "Curves Topology" }, curvesTopologyInspectionProvider },
	{ { "Selection", "Object", "Parameters" }, objectParametersInspectionProvider },
	{ { "Selection", "Object", "Primitive Variables" }, primitiveVariablesInspectionProvider },
	{ { "Selection", "Object", "Subdivision" }, subdivisionInspectionProvider },
	{ { "Globals", "Attributes" }, globalAttributesInspectionProvider },
	{ { "Globals", "Options" }, optionInspectionProvider },
	{ { "Globals", "Outputs" }, outputsInspectionProvider },
} );

void registerInspectors( const vector<InternedString> &path, object pythonInspectionProvider )
{
	InspectionProvider inspectionProvider = [pythonInspectionProvider] ( ScenePlug *scene, const Gaffer::PlugPtr &editScope ) {
		Inspections result;
		IECorePython::ScopedGILLock gilLock;
		try
		{
			object pythonInspections = pythonInspectionProvider( ScenePlugPtr( scene ), editScope );
			dict inspectionsDict = extract<dict>( pythonInspections );
			boost::python::list items = inspectionsDict.items();
			for( size_t i = 0, e = len( items ); i < e; ++i )
			{
				vector<InternedString> path = extract<vector<InternedString>>( items[i][0] );
				Private::InspectorPtr inspector = extract<Private::InspectorPtr>( items[i][1] );
				result[path] = inspector;
			}
		}
		catch( const error_already_set & )
		{
			IECorePython::ExceptionAlgo::translatePythonException();
		}
		return result;
	};
	g_inspectionProviders->insert( { path, inspectionProvider } );
}

// void registerInspectionProvider(
// 	std::vector<InternedString> rootPath,
// 	InspectionProvider provider
// )
// {
// 	g_inspectionProviders.insert( { rootPath, provider } );
// }

// struct InspectorMapValue;

// struct InspectorMapValue
// {

// 	std::variant<
// 		GafferSceneUI::Private::ConstInspectorPtr,
//         std::map<std::string, InspectorMapValue>
//     > value;


// };

// using InspectorMapValue = std::pair<InternedString

// using InspectorMap = multi_index::multi_index_container<
// 	NamedValue,
// 	multi_index::indexed_by<
// 		multi_index::ordered_unique<
// 			multi_index::member<NamedValue, InternedString, &NamedValue::first>
// 		>,
// 		multi_index::sequenced<>
// 	>
// >;

//using InspectorMap = std::map<InternedString,

} // namespace

//////////////////////////////////////////////////////////////////////////
// InspectorPath
//////////////////////////////////////////////////////////////////////////

namespace
{

const IECore::InternedString g_contextPropertyName( "inspector:context" );
const IECore::InternedString g_contextAPropertyName( "inspector:contextA" );
const IECore::InternedString g_contextBPropertyName( "inspector:contextB" );
const IECore::InternedString g_inspectorPropertyName( "inspector:inspector" );

class InspectorPath : public Gaffer::Path
{

	public :

		/// Context for each side of an A/B diff.
		using Contexts = std::array<Gaffer::ConstContextPtr, 2>;

		/// TODO : CONST CONTEXT, const & for all ppointers
		InspectorPath( const ScenePlugPtr &scene, const Contexts &contexts, const Gaffer::PlugPtr &editScope, const Names &names, const IECore::InternedString &root = "/", Gaffer::PathFilterPtr filter = nullptr )
			:	Path( names, root, filter ), m_scene( scene ), m_contexts( contexts ), m_editScope( editScope )
		{
			m_plugDirtiedConnection = m_scene->node()->plugDirtiedSignal().connect( boost::bind( &InspectorPath::emitPathChanged, this ) );
		}

		IE_CORE_DECLARERUNTIMETYPEDEXTENSION( InspectorPath, GafferSceneUI::InspectorPathTypeId, Gaffer::Path );

		~InspectorPath() override
		{
		}

		bool isValid( const IECore::Canceller *canceller = nullptr ) const override
		{
			if( !Path::isValid() )
			{
				return false;
			}

			return true; // TODO
			//const PathMatcher p = pathMatcher( canceller );
			//return p.match( names() ) & ( PathMatcher::ExactMatch | PathMatcher::DescendantMatch );
		}

		bool isLeaf( const IECore::Canceller *canceller ) const override
		{
			return false; // TODO
			// const PathMatcher p = pathMatcher( canceller );
			// const unsigned match = p.match( names() );
			// return match & PathMatcher::ExactMatch && !( match & PathMatcher::DescendantMatch );
		}

		PathPtr copy() const override
		{
			//return nullptr;
			return new InspectorPath( m_scene, m_contexts, m_editScope, names(), root(), const_cast<PathFilter *>( getFilter() ) );
		}

		void propertyNames( std::vector<IECore::InternedString> &names, const IECore::Canceller *canceller = nullptr ) const override
		{
			Path::propertyNames( names, canceller );
			names.push_back( g_inspectorPropertyName );
			names.push_back( g_contextPropertyName );
			names.push_back( g_contextAPropertyName );
			names.push_back( g_contextBPropertyName );
		}

		IECore::ConstRunTimeTypedPtr property( const IECore::InternedString &name, const IECore::Canceller *canceller = nullptr ) const override
		{
			if( name == g_contextPropertyName || name == g_contextAPropertyName )
			{
				return m_contexts[0];
			}
			else if( name == g_contextBPropertyName )
			{
				return m_contexts[1];
			}
			else if( name == g_inspectorPropertyName )
			{
				auto x = allInspections( canceller );
				auto it = x.find( names() );
				if( it == x.end() )
				{
					return nullptr;
				}
				return it->second;
			}

			return Path::property( name, canceller );
		}

		const Gaffer::Plug *cancellationSubject() const override
		{
			return m_scene.get();
		}

	protected :

		void doChildren( std::vector<PathPtr> &children, const IECore::Canceller *canceller ) const override
		{
			auto x = allInspections( canceller );

			// auto it = x.lower_bound( names() );
			// if( it != )

			// TODO : DON'T BE QUADRATIC!!!

			InternedString lastChildName;
			for( const auto &[path, inspector] : x )
			{
				if( path.size() <= names().size() )
				{
					continue;
				}

				if( !std::equal( names().begin(), names().end(), path.begin() ) )
				{
					continue;
				}

				const InternedString childName = path[names().size()];
				if( childName == lastChildName )
				{
					continue;
				}

				auto newNames = names();
				newNames.push_back( childName );
				children.push_back(
					new InspectorPath( m_scene, m_contexts, m_editScope, newNames, root(), const_cast<PathFilter *>( getFilter() ) ) // TODO : WHY CONST CAST?
				);
				lastChildName = childName;
			}
		}

	private :

		map<vector<InternedString>, GafferSceneUI::Private::ConstInspectorPtr> allInspections( const IECore::Canceller *canceller ) const
		{
			map<vector<InternedString>, GafferSceneUI::Private::ConstInspectorPtr> result;

			for( const auto &context : m_contexts )
			{
				if( !context )
				{
					continue;
				}

				Context::EditableScope scope( context.get() );
				if( canceller )
				{
					scope.setCanceller( canceller );
				}

				for( const auto &[root, thing] : *g_inspectionProviders )
				{
					if( names().size() && root[0] != names()[0] )
					{
						// Skip stuff not matching the first part of the name. We can do better than this I think,
						// but this is the minimum needed to avoid evaluating per-location stuff with the context
						// for `/Globals`.
						continue;
					}

					if( names().size() && names()[0] == "Selection" )
					{
						if( !m_scene->existsPlug()->getValue() )
						{
							continue;
						}
					}

					auto x = thing( m_scene.get(), m_editScope );
					for( const auto &[subPath, inspector] : x )
					{
						auto p = root;
						for( auto n : subPath )
						{
							p.push_back( n ); // TODO : INSERT RANGE
						}
						result[p] = inspector;
					}

				}

			}

			return result;
		}

		ScenePlugPtr m_scene;
		Contexts m_contexts;
		Gaffer::PlugPtr m_editScope;
		Gaffer::Signals::ScopedConnection m_plugDirtiedConnection;

};

IE_CORE_DEFINERUNTIMETYPED( InspectorPath );

InspectorPath::Ptr inspectorPathConstructor( ScenePlug &scene, object pythonContexts, const Gaffer::PlugPtr &editScope, const Path::Names &names, const IECore::InternedString &root, Gaffer::PathFilterPtr filter )
{
	InspectorPath::Contexts contexts;
	for( size_t i = 0; i < contexts.size(); ++i )
	{
		contexts[i] = extract<ContextPtr>( pythonContexts[i] )();
	}
	return new InspectorPath( &scene, contexts, editScope, names, root, filter );
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// InspectorDiffColumn
//////////////////////////////////////////////////////////////////////////

namespace
{

const std::array<ConstStringDataPtr, 2> g_diffColumnHeaders = {
	new StringData( "A" ),
	new StringData( "B" )
};

const std::array<ConstColor4fDataPtr, 2> g_diffColumnBackgroundColors = {
	new Color4fData( Color4f( 0.7, 0.12, 0, 0.3 ) ),
	new Color4fData( Color4f( 0.13, 0.62, 0, 0.3 ) )
};

const std::array<InternedString, 2> g_diffColumnContextProperties = { "inspector:contextA", "inspector:contextB" };

class InspectorDiffColumn : public GafferSceneUI::Private::InspectorColumn
{

	public :

		IE_CORE_DECLAREMEMBERPTR( InspectorDiffColumn )

		enum class DiffContext { A, B };

		InspectorDiffColumn( DiffContext diffContext )
			:	InspectorColumn(
					"inspector:inspector",
					CellData( g_diffColumnHeaders[(int)diffContext] ),
					g_diffColumnContextProperties[(int)diffContext]
				),
				m_backgroundColor( g_diffColumnBackgroundColors[(int)diffContext] )
		{
			const DiffContext otherContext = diffContext == DiffContext::A ? DiffContext::B : DiffContext::A;
			m_otherColumn = new InspectorColumn( "inspector:inspector", CellData( g_diffColumnHeaders[(int)diffContext] ), g_diffColumnContextProperties[(int)otherContext] );
		}

		CellData cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const override
		{
			CellData result = InspectorColumn::cellData( path, canceller );

			GafferSceneUI::Private::Inspector::ResultPtr inspectionA = inspect( path, canceller );
			GafferSceneUI::Private::Inspector::ResultPtr inspectionB = m_otherColumn->inspect( path, canceller );
			const Object *valueA = inspectionA ? inspectionA->value() : nullptr;
			const Object *valueB = inspectionB ? inspectionB->value() : nullptr;

			bool different = false;
			if( (bool)valueA != (bool)valueB )
			{
				different = true;
			}
			else if( valueA )
			{
				different = valueA->isNotEqualTo( valueB );
			}

			result.background = different ? m_backgroundColor : nullptr;

			return result;
		}

	private :

		GafferSceneUI::Private::ConstInspectorColumnPtr m_otherColumn;
		ConstColor4fDataPtr m_backgroundColor;

};

} // namespace

//////////////////////////////////////////////////////////////////////////
// Bindings
//////////////////////////////////////////////////////////////////////////

void GafferSceneUIModule::bindSceneInspector()
{

	object module( borrowed( PyImport_AddModule( "GafferSceneUI._SceneInspector" ) ) );
	scope().attr( "_SceneInspector" ) = module;
	scope moduleScope( module );

	PathClass<InspectorPath>()

		.def(
			"__init__",
			make_constructor(
				inspectorPathConstructor,
				default_call_policies(),
				(
					boost::python::arg( "scene" ),
					boost::python::arg( "contexts" ),
					boost::python::arg( "editScope" ),
					boost::python::arg( "names" ) = boost::python::list(),
					boost::python::arg( "root" ) = "/",
					boost::python::arg( "filter" ) = object()
				)
			)
		)
	;

	{
		scope s = RefCountedClass<InspectorDiffColumn, GafferSceneUI::Private::InspectorColumn>( "InspectorDiffColumn" )
			.def( init<InspectorDiffColumn::DiffContext>() )
		;

		enum_<InspectorDiffColumn::DiffContext>( "DiffContext" )
			.value( "A", InspectorDiffColumn::DiffContext::A )
			.value( "B", InspectorDiffColumn::DiffContext::B )
		;
	}

	def( "registerInspectors", &registerInspectors );

}
