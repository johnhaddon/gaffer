//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2017, Image Engine Design Inc. All rights reserved.
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

#include "GafferUI/NoduleLayout.h"
#include "GafferUI/PlugAdder.h"
#include "GafferUI/StandardNodeGadget.h"

#include "Gaffer/Box.h"
#include "Gaffer/BoxIn.h"
#include "Gaffer/BoxOut.h"
#include "Gaffer/MetadataAlgo.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/UndoScope.h"

#include "boost/bind/bind.hpp"

using namespace IECore;
using namespace Gaffer;
using namespace GafferUI;

namespace
{

class BoxNodeGadget : public StandardNodeGadget
{

	public :

		BoxNodeGadget( const Gaffer::NodePtr &node )
			:	StandardNodeGadget( node )
		{
			auto box = runTimeCast<Box>( node.get() );
			if( !box )
			{
				throw Exception( "BoxNodeGadget requires a Box" );
			}

			// Our base class responds to the `icon` metadata, but doesn't know
			// that it changes based on `Box::isReference()`. Arrange to emit
			// `Metadata::nodeValueChangedSignal()` whenever that changes. We make
			// sure we only emit the signal once, even if there are multiple gadgets
			// in existence.
			RefCountedConnection &connection = g_connections[box];
			if( !connection.refCount )
			{
				connection.scope = box->referenceChangedSignal().connect( &referenceChanged );
			}
			connection.refCount++;
		}

		~BoxNodeGadget() override
		{
			auto it = g_connections.find( node() );
			assert( it != g_connections.end() && it->second.refCount );
			it->second.refCount--;
			if( !it->second.refCount )
			{
				g_connections.erase( it );
			}
		}

	private :

		struct RefCountedConnection
		{
			Signals::ScopedConnection scope;
			size_t refCount = 0;
		};

		static std::unordered_map<const Node *, RefCountedConnection> g_connections;

		static void referenceChanged( SubGraph *box )
		{
			Metadata::nodeValueChangedSignal( box )( box, "icon", Metadata::ValueChangedReason::InstanceRegistration );
		}

		static const NodeGadgetTypeDescription<BoxNodeGadget> g_description;

};

std::unordered_map<const Node *, BoxNodeGadget::RefCountedConnection> BoxNodeGadget::g_connections;

const NodeGadget::NodeGadgetTypeDescription<BoxNodeGadget> BoxNodeGadget::g_description( Box::staticTypeId() );

class BoxPlugAdder : public PlugAdder
{

	public :

		BoxPlugAdder( BoxPtr box )
			:	m_box( box )
		{
		}

	protected :

		bool canCreateConnection( const Plug *endpoint ) const override
		{
			if( !PlugAdder::canCreateConnection( endpoint ) )
			{
				return false;
			}

			if( endpoint->node() == m_box )
			{
				return false;
			}

			if( MetadataAlgo::readOnly( m_box.get() ) )
			{
				return false;
			}

			return true;
		}

		void createConnection( Plug *endpoint ) override
		{
			BoxIOPtr boxIO;
			if( endpoint->direction() == Plug::In )
			{
				boxIO = new BoxOut;
			}
			else
			{
				boxIO = new BoxIn;
			}

			m_box->addChild( boxIO );
			boxIO->setup( endpoint );

			if( endpoint->direction() == Plug::In )
			{
				endpoint->setInput( boxIO->promotedPlug() );
			}
			else
			{
				boxIO->promotedPlug()->setInput( endpoint );
			}

			applyEdgeMetadata( boxIO->promotedPlug() );
			applyEdgeMetadata( boxIO->plug(), /* opposite = */ true );
		}

	private :

		BoxPtr m_box;

};

struct Registration
{

	Registration()
	{
		NoduleLayout::registerCustomGadget( "GafferUI.BoxUI.PlugAdder", &create );
	}

	private :

		static GadgetPtr create( GraphComponentPtr parent )
		{
			if( BoxPtr box = runTimeCast<Box>( parent ) )
			{
				return new BoxPlugAdder( box );
			}
			throw IECore::Exception( "Expected a Box" );
		}

};

Registration g_registration;

} // namespace
