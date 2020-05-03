//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2019, Image Engine Design Inc. All rights reserved.
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

#include "GafferScene/Deformer.h"

#include "GafferScene/SceneAlgo.h"

using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

GAFFER_GRAPHCOMPONENT_DEFINE_TYPE( Deformer );

size_t Deformer::g_firstPlugIndex = 0;

Deformer::Deformer( IECore::InternedString name )
	:	ObjectProcessor( name )
{
	init();
}

Deformer::Deformer( IECore::InternedString name, size_t minInputs, size_t maxInputs )
	:	ObjectProcessor( name, minInputs, maxInputs )
{
	init();
}

Deformer::Deformer( IECore::InternedString name, IECore::PathMatcher::Result filterDefault )
	:	ObjectProcessor( name, filterDefault )
{
	init();
}

void Deformer::init()
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new BoolPlug( "adjustBounds", Plug::In, true ) );
	// Remove pass-through created by base class
	outPlug()->boundPlug()->setInput( nullptr );
}

Deformer::~Deformer()
{
}

Gaffer::BoolPlug *Deformer::adjustBoundsPlug()
{
	return getChild<BoolPlug>( g_firstPlugIndex );
}

const Gaffer::BoolPlug *Deformer::adjustBoundsPlug() const
{
	return getChild<BoolPlug>( g_firstPlugIndex );
}

void Deformer::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	ObjectProcessor::affects( input, outputs );

	if( input == outPlug()->objectPlug() || input == inPlug()->boundPlug() || input == adjustBoundsPlug() )
	{
		outputs.push_back( outPlug()->boundPlug() );
	}
}

bool Deformer::adjustBounds() const
{
	return adjustBoundsPlug()->getValue();
}

void Deformer::hashBound( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	bool adjustBounds;
	{
		ScenePlug::GlobalScope globalScope( context );
		adjustBounds = this->adjustBounds();
	}

	if( adjustBounds )
	{
		const PathMatcher::Result m = filterValue( context );
		if( m & ( PathMatcher::ExactMatch | PathMatcher::DescendantMatch ) )
		{
			ObjectProcessor::hashBound( path, context, parent, h );
			if( m & PathMatcher::ExactMatch )
			{
				outPlug()->objectPlug()->hash( h );
			}
			else
			{
				inPlug()->objectPlug()->hash( h );
			}

			if( m & PathMatcher::DescendantMatch )
			{
				h.append( hashOfTransformedChildBounds( path, outPlug() ) );
			}
			else
			{
				h.append( hashOfTransformedChildBounds( path, inPlug() ) );
			}
			return;
		}
		// Fall through to pass-through
	}

	h = inPlug()->boundPlug()->hash();
}

Imath::Box3f Deformer::computeBound( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	bool adjustBounds;
	{
		// We can't allow the result of `adjustBounds()` to vary per location,
		// because that would prevent us from successfully propagating bounds
		// changes up to ancestor locations. To enforce this, we evaluate
		// `adjustBounds()` in a global scope.
		ScenePlug::GlobalScope globalScope( context );
		adjustBounds = this->adjustBounds();
	}

	if( adjustBounds )
	{
		const PathMatcher::Result m = filterValue( context );
		if( m & ( PathMatcher::ExactMatch | PathMatcher::DescendantMatch ) )
		{
			// Need to compute new bounds. This consists of the bounds of
			// the (potentially deformed) object at this location and the
			// (potentially deformed) bounds of our children.
			Box3f result;
			if( m & PathMatcher::ExactMatch )
			{
				// Get bounds from deformed output object.
				/// \todo Some derived classes may be able to compute an output bound
				/// for the object without computing the full deformation. We could add
				/// virtual methods to allow that.
				result.extendBy( SceneAlgo::bound( outPlug()->objectPlug()->getValue().get() ) );
			}
			else
			{
				result.extendBy( SceneAlgo::bound( inPlug()->objectPlug()->getValue().get() ) );
			}

			if( m & PathMatcher::DescendantMatch )
			{
				result.extendBy( unionOfTransformedChildBounds( path, outPlug() ) );
			}
			else
			{
				result.extendBy( unionOfTransformedChildBounds( path, inPlug() ) );
			}
			return result;
		}
		// Fall through to pass-through
	}

	return inPlug()->boundPlug()->getValue();
}
