//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2024, Cinesite VFX Ltd. All rights reserved.
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

#pragma once

#include "GafferUI/Export.h"

#include "Gaffer/Set.h"
#include "Gaffer/Signals.h"

#include "IECore/RefCounted.h"

namespace Gaffer
{

class Plug;
IE_CORE_FORWARDDECLARE( Node );
IE_CORE_FORWARDDECLARE( Context )

} // namespace Gaffer

namespace GafferUI
{

class GAFFERUI_API UpstreamContexts final : public IECore::RefCounted, Gaffer::Signals::Trackable
{

	public :

		//UpstreamContexts( const Gaffer::Plug *plug ); // TODO : DO WE NEED THIS ONE?
		UpstreamContexts( const Gaffer::ConstNodePtr &node, const Gaffer::ConstContextPtr &context );
		//UpstreamContexts( const Gaffer::Set *set );

		IE_CORE_DECLAREMEMBERPTR( UpstreamContexts );

		~UpstreamContexts() override;

		// TODO : ARE WE HAPPY WITH JUST ONE CONTEXT? WHAT WOULD WE DO WITH MORE???
		Gaffer::ConstContextPtr context( const Gaffer::Plug *plug ) const; // TODO : DO THESE WAIT???
		Gaffer::ConstContextPtr context( const Gaffer::Node *node ) const;

		using ChangedSignal = Gaffer::Signals::Signal<void()>;
		ChangedSignal &changedSignal();

		//ConstPtr acquire( const Gaffer::Plug *plug ); // TODO : DO WE NEED THIS ONE???
		static ConstPtr acquire( const Gaffer::Node *node );

		// COULD WE MAKE THIS ONE SHARE WORK WITH ONES ACQUIRED FROM A NODE???
		//static ConstPtr acquire( const Gaffer::Set *set );  // ?? MAYBE `acquireForFocus()`?

	//private : TODO

		void update();

		Gaffer::ConstNodePtr m_node;
		Gaffer::ConstContextPtr m_context;

		/// TODO : IS THERE ANY WAY WE COULD USE RAW POINTER?
		//std::unordered_map<IECore::MurmurHash,
		// TODO : DOCUMENT SPARSENESS
		std::unordered_map<Gaffer::ConstPlugPtr, Gaffer::ConstContextPtr> m_plugContexts;
		std::unordered_map<Gaffer::ConstNodePtr, Gaffer::ConstContextPtr> m_nodeContexts;

};

IE_CORE_DECLAREPTR( UpstreamContexts )

} // namespace GafferUI
