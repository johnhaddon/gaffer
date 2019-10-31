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

#ifndef GAFFER_GRAPHCOMPONENTALGO_H
#define GAFFER_GRAPHCOMPONENTALGO_H

#include "Gaffer/Export.h"
#include "Gaffer/GraphComponent.h"

namespace Gaffer
{

namespace GraphComponentAlgo
{

/// Returns the first ancestor of type T which
/// is also an ancestor of other.
template<typename T=GraphComponent>
GAFFER_API T *commonAncestor( GraphComponent *graphComponent, const GraphComponent *other );
template<typename T=GraphComponent>
GAFFER_API const T *commonAncestor( const GraphComponent *graphComponent, const GraphComponent *other );

/// As above, but taking a TypeId to specify type - this is mainly provided for use in the Python binding.
GAFFER_API GraphComponent *commonAncestor( GraphComponent *graphComponent, const GraphComponent *other, IECore::TypeId ancestorType );
GAFFER_API const GraphComponent *commonAncestor( const GraphComponent *graphComponent, const GraphComponent *other, IECore::TypeId ancestorType );

/// Returns the equivalent of `correspondingAncestor->descendant<T>( descendant->relativeName( ancestor ) )`.
/// Returns null if `descendant` is not a descendant of `ancestor`.
template<typename T=GraphComponent>
const T *correspondingDescendant( const GraphComponent *ancestor, const GraphComponent *descendant, const GraphComponent *correspondingAncestor );

} // namespace GraphComponentAlgo

} // namespace Gaffer

#include "GraphComponentAlgo.inl"

#endif // GAFFER_GRAPHCOMPONENTALGO_H
