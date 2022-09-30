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

#include "GafferUI/PathColumn.h"

#include "Gaffer/FileSystemPath.h"

#include "IECore/MessageHandler.h"
#include "IECore/SplineData.h"

#include "QtWidgets/QFileIconProvider"

#include "boost/lexical_cast.hpp"

using namespace IECore;
using namespace Gaffer;
using namespace GafferUI;

namespace
{

const std::string basisName( StandardCubicBasis basis )
{
	switch( basis )
	{
		case StandardCubicBasis::Bezier : return "Bezier"; break;
		case StandardCubicBasis::BSpline : return "BSpline"; break;
		case StandardCubicBasis::CatmullRom : return "CatmullRom"; break;
		case StandardCubicBasis::Linear : return "Linear"; break;
		case StandardCubicBasis::Constant : return "Constant"; break;
		default: break;
	}

	return "Unknown";
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// PathColumn
//////////////////////////////////////////////////////////////////////////

PathColumn::PathColumnSignal &PathColumn::changedSignal()
{
	return m_changedSignal;
}

PathColumn::ButtonSignal &PathColumn::buttonPressSignal()
{
	return m_buttonPressSignal;
}

PathColumn::ButtonSignal &PathColumn::buttonReleaseSignal()
{
	return m_buttonReleaseSignal;
}

//////////////////////////////////////////////////////////////////////////
// StandardPathColumn
//////////////////////////////////////////////////////////////////////////

StandardPathColumn::StandardPathColumn( const std::string &label, IECore::InternedString property )
	:	m_label( new IECore::StringData( label ) ), m_property( property )
{
}

IECore::InternedString StandardPathColumn::property() const
{
	return m_property;
}

PathColumn::CellData StandardPathColumn::cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const
{
	IECore::ConstDataPtr data = runTimeCast<const IECore::Data>( path.property( m_property, canceller ) );
	CellData cellData = CellData( data );

	if( auto color = runTimeCast<const Color3fData>( data.get() ) )
	{
		cellData.icon = color;
	}
	else if( auto color = runTimeCast<const Color4fData>( data.get() ) )
	{
		cellData.icon = color;
	}
	else if( auto spline = runTimeCast<const SplineffData>( data.get() ) )
	{
		cellData.value = new StringData( basisName( spline->readable().basis.standardBasis() ) );
	}
	else if( auto spline = runTimeCast<const SplineddData>( data.get() ) )
	{
		cellData.value = new StringData( basisName( spline->readable().basis.standardBasis() ) );
	}
	else if( auto spline = runTimeCast<const SplinefColor3fData>( data.get() ) )
	{
		cellData.value = new StringData( basisName( spline->readable().basis.standardBasis() ) );
	}
	else if( auto spline = runTimeCast<const SplinefColor4fData>( data.get() ) )
	{
		cellData.value = new StringData( basisName( spline->readable().basis.standardBasis() ) );
	}

	return CellData( cellData );
}

PathColumn::CellData StandardPathColumn::headerData( const IECore::Canceller *canceller ) const
{
	return CellData( m_label );
}

//////////////////////////////////////////////////////////////////////////
// IconPathColumn
//////////////////////////////////////////////////////////////////////////

IconPathColumn::IconPathColumn( const std::string &label, const std::string &prefix, IECore::InternedString property )
	:	m_label( new StringData( label ) ), m_prefix( prefix ), m_property( property )
{
}

PathColumn::CellData IconPathColumn::cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const
{
	CellData result;

	ConstRunTimeTypedPtr property = path.property( m_property, canceller );
	if( !property )
	{
		return result;
	}

	std::string fileName = m_prefix;
	switch( property->typeId() )
	{
		case IECore::StringDataTypeId :
			fileName += static_cast<const IECore::StringData *>( property.get() )->readable();
			break;
		case IECore::IntDataTypeId :
			fileName += boost::lexical_cast<std::string>( static_cast<const IECore::IntData *>( property.get() )->readable() );
			break;
		case IECore::UInt64DataTypeId :
			fileName += boost::lexical_cast<std::string>( static_cast<const IECore::UInt64Data *>( property.get() )->readable() );
			break;
		case IECore::BoolDataTypeId :
			fileName += boost::lexical_cast<std::string>( static_cast<const IECore::BoolData *>( property.get() )->readable() );
			break;
		default :
			IECore::msg( IECore::Msg::Warning, "IconPathColumn", boost::str( boost::format( "Unsupported property type \"%s\"" ) % property->typeName() ) );
			return result;
	}

	result.icon = new IECore::StringData( fileName += ".png" );
	return result;
}

PathColumn::CellData IconPathColumn::headerData( const IECore::Canceller *canceller ) const
{
	return CellData( m_label );
}

//////////////////////////////////////////////////////////////////////////
// FileIconPathColumn
//////////////////////////////////////////////////////////////////////////

FileIconPathColumn::FileIconPathColumn()
	:	m_label( new IECore::StringData( "Type" ) )
{
}

PathColumn::CellData FileIconPathColumn::cellData( const Gaffer::Path &path, const IECore::Canceller *canceller ) const
{
	std::string s = path.string();
	if( const FileSystemPath *fileSystemPath = runTimeCast<const FileSystemPath>( &path ) )
	{
		if( fileSystemPath->getIncludeSequences() )
		{
			IECore::FileSequencePtr seq = fileSystemPath->fileSequence();
			if( seq )
			{
				std::vector<IECore::FrameList::Frame> frames;
				seq->getFrameList()->asList( frames );
				s = seq->fileNameForFrame( *frames.begin() );
			}
		}
	}

	return CellData( /* value = */ nullptr, /* icon = */ new StringData( "fileIcon:" + s ) );
}

PathColumn::CellData FileIconPathColumn::headerData( const IECore::Canceller *canceller ) const
{
	return CellData( m_label );
}
