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

#include "GafferUI/UpstreamContexts.h"

#include "Gaffer/Context.h"
#include "Gaffer/ContextVariables.h"
#include "Gaffer/Loop.h"
#include "Gaffer/Switch.h"

#include "boost/bind/bind.hpp"
#include "boost/bind/placeholders.hpp"

#include <unordered_set>

using namespace Gaffer;
using namespace GafferUI;
using namespace IECore;
using namespace boost::placeholders;
using namespace std;

UpstreamContexts::UpstreamContexts( const Gaffer::NodePtr &node, const Gaffer::ContextPtr &context )
	:	m_node( node ), m_context( context )
{
	node->plugDirtiedSignal().connect( boost::bind( &UpstreamContexts::plugDirtied, this, ::_1 ) );
	context->changedSignal().connect( boost::bind( &UpstreamContexts::contextChanged, this, ::_2 ) );
	update();
}

UpstreamContexts::~UpstreamContexts()
{
	disconnectTrackedConnections();
}

bool UpstreamContexts::isActive( const Gaffer::Plug *plug ) const
{
	optional<const Context *> c = findPlugContext( plug );
	if( c )
	{
		return *c;
	}

	auto it = m_nodeContexts.find( plug->node() );
	return it != m_nodeContexts.end() && ( plug->direction() == Plug::Out || it->second.enabled );
}

bool UpstreamContexts::isActive( const Gaffer::Node *node ) const
{
	return m_nodeContexts.find( node ) != m_nodeContexts.end();
}

Gaffer::ConstContextPtr UpstreamContexts::context( const Gaffer::Plug *plug ) const
{
	optional<const Context *> c = findPlugContext( plug );
	if( c && *c )
	{
		return *c;
	}

	return context( plug->node() );
}

Gaffer::ConstContextPtr UpstreamContexts::context( const Gaffer::Node *node ) const
{
	auto it = m_nodeContexts.find( node );
	return it != m_nodeContexts.end() ? it->second.context : m_context;
}

void UpstreamContexts::plugDirtied( const Gaffer::Plug *plug )
{
	update();
}

void UpstreamContexts::contextChanged( IECore::InternedString variable )
{
	update();
}

void UpstreamContexts::update()
{
	m_nodeContexts.clear();
	m_plugContexts.clear();

	std::deque<std::pair<const Plug *, ConstContextPtr>> toVisit;

	for( Plug::RecursiveOutputIterator it( m_node.get() ); !it.done(); ++it )
	{
		toVisit.push_back( { it->get(), m_context } );
		it.prune();
	}

	std::unordered_set<MurmurHash> visited;

	while( !toVisit.empty() )
	{
		auto [plug, context] = toVisit.front();
		toVisit.pop_front();

		MurmurHash h = context->hash();
		h.append( (uintptr_t)plug );
		if( !visited.insert( h ).second )
		{
			continue;
		}

		// TODO : CANCELLATION CHECK

		Context::Scope scopedContext( context.get() );

		const Node *node = plug->node();
		bool nodeEnabled = true; // TODO : USE DOWN BELOW

		if( node )
		{
			if( auto dependencyNode = runTimeCast<const DependencyNode>( node ) )
			{
				if( auto enabledPlug = dependencyNode->enabledPlug() )
				{
					nodeEnabled = enabledPlug->getValue();
				}
			}

			// TODO : OVERRIDE IF PREVIOUSLY DISABLED
			m_nodeContexts.insert( { plug->node(), { context, nodeEnabled } } ); // TODO : IGNORE INPUT PLUGS?
		}
		else
		{
			m_plugContexts.insert( { plug, context } );
		}

		if( auto input = plug->getInput() )
		{
			toVisit.push_back( { input, context } );
		}
		else
		{
			for( Plug::RecursiveInputIterator it( plug ); !it.done(); ++it )
			{
				if( auto input = (*it)->getInput() )
				{
					toVisit.push_back( { input, context } );
					it.prune();
				}
			}
		}

		if( plug->direction() != Plug::Out || !plug->node() )
		{
			continue;
		}

		// TODO : DISABLED DEPENDENCYNODE

		// TODO : DO YOU NEED TO USE GLOBAL SCOPES ANYWHERE HERE?
		if( auto dependencyNode = runTimeCast<const DependencyNode>( node ) )
		{
			if( auto enabledPlug = dependencyNode->enabledPlug() )
			{
				if( !enabledPlug->getValue() )
				{
					if( auto inPlug = dependencyNode->correspondingInput( plug ) )
					{
						m_plugContexts.insert( { inPlug, context } );
						m_plugContexts.insert( { enabledPlug, context } );
						toVisit.push_back( { inPlug, context } );
						toVisit.push_back( { enabledPlug, context } );
					}
					continue;
				}
			}
		}

		if( auto switchNode = runTimeCast<const Switch>( node ) )
		{
			if( const Plug *activeIn = switchNode->activeInPlug( plug ) )
			{
				for( auto &inPlug : Plug::Range( *switchNode->inPlugs() ) )
				{
					// Initialises to `nullptr` if not present already. This is
					// what we want for inputs which are not active.
					auto &inPlugContext = m_plugContexts[inPlug];
					// If not already visited in another context, then assign
					// context for the active plug.
					if( !inPlugContext && ( inPlug == activeIn || inPlug->isAncestorOf( activeIn ) ) )
					{
						inPlugContext = context;
					}
				}
				toVisit.push_back( { activeIn, context } );
			}
			else
			{
				// DON'T CARE : DOCUMENT WHY
			}
		}
		else if( auto contextProcessor = runTimeCast<const ContextProcessor>( node ) )
		{
			if( plug == contextProcessor->outPlug() )
			{
				ConstContextPtr inContext = contextProcessor->inPlugContext();
				toVisit.push_back( { contextProcessor->inPlug(), inContext } );
			}
			else
			{
				// DON'T CARE
			}
		}
		else if( auto loop = runTimeCast<const Loop>( node ) )
		{
			if( plug == loop->outPlug() )
			{
				toVisit.push_back( { loop->inPlug(), context } );
				if( auto nextContext = loop->nextIterationContext() )
				{
					m_plugContexts.insert( { loop->nextPlug(), nextContext } ); // TODO : DO THIS AUTOMAGICALLY (SAME FOR OTHERS)
					toVisit.push_back( { loop->nextPlug(), nextContext } );
				}
				else
				{
					m_plugContexts.insert( { loop->nextPlug(), nullptr } );
				}
			}
		}
		else
		{
			// TODO : MIGHT NEED TO GET HERE FOR OTHER INPUTS OF THE SPECIAL-CASE NODES ABOVE?
			// CAN WE USE `m_plugContexts` to BLOCK PROCESSING OF ALREADY-PROCESSED THINGS?
			for( const auto &inputPlug : Plug::InputRange( *node ) )
			{
				if( inputPlug != plug )
				{
					toVisit.push_back( { inputPlug.get(), context } );
				}
			}
		}
	}
}

std::optional<const Gaffer::Context *> UpstreamContexts::findPlugContext( const Gaffer::Plug *plug ) const
{
	while( plug )
	{
		auto it = m_plugContexts.find( plug );
		if( it != m_plugContexts.end() )
		{
			return it->second.get();
		}

		plug = plug->parent<Plug>();
	}

	return std::nullopt;
}
