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
#include "GafferSceneUI/TypeIds.h"

// #include "GafferScene/ScenePlug.h"

// #include "GafferUI/PathColumn.h"

#include "GafferBindings/PathBinding.h"

#include "Gaffer/Context.h"
// #include "Gaffer/Node.h"
#include "Gaffer/Path.h"
#include "Gaffer/PathFilter.h"
// #include "Gaffer/Private/IECorePreview/LRUCache.h"

// #include "IECorePython/RefCountedBinding.h"

// #include "IECore/StringAlgo.h"

#include "boost/algorithm/string/predicate.hpp"
// #include "boost/bind/bind.hpp"

#include <map>

using namespace std;
// using namespace boost::placeholders;
using namespace boost::python;
using namespace IECore;
// using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;
// using namespace GafferUI;
using namespace GafferScene;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Registry Shenanigans TODO : BETTER NAME PLEASE!
//////////////////////////////////////////////////////////////////////////

namespace
{

struct Inspection
{
	std::vector<InternedString> path;
	GafferSceneUI::Private::ConstInspectorPtr inspector;
};

using Inspections = std::vector<Inspection>;

using InspectionProvider = std::function<Inspections ( ScenePlug * )>;

Inspections attributeInspectionProvider( ScenePlug *scene )
{
	ConstCompoundObjectPtr attributes = scene->attributesPlug()->getValue();
	Inspections result;
	for( const auto &[name, value] : attributes->members() )
	{
		/// \todo EditScope
		result.push_back( { std::vector<InternedString>( { name } ), new GafferSceneUI::Private::AttributeInspector( scene, nullptr, name ) } );
	}
	return result;
}

multimap<vector<InternedString>, InspectionProvider> g_inspectionProviders = {
	{ { "Attributes" }, attributeInspectionProvider }
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
			names.push_back( g_contextAPropertyName );
			names.push_back( g_contextBPropertyName );
		}

		IECore::ConstRunTimeTypedPtr property( const IECore::InternedString &name, const IECore::Canceller *canceller = nullptr ) const override
		{
			if( name == g_contextAPropertyName )
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


			// const PathMatcher p = pathMatcher( canceller );

			// auto it = p.find( names() );
			// if( it == p.end() )
			// {
			// 	return;
			// }

			// ++it;
			// while( it != p.end() && it->size() == names().size() + 1 )
			// {
			// 	children.push_back( new RenderPassPath( m_scene, m_context, *it, root(), const_cast<PathFilter *>( getFilter() ), m_grouped ) );
			// 	it.prune();
			// 	++it;
			// }

			// std::sort(
			// 	children.begin(), children.end(),
			// 	[]( const PathPtr &a, const PathPtr &b ) {
			// 		return a->names().back().string() < b->names().back().string();
			// 	}
			// );
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
					for( const auto &xx : x )
					{
						auto p = root;
						for( auto n : xx.path )
						{
							p.push_back( n ); // TODO : INSERT RANGE
						}
						result[p] = xx.inspector;
					}

				}

			}

			return result;
		}

		// void plugDirtied( Gaffer::Plug *plug )
		// {
		// 	if( plug == m_scene->globalsPlug() )
		// 	{
		// 		emitPathChanged();
		// 	}
		// }

		// Gaffer::NodePtr m_node;
		ScenePlugPtr m_scene;
		Contexts m_contexts;
		// Gaffer::Signals::ScopedConnection m_plugDirtiedConnection;
		// Gaffer::Signals::ScopedConnection m_contextChangedConnection;
		// bool m_grouped;

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

// RenderPassPath::Ptr constructor1( ScenePlug &scene, Context &context, PathFilterPtr filter, const bool grouped )
// {
// 	return new RenderPassPath( &scene, &context, filter, grouped );
// }

// RenderPassPath::Ptr constructor2( ScenePlug &scene, Context &context, const std::vector<IECore::InternedString> &names, const IECore::InternedString &root, PathFilterPtr filter, const bool grouped )
// {
// 	return new RenderPassPath( &scene, &context, names, root, filter, grouped );
// }

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

		// .def(
		// 	"__init__",
		// 	make_constructor(
		// 		constructor1,
		// 		default_call_policies(),
		// 		(
		// 			boost::python::arg( "scene" ),
		// 			boost::python::arg( "context" ),
		// 			boost::python::arg( "filter" ) = object(),
		// 			boost::python::arg( "grouped" ) = false
		// 		)
		// 	)
		// )
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
			// init<const ScenePlugPtr &, const InspectorPath::Contexts &, const Path::Names &, const InternedString, Gaffer::PathFilterPtr>( (
			// 	boost::python::arg( "scene" ),
			// 	boost::python::arg( "contexts" ),
			// 	boost::python::arg( "names" ) = boost::python::list(),
			// 	boost::python::arg( "root" ) = "/",
			// 	boost::python::arg( "filter" ) = object()
			// ) )
			// make_constructor(
			// 	constructor2,
			// 	default_call_policies(),
			// 	(
			// 		boost::python::arg( "scene" ),
			// 		boost::python::arg( "context" ),
			// 		boost::python::arg( "names" ) = list(),
			// 		boost::python::arg( "root" ) = "/",
			// 		boost::python::arg( "filter" ) = object(),
			// 		boost::python::arg( "grouped" ) = false
			// 	)
			// )
		// .def( "setScene", &RenderPassPath::setScene )
		// .def( "getScene", (ScenePlug *(RenderPassPath::*)())&RenderPassPath::getScene, return_value_policy<CastToIntrusivePtr>() )
		// .def( "setContext", &RenderPassPath::setContext )
		// .def( "getContext", (Context *(RenderPassPath::*)())&RenderPassPath::getContext, return_value_policy<CastToIntrusivePtr>() )
		// .def( "registerPathGroupingFunction", &registerPathGroupingFunctionWrapper )
		// .staticmethod( "registerPathGroupingFunction" )
		// .def( "pathGroupingFunction", &pathGroupingFunctionWrapper )
		// .staticmethod( "pathGroupingFunction" )
	;

	// RefCountedClass<RenderPassNameColumn, GafferUI::PathColumn>( "RenderPassNameColumn" )
	// 	.def( init<>() )
	// ;

	// RefCountedClass<RenderPassActiveColumn, GafferUI::PathColumn>( "RenderPassActiveColumn" )
	// 	.def( init<>() )
	// ;

	// RefCountedClass<RenderPassEditorSearchFilter, PathFilter>( "SearchFilter" )
	// 	.def( init<IECore::CompoundDataPtr>( ( boost::python::arg( "userData" ) = object() ) ) )
	// 	.def( "setMatchPattern", &RenderPassEditorSearchFilter::setMatchPattern )
	// 	.def( "getMatchPattern", &RenderPassEditorSearchFilter::getMatchPattern, return_value_policy<copy_const_reference>() )
	// ;

	// RefCountedClass<DisabledRenderPassFilter, PathFilter>( "DisabledRenderPassFilter" )
	// 	.def( init<IECore::CompoundDataPtr>( ( boost::python::arg( "userData" ) = object() ) ) )
	// ;

}
