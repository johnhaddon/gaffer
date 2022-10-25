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

#include "Gaffer/Context.h"
#include "Gaffer/Node.h"
#include "Gaffer/PathFilter.h"

#include "IECore/StringAlgo.h"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind/bind.hpp"
#include "boost/container/flat_set.hpp"

using namespace std;
using namespace boost::placeholders;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

//////////////////////////////////////////////////////////////////////////
// Internal implementation
//////////////////////////////////////////////////////////////////////////

namespace
{

const boost::container::flat_set<InternedString> g_standardSets = {
	"__lights",
	"__lightFilters",
	"__cameras",
	"__coordinateSystems",
	"defaultLights",
	"soloLights"
};

Path::Names parent( InternedString setName )
{
	if( g_standardSets.contains( setName ) )
	{
		return { "Standard" };
	}
	else
	{
		Path::Names result;
		StringAlgo::tokenize( setName.string(), ':', result );
		if( result.size() == 1 )
		{
			// Nonsense for Cinesite
			result.clear();
			StringAlgo::tokenize( setName.string(), '_', result );
			result.resize( std::min( result.size(), (size_t)2 ) );
		}
		result.pop_back();
		return result;
	}
}

/// TODO : CACHE ME, OR DO WITHOUT ME. PROBABLY CACHE ME, THEN CINESITE CAN
/// INDULGE IN A PYTHON-BASED `PARENT()` FUNCTION.
PathMatcher pathMatcher( const ScenePlug *scene )
{
	ConstInternedStringVectorDataPtr setNamesData = scene->setNames();

	PathMatcher result;
	for( const auto &setName : setNamesData->readable() )
	{
		Path::Names path = parent( setName );
		path.push_back( setName );
		result.addPath( path );
	}

	return result;
}

const InternedString g_setNamePropertyName( "setPath:setName" );

} // namespace

//////////////////////////////////////////////////////////////////////////
// SetPath
//////////////////////////////////////////////////////////////////////////

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

void SetPath::propertyNames( std::vector<IECore::InternedString> &names, const IECore::Canceller *canceller ) const
{
	Path::propertyNames( names, canceller );
	names.push_back( g_setNamePropertyName );
}

IECore::ConstRunTimeTypedPtr SetPath::property( const IECore::InternedString &name, const IECore::Canceller *canceller ) const
{
	if( name == g_setNamePropertyName )
	{
		const PathMatcher p = pathMatcher( canceller );
		if( p.match( names() ) & PathMatcher::ExactMatch )
		{
			return new StringData( names().back().string() );
		}
	}
	return Path::property( name, canceller );
}

const Gaffer::Plug *SetPath::cancellationSubject() const
{
	return m_scene.get();
}

void SetPath::doChildren( std::vector<PathPtr> &children, const IECore::Canceller *canceller ) const
{
	const PathMatcher p = pathMatcher( canceller );

	auto it = p.find( names() );
	if( it == p.end() )
	{
		return;
	}

	++it;
	while( it != p.end() && it->size() == names().size() + 1 )
	{
		children.push_back( new SetPath( m_scene, m_context, *it, root(), const_cast<PathFilter *>( getFilter() ) ) );
		it.prune();
		++it;
	}

	std::sort(
		children.begin(), children.end(),
		[]( const PathPtr &a, const PathPtr &b ) {
			return a->names().back().string() < b->names().back().string();
		}
	);
}

const IECore::PathMatcher SetPath::pathMatcher( const IECore::Canceller *canceller ) const
{
	Context::EditableScope scopedContext( m_context.get() );
	if( canceller )
	{
		scopedContext.setCanceller( canceller );
	}
	return ::pathMatcher( m_scene.get() );
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
