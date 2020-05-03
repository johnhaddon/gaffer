//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
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

#include "ViewBinding.h"

#include "GafferUI/View.h"

#include "GafferBindings/NodeBinding.h"

#include "Gaffer/Context.h"
#include "Gaffer/EditScope.h"
#include "Gaffer/Plug.h"

#include "IECorePython/ScopedGILRelease.h"

#include "boost/python/suite/indexing/container_utils.hpp"

using namespace boost::python;
using namespace Gaffer;
using namespace GafferUI;

namespace
{

class ViewWrapper : public GafferBindings::NodeWrapper<View>
{

	public :

		ViewWrapper( PyObject *self, IECore::InternedString name, PlugPtr input )
			:	GafferBindings::NodeWrapper<View>( self, name, input )
		{
		}

};

struct ViewCreator
{
	ViewCreator( object fn )
		:	m_fn( fn )
	{
	}

	ViewPtr operator()( Gaffer::PlugPtr plug )
	{
		IECorePython::ScopedGILLock gilLock;
		ViewPtr result = extract<ViewPtr>( m_fn( plug ) );
		return result;
	}

	private :

		object m_fn;

};

void registerView1( IECore::TypeId plugType, object creator )
{
	View::registerView( plugType, ViewCreator( creator ) );
}

void registerView2( IECore::TypeId nodeType, const std::string &plugPath, object creator )
{
	View::registerView( nodeType, plugPath, ViewCreator( creator ) );
}

ViewPtr create( Gaffer::PlugPtr input )
{
	IECorePython::ScopedGILRelease gilRelease;
	return View::create( input );
}

} // namespace

namespace GafferUIModule
{

Gaffer::NodePtr getPreprocessor( View &v )
{
	return v.getPreprocessor();
}

void bindView()
{
	GafferBindings::NodeClass<View, ViewWrapper>( nullptr, no_init )
		.def( init<IECore::InternedString, PlugPtr>() )
		.def( "editScope", (EditScope *(View::*)())&View::editScope, return_value_policy<IECorePython::CastToIntrusivePtr>() )
		.def( "getContext", (Context *(View::*)())&View::getContext, return_value_policy<IECorePython::CastToIntrusivePtr>() )
		.def( "setContext", &View::setContext )
		.def( "contextChangedSignal", &View::contextChangedSignal, return_internal_reference<1>() )
		.def( "viewportGadget", (ViewportGadget *(View::*)())&View::viewportGadget, return_value_policy<IECorePython::CastToIntrusivePtr>() )
		.def( "_setPreprocessor", &View::setPreprocessor )
		.def( "_getPreprocessor", &getPreprocessor )
		.def( "create", &create )
		.staticmethod( "create" )
		.def( "registerView", &registerView1 )
		.def( "registerView", &registerView2 )
		.staticmethod( "registerView" )
	;
}

} // namespace GafferUIModule
