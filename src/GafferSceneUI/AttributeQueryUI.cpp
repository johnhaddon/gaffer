//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2021, Cinesite VFX Ltd. All rights reserved.
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

#include "GafferUI/PlugAdder.h"
#include "GafferUI/NoduleLayout.h"

#include "GafferScene/AttributeQuery.h"

#include "IECore/Exception.h"

#include <boost/bind.hpp>

#include <cassert>

namespace
{

struct AttributeQueryPlugAdder : GafferUI::PlugAdder
{
	AttributeQueryPlugAdder( GafferScene::AttributeQuery& query )
	: GafferUI::PlugAdder()
	, m_query( & query )
	{
		m_query->childAddedSignal().connect( boost::bind( & AttributeQueryPlugAdder::updateVisibility, this ) );
		m_query->childRemovedSignal().connect( boost::bind( & AttributeQueryPlugAdder::updateVisibility, this ) );

		updateVisibility();
	}

	~AttributeQueryPlugAdder() override
	{}

protected:

	bool canCreateConnection( const Gaffer::Plug* plug ) const override;
	void createConnection( Gaffer::Plug* plug ) override;

private:

	void updateVisibility()
	{
		setVisible( m_query->valuePlug() == 0 );
	}

	struct Register
	{
		Register()
		{
			GafferUI::NoduleLayout::registerCustomGadget( "GafferUI.AttributeQueryUI.PlugAdder", & Register::create );
		}

		static GafferUI::GadgetPtr create( Gaffer::GraphComponentPtr parent )
		{
			const GafferScene::AttributeQueryPtr query = IECore::runTimeCast< GafferScene::AttributeQuery >( parent );

			if( ! query )
			{
				throw IECore::Exception( "AttributeQueryPlugAdder requires an AttributeQuery" );
			}

			const GafferUI::GadgetPtr gadget( new AttributeQueryPlugAdder( *query ) );
			return gadget;
		}
	};

	static Register m_register;

	GafferScene::AttributeQueryPtr m_query;
};

bool AttributeQueryPlugAdder::canCreateConnection( const Gaffer::Plug* const plug ) const
{
	assert( m_query );

	return (
		( GafferUI::PlugAdder::canCreateConnection( plug ) ) &&
		( plug->direction() == Gaffer::Plug::In ) &&
		( plug->node() != m_query ) &&
		( m_query->canSetup( IECore::runTimeCast< const Gaffer::ValuePlug >( plug ) ) ) );
}

void AttributeQueryPlugAdder::createConnection( Gaffer::Plug* const plug )
{
	assert( plug->direction() == Gaffer::Plug::In );

	m_query->setup( IECore::assertedStaticCast< const Gaffer::ValuePlug >( plug ) );

	plug->setInput( m_query->valuePlug() );
}

AttributeQueryPlugAdder::Register AttributeQueryPlugAdder::m_register;

} // namespace
