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

#ifndef GAFFER_GRAPHCOMPONENTALGO_INL
#define GAFFER_GRAPHCOMPONENTALGO_INL

namespace Gaffer
{

namespace GraphComponentAlgo
{

template<typename T>
T *commonAncestor( GraphComponent *graphComponent, const GraphComponent *other )
{
	return static_cast<T *>( commonAncestor( graphComponent, other, T::staticTypeId() ) );
}

template<typename T>
const T *commonAncestor( const GraphComponent *graphComponent, const GraphComponent *other )
{
	return static_cast<const T *>( commonAncestor( graphComponent, other, T::staticTypeId() ) );
}

template<typename T>
const T *correspondingDescendant( const GraphComponent *ancestor, const GraphComponent *descendant, const GraphComponent *correspondingAncestor )
{
	if( descendant == ancestor )
	{
		// We're already at plugAncestor, so the relative path has zero length
		// and we can return oppositeAncestor.
		return correspondingAncestor;
	}

	// now we find the corresponding descendant of plug->parent(), and
	// return its child with the same name as "plug" (if either of those things exist):

	const GraphComponent *descendantParent = descendant->parent();
	if( !descendantParent )
	{
		// `descendant` wasn't a descendant of `ancestor`.
		return nullptr;
	}

	// find the corresponding plug for the parent:
	const GraphComponent *correspondingParent = correspondingDescendant( ancestor, descendantParent, correspondingAncestor );
	if( !correspondingParent )
	{
		return nullptr;
	}

	// find the child corresponding to "plug"
	return correspondingParent->getChild<T>( descendant->getName() ); // CAST TO T HERE IS WRONG!!!!!!!
}

} // namespace GraphComponentAlgo

} // namespace Gaffer

#endif // GAFFER_GRAPHCOMPONENTALGO_INL
