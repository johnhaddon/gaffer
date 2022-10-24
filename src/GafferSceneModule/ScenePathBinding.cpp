//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2014, Image Engine Design Inc. All rights reserved.
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

#include "ScenePathBinding.h"

#include "GafferScene/Filter.h"
#include "GafferScene/SceneFilterPathFilter.h"
#include "GafferScene/ScenePath.h"
#include "GafferScene/ScenePlug.h"
#include "GafferScene/SetPath.h"

#include "GafferBindings/PathBinding.h"

#include "Gaffer/Context.h"
#include "Gaffer/PathFilter.h"

#include "boost/python/suite/indexing/container_utils.hpp"

using namespace boost::python;
using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;
using namespace GafferScene;

namespace
{

template<typename T>
typename T::Ptr constructor1( ScenePlug &scene, Context &context, PathFilterPtr filter )
{
	return new T( &scene, &context, filter );
}

template<typename T>
typename T::Ptr constructor2( ScenePlug &scene, Context &context, const std::string &path, PathFilterPtr filter )
{
	return new T( &scene, &context, path, filter );
}

PathFilterPtr createStandardFilter( object pythonSetNames, const std::string &setsLabel )
{
	std::vector<std::string> setNames;
	boost::python::container_utils::extend_container( setNames, pythonSetNames );
	return ScenePath::createStandardFilter( setNames, setsLabel );
}

} // namespace

void GafferSceneModule::bindScenePath() // TODO : RENAME TO BINDPATH, AND RENAME FILE TOO
{

	PathClass<ScenePath>()
		.def(
			"__init__",
			make_constructor(
				constructor1<ScenePath>,
				default_call_policies(),
				(
					arg( "scene" ),
					arg( "context" ),
					arg( "filter" ) = object()
				)
			)
		)
		.def(
			"__init__",
			make_constructor(
				constructor2<ScenePath>,
				default_call_policies(),
				(
					arg( "scene" ),
					arg( "context" ),
					arg( "path" ),
					arg( "filter" ) = object()
				)
			)
		)
		.def( "setScene", &ScenePath::setScene )
		.def( "getScene", (ScenePlug *(ScenePath::*)())&ScenePath::getScene, return_value_policy<CastToIntrusivePtr>() )
		.def( "setContext", &ScenePath::setContext )
		.def( "getContext", (Context *(ScenePath::*)())&ScenePath::getContext, return_value_policy<CastToIntrusivePtr>() )
		.def( "createStandardFilter", &createStandardFilter, (
				arg( "setNames" ) = list(),
				arg( "setsLabel" ) = ""
			)
		)
		.staticmethod( "createStandardFilter" )
	;

	RunTimeTypedClass<SceneFilterPathFilter>()
		.def( init<FilterPtr, IECore::CompoundDataPtr>( ( arg( "filter" ), arg( "userData" ) = object() ) ) )
	;

	PathClass<SetPath>()
		.def(
			"__init__",
			make_constructor(
				constructor1<SetPath>,
				default_call_policies(),
				(
					arg( "scene" ),
					arg( "context" ),
					arg( "filter" ) = object()
				)
			)
		)
		.def(
			"__init__",
			make_constructor(
				constructor2<SetPath>,
				default_call_policies(),
				(
					arg( "scene" ),
					arg( "context" ),
					arg( "path" ),
					arg( "filter" ) = object()
				)
			)
		)
		.def( "setScene", &SetPath::setScene )
		.def( "getScene", (ScenePlug *(SetPath::*)())&SetPath::getScene, return_value_policy<CastToIntrusivePtr>() )
		.def( "setContext", &SetPath::setContext )
		.def( "getContext", (Context *(SetPath::*)())&SetPath::getContext, return_value_policy<CastToIntrusivePtr>() )
	;

}
