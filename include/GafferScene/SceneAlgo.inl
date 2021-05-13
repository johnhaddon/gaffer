//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, Image Engine Design Inc. All rights reserved.
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

#include "Gaffer/Context.h"

#include "tbb/parallel_for.h"

namespace GafferScene
{

namespace Detail
{

template<typename Functor>
struct Range
{

	using ChildNameIterator = std::vector<IECore::InternedString>::const_iterator;

	// Implementation of TBB Range concept

	Range( const Range & ) = default;

	//Range & operator = ( const Range &rhs ) = default;

	Range( Range &r, tbb::split )
		:	m_scene( r.m_scene ), m_threadState( r.m_threadState ),
			m_functor( r.m_functor )
	{
		if( r.m_end - r.m_begin == 1 )
		{
			// Single child. Recurse before splitting.
			ScenePlug::ScenePath child = r.m_parent;
			child.push_back( *r.m_begin );
			//std::cerr << "RECURSIVE SPLITTING" << std::endl;
			Range childRange( r.m_scene, r.m_threadState, r.m_functor, child );
			r.m_parent = childRange.m_parent;
			r.m_childNames = childRange.m_childNames;
			r.m_begin = childRange.m_begin;
			r.m_end = childRange.m_end; /// DON'T COPY THIS
			//std::cerr << "   " << r.m_parent << " " << ( r.m_end - r.m_begin ) << std::endl;
		}

		// Split children equally.
		//std::cerr << "SPLITTING " << (r.m_end - r.m_begin) << std::endl;
		ChildNameIterator mid = r.m_begin + (r.m_end - r.m_begin)/2;
		m_parent = r.m_parent;
		m_childNames = r.m_childNames;
		m_begin = mid;
		m_end = r.m_end;
		r.m_end = mid;
		//std::cerr << "   " << (r.m_end - r.m_begin) << " " << (m_end - m_begin) << std::endl;
	}

	bool empty() const
	{
		return m_begin == m_end;
	}

	bool is_divisible() const
	{
		// We don't really know if we're divisible until
		// we evaluate our children. Even one child could
		// lead to loads of recursive splitting later.
		return !empty();
	}

	// Our methods used to initialise and traverse the range.

	Range( const ScenePlug *scene, const Gaffer::ThreadState &threadState, Functor &f, const ScenePlug::ScenePath &parent )
		:	m_scene( scene ), m_threadState( threadState ),
			m_functor( f ), m_parent( parent )
	{
		//std::cerr << "CONSTRUCTING " << m_parent << std::endl;
		ScenePlug::PathScope scope( m_threadState, &m_parent );
		if( m_functor( m_scene, m_parent ) )
		{
		//	std::cerr << "  FUNCTOR 1" << std::endl;
			m_childNames = m_scene->childNamesPlug()->getValue();
		}
		else
		{
			//std::cerr << "  FUNCTOR 0" << std::endl;
			m_childNames = m_scene->childNamesPlug()->defaultValue();
		}

		m_begin = m_childNames->readable().begin();
		m_end = m_childNames->readable().end();
	}

	void execute() const
	{
		//std::cerr << "EXECUTING " << m_parent << " " << (m_end - m_begin) << std::endl;
		if( empty() )
		{
			//std::cerr << "   EARLY OUT" << std::endl;
			return;
		}

		ScenePlug::ScenePath childPath = m_parent;
		childPath.push_back( IECore::InternedString() );
		//std::cerr << "CHILD PATH " << childPath << std::endl;
		for( ChildNameIterator it = m_begin; it != m_end; ++it )
		{
			//std::cerr << *it << std::endl;
			childPath.back() = *it;
			Range( m_scene, m_threadState, m_functor, childPath ).execute();
		}
	}

	//private :

		const ScenePlug *m_scene;
		const Gaffer::ThreadState &m_threadState;
		Functor &m_functor;
		ScenePlug::ScenePath m_parent;
		IECore::ConstInternedStringVectorDataPtr m_childNames;
		ChildNameIterator m_begin;
		ChildNameIterator m_end;

};

template<typename ThreadableFunctor>
void parallelProcessLocationsWalk( const GafferScene::ScenePlug *scene, const Gaffer::ThreadState &threadState, const ScenePlug::ScenePath &path, ThreadableFunctor &f, tbb::task_group_context &taskGroupContext )
{
	ScenePlug::PathScope pathScope( threadState, &path );

	if( !f( scene, path ) )
	{
		return;
	}

	IECore::ConstInternedStringVectorDataPtr childNamesData = scene->childNamesPlug()->getValue();
	const std::vector<IECore::InternedString> &childNames = childNamesData->readable();
	if( childNames.empty() )
	{
		return;
	}

	using ChildNameRange = tbb::blocked_range<std::vector<IECore::InternedString>::const_iterator>;
	const ChildNameRange loopRange( childNames.begin(), childNames.end() );

	auto loopBody = [&] ( const ChildNameRange &range ) {
		ScenePlug::ScenePath childPath = path;
		childPath.push_back( IECore::InternedString() ); // Space for the child name
		for( auto &childName : range )
		{
			ThreadableFunctor childFunctor( f );
			childPath.back() = childName;
			parallelProcessLocationsWalk( scene, threadState, childPath, childFunctor, taskGroupContext );
		}
	};

	if( childNames.size() > 1 )
	{
		tbb::parallel_for( loopRange, loopBody, taskGroupContext );
	}
	else
	{
		// Serial execution
		loopBody( loopRange );
	}
}

template <class ThreadableFunctor>
struct ThreadableFilteredFunctor
{
	ThreadableFilteredFunctor( ThreadableFunctor &f, const GafferScene::FilterPlug *filter ): m_f( f ), m_filter( filter ){}

	bool operator()( const GafferScene::ScenePlug *scene, const GafferScene::ScenePlug::ScenePath &path )
	{
		IECore::PathMatcher::Result match = (IECore::PathMatcher::Result)m_filter->match( scene );

		if( match & IECore::PathMatcher::ExactMatch )
		{
			if( !m_f( scene, path ) )
			{
				return false;
			}
		}

		return ( match & IECore::PathMatcher::DescendantMatch ) != 0;
	}

	ThreadableFunctor &m_f;
	const FilterPlug *m_filter;

};

template<class ThreadableFunctor>
struct PathMatcherFunctor
{

	PathMatcherFunctor( ThreadableFunctor &f, const IECore::PathMatcher &filter )
		: m_f( f ), m_filter( filter )
	{
	}

	bool operator()( const GafferScene::ScenePlug *scene, const GafferScene::ScenePlug::ScenePath &path )
	{
		const unsigned match = m_filter.match( path );
		if( match & IECore::PathMatcher::ExactMatch )
		{
			if( !m_f( scene, path ) )
			{
				return false;
			}
		}

		return match & IECore::PathMatcher::DescendantMatch;
	}

	private :

		ThreadableFunctor &m_f;
		const IECore::PathMatcher &m_filter;

};

} // namespace Detail

namespace SceneAlgo
{

template<typename Range>
void walkRange( Range &r )
{
	//std::cerr << "WALK " << ScenePlug::pathToString( r.m_parent ) << std::endl;
	if( r.is_divisible() )
	{
		Range s( r, tbb::split() );
		walkRange( r );
		walkRange( s );
	}
}

template <class ThreadableFunctor>
void parallelProcessLocations( const GafferScene::ScenePlug *scene, ThreadableFunctor &f, const ScenePlug::ScenePath &root )
{
	tbb::task_group_context taskGroupContext( tbb::task_group_context::isolated ); // Prevents outer tasks silently cancelling our tasks

	Detail::Range<ThreadableFunctor> range( scene, Gaffer::ThreadState::current(), f, root );
	if( range.empty() )
	{
		return;
	}

	//walkRange( range );

	tbb::parallel_for(
		range,
		[] ( const Detail::Range<ThreadableFunctor> &range ) {
			range.execute();
		},
		taskGroupContext
	);
}

template <class ThreadableFunctor>
void parallelTraverse( const ScenePlug *scene, ThreadableFunctor &f, const ScenePlug::ScenePath &root )
{
	// `parallelProcessLocations()` takes a copy of the functor at each location, whereas
	// `parallelTraverse()` is intended to use the same functor for all locations. Wrap the
	// functor in a cheap-to-copy lambda, so that the functor itself won't be copied.
	auto reference = [&f] ( const ScenePlug *scene, const ScenePlug::ScenePath &path ) {
		return f( scene, path );
	};
	parallelProcessLocations( scene, reference, root );
}

template <class ThreadableFunctor>
void filteredParallelTraverse( const ScenePlug *scene, const GafferScene::FilterPlug *filterPlug, ThreadableFunctor &f, const ScenePlug::ScenePath &root )
{
	Detail::ThreadableFilteredFunctor<ThreadableFunctor> ff( f, filterPlug );
	parallelTraverse( scene, ff, root );
}

template <class ThreadableFunctor>
void filteredParallelTraverse( const ScenePlug *scene, const IECore::PathMatcher &filter, ThreadableFunctor &f, const ScenePlug::ScenePath &root )
{
	Detail::PathMatcherFunctor<ThreadableFunctor> ff( f, filter );
	parallelTraverse( scene, ff, root );
}

} // namespace SceneAlgo

} // namespace GafferScene
