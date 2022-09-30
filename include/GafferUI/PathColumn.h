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

#ifndef GAFFERUI_PATHCOLUMN_H
#define GAFFERUI_PATHCOLUMN_H

#include "GafferUI/Export.h"

#include "Gaffer/Path.h"

#include "IECore/SimpleTypedData.h"

namespace GafferUI
{

/// Abstract class for extracting properties from a Path in a form
/// suitable for display in a table column. Primarily intended for
/// use in the PathListingWidget.
class GAFFERUI_API PathColumn : public IECore::RefCounted, public Gaffer::Signals::Trackable
{

	public :

		IE_CORE_DECLAREMEMBERPTR( PathColumn )

		struct CellData
		{
			CellData(
				const IECore::ConstDataPtr &value = nullptr,
				const IECore::ConstDataPtr &icon = nullptr,
				const IECore::ConstDataPtr &background = nullptr,
				const IECore::ConstDataPtr &toolTip = nullptr
			)	:	value( value ), icon( icon ), background( background ), toolTip( toolTip ) {}
			CellData( const CellData &other ) = default;

			/// The primary value to be displayed in a cell or header.
			/// Supported types :
			///
			/// - StringData
			/// - IntData, UIntData, UInt64Data
			/// - FloatData, DoubleData
			/// - DateTimeData
			/// - V2fData, V3fData, Color3fData, Color4fData
			IECore::ConstDataPtr value;
			/// An additional icon to be displayed next to the primary
			/// value. Supported types :
			///
			/// - StringData (providing icon name)
			/// - Color3fData (drawn as swatch)
			/// - CompoundData (containing `state:normal` and/or `state:highlighted`
			//    keys mapping to StringData providing an icon name for each state)
			IECore::ConstDataPtr icon;
			/// The background colour for the cell. Supported types :
			///
			/// - Color3fData
			/// - Color4fData
			IECore::ConstDataPtr background;
			/// Tip to be displayed on hover. Supported types :
			///
			/// - StringData
			IECore::ConstDataPtr toolTip;

			private :

				IECore::ConstDataPtr m_reserved1;
				IECore::ConstDataPtr m_reserved2;

		};

		/// Returns the data needed to draw a column cell.
		virtual CellData cellData( const Gaffer::Path &path, const IECore::Canceller *canceller = nullptr ) const = 0;
		/// Returns the data needed to draw a column header.
		virtual CellData headerData( const IECore::Canceller *canceller = nullptr ) const = 0;

		using PathColumnSignal = Gaffer::Signals::Signal<void ( PathColumn * ), Gaffer::Signals::CatchingCombiner<void>>;
		/// Subclasses should emit this signal when something changes
		/// in a way that would affect the results of `cellValue()`
		/// or `headerValue()`.
		PathColumnSignal &changedSignal();

	private :

		PathColumnSignal m_changedSignal;

};

IE_CORE_DECLAREPTR( PathColumn )

/// Standard column type which simply displays a property of the path.
class GAFFERUI_API StandardPathColumn : public PathColumn
{

	public :

		IE_CORE_DECLAREMEMBERPTR( StandardPathColumn )

		StandardPathColumn( const std::string &label, IECore::InternedString property );

		IECore::InternedString property() const;

		CellData cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const override;
		CellData headerData( const IECore::Canceller *canceller ) const override;

	private :

		IECore::ConstStringDataPtr m_label;
		IECore::InternedString m_property;

};

IE_CORE_DECLAREPTR( StandardPathColumn )

/// Column which uses a property of the path to specify an icon.
class GAFFERUI_API IconPathColumn : public PathColumn
{

	public :

		IE_CORE_DECLAREMEMBERPTR( IconPathColumn )

		/// The name for the icon is `<prefix><property>`, with `property` being queried
		/// by `Path::property()`. Supported property types :
		///
		/// - StringData
		/// - IntData, UInt44Data
		/// - BoolData
		IconPathColumn( const std::string &label, const std::string &prefix, IECore::InternedString property );

		CellData cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const override;
		CellData headerData( const IECore::Canceller *canceller ) const override;

	private :

		IECore::ConstStringDataPtr m_label;
		std::string m_prefix;
		IECore::InternedString m_property;

};

IE_CORE_DECLAREPTR( IconPathColumn )

/// Column type suitable for displaying an icon for
/// FileSystemPaths.
class GAFFERUI_API FileIconPathColumn : public PathColumn
{

	public :

		IE_CORE_DECLAREMEMBERPTR( FileIconPathColumn )

		FileIconPathColumn();

		CellData cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const override;
		CellData headerData( const IECore::Canceller *canceller ) const override;

	private :

		const IECore::StringDataPtr m_label;

};

IE_CORE_DECLAREPTR( FileIconPathColumn )

} // namespace GafferUI

#endif // GAFFERUI_PATHCOLUMN_H
