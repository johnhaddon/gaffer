//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2022, Cinesite VFX Ltd. All rights reserved.
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

#include "GafferScene/SetPath.h"

#include "GafferScene/ScenePlug.h"

//#include "Gaffer/ArrayPlug.h"
#include "Gaffer/Context.h"
#include "Gaffer/Node.h"
#include "Gaffer/PathFilter.h"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind/bind.hpp"

using namespace boost::placeholders;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

IE_CORE_DEFINERUNTIMETYPED( SetPath );

SetPath::SetPath( ScenePlugPtr scene, Gaffer::ContextPtr context, Gaffer::PathFilterPtr filter )
	:	Path( filter )
{
	setScene( scene );
	setContext( context );
}

SetPath::SetPath( ScenePlugPtr scene, Gaffer::ContextPtr context, const std::string &path, Gaffer::PathFilterPtr filter )
	:	Path( path, filter )
{
	setScene( scene );
	setContext( context );
}

SetPath::SetPath( ScenePlugPtr scene, Gaffer::ContextPtr context, const Names &names, const IECore::InternedString &root, Gaffer::PathFilterPtr filter )
	:	Path( names, root, filter )
{
	setScene( scene );
	setContext( context );
}

SetPath::~SetPath()
{
}

void SetPath::setScene( ScenePlugPtr scene )
{
	if( m_scene == scene )
	{
		return;
	}

	m_scene = scene;
	m_plugDirtiedConnection = scene->node()->plugDirtiedSignal().connect( boost::bind( &SetPath::plugDirtied, this, ::_1 ) );

	emitPathChanged();
}

ScenePlug *SetPath::getScene()
{
	return m_scene.get();
}

const ScenePlug *SetPath::getScene() const
{
	return m_scene.get();
}

void SetPath::setContext( Gaffer::ContextPtr context )
{
	if( m_context == context )
	{
		return;
	}


	m_context = context;
	m_contextChangedConnection = context->changedSignal().connect( boost::bind( &SetPath::contextChanged, this, ::_2 ) );

	emitPathChanged();
}

Gaffer::Context *SetPath::getContext()
{
	return m_context.get();
}

const Gaffer::Context *SetPath::getContext() const
{
	return m_context.get();
}

bool SetPath::isValid( const IECore::Canceller *canceller ) const
{
	if( !Path::isValid() )
	{
		return false;
	}

	std::cerr << "IS VALID CALLED" << std::endl;

	// Context::EditableScope scopedContext( m_context.get() );
	// if( canceller )
	// {
	// 	scopedContext.setCanceller( canceller );
	// }
	// return m_scene->exists( names() );

	return true;
}

bool SetPath::isLeaf( const IECore::Canceller *canceller ) const
{
	// DUNNO!

	std::cerr << "IS LEAF CALLED" << std::endl;

	return names().size();
}

PathPtr SetPath::copy() const
{
	return new SetPath( m_scene, m_context, names(), root(), const_cast<PathFilter *>( getFilter() ) );
}

const Gaffer::Plug *SetPath::cancellationSubject() const
{
	return m_scene.get();
}

void SetPath::doChildren( std::vector<PathPtr> &children, const IECore::Canceller *canceller ) const
{
	if( names().size() )
	{
		return;
	}

	Context::EditableScope scopedContext( m_context.get() );
	if( canceller )
	{
		scopedContext.setCanceller( canceller );
	}

	ConstInternedStringVectorDataPtr setNamesData = m_scene->setNames();
	for( const auto &setName : setNamesData->readable() )
	{
		children.push_back( new SetPath( m_scene, m_context, { setName }, root(), const_cast<PathFilter *>( getFilter() ) ) );
	}
}

void SetPath::contextChanged( const IECore::InternedString &key )
{
	if( !boost::starts_with( key.c_str(), "ui:" ) )
	{
		emitPathChanged();
	}
}

void SetPath::plugDirtied( Gaffer::Plug *plug )
{
	if( plug == m_scene->setNamesPlug() )
	{
		std::cerr << "emitPathChanged " << plug->fullName() << " " << this << std::endl;
		emitPathChanged();
	}
}
