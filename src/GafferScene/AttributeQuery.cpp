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

template< typename ValueType >
bool setCompoundNumericPlugValue( const Gaffer::Plug* const root, Gaffer::ValuePlug* const plug, const ValueType& value )
{
	assert( root != 0 );
	assert( plug != 0 );

	const Gaffer::CompoundNumericPlug< ValueType >& cnp =
		*( IECore::assertedStaticCast< const Gaffer::CompoundNumericPlug< ValueType > >( root ) );
	Gaffer::NumericPlug< typename ValueType::BaseType >& vp =
		*( IECore::assertedStaticCast< Gaffer::NumericPlug< typename ValueType::BaseType > >( plug ) );

	for( unsigned int i = 0; i < ValueType::dimensions(); ++i )
	{
		if( & vp == cnp.getChild( i ) )
		{
			vp.setValue( value[ i ] );
			return true;
		}
	}

	return false;
}

template< typename ValueType >
bool setBoxPlugValue( const Gaffer::Plug* const root, Gaffer::ValuePlug* const plug, const ValueType& value )
{
	assert( root != 0 );
	assert( plug != 0 );

	const Gaffer::BoxPlug< ValueType >& bp =
		*( IECore::assertedStaticCast< const Gaffer::BoxPlug< ValueType > >( root ) );

	if( plug->parent() == bp.getChild( 0 ) )
	{
		return setCompoundNumericPlugValue( bp.minPlug(), plug, value.min );
	}
	else if( plug->parent() == bp.getChild( 1 ) )
	{
		return setCompoundNumericPlugValue( bp.maxPlug(), plug, value.max );
	}

	return false;
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

	switch( pid )
	{
		case Gaffer::BoolPlugTypeId:
			switch( oid )
			{
				case IECore::BoolDataTypeId:
					IECore::assertedStaticCast< Gaffer::BoolPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::BoolData >( object )->readable() );
					return true;
				case IECore::FloatDataTypeId:
					IECore::assertedStaticCast< Gaffer::BoolPlug >( plug )->setValue(
						static_cast< bool >(
							IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					return true;
				case IECore::IntDataTypeId:
					IECore::assertedStaticCast< Gaffer::BoolPlug >( plug )->setValue(
						static_cast< bool >(
							IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
					return true;
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
					return true;
				case IECore::FloatDataTypeId:
					IECore::assertedStaticCast< Gaffer::FloatPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() );
					return true;
				case IECore::IntDataTypeId:
					IECore::assertedStaticCast< Gaffer::FloatPlug >( plug )->setValue(
						static_cast< float >(
							IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
					return true;
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
					return true;
				case IECore::FloatDataTypeId:
					IECore::assertedStaticCast< Gaffer::IntPlug >( plug )->setValue(
						static_cast< int >(
							IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
					return true;
				case IECore::IntDataTypeId:
					IECore::assertedStaticCast< Gaffer::IntPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::IntData >( object )->readable() );
					return true;
				default:
					break;
			}
			break;
		case Gaffer::BoolVectorDataPlugTypeId:
			if( oid == IECore::BoolVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::BoolVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::BoolVectorData >( object ) );
				return true;
			}
			break;
		case Gaffer::FloatVectorDataPlugTypeId:
			if( oid == IECore::FloatVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::FloatVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::FloatVectorData >( object ) );
				return true;
			}
			break;
		case Gaffer::IntVectorDataPlugTypeId:
			if( oid == IECore::IntVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::IntVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::IntVectorData >( object ) );
				return true;
			}
			break;
		case Gaffer::StringPlugTypeId:
			switch( oid )
			{
				case IECore::StringDataTypeId:
					IECore::assertedStaticCast< Gaffer::StringPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::StringData >( object )->readable() );
					return true;
				case IECore::InternedStringDataTypeId:
					IECore::assertedStaticCast< Gaffer::StringPlug >( plug )->setValue(
						IECore::assertedStaticCast< const IECore::InternedStringData >( object )->readable().value() );
					return true;
				default:
					break;
			}
			break;
		case Gaffer::StringVectorDataPlugTypeId:
			if( oid == IECore::StringVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::StringVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::StringVectorData >( object ) );
				return true;
			}
			break;
		case Gaffer::InternedStringVectorDataPlugTypeId:
			if( oid == IECore::InternedStringVectorDataTypeId )
			{
				IECore::assertedStaticCast< Gaffer::InternedStringVectorDataPlug >( plug )->setValue(
					IECore::assertedStaticCast< const IECore::InternedStringVectorData >( object ) );
				return true;
			}
			break;
		case Gaffer::ObjectPlugTypeId:
			IECore::assertedStaticCast< Gaffer::ObjectPlug >( plug )->setValue( object );
			return true;
		case Gaffer::Color3fPlugTypeId:
			switch( oid )
			{
				case IECore::Color4fDataTypeId:
				{
					Imath::Color4f const& c = IECore::assertedStaticCast< const IECore::Color4fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color3f( c.r, c.g, c.b ) );
				}
				case IECore::Color3fDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::Color3fData >( object )->readable() );
				case IECore::V3fDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						static_cast< Imath::Color3f >(
							IECore::assertedStaticCast< const IECore::V3fData >( object )->readable() ) );
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color3f( v.x, v.y, 0.f ) );
				}
				case IECore::FloatDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color3f( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
				case IECore::IntDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color3f( static_cast< float >(
							IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) ) );
				default:
					break;
			}
			break;
		case Gaffer::Color4fPlugTypeId:
			switch( oid )
			{
				case IECore::Color4fDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::Color4fData >( object )->readable() );
				case IECore::Color3fDataTypeId:
				{
					const Imath::Color3f& c = IECore::assertedStaticCast< const IECore::Color3fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color4f( c.x, c.y, c.z, 1.f ) );
				}
				case IECore::V3fDataTypeId:
				{
					const Imath::V3f& v = IECore::assertedStaticCast< const IECore::V3fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color4f( v.x, v.y, v.z, 1.f ) );
				}
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color4f( v.x, v.y, 0.f, 1.f ) );
				}
				case IECore::FloatDataTypeId:
				{
					const float v = IECore::assertedStaticCast< const IECore::FloatData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color4f( v, v, v, 1.f ) );
				}
				case IECore::IntDataTypeId:
				{
					const float v = static_cast< float >( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() );
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::Color4f( v, v, v, 1.f ) );
				}
				default:
					break;
			}
			break;
		case Gaffer::V3fPlugTypeId:
			switch( oid )
			{
				case IECore::Color3fDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						static_cast< Imath::V3f >(
							IECore::assertedStaticCast< const IECore::Color3fData >( object )->readable() ) );
				case IECore::V3fDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::V3fData >( object )->readable() );
				case IECore::V3iDataTypeId:
				{
					const Imath::V3i& v = IECore::assertedStaticCast< const IECore::V3iData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3f( static_cast< float >( v.x ), static_cast< float >( v.y ), static_cast< float >( v.z ) ) );
				}
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3f( v.x, v.y, 0.f ) );
				}
				case IECore::V2iDataTypeId:
				{
					const Imath::V2i& v = IECore::assertedStaticCast< const IECore::V2iData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3f( static_cast< float >( v.x ), static_cast< float >( v.y ), 0.f ) );
				}
				case IECore::FloatDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3f( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
				case IECore::IntDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3f( static_cast< float >( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) ) );
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
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3i( static_cast< int >( v.x ), static_cast< int >( v.y ), static_cast< int >( v.z ) ) );
				}
				case IECore::V3iDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::V3iData >( object )->readable() );
				case IECore::V2fDataTypeId:
				{
					const Imath::V2f& v = IECore::assertedStaticCast< const IECore::V2fData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3i( static_cast< int >( v.x ), static_cast< int >( v.y ), 0 ) );
				}
				case IECore::V2iDataTypeId:
				{
					const Imath::V2i& v = IECore::assertedStaticCast< const IECore::V2iData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3i( v.x, v.y, 0 ) );
				}
				case IECore::FloatDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3i( static_cast< int >( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) ) );
				case IECore::IntDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V3i( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
				default:
					break;
			}
			break;
		case Gaffer::V2fPlugTypeId:
			switch( oid )
			{
				case IECore::V2fDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::V2fData >( object )->readable() );
				case IECore::V2iDataTypeId:
				{
					const Imath::V2i& v = IECore::assertedStaticCast< const IECore::V2iData >( object )->readable();
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V2f( static_cast< float >( v.x ), static_cast< float >( v.y ) ) );
				}
				case IECore::FloatDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V2f( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) );
				case IECore::IntDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V2f( static_cast< float >( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) ) );
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
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V2i( static_cast< int >( v.x ), static_cast< int >( v.y ) ) );
				}
				case IECore::V2iDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::V2iData >( object )->readable() );
				case IECore::FloatDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V2i( static_cast< int >( IECore::assertedStaticCast< const IECore::FloatData >( object )->readable() ) ) );
				case IECore::IntDataTypeId:
					return setCompoundNumericPlugValue( vplug, plug,
						Imath::V2i( IECore::assertedStaticCast< const IECore::IntData >( object )->readable() ) );
				default:
					break;
			}
			break;
		case Gaffer::Box3fPlugTypeId:
			switch( oid )
			{
				case IECore::Box3fDataTypeId:
					return setBoxPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::Box3fData >( object )->readable() );
				case IECore::Box3iDataTypeId:
				{
					const Imath::Box3i& b = IECore::assertedStaticCast< const IECore::Box3iData >( object )->readable();
					return setBoxPlugValue( vplug, plug,
						Imath::Box3f(
							Imath::V3f( static_cast< float >( b.min.x ), static_cast< float >( b.min.y ), static_cast< float >( b.min.z ) ),
							Imath::V3f( static_cast< float >( b.max.x ), static_cast< float >( b.max.y ), static_cast< float >( b.max.z ) ) ) );
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
					return setBoxPlugValue( vplug, plug,
						Imath::Box3i(
							Imath::V3i( static_cast< int >( b.min.x ), static_cast< int >( b.min.y ), static_cast< int >( b.min.z ) ),
							Imath::V3i( static_cast< int >( b.max.x ), static_cast< int >( b.max.y ), static_cast< int >( b.max.z ) ) ) );
				}
				case IECore::Box3iDataTypeId:
					return setBoxPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::Box3iData >( object )->readable() );
				default:
					break;
			}
			break;
		case Gaffer::Box2fPlugTypeId:
			switch( oid )
			{
				case IECore::Box2fDataTypeId:
					return setBoxPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::Box2fData >( object )->readable() );
				case IECore::Box2iDataTypeId:
				{
					const Imath::Box2i& b = IECore::assertedStaticCast< const IECore::Box2iData >( object )->readable();
					return setBoxPlugValue( vplug, plug,
						Imath::Box2f(
							Imath::V2f( static_cast< float >( b.min.x ), static_cast< float >( b.min.y ) ),
							Imath::V2f( static_cast< float >( b.max.x ), static_cast< float >( b.max.y ) ) ) );
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
					return setBoxPlugValue( vplug, plug,
						Imath::Box2i(
							Imath::V2i( static_cast< int >( b.min.x ), static_cast< int >( b.min.y ) ),
							Imath::V2i( static_cast< int >( b.max.x ), static_cast< int >( b.max.y ) ) ) );
				}
				case IECore::Box2iDataTypeId:
					return setBoxPlugValue( vplug, plug,
						IECore::assertedStaticCast< const IECore::Box2iData >( object )->readable() );
				default:
					break;
			}
			break;
		default:
			break;
	}

	return false;
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
