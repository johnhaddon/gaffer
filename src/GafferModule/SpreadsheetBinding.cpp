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

class CellPlugSerialiser : public ValuePlugSerialiser
{

	public :

		std::string constructor( const Gaffer::GraphComponent *graphComponent, const Serialisation &serialisation ) const override
		{
			auto cell = static_cast<const Spreadsheet::CellPlug *>( graphComponent );
			return repr( cell, &serialisation );
		}

		static std::string repr( const Spreadsheet::CellPlug *cell, const Serialisation *serialisation = nullptr )
		{
			std::string args = "value = " + ValuePlugSerialiser::repr( cell->valuePlug(), "", serialisation );
			if( cell->enabledPlug()->parent() == cell->valuePlug() )
			{
				args += ", adoptEnabledPlug = True";
			}
			return ValuePlugSerialiser::repr( cell, args, serialisation );
		}

};

std::string cellPlugRepr( const Spreadsheet::CellPlug &cell )
{
	return CellPlugSerialiser::repr( &cell );
}

class RowPlugSerialiser : public ValuePlugSerialiser
{

	public :

		std::string constructor( const Gaffer::GraphComponent *graphComponent, const Serialisation &serialisation ) const override
		{
			return "";
		}

		static std::string repr( const Spreadsheet::RowPlug *row, const Serialisation *serialisation = nullptr )
		{
			return "";
		}

};

std::string rowPlugRepr( const Spreadsheet::RowPlug &row )
{
	return RowPlugSerialiser::repr( &row );
}

class RowsPlugSerialiser : public ValuePlugSerialiser
{

	public :

		std::string postConstructor( const Gaffer::GraphComponent *graphComponent, const std::string &identifier, const Serialisation &serialisation ) const override
		{
			std::string result = ValuePlugSerialiser::postConstructor( graphComponent, identifier, serialisation );

			const auto *plug = static_cast<const Spreadsheet::RowsPlug *>( graphComponent );
			if( shouldResetPlugDefault( plug, &serialisation ) )
			{
				// Each cell will potentially be receiving different default values.
				// Fall back to the exhaustive serialisation of constructors for all
				// rows/cells.
				for( const auto &row : Spreadsheet::RowPlug::Range( *plug ) )
				{
					if( row != plug->defaultRow() )
					{
						result += identifier + ".addChild( " + RowPlugSerialiser::repr( row.get(), &serialisation ) + " )\n";
					}
					const std::string cellsIdentifier = serialisation.identifier( row->cellsPlug() );
					for( const auto &cell : Spreadsheet::CellPlug::Range( *row->cellsPlug() ) )
					{
						result += cellsIdentifier + ".addChild( " + CellPlugSerialiser::repr( cell.get(), &serialisation ) + " )\n";
					}
				}
				return result;
			}

			// We can serialise much more compactly via calls to `addRows()` and `addColumn()`.

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

			const size_t numRows = plug->children().size();
			if( numRows > 1 )
			{
				result += identifier + ".addRows( " + std::to_string( numRows - 1 ) + " )\n";
			}
			return result;
		}

		bool childNeedsConstruction( const Gaffer::GraphComponent *child, const Serialisation &serialisation ) const override
		{
			// We can serialise much more compactly via the `addRows()` call made by `postConstructor()`.
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
		.def( init<std::string, Plug::Direction, unsigned>(
				(
					arg( "name" )=GraphComponent::defaultName<Spreadsheet::RowPlug>(),
					arg( "direction" )=Plug::In,
					arg( "flags" )=Plug::Default
				)
			)
		)
		.def( "__repr__", &rowPlugRepr )
		.attr( "__qualname__" ) = "Spreadsheet.RowPlug"
	;

	PlugClass<Spreadsheet::CellPlug>()
		.def( init<std::string, const Gaffer::Plug *, bool, Plug::Direction, unsigned>(
				(
					arg( "name" ),
					arg( "value" ),
					arg( "adoptEnabledPlug" ) = false,
					arg( "direction" ) = Plug::In,
					arg( "flags" )=Plug::Default
				)
			)
		)
		.def( "enabledPlug", &cellPlugEnabledPlug )
		.def( "__repr__", &cellPlugRepr )
		.attr( "__qualname__" ) = "Spreadsheet.CellPlug"
	;

	Serialisation::registerSerialiser( Gaffer::Spreadsheet::RowsPlug::staticTypeId(), new RowsPlugSerialiser );
	Serialisation::registerSerialiser( Gaffer::Spreadsheet::RowPlug::staticTypeId(), new RowPlugSerialiser );
	Serialisation::registerSerialiser( Gaffer::Spreadsheet::CellPlug::staticTypeId(), new CellPlugSerialiser );

}
