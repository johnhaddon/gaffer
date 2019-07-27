//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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

#include "GafferScene/PathFilter.h"

#include "GafferScene/ScenePlug.h"

#include "Gaffer/Context.h"

#include "boost/bind.hpp"

using namespace GafferScene;
using namespace Gaffer;
using namespace IECore;
using namespace std;

namespace
{

/// \todo Move to PlugAlgo and use instead of `Switch::variesWithContext()`
bool isComputed( const ValuePlug *plug )
{
	const Plug *source = plug->source();
	return source->direction() == Plug::Out && IECore::runTimeCast<const ComputeNode>( source->node() );
}

} // namespace

IE_CORE_DEFINERUNTIMETYPED( PathFilter );

size_t PathFilter::g_firstPlugIndex = 0;

PathFilter::PathFilter( const std::string &name )
	:	Filter( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringVectorDataPlug( "paths", Plug::In, new StringVectorData ) );
	addChild( new BoolVectorDataPlug( "enabledPaths", Plug::In, new BoolVectorData ) );
	addChild( new PathMatcherDataPlug( "__pathMatcher", Plug::Out, new PathMatcherData ) );

	plugDirtiedSignal().connect( boost::bind( &PathFilter::plugDirtied, this, ::_1 ) );
}

PathFilter::~PathFilter()
{
}

Gaffer::StringVectorDataPlug *PathFilter::pathsPlug()
{
	return getChild<Gaffer::StringVectorDataPlug>( g_firstPlugIndex );
}

const Gaffer::StringVectorDataPlug *PathFilter::pathsPlug() const
{
	return getChild<Gaffer::StringVectorDataPlug>( g_firstPlugIndex );
}

Gaffer::BoolVectorDataPlug *PathFilter::enabledPathsPlug()
{
	return getChild<Gaffer::BoolVectorDataPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::BoolVectorDataPlug *PathFilter::enabledPathsPlug() const
{
	return getChild<Gaffer::BoolVectorDataPlug>( g_firstPlugIndex + 1 );
}

Gaffer::PathMatcherDataPlug *PathFilter::pathMatcherPlug()
{
	return getChild<PathMatcherDataPlug>( g_firstPlugIndex + 2 );
}

const Gaffer::PathMatcherDataPlug *PathFilter::pathMatcherPlug() const
{
	return getChild<PathMatcherDataPlug>( g_firstPlugIndex + 2 );
}

void PathFilter::plugDirtied( const Gaffer::Plug *plug )
{
	if( plug == pathsPlug() || plug == enabledPathsPlug() )
	{
		if( !isComputed( pathsPlug() ) && !isComputed( enabledPathsPlug() ) )
		{
			// Paths are constant, so we an optimise by precomputing and
			// storing them locally.
			m_pathMatcher = pathMatcherPlug()->getValue();
		}
		else
		{
			m_pathMatcher = nullptr;
		}
	}
}

void PathFilter::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	Filter::affects( input, outputs );

	if( input == pathsPlug() || input == enabledPathsPlug() )
	{
		outputs.push_back( pathMatcherPlug() );
	}
	else if( input == pathMatcherPlug() )
	{
		outputs.push_back( outPlug() );
	}
}

void PathFilter::hash( const Gaffer::ValuePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	Filter::hash( output, context, h );

	if( output == pathMatcherPlug() )
	{
		pathsPlug()->hash( h );
		enabledPathsPlug()->hash( h );
	}
}

void PathFilter::compute( Gaffer::ValuePlug *output, const Gaffer::Context *context ) const
{
	if( output == pathMatcherPlug() )
	{
		ConstStringVectorDataPtr pathsData = pathsPlug()->getValue();
		const auto &paths = pathsData->readable();

		ConstBoolVectorDataPtr enabledPathsData = enabledPathsPlug()->getValue();
		const auto &enabledPaths = enabledPathsData->readable();

		PathMatcherDataPtr pathMatcherData = new PathMatcherData;
		auto &pathMatcher = pathMatcherData->writable();

		for( int i = 0, e = paths.size(); i < e; ++i )
		{
			if( enabledPaths.size() <= i || enabledPaths[i] )
			{
				pathMatcher.addPath( paths[i] );
			}
		}

		static_cast<PathMatcherDataPlug *>( output )->setValue( pathMatcherData );
		return;
	}

	Filter::compute( output, context );
}

void PathFilter::hashMatch( const ScenePlug *scene, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	typedef IECore::TypedData<ScenePlug::ScenePath> ScenePathData;
	const ScenePathData *pathData = context->get<ScenePathData>( ScenePlug::scenePathContextName, nullptr );
	if( pathData )
	{
		const ScenePlug::ScenePath &path = pathData->readable();
		h.append( &(path[0]), path.size() );
	}
	if( m_pathMatcher )
	{
		m_pathMatcher->hash( h );
	}
	else
	{
		pathMatcherPlug()->hash( h );
	}
}

unsigned PathFilter::computeMatch( const ScenePlug *scene, const Gaffer::Context *context ) const
{
	typedef IECore::TypedData<ScenePlug::ScenePath> ScenePathData;
	const ScenePathData *pathData = context->get<ScenePathData>( ScenePlug::scenePathContextName, nullptr );
	if( pathData )
	{
		// If we have a precomputed PathMatcher, we use that to compute matches, otherwise
		// we grab the PathMatcher from the intermediate plug (which is a bit more expensive
		// as it involves graph evaluations):

		ConstPathMatcherDataPtr pathMatcher = m_pathMatcher ? m_pathMatcher : pathMatcherPlug()->getValue();
		return pathMatcher->readable().match( pathData->readable() );
	}
	return IECore::PathMatcher::NoMatch;
}
