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

#include "GafferScene/AttributeQuery.h"
#include "Gaffer/MetadataAlgo.h"
#include "Gaffer/NumericPlug.h"

#include "IECore/NullObject.h"
#include "IECore/CompoundObject.h"
#include "IECore/Exception.h"

#include "boost/container/small_vector.hpp"

#include <cassert>
#include <vector>

namespace
{

bool setColor4fPlugValue( const Gaffer::Color4fPlug& parent, Gaffer::FloatPlug& child, const Imath::Color4f& value )
{
	if( & child == parent.getChild( 0 ) )
	{
		child.setValue( value.r );
	}
	else if( & child == parent.getChild( 1 ) )
	{
		child.setValue( value.g );
	}
	else if( & child == parent.getChild( 2 ) )
	{
		child.setValue( value.b );
	}
	else if( & child == parent.getChild( 3 ) )
	{
		child.setValue( value.a );
	}
	else
	{
		return false;
	}

	return true;
}

bool setColor3fPlugValue( const Gaffer::Color3fPlug& parent, Gaffer::FloatPlug& child, const Imath::Color3f& value )
{
	if( & child == parent.getChild( 0 ) )
	{
		child.setValue( value.x );
	}
	else if( & child == parent.getChild( 1 ) )
	{
		child.setValue( value.y );
	}
	else if( & child == parent.getChild( 2 ) )
	{
		child.setValue( value.z );
	}
	else
	{
		return false;
	}

	return true;
}

bool setV3fPlugValue( const Gaffer::V3fPlug& parent, Gaffer::FloatPlug& child, const Imath::V3f& value )
{
	if( & child == parent.getChild( 0 ) )
	{
		child.setValue( value.x );
	}
	else if( & child == parent.getChild( 1 ) )
	{
		child.setValue( value.y );
	}
	else if( & child == parent.getChild( 2 ) )
	{
		child.setValue( value.z );
	}
	else
	{
		return false;
	}

	return true;
}

bool setV3iPlugValue( const Gaffer::V3iPlug& parent, Gaffer::IntPlug& child, const Imath::V3i& value )
{
	if( & child == parent.getChild( 0 ) )
	{
		child.setValue( value.x );
	}
	else if( & child == parent.getChild( 1 ) )
	{
		child.setValue( value.y );
	}
	else if( & child == parent.getChild( 2 ) )
	{
		child.setValue( value.z );
	}
	else
	{
		return false;
	}

	return true;
}

bool setV2fPlugValue( const Gaffer::V2fPlug& parent, Gaffer::FloatPlug& child, const Imath::V2f& value )
{
	if( & child == parent.getChild( 0 ) )
	{
		child.setValue( value.x );
	}
	else if( & child == parent.getChild( 1 ) )
	{
		child.setValue( value.y );
	}
	else
	{
		return false;
	}

	return true;
}

bool setV2iPlugValue( const Gaffer::V2iPlug& parent, Gaffer::IntPlug& child, const Imath::V2i& value )
{
	if( & child == parent.getChild( 0 ) )
	{
		child.setValue( value.x );
	}
	else if( & child == parent.getChild( 1 ) )
	{
		child.setValue( value.y );
	}
	else
	{
		return false;
	}

	return true;
}

bool setBox3fPlugValue( const Gaffer::Box3fPlug& parent, Gaffer::FloatPlug& child, const Imath::Box3f& value )
{
	if( child.parent() == parent.getChild( 0 ) )
	{
		return setV3fPlugValue( *( parent.minPlug() ), child, value.min );
	}
	else if( child.parent() == parent.getChild( 1 ) )
	{
		return setV3fPlugValue( *( parent.maxPlug() ), child, value.max );
	}
	else
	{
		return false;
	}

	return true;
}

bool setBox3iPlugValue( const Gaffer::Box3iPlug& parent, Gaffer::IntPlug& child, const Imath::Box3i& value )
{
	if( child.parent() == parent.getChild( 0 ) )
	{
		return setV3iPlugValue( *( parent.minPlug() ), child, value.min );
	}
	else if( child.parent() == parent.getChild( 1 ) )
	{
		return setV3iPlugValue( *( parent.maxPlug() ), child, value.max );
	}
	else
	{
		return false;
	}

	return true;
}

bool setBox2fPlugValue( const Gaffer::Box2fPlug& parent, Gaffer::FloatPlug& child, const Imath::Box2f& value )
{
	if( child.parent() == parent.getChild( 0 ) )
	{
		return setV2fPlugValue( *( parent.minPlug() ), child, value.min );
	}
	else if( child.parent() == parent.getChild( 1 ) )
	{
		return setV2fPlugValue( *( parent.maxPlug() ), child, value.max );
	}
	else
	{
		return false;
	}

	return true;
}

bool setBox2iPlugValue( const Gaffer::Box2iPlug& parent, Gaffer::IntPlug& child, const Imath::Box2i& value )
{
	if( child.parent() == parent.getChild( 0 ) )
	{
		return setV2iPlugValue( *( parent.minPlug() ), child, value.min );
	}
	else if( child.parent() == parent.getChild( 1 ) )
	{
		return setV2iPlugValue( *( parent.maxPlug() ), child, value.max );
	}
	else
	{
		return false;
	}

	return true;
}

bool canSetPlugType( const Gaffer::TypeId pid )
{
	bool result = false;

	switch( pid )
	{
		case Gaffer::BoolPlugTypeId:
		case Gaffer::FloatPlugTypeId:
		case Gaffer::IntPlugTypeId:
		case Gaffer::BoolVectorDataPlugTypeId:
		case Gaffer::FloatVectorDataPlugTypeId:
		case Gaffer::IntVectorDataPlugTypeId:
		case Gaffer::StringPlugTypeId:
		case Gaffer::StringVectorDataPlugTypeId:
		case Gaffer::InternedStringVectorDataPlugTypeId:
		case Gaffer::Color3fPlugTypeId:
		case Gaffer::Color4fPlugTypeId:
		case Gaffer::V3fPlugTypeId:
		case Gaffer::V3iPlugTypeId:
		case Gaffer::V2fPlugTypeId:
		case Gaffer::V2iPlugTypeId:
		case Gaffer::Box3fPlugTypeId:
		case Gaffer::Box3iPlugTypeId:
		case Gaffer::Box2fPlugTypeId:
		case Gaffer::Box2iPlugTypeId:
		case Gaffer::ObjectPlugTypeId:
			result = true;
			break;
		default:
			break;
	}

	return result;
}

bool setPlugFromObject( const Gaffer::Plug* const vplug, Gaffer::ValuePlug* const plug, const IECore::Object* const object )
{
	assert( vplug != 0 );
	assert( plug != 0 );
	assert( object != 0 );

	const Gaffer::TypeId pid = static_cast< Gaffer::TypeId >( vplug->typeId() );
	const IECore::TypeId oid = object->typeId();

	bool result = false;

	switch( pid )
	{
		case Gaffer::BoolPlugTypeId:
			switch( oid )
			{
				case IECore::BoolDataTypeId:
					IECore::assertedStaticCast< Gaffer::BoolPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::BoolData >( object )->readable() );
					result = true;
					break;
				case IECore::FloatDataTypeId:
					IECore::assertedStaticCast< Gaffer::BoolPlug >( plug )->setValue(
						static_cast< bool >(
							IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					result = true;
					break;
				case IECore::IntDataTypeId:
					IECore::assertedStaticCast< Gaffer::BoolPlug >( plug )->setValue(
						static_cast< bool >(
							IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
					result = true;
					break;
				default:
					break;
			}
			break;
		case Gaffer::FloatPlugTypeId:
			switch( oid )
			{
				case IECore::BoolDataTypeId:
					IECore::assertedStaticCast< Gaffer::FloatPlug >( plug )->setValue(
						static_cast< float >(
							IECore::assertedStaticCast< const IECore::BoolData >( object )->readable() ) );
					result = true;
					break;
				case IECore::FloatDataTypeId:
					IECore::assertedStaticCast< Gaffer::FloatPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() );
					result = true;
					break;
				case IECore::IntDataTypeId:
					IECore::assertedStaticCast< Gaffer::FloatPlug >( plug )->setValue(
						static_cast< float >(
							IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
					result = true;
					break;
				default:
					break;
			}
			break;
		case Gaffer::IntPlugTypeId:
			switch( oid )
			{
				case IECore::BoolDataTypeId:
					IECore::assertedStaticCast< Gaffer::IntPlug >( plug )->setValue(
						static_cast< int >(
							IECore::assertedStaticCast< const IECore::BoolData >( object )->readable() ) );
					result = true;
					break;
				case IECore::FloatDataTypeId:
					IECore::assertedStaticCast< Gaffer::IntPlug >( plug )->setValue(
						static_cast< int >(
							IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					result = true;
					break;
				case IECore::IntDataTypeId:
					IECore::assertedStaticCast< Gaffer::IntPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::IntData >( object )->readable() );
					result = true;
					break;
				default:
					break;
			}
			break;
		case Gaffer::BoolVectorDataPlugTypeId:
			if( oid == IECore::BoolVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::BoolVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::BoolVectorData >( object ) );
				result = true;
			}
			break;
		case Gaffer::FloatVectorDataPlugTypeId:
			if( oid == IECore::FloatVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::FloatVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::FloatVectorData >( object ) );
				result = true;
			}
			break;
		case Gaffer::IntVectorDataPlugTypeId:
			if( oid == IECore::IntVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::IntVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::IntVectorData >( object ) );
				result = true;
			}
			break;
		case Gaffer::StringPlugTypeId:
			switch( oid )
			{
				case IECore::StringDataTypeId:
					IECore::assertedStaticCast< Gaffer::StringPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::StringData >( object )->readable() );
					result = true;
					break;
				case IECore::InternedStringDataTypeId:
					IECore::assertedStaticCast< Gaffer::StringPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::InternedStringData >( object )->readable().value() );
					result = true;
					break;
				default:
					break;
			}
			break;
		case Gaffer::StringVectorDataPlugTypeId:
			if( oid == IECore::StringVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::StringVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::StringVectorData >( object ) );
				result = true;
			}
			break;
		case Gaffer::InternedStringVectorDataPlugTypeId:
			if( oid == IECore::InternedStringVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::InternedStringVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::InternedStringVectorData >( object ) );
				result = true;
			}
			break;
		case Gaffer::ObjectPlugTypeId:
			IECore::assertedStaticCast< Gaffer::ObjectPlug >( plug )->setValue( object );
			result = true;
			break;
		case Gaffer::Color3fPlugTypeId:
			switch( oid )
			{
				case IECore::Color4fDataTypeId:
				{
					Imath::Color4f const& c = IECore::assertedStaticCast< const IECore::Color4fData >( object )->readable();
					result = setColor3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color3f( c.r, c.g, c.b ) );
					break;
				}
				case IECore::Color3fDataTypeId:
					result = setColor3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Color3fData >( object )->readable() );
					break;
				case IECore::V3fDataTypeId:
					result = setColor3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::V3fData >( object )->readable() );
					break;
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					result = setColor3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color3f( v.x, v.y, 0.f ) );
					break;
				}
				case IECore::FloatDataTypeId:
					result = setColor3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color3f( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					break;
				case IECore::IntDataTypeId:
					result = setColor3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color3f( static_cast< float >(
							IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) ) );
					break;
				default:
					break;
			}
			break;
		case Gaffer::Color4fPlugTypeId:
			switch( oid )
			{
				case IECore::Color4fDataTypeId:
					result = setColor4fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color4fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Color4fData >( object )->readable() );
					break;
				case IECore::Color3fDataTypeId:
				{
					const Imath::Color3f& c = IECore::assertedStaticCast< const IECore::Color3fData >( object )->readable();
					result = setColor4fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color4fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color4f( c.x, c.y, c.z, 1.f ) );
					break;
				}
				case IECore::V3fDataTypeId:
				{
					const Imath::V3f& v = IECore::assertedStaticCast< const IECore::V3fData >( object )->readable();
					result = setColor4fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color4fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color4f( v.x, v.y, v.z, 1.f ) );
					break;
				}
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					result = setColor4fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color4fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color4f( v.x, v.y, 0.f, 1.f ) );
					break;
				}
				case IECore::FloatDataTypeId:
				{
					const float v = IECore::assertedStaticCast< const IECore::FloatData >( object )->readable();
					result = setColor4fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color4fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color4f( v, v, v, 1.f ) );
					break;
				}
				case IECore::IntDataTypeId:
				{
					const float v = static_cast< float >( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() );
					result = setColor4fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Color4fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Color4f( v, v, v, 1.f ) );
					break;
				}
				default:
					break;
			}
			break;
		case Gaffer::V3fPlugTypeId:
			switch( oid )
			{
				case IECore::Color3fDataTypeId:
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Color3fData >( object )->readable() );
					break;
				case IECore::V3fDataTypeId:
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::V3fData >( object )->readable() );
					break;
				case IECore::V3iDataTypeId:
				{
					const Imath::V3i& v = IECore::assertedStaticCast< const IECore::V3iData >( object )->readable();
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V3f( static_cast< float >( v.x ), static_cast< float >( v.y ), static_cast< float >( v.z ) ) );
					break;
				}
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V3f( v.x, v.y, 0.f ) );
					break;
				}
				case IECore::V2iDataTypeId:
				{
					const Imath::V2i& v = IECore::assertedStaticCast< const IECore::V2iData >( object )->readable();
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V3f( static_cast< float >( v.x ), static_cast< float >( v.y ), 0.f ) );
					break;
				}
				case IECore::FloatDataTypeId:
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V3f( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					break;
				case IECore::IntDataTypeId:
					result = setV3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V3f( static_cast< float >( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) ) );
					break;
				default:
					break;
			}
			break;
		case Gaffer::V3iPlugTypeId:
			switch( oid )
			{
				case IECore::V3fDataTypeId:
				{
					const Imath::V3f& v = IECore::assertedStaticCast< const IECore::V3fData >( object )->readable();
					result = setV3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V3i( static_cast< int >( v.x ), static_cast< int >( v.y ), static_cast< int >( v.z ) ) );
					break;
				}
				case IECore::V3iDataTypeId:
					result = setV3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::V3iData >( object )->readable() );
					break;
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					result = setV3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V3i( static_cast< int >( v.x ), static_cast< int >( v.y ), 0 ) );
					break;
				}
				case IECore::V2iDataTypeId:
				{
					const Imath::V2i& v = IECore::assertedStaticCast< const IECore::V2iData >( object )->readable();
					result = setV3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V3i( v.x, v.y, 0 ) );
					break;
				}
				case IECore::FloatDataTypeId:
					result = setV3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V3i( static_cast< int >( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) ) );
					break;
				case IECore::IntDataTypeId:
					result = setV3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V3i( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
					break;
				default:
					break;
			}
			break;
		case Gaffer::V2fPlugTypeId:
			switch( oid )
			{
				case IECore::V2fDataTypeId:
					result = setV2fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::V2fData >( object )->readable() );
					break;
				case IECore::V2iDataTypeId:
				{
					const Imath::V2i& v = IECore::assertedStaticCast< const IECore::V2iData >( object )->readable();
					result = setV2fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V2f( static_cast< float >( v.x ), static_cast< float >( v.y ) ) );
					break;
				}
				case IECore::FloatDataTypeId:
					result = setV2fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V2f( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					break;
				case IECore::IntDataTypeId:
					result = setV2fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::V2f( static_cast< float >( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) ) );
					break;
				default:
					break;
			}
			break;
		case Gaffer::V2iPlugTypeId:
			switch( oid )
			{
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					result = setV2iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V2i( static_cast< int >( v.x ), static_cast< int >( v.y ) ) );
					break;
				}
				case IECore::V2iDataTypeId:
					result = setV2iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::V2iData >( object )->readable() );
					break;
				case IECore::FloatDataTypeId:
					result = setV2iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V2i( static_cast< int >( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) ) );
					break;
				case IECore::IntDataTypeId:
					result = setV2iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::V2iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::V2i( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
					break;
				default:
					break;
			}
			break;
		case Gaffer::Box3fPlugTypeId:
			switch( oid )
			{
				case IECore::Box3fDataTypeId:
					result = setBox3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Box3fData >( object )->readable() );
					break;
				case IECore::Box3iDataTypeId:
				{
					const Imath::Box3i& b = IECore::assertedStaticCast< const IECore::Box3iData >( object )->readable();
					result = setBox3fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box3fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Box3f(
							Imath::V3f( static_cast< float >( b.min.x ), static_cast< float >( b.min.y ), static_cast< float >( b.min.z ) ),
							Imath::V3f( static_cast< float >( b.max.x ), static_cast< float >( b.max.y ), static_cast< float >( b.max.z ) ) ) );
					break;
				}
				default:
					break;
			}
			break;
		case Gaffer::Box3iPlugTypeId:
			switch( oid )
			{
				case IECore::Box3fDataTypeId:
				{
					const Imath::Box3f& b = IECore::assertedStaticCast< const IECore::Box3fData >( object )->readable();
					result = setBox3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::Box3i(
							Imath::V3i( static_cast< int >( b.min.x ), static_cast< int >( b.min.y ), static_cast< int >( b.min.z ) ),
							Imath::V3i( static_cast< int >( b.max.x ), static_cast< int >( b.max.y ), static_cast< int >( b.max.z ) ) ) );
					break;
				}
				case IECore::Box3iDataTypeId:
					result = setBox3iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box3iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Box3iData >( object )->readable() );
					break;
				default:
					break;
			}
			break;
		case Gaffer::Box2fPlugTypeId:
			switch( oid )
			{
				case IECore::Box2fDataTypeId:
					result = setBox2fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box2fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Box2fData >( object )->readable() );
					break;
				case IECore::Box2iDataTypeId:
				{
					const Imath::Box2i& b = IECore::assertedStaticCast< const IECore::Box2iData >( object )->readable();
					result = setBox2fPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box2fPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::FloatPlug >( plug ) ),
						Imath::Box2f(
							Imath::V2f( static_cast< float >( b.min.x ), static_cast< float >( b.min.y ) ),
							Imath::V2f( static_cast< float >( b.max.x ), static_cast< float >( b.max.y ) ) ) );
					break;
				}
				default:
					break;
			}
			break;
		case Gaffer::Box2iPlugTypeId:
			switch( oid )
			{
				case IECore::Box2fDataTypeId:
				{
					const Imath::Box2f& b = IECore::assertedStaticCast< const IECore::Box2fData >( object )->readable();
					result = setBox2iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box2iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						Imath::Box2i(
							Imath::V2i( static_cast< int >( b.min.x ), static_cast< int >( b.min.y ) ),
							Imath::V2i( static_cast< int >( b.max.x ), static_cast< int >( b.max.y ) ) ) );
					break;
				}
				case IECore::Box2iDataTypeId:
					result = setBox2iPlugValue(
						*( IECore::assertedStaticCast< const Gaffer::Box2iPlug >( vplug ) ),
						*( IECore::assertedStaticCast< Gaffer::IntPlug >( plug ) ),
						IECore::assertedStaticCast< const IECore::Box2iData >( object )->readable() );
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	return result;
}

Gaffer::Plug const* correspondingPlug( const Gaffer::Plug* const parent, const Gaffer::Plug* const child, const Gaffer::Plug* const other )
{
	assert( parent != 0 );
	assert( child != 0 );
	assert( other != 0 );

	boost::container::small_vector< const Gaffer::Plug*, 4 > path;

	const Gaffer::Plug* plug = child;

	while( plug != parent )
	{
		path.push_back( plug );
		plug = plug->parent< Gaffer::Plug >();
	}

	plug = other;

	while( ! path.empty() )
	{
		plug = plug->getChild< Gaffer::Plug >( path.back()->getName() );
		path.pop_back();
	}

	return plug;
}

void addChildPlugsToAffectedOutputs( const Gaffer::Plug* const plug, Gaffer::DependencyNode::AffectedPlugsContainer& outputs )
{
	assert( plug != 0 );

	if( plug->children().empty() )
	{
		outputs.push_back( plug );
	}
	else for( Gaffer::GraphComponent::ChildIterator it = plug->children().begin(), it_end = plug->children().end(); it != it_end; ++it )
	{
		const Gaffer::Plug* const child = IECore::runTimeCast< const Gaffer::Plug >( ( *it ).get() );

		if( ( child != 0 ) && ( child->direction() == Gaffer::Plug::Out ) )
		{
			addChildPlugsToAffectedOutputs( child, outputs );
		}
	}
}

IECore::InternedString g_defPlugName( "default" );
IECore::InternedString g_valPlugName( "value" );

} // namespace

namespace GafferScene
{

size_t AttributeQuery::g_firstPlugIndex = 0;

GAFFER_NODE_DEFINE_TYPE( AttributeQuery );

AttributeQuery::AttributeQuery( const std::string& name )
: Gaffer::ComputeNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new ScenePlug( "scene" ) );
	addChild( new Gaffer::StringPlug( "location" ) );
	addChild( new Gaffer::StringPlug( "attribute" ) );
	addChild( new Gaffer::BoolPlug( "inherit", Gaffer::Plug::In, false ) );
	addChild( new Gaffer::BoolPlug( "exists", Gaffer::Plug::Out, false ) );
	addChild( new Gaffer::ObjectPlug( "__internalObject", Gaffer::Plug::Out, IECore::NullObject::defaultNullObject() ) );
}

AttributeQuery::~AttributeQuery()
{}

ScenePlug* AttributeQuery::scenePlug()
{
	return const_cast< ScenePlug* >(
		static_cast< const AttributeQuery* >( this )->scenePlug() );
}

const ScenePlug* AttributeQuery::scenePlug() const
{
	return getChild< ScenePlug >( g_firstPlugIndex );
}

Gaffer::StringPlug* AttributeQuery::locationPlug()
{
	return const_cast< Gaffer::StringPlug* >(
		static_cast< const AttributeQuery* >( this )->locationPlug() );
}

const Gaffer::StringPlug* AttributeQuery::locationPlug() const
{
	return getChild< Gaffer::StringPlug >( g_firstPlugIndex + 1 );
}

Gaffer::StringPlug* AttributeQuery::attributePlug()
{
	return const_cast< Gaffer::StringPlug* >(
		static_cast< const AttributeQuery* >( this )->attributePlug() );
}

const Gaffer::StringPlug* AttributeQuery::attributePlug() const
{
	return getChild< Gaffer::StringPlug >( g_firstPlugIndex + 2 );
}

Gaffer::BoolPlug* AttributeQuery::inheritPlug()
{
	return const_cast< Gaffer::BoolPlug* >(
		static_cast< const AttributeQuery* >( this )->inheritPlug() );
}

const Gaffer::BoolPlug* AttributeQuery::inheritPlug() const
{
	return getChild< Gaffer::BoolPlug >( g_firstPlugIndex + 3 );
}

Gaffer::BoolPlug* AttributeQuery::existsPlug()
{
	return const_cast< Gaffer::BoolPlug* >(
		static_cast< const AttributeQuery* >( this )->existsPlug() );
}

const Gaffer::BoolPlug* AttributeQuery::existsPlug() const
{
	return getChild< Gaffer::BoolPlug >( g_firstPlugIndex + 4 );
}

Gaffer::ObjectPlug* AttributeQuery::internalObjectPlug()
{
	return const_cast< Gaffer::ObjectPlug* >(
		static_cast< const AttributeQuery* >( this )->internalObjectPlug() );
}

const Gaffer::ObjectPlug* AttributeQuery::internalObjectPlug() const
{
	return getChild< Gaffer::ObjectPlug >( g_firstPlugIndex + 5 );
}

bool AttributeQuery::isSetup() const
{
	return ( defaultPlug() != 0 ) && ( valuePlug() != 0 );
}

bool AttributeQuery::canSetup( const Gaffer::ValuePlug* const plug ) const
{
	const bool success =
		( plug != 0 ) &&
		( ! isSetup() ) &&
		( canSetPlugType( static_cast< Gaffer::TypeId >( plug->typeId() ) ) );

	return success;
}

void AttributeQuery::setup( const Gaffer::ValuePlug* const plug )
{
	if( defaultPlug() )
	{
		throw IECore::Exception( "AttributeQuery already has a \"default\" plug." );
	}
	if( valuePlug() )
	{
		throw IECore::Exception( "AttributeQuery already has a \"value\" plug." );
	}

	assert( canSetup( plug ) );

	Gaffer::PlugPtr def = plug->createCounterpart( g_defPlugName, Gaffer::Plug::In );
	def->setFlags( Gaffer::Plug::Serialisable, true );
	addChild( def );

	Gaffer::PlugPtr val = plug->createCounterpart( g_valPlugName, Gaffer::Plug::Out );
	val->setFlags( Gaffer::Plug::Serialisable, true );
	addChild( val );
}

Gaffer::ValuePlug* AttributeQuery::defaultPlug()
{
	return const_cast< Gaffer::ValuePlug* >(
		static_cast< const AttributeQuery* >( this )->defaultPlug() );
}

const Gaffer::ValuePlug* AttributeQuery::defaultPlug() const
{
	return getChild< Gaffer::ValuePlug >( g_defPlugName );
}

Gaffer::ValuePlug* AttributeQuery::valuePlug()
{
	return const_cast< Gaffer::ValuePlug* >(
		static_cast< const AttributeQuery* >( this )->valuePlug() );
}

const Gaffer::ValuePlug* AttributeQuery::valuePlug() const
{
	return getChild< Gaffer::ValuePlug >( g_valPlugName );
}

void AttributeQuery::affects( const Gaffer::Plug* const input, AffectedPlugsContainer& outputs ) const
{
	ComputeNode::affects( input, outputs );

	if( input == internalObjectPlug() )
	{
		const Gaffer::Plug* const vplug = valuePlug();

		if( vplug != 0 )
		{
			addChildPlugsToAffectedOutputs( vplug, outputs );
		}

		outputs.push_back( existsPlug() );
	}
	else if(
		( input == inheritPlug() ) ||
		( input == locationPlug() ) ||
		( input == attributePlug() ) ||
		( input == scenePlug()->existsPlug() ) ||
		( input == scenePlug()->attributesPlug() ) )
	{
		outputs.push_back( internalObjectPlug() );
	}
	else
	{
		assert( input != 0 );

		const Gaffer::Plug* const dplug = defaultPlug();

		if(
			( dplug == input ) ||
			( dplug->isAncestorOf( input ) ) )
		{
			const Gaffer::Plug* const vplug = valuePlug();

			if( vplug != 0 )
			{
				outputs.push_back( correspondingPlug( dplug, input, vplug ) );
			}
		}
	}
}

void AttributeQuery::hash( const Gaffer::ValuePlug* const output, const Gaffer::Context* const context, IECore::MurmurHash& h ) const
{
	ComputeNode::hash( output, context, h );

	if( output == internalObjectPlug() )
	{
		const std::string loc = locationPlug()->getValue();

		if( ! loc.empty() )
		{
			const ScenePlug* const splug = scenePlug();

			const ScenePlug::ScenePath path = ScenePlug::stringToPath( loc );

			if( splug->exists( path ) )
			{
				h.append( ( inheritPlug()->getValue() )
					? splug->fullAttributesHash( path )
					: splug->attributesHash( path ) );
				attributePlug()->hash( h );
			}
		}
	}
	else if( output == existsPlug() )
	{
		internalObjectPlug()->hash( h );
	}
	else
	{
		assert( output != 0 );

		const Gaffer::Plug* const vplug = valuePlug();

		if(
			( vplug == output ) ||
			( vplug->isAncestorOf( output ) ) )
		{
			internalObjectPlug()->hash( h );
			IECore::assertedStaticCast< const Gaffer::ValuePlug >( correspondingPlug( vplug, output, defaultPlug() ) )->hash( h );
		}
	}
}

void AttributeQuery::compute( Gaffer::ValuePlug* const output, const Gaffer::Context* const context ) const
{
	if( output == internalObjectPlug() )
	{
		IECore::ObjectPtr obj( IECore::NullObject::defaultNullObject() );

		const std::string loc = locationPlug()->getValue();

		if( ! loc.empty() )
		{
			const ScenePlug* const splug = scenePlug();

			const ScenePlug::ScenePath path = ScenePlug::stringToPath( loc );

			if( splug->exists( path ) )
			{
				const std::string name = attributePlug()->getValue();

				if( ! name.empty() )
				{
					const IECore::ConstCompoundObjectPtr cobj = ( inheritPlug()->getValue() )
						? boost::static_pointer_cast< const IECore::CompoundObject >( splug->fullAttributes( path ) )
						: ( splug->attributes( path ) );
					assert( cobj );

					const IECore::CompoundObject::ObjectMap& objmap = cobj->members();
					const IECore::CompoundObject::ObjectMap::const_iterator it = objmap.find( name );

					if( it != objmap.end() )
					{
						obj = ( *it ).second;
					}
				}
			}
		}

		IECore::assertedStaticCast< Gaffer::ObjectPlug >( output )->setValue( obj );
	}
	else if( output == existsPlug() )
	{
		const IECore::ConstObjectPtr object = internalObjectPlug()->getValue();
		assert( object );

		IECore::assertedStaticCast< Gaffer::BoolPlug >( output )->setValue( object->isNotEqualTo( IECore::NullObject::defaultNullObject() ) );
	}
	else
	{
		assert( output != 0 );

		const Gaffer::Plug* const vplug = valuePlug();

		if(
			( vplug == output ) ||
			( vplug->isAncestorOf( output ) ) )
		{
			const IECore::ConstObjectPtr object = internalObjectPlug()->getValue();
			assert( object );

			if(
				( object->isEqualTo( IECore::NullObject::defaultNullObject() ) ) ||
				( ! setPlugFromObject( vplug, output, object.get() ) ) )
			{
				output->setFrom( IECore::assertedStaticCast< const Gaffer::ValuePlug >(
					correspondingPlug( vplug, output, defaultPlug() ) ) );
			}
		}
	}
}

} // GafferScene
