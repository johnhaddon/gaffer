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
#include "GafferSceneUI/Private/BoundInspector.h"
#include "GafferSceneUI/TypeIds.h"

// #include "GafferScene/ScenePlug.h"

// #include "GafferUI/PathColumn.h"

#include "GafferBindings/PathBinding.h"

#include "Gaffer/Context.h"
// #include "Gaffer/Node.h"
#include "Gaffer/ParallelAlgo.h"
#include "Gaffer/Path.h"
#include "Gaffer/PathFilter.h"
#include "Gaffer/Private/IECorePreview/LRUCache.h"

#include "IECoreScene/Camera.h"
#include "IECoreScene/ExternalProcedural.h"
#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/Primitive.h"

// #include "IECorePython/RefCountedBinding.h"

// #include "IECore/StringAlgo.h"

#include "boost/algorithm/string/predicate.hpp"
// #include "boost/bind/bind.hpp"

#include "Imath/ImathMatrixAlgo.h"

#include <map>

using namespace std;
using namespace Imath;
// using namespace boost::placeholders;
using namespace boost::python;
using namespace IECore;
using namespace IECoreScene;
// using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;
// using namespace GafferUI;
using namespace GafferScene;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// History cache for BasicInspector
//////////////////////////////////////////////////////////////////////////

namespace
{

// This uses the same strategy that ValuePlug uses for the hash cache,
// using `plug->dirtyCount()` to invalidate previous cache entries when
// a plug is dirtied.
/// \todo SHOULD THIS BE SHARED SOMEWHERE? AT LEAST WITH BOUNDINSPECTOR? OR IS BOUNDINSPECTOR A BASICINSPECTOR? DO WE PUT IT ALL IN SCENEINSPECTORBINDING FOR NOW?
struct HistoryCacheKey
{
	HistoryCacheKey() {};
	HistoryCacheKey( const ValuePlug *plug )
		:	plug( plug ), contextHash( Context::current()->hash() ), dirtyCount( plug->dirtyCount() )
	{
	}

	bool operator==( const HistoryCacheKey &rhs ) const
	{
		return
			plug == rhs.plug &&
			contextHash == rhs.contextHash &&
			dirtyCount == rhs.dirtyCount
		;
	}

	const ValuePlug *plug;
	IECore::MurmurHash contextHash;
	uint64_t dirtyCount;
};

size_t hash_value( const HistoryCacheKey &key )
{
	size_t result = 0;
	boost::hash_combine( result, key.plug );
	boost::hash_combine( result, key.contextHash );
	boost::hash_combine( result, key.dirtyCount );
	return result;
}

using HistoryCache = IECorePreview::LRUCache<HistoryCacheKey, SceneAlgo::History::ConstPtr>;

HistoryCache g_historyCache(
	// Getter
	[] ( const HistoryCacheKey &key, size_t &cost, const IECore::Canceller *canceller ) {
		assert( canceller == Context::current()->canceller() );
		cost = 1;
		return SceneAlgo::history(
			key.plug, Context::current()->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName )
		);
	},
	// Max cost
	1000,
	// Removal callback
	[] ( const HistoryCacheKey &key, const SceneAlgo::History::ConstPtr &history ) {
		// Histories contain PlugPtrs, which could potentially be the sole
		// owners. Destroying plugs can trigger dirty propagation, so as a
		// precaution we destroy the history on the UI thread, where this would
		// be OK.
		ParallelAlgo::callOnUIThread(
			[history] () {}
		);
	}

);

} // namespace

//////////////////////////////////////////////////////////////////////////
// BasicInspector
//////////////////////////////////////////////////////////////////////////

namespace
{

class BasicInspector : public GafferSceneUI::Private::Inspector
{

	public :

		using ValueFunction = std::function<IECore::ConstObjectPtr( const GafferScene::SceneAlgo::History *history )>;

		BasicInspector(
			const Gaffer::ValuePlugPtr &plug,
			const Gaffer::PlugPtr &editScope,
			const ValueFunction &valueFunction
		)
			:	Inspector( "", "", editScope ), m_plug( plug ), m_valueFunction( valueFunction )
		{
		}

		IE_CORE_DECLARERUNTIMETYPEDEXTENSION( BasicInspector, BoundInspectorTypeId, GafferSceneUI::Private::Inspector );

	protected :

		GafferScene::SceneAlgo::History::ConstPtr history() const override
		{
			const auto scenePlug = m_plug->parent<ScenePlug>();
			if( m_plug != scenePlug->globalsPlug() &&m_plug != scenePlug->setNamesPlug() && m_plug != scenePlug->setPlug() )
			{
				if( !scenePlug->existsPlug()->getValue() )
				{
					return nullptr;
				}
			}

			return g_historyCache.get( HistoryCacheKey( m_plug.get() ), Context::current()->canceller() );
		}

		IECore::ConstObjectPtr value( const GafferScene::SceneAlgo::History *history ) const override
		{
			Context::Scope scope( history->context.get() ); // TODO : CANCELLER PLEASE??? OR IS IT IN THERE ALREADY?
			return m_valueFunction( history );
		}

	private :

		void plugDirtied( Gaffer::Plug *plug );

		const Gaffer::ValuePlugPtr m_plug;
		ValueFunction m_valueFunction;

};

IE_CORE_DECLAREPTR( BasicInspector )

} // namespace

//////////////////////////////////////////////////////////////////////////
// Registry Shenanigans TODO : BETTER NAME PLEASE!
//////////////////////////////////////////////////////////////////////////

namespace
{

using Inspections = map<vector<InternedString>, GafferSceneUI::Private::ConstInspectorPtr>;
using InspectionProvider = std::function<Inspections ( ScenePlug * )>;

Inspections transformInspectionProvider( ScenePlug *scene )
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
			result[path] = new BasicInspector(
				scene->transformPlug(), nullptr,
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

Inspections boundInspectionProvider( ScenePlug *scene )
{
	Inspections result;
	result[{"Local"}] = new BasicInspector(
		scene->boundPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			return new Box3fData( history->scene->boundPlug()->getValue() );
		}
	);
	result[{"World"}] = new BasicInspector(
		scene->boundPlug(), nullptr,
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

Inspections attributeInspectionProvider( ScenePlug *scene )
{
	ConstCompoundObjectPtr attributes = scene->attributesPlug()->getValue();
	Inspections result;
	for( const auto &[name, value] : attributes->members() )
	{
		/// \todo EditScope
		result.insert( { std::vector<InternedString>( { name } ), new GafferSceneUI::Private::AttributeInspector( scene, nullptr, name ) } );
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

Inspections objectParametersInspectionProvider( ScenePlug *scene )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( auto parameters = objectParameters( object.get() ) )
	{
		for( const auto &[name, value] : parameters->readable() )
		{
			result[{ name }] = new BasicInspector(
				scene->objectPlug(), nullptr,
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

Inspections objectTypeInspectionProvider( ScenePlug *scene )
{
	Inspections result;
	ConstObjectPtr object = scene->objectPlug()->getValue();
	if( object->typeId() != NullObjectTypeId )
	{
		result[{"Type"}] = new BasicInspector(
			scene->objectPlug(), nullptr,
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

Inspections primitiveVariablesInspectionProvider( ScenePlug *scene )
{
	ConstObjectPtr object = scene->objectPlug()->getValue();
	Inspections result;
	if( auto primitive = runTimeCast<const Primitive>( object.get() ) )
	{
		for( const auto &[name, variable] : primitive->variables )
		{
			result[{ name, "Interpolation" }] = new BasicInspector(
				scene->objectPlug(), nullptr,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableInterpolation( name, history );
				}
			);
			result[{ name, "Type" }] = new BasicInspector(
				scene->objectPlug(), nullptr,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableType( name, history );
				}
			);
			result[{ name, "Data" }] = new BasicInspector(
				scene->objectPlug(), nullptr,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableData( name, history );
				}
			);
			result[{ name, "Indices" }] = new BasicInspector(
				scene->objectPlug(), nullptr,
				[name] ( const SceneAlgo::History *history ) {
					return primitiveVariableIndices( name, history );
				}
			);
		}
	}
	return result;
}

Inspections subdivisionInspectionProvider( ScenePlug *scene )
{
	Inspections result;

	ConstObjectPtr object = scene->objectPlug()->getValue();
	auto mesh = runTimeCast<const MeshPrimitive>( object.get() );
	if( !mesh )
	{
		return result;
	}

	result[{"Interpolation"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->interpolation() ) : nullptr;
		}
	);

	result[{"Corners"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new UInt64Data( mesh->cornerIds()->readable().size() ) : nullptr;
		}
	);

	result[{"Corners","Indices"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->cornerIds() : nullptr;
		}
	);

	result[{"Corners","Sharpnesses"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->cornerSharpnesses() : nullptr;
		}
	);

	result[{"Creases"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new UInt64Data( mesh->creaseLengths()->readable().size() ) : nullptr;
		}
	);

	result[{"Creases","Lengths"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->creaseLengths() : nullptr;
		}
	);

	result[{"Creases","Ids"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->creaseIds() : nullptr;
		}
	);

	result[{"Creases","Sharpnesses"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? mesh->creaseSharpnesses() : nullptr;
		}
	);

	result[{"Interpolate Boundary"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->getInterpolateBoundary() ) : nullptr;
		}
	);

	result[{"FaceVarying Linear Interpolation"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->getFaceVaryingLinearInterpolation() ) : nullptr;
		}
	);

	result[{"Triangle Subdivision Rule"}] = new BasicInspector(
		scene->objectPlug(), nullptr,
		[] ( const SceneAlgo::History *history ) {
			ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( history->scene->objectPlug()->getValue() );
			return mesh ? new StringData( mesh->getTriangleSubdivisionRule() ) : nullptr;
		}
	);
	return result;
}

multimap<vector<InternedString>, InspectionProvider> g_inspectionProviders = {
	{ { "Bound" }, boundInspectionProvider },
	{ { "Transform" }, transformInspectionProvider },
	{ { "Attributes" }, attributeInspectionProvider },
	{ { "Object" }, objectTypeInspectionProvider },
	{ { "Object", "Parameters" }, objectParametersInspectionProvider },
	{ { "Object", "Primitive Variables" }, primitiveVariablesInspectionProvider },
	{ { "Object", "Subdivision" }, subdivisionInspectionProvider },
};

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
		InspectorPath( const ScenePlugPtr &scene, const Contexts &contexts, const Names &names, const IECore::InternedString &root = "/", Gaffer::PathFilterPtr filter = nullptr )
			:	Path( names, root, filter ), m_scene( scene ), m_contexts( contexts )
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
			return new InspectorPath( m_scene, m_contexts, names(), root(), const_cast<PathFilter *>( getFilter() ) );
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
					new InspectorPath( m_scene, m_contexts, newNames, root(), const_cast<PathFilter *>( getFilter() ) ) // TODO : WHY CONST CAST?
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

				for( const auto &[root, thing] : g_inspectionProviders )
				{
					auto x = thing( m_scene.get() );
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
		Gaffer::Signals::ScopedConnection m_plugDirtiedConnection;

};

IE_CORE_DEFINERUNTIMETYPED( InspectorPath );

// const ScenePlugPtr &scene, const Contexts &contexts, const Names &names, const IECore::InternedString &root = "/", Gaffer::PathFilterPtr filter;;
InspectorPath::Ptr inspectorPathConstructor( ScenePlug &scene, object pythonContexts, const Path::Names &names, const IECore::InternedString &root, Gaffer::PathFilterPtr filter )
{
	InspectorPath::Contexts contexts;
	for( size_t i = 0; i < contexts.size(); ++i )
	{
		contexts[i] = extract<ContextPtr>( pythonContexts[i] )();
	}
	return new InspectorPath( &scene, contexts, names, root, filter );
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// InspectorDiffColumn
//////////////////////////////////////////////////////////////////////////

// ConstStringDataPtr g_adaptorDisabledRenderPassIcon = new StringData( "adaptorDisabledRenderPass.png" );
// ConstStringDataPtr g_disabledRenderPassIcon = new StringData( "disabledRenderPass.png" );
// ConstStringDataPtr g_renderPassIcon = new StringData( "renderPass.png" );
// ConstStringDataPtr g_renderPassFolderIcon = new StringData( "renderPassFolder.png" );
// ConstStringDataPtr g_disabledToolTip = new StringData( "Disabled." );
// ConstStringDataPtr g_adaptorDisabledToolTip = new StringData( "Automatically disabled by a render adaptor.");
// const Color4fDataPtr g_dimmedForegroundColor = new Color4fData( Imath::Color4f( 152, 152, 152, 255 ) / 255.0f );

// class InspectorDiffColumn : public InspectorColumn
// {

// 	public :

// 		IE_CORE_DECLAREMEMBERPTR( InspectorDiffColumn )

// 		// InspectorDiffColumn()
// 		// 	:	StandardPathColumn( "Name", "name" )
// 		// {
// 		// }

// 		// CellData cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const override
// 		// {
// 		// 	CellData result = StandardPathColumn::cellData( path, canceller );

// 		// 	if( !runTimeCast<const IECore::StringData>( path.property( g_renderPassNamePropertyName, canceller ) ) )
// 		// 	{
// 		// 		result.icon = g_renderPassFolderIcon;
// 		// 		return result;
// 		// 	}

// 		// 	// Enable render adaptors as they may have disabled or deleted render passes.
// 		// 	auto pathCopy = runTimeCast<RenderPassPath>( path.copy() );
// 		// 	if( !pathCopy )
// 		// 	{
// 		// 		return result;
// 		// 	}
// 		// 	ContextPtr adaptorEnabledContext = new Context( *pathCopy->getContext() );
// 		// 	adaptorEnabledContext->set<bool>( g_enableAdaptorsContextName, true );
// 		// 	pathCopy->setContext( adaptorEnabledContext );

// 		// 	bool enabled = true;
// 		// 	if( !runTimeCast<const IECore::StringData>( pathCopy->property( g_renderPassNamePropertyName, canceller ) ) )
// 		// 	{
// 		// 		// The render pass has been deleted by a render adaptor, so present it to the user as disabled.
// 		// 		enabled = false;
// 		// 	}
// 		// 	else if( const auto enabledData = runTimeCast<const IECore::BoolData>( pathCopy->property( g_renderPassEnabledPropertyName, canceller ) ) )
// 		// 	{
// 		// 		enabled = enabledData->readable();
// 		// 	}

// 		// 	if( enabled )
// 		// 	{
// 		// 		result.icon = g_renderPassIcon;
// 		// 		return result;
// 		// 	}

// 		// 	// Check `renderPass:enabled` without render adaptors enabled
// 		// 	// to determine whether the render pass was disabled upstream
// 		// 	// or by a render adaptor.
// 		// 	const auto enabledData = runTimeCast<const IECore::BoolData>( path.property( g_renderPassEnabledPropertyName, canceller ) );
// 		// 	enabled = !enabledData || enabledData->readable();

// 		// 	result.icon = enabled ? g_adaptorDisabledRenderPassIcon : g_disabledRenderPassIcon;
// 		// 	result.toolTip = enabled ? g_adaptorDisabledToolTip : g_disabledToolTip;
// 		// 	result.foreground = g_dimmedForegroundColor;

// 		// 	return result;
// 		// }

// };

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
					boost::python::arg( "names" ) = boost::python::list(),
					boost::python::arg( "root" ) = "/",
					boost::python::arg( "filter" ) = object()
				)
			)
		)
	;

	// RefCountedClass<RenderPassNameColumn, GafferUI::PathColumn>( "RenderPassNameColumn" )
	// 	.def( init<>() )
	// ;

	// RefCountedClass<RenderPassActiveColumn, GafferUI::PathColumn>( "RenderPassActiveColumn" )
	// 	.def( init<>() )
	// ;

	// RefCountedClass<DisabledRenderPassFilter, PathFilter>( "DisabledRenderPassFilter" )
	// 	.def( init<IECore::CompoundDataPtr>( ( boost::python::arg( "userData" ) = object() ) ) )
	// ;

}
