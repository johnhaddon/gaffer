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

#include "boost/python.hpp"

#include "SpreadsheetBinding.h"

#include "Gaffer/Reference.h"

#include "GafferBindings/DependencyNodeBinding.h"
#include "GafferBindings/ValuePlugBinding.h"

#include "Gaffer/Spreadsheet.h"

using namespace boost::python;
using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;

namespace
{

Spreadsheet::RowPlugPtr defaultRow( Spreadsheet::RowsPlug &rowsPlug )
{
	return rowsPlug.defaultRow();
}

Spreadsheet::RowPlugPtr row( Spreadsheet::RowsPlug &rowsPlug, const std::string &name )
{
	return rowsPlug.row( name );
}

size_t addColumn( Spreadsheet::RowsPlug &rowsPlug, ValuePlug &value, IECore::InternedString name, bool adoptEnabledPlug )
{
	ScopedGILRelease gilRelease;
	return rowsPlug.addColumn( &value, name, adoptEnabledPlug );
}

void removeColumn( Spreadsheet::RowsPlug &rowsPlug, size_t columnIndex )
{
	ScopedGILRelease gilRelease;
	return rowsPlug.removeColumn( columnIndex );
}

Spreadsheet::RowPlugPtr addRow( Spreadsheet::RowsPlug &rowsPlug )
{
	ScopedGILRelease gilRelease;
	return rowsPlug.addRow();
}

void addRows( Spreadsheet::RowsPlug &rowsPlug, size_t numRows )
{
	ScopedGILRelease gilRelease;
	rowsPlug.addRows( numRows );
}

void removeRow( Spreadsheet::RowsPlug &rowsPlug, Spreadsheet::RowPlug &row )
{
	ScopedGILRelease gilRelease;
	rowsPlug.removeRow( &row );
}

BoolPlugPtr cellPlugEnabledPlug( Spreadsheet::CellPlug &cellPlug )
{
	return cellPlug.enabledPlug();
}

ValuePlugPtr activeInPlug( Spreadsheet &s, const ValuePlug &outPlug )
{
	ScopedGILRelease gilRelease;
	return s.activeInPlug( &outPlug );
}

const IECore::InternedString g_omitParentNodePlugValues( "valuePlugSerialiser:omitParentNodePlugValues" );

class RowsPlugSerialiser : public ValuePlugSerialiser
{

	public :

		std::string postConstructor( const Gaffer::GraphComponent *graphComponent, const std::string &identifier, const Serialisation &serialisation ) const override
		{
			std::string result = ValuePlugSerialiser::postConstructor( graphComponent, identifier, serialisation );
			const auto *plug = static_cast<const Spreadsheet::RowsPlug *>( graphComponent );
			if( IECore::runTimeCast<const Reference>( plug->node() ) )
			{
				// References add all their plugs in `loadReference()`, so we don't need to serialise
				// the rows and columns ourselves.
				/// \todo For other plug types, the Reference prevents constructor serialisation
				/// by removing the `Dynamic` flag from the plugs. We are aiming to remove this
				/// flag though, so haven't exposed it via the `addColumn()/addRow()` API. In future
				/// we need to improve the serialisation API so that Reference nodes can directly
				/// request what they want without using flags.
				return result;
			}

			// Serialise columns

			for( const auto &cell : Spreadsheet::CellPlug::Range( *plug->getChild<Spreadsheet::RowPlug>( 0 )->cellsPlug() ) )
			{
				PlugPtr p = cell->valuePlug()->createCounterpart( cell->getName(), Plug::In );
				const Serialiser *plugSerialiser = Serialisation::acquireSerialiser( p.get() );
				result += identifier + ".addColumn( " + plugSerialiser->constructor( p.get(), serialisation );
				if( !cell->getChild<BoolPlug>( "enabled" ) )
				{
					result += ", adoptEnabledPlug = True";
				}
				result += " )\n";
			}

			// Serialise rows. We do this as an `addRows()` call because it is much faster
			// than serialising a constructor for every single cell. It also shows people the
			// API they should use for making their own spreadsheets.

			const size_t numRows = plug->children().size();
			if( numRows > 1 )
			{
				result += identifier + ".addRows( " + std::to_string( numRows - 1 ) + " )\n";
			}

			// If the default values for any cells have been modified, then we need to serialise
			// those separately as `setDefaultValue()` calls. We want to do this as at high a level
			// as possible in the hierarchy, so that we don't serialise separate defaults for every
			// child of a V3fPlug for instance.

			std::string defaultValueSerialisation;
			for( size_t rowIndex = 1; rowIndex < numRows; ++rowIndex )
			{
				const auto *row = plug->getChild<Spreadsheet::RowPlug>( rowIndex );
				defaultValueSerialisationsWalk( row, plug->defaultRow(), serialisation, defaultValueSerialisation );
			}

			if( defaultValueSerialisation.size() )
			{
				result += defaultValueSerialisation + identifier + ".resetDefault()\n";
			}

			return result;
		}

		bool childNeedsConstruction( const Gaffer::GraphComponent *child, const Serialisation &serialisation ) const override
		{
			// We can serialise much more compactly via the `addRows()` call made by `postConstructor()`.
			return false;
		}

	private :

		bool defaultValueSerialisationsWalk( const ValuePlug *plug, const ValuePlug *defaultPlug, const Serialisation &serialisation, std::string &result ) const
		{
			const size_t numChildren = plug->children().size();
			assert( defaultPlug->children().size() == numChildren );
			if( !numChildren )
			{
				// Leaf plug.
				return plug->defaultHash() != defaultPlug->defaultHash();
			}

			// Compound plug. See if the children need their default values to be serialised.

			std::vector<const ValuePlug *> childrenToSerialise;
			for( size_t childIndex = 0; childIndex < numChildren; ++childIndex )
			{
				const auto *childPlug = plug->getChild<ValuePlug>( childIndex );
				const bool childRequiresSerialisation = defaultValueSerialisationsWalk(
					childPlug, defaultPlug->getChild<ValuePlug>( childIndex ), serialisation, result
				);
				if( childRequiresSerialisation )
				{
					childrenToSerialise.push_back( childPlug );
				}
			}

			if( !childrenToSerialise.size() )
			{
				return false;
			}

			if( childrenToSerialise.size() == numChildren )
			{
				// All children want serialisation. As long as we have the appropriate
				// methods, we can delegate all the work to our parent.
				object pythonPlug( PlugPtr( const_cast<ValuePlug *>( plug ) ) );
				if( PyObject_HasAttrString( pythonPlug.ptr(), "setValue" ) )
				{
					return true;
				}
			}

			// Only a subset of children want to change their default value, or
			// it's not possible to change the default at this level. Add serialisations
			// for each child.

			for( auto childPlug : childrenToSerialise )
			{
				object pythonChildPlug( PlugPtr( const_cast<ValuePlug *>( childPlug ) ) );
				object pythonDefaultValue = pythonChildPlug.attr( "defaultValue" )();
				/// \todo Build identifier recursively (but lazily) and making sure to use the faster
				/// version of `childIdentifier()`.
				const std::string childPlugIdentifier = serialisation.identifier( childPlug );
				result += childPlugIdentifier + ".setValue( " + valueRepr( pythonDefaultValue ) + " )\n";
			}

			return false;
		}

};

} // namespace

void GafferModule::bindSpreadsheet()
{

	scope s = DependencyNodeClass<Spreadsheet>()
		.def( "activeInPlug", &activeInPlug )
	;

	PlugClass<Spreadsheet::RowsPlug>()
		.def( init<std::string, Plug::Direction, unsigned>(
				(
					arg( "name" )=GraphComponent::defaultName<Spreadsheet::RowsPlug>(),
					arg( "direction" )=Plug::In,
					arg( "flags" )=Plug::Default
				)
			)
		)
		.def( "defaultRow", &defaultRow )
		.def( "row", &row )
		.def( "addColumn", &addColumn, ( arg( "value" ), arg( "name" ) = "", arg( "adoptEnabledPlug" ) = false ) )
		.def( "removeColumn", &removeColumn )
		.def( "addRow", &addRow )
		.def( "addRows", &addRows )
		.def( "removeRow", &removeRow )
		.attr( "__qualname__" ) = "Spreadsheet.RowsPlug"
	;

	PlugClass<Spreadsheet::RowPlug>()
		.attr( "__qualname__" ) = "Spreadsheet.RowPlug"
	;

	PlugClass<Spreadsheet::CellPlug>()
		.def( "enabledPlug", &cellPlugEnabledPlug )
		.attr( "__qualname__" ) = "Spreadsheet.CellPlug"
	;

	Serialisation::registerSerialiser( Gaffer::Spreadsheet::RowsPlug::staticTypeId(), new RowsPlugSerialiser );

}
