//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2018, Alex Fuller. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
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

#include "GafferCycles/IECoreCyclesPreview/GeometryAlgo.h"

#include "IECore/MessageHandler.h"

#include "IECoreScene/PrimitiveVariable.h"

#include "IECore/SimpleTypedData.h"

IECORE_PUSH_DEFAULT_VISIBILITY
#include "scene/image.h"
#include "scene/image_vdb.h"
// Cycles (for ustring)
#include "util/param.h"
#include "util/version.h"
#undef fmix // OpenImageIO's farmhash inteferes with IECore::MurmurHash
IECORE_POP_DEFAULT_VISIBILITY

#include "fmt/format.h"

#include <unordered_map>

using namespace std;
using namespace IECore;
using namespace IECoreScene;

//////////////////////////////////////////////////////////////////////////
// Internal utilities
//////////////////////////////////////////////////////////////////////////

namespace std
{

/// \todo Move to IECore/TypeIds.h
template<>
struct hash<IECore::TypeId>
{
	size_t operator()( IECore::TypeId typeId ) const
	{
		return hash<size_t>()( typeId );
	}
};

} // namespace std

namespace
{

using namespace IECoreCycles;

struct Converters
{

	GeometryAlgo::Converter converter;
	GeometryAlgo::MotionConverter motionConverter;

};

using Registry = std::unordered_map<IECore::TypeId, Converters>;

Registry &registry()
{
	static Registry r;
	return r;
}

ccl::TypeDesc typeFromGeometricDataInterpretation( IECore::GeometricData::Interpretation dataType )
{
	switch( dataType )
	{
		case GeometricData::Numeric :
			return ccl::TypeVector;
		case GeometricData::Point :
			return ccl::TypePoint;
		case GeometricData::Normal :
			return ccl::TypeNormal;
		case GeometricData::Vector :
			return ccl::TypeVector;
		case GeometricData::Color :
			return ccl::TypeColor;
		case GeometricData::UV :
			return ccl::TypePoint;
		default :
			return ccl::TypeVector;
	}
}

template<typename T>
size_t dataSize( const TypedData<std::vector<T>> *data )
{
	return data->readable().size();
}

template<typename T>
size_t dataSize( const TypedData<T> *data )
{
	return 1;
}

template<typename T>
ccl::Attribute *convertTypedPrimitiveVariable( const std::string &name, const PrimitiveVariable &primitiveVariable, ccl::AttributeSet &attributes, ccl::TypeDesc typeDesc, ccl::AttributeElement attributeElement )
{
	// Get the data to convert, expanding indexed data if necessary, since Cycles doesn't support it
	// natively.

	const T *data;
	typename T::Ptr expandedData;
	if( primitiveVariable.indices )
	{
		expandedData = boost::static_pointer_cast<T>( primitiveVariable.expandedData() );
		data = expandedData.get();
	}
	else
	{
		data = static_cast<const T *>( primitiveVariable.data.get() );
	}

	// Create attribute. Cycles will allocate a buffer based on `attributeElement` and the information
	// `attributes.geometry` contains.

	ccl::Attribute *attribute = attributes.add( ccl::ustring( name.c_str() ), typeDesc, attributeElement );

	// Sanity check the size of the buffer, so we don't run off the end when copying our data into it.
	// Note that we do allow the buffer to be _bigger_ than we expect, because Cycles reserves additional
	// space for its own usage. For instance, vertex attributes on subdivs reserve one extra element for
	// each non-quad face.

	const size_t allocatedSize = attribute->element_size( attributes.geometry, attributes.prim );
	if( dataSize( data ) > allocatedSize )
	{
		msg(
			Msg::Warning, "IECoreCyles::GeometryAlgo::convertPrimitiveVariable",
			fmt::format(
				"Primitive variable \"{}\" has size {} but Cycles allocated size {}.",
				name, dataSize( data ), allocatedSize
			)
		);
		return nullptr;
	}

	// Copy data into buffer.

	if constexpr( std::is_same_v<T, V3fVectorData> || std::is_same_v<T, Color3fVectorData> )
	{
		// Special case for arrays of `float3`, where each element actually contains 4 floats for alignment purposes.
		ccl::float3 *f3 = attribute->data_float3();
		for( const auto &v : data->readable() )
		{
			*f3++ = ccl::make_float3( v.x, v.y, v.z );
		}
	}
	else
	{
		// All other cases, (including int to float conversion) are a simple element-by-element copy.
		std::copy( data->baseReadable(), data->baseReadable() + data->baseSize(), (float *)attribute->data() );
	}

	return attribute;
}

// The only reason this class exists is to allow us to initialise the
// otherwise-protected `precision` member.
class VolumeLoader : public ccl::VDBImageLoader
{
	public :

		VolumeLoader( openvdb::GridBase::ConstPtr grid, const string &gridName, int precision_ )
			:	VDBImageLoader( grid, gridName )
		{
			precision = precision_;
		}

};

} // namespace

//////////////////////////////////////////////////////////////////////////
// Implementation of public API
//////////////////////////////////////////////////////////////////////////

namespace IECoreCycles
{

namespace GeometryAlgo
{

ccl::Geometry *convert( const IECore::Object *object, const std::string &nodeName, ccl::Scene *scene )
{
	const Registry &r = registry();
	Registry::const_iterator it = r.find( object->typeId() );
	if( it == r.end() )
	{
		return nullptr;
	}
	return it->second.converter( object, nodeName, scene );
}

ccl::Geometry *convert( const std::vector<const IECore::Object *> &samples, const std::vector<float> &times, const int frameIdx, const std::string &nodeName, ccl::Scene *scene )
{
	if( samples.empty() )
	{
		return nullptr;
	}

	const IECore::Object *firstSample = samples.front();
	const IECore::TypeId firstSampleTypeId = firstSample->typeId();
	for( std::vector<const IECore::Object *>::const_iterator it = samples.begin()+1, eIt = samples.end(); it != eIt; ++it )
	{
		if( (*it)->typeId() != firstSampleTypeId )
		{
			throw IECore::Exception( "Inconsistent object types." );
		}
	}

	const Registry &r = registry();
	Registry::const_iterator it = r.find( firstSampleTypeId );
	if( it == r.end() )
	{
		return nullptr;
	}
	if( it->second.motionConverter )
	{
		return it->second.motionConverter( samples, times, frameIdx, nodeName, scene );
	}
	else
	{
		return it->second.converter( samples.front(), nodeName, scene );
	}
}

void registerConverter( IECore::TypeId fromType, Converter converter, MotionConverter motionConverter )
{
	registry()[fromType] = { converter, motionConverter };
}

void convertPrimitiveVariable( const std::string &name, const IECoreScene::PrimitiveVariable &primitiveVariable, ccl::AttributeSet &attributes, ccl::AttributeElement attributeElement )
{
	ccl::Attribute *attr = nullptr;
	switch( primitiveVariable.data->typeId() )
	{
		// Simple int-based data. Cycles doesn't support int attributes, so we promote to the equivalent float types.

		case IntDataTypeId :
			attr = convertTypedPrimitiveVariable<IntData>( name, primitiveVariable, attributes, ccl::TypeFloat, attributeElement );
			break;
		case V2iDataTypeId :
			attr = convertTypedPrimitiveVariable<V2iData>( name, primitiveVariable, attributes, ccl::TypeFloat2, attributeElement );
			break;
		case V3iDataTypeId :
			attr = convertTypedPrimitiveVariable<V3iData>(
				name, primitiveVariable, attributes,
				typeFromGeometricDataInterpretation(
					static_cast<const V3iData *>( primitiveVariable.data.get() )->getInterpretation()
				),
				attributeElement
			);
			break;

		// Vectors of int-based data. Cycles doesn't support int attributes, so we promote to the equivalent float types.

		case IntVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<IntVectorData>( name, primitiveVariable, attributes, ccl::TypeFloat, attributeElement );
			break;
		case V2iVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<V2iVectorData>( name, primitiveVariable, attributes, ccl::TypeFloat2, attributeElement );
			break;
		case V3iVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<V3iVectorData>(
				name, primitiveVariable, attributes,
				typeFromGeometricDataInterpretation(
					static_cast<const V3iVectorData *>( primitiveVariable.data.get() )->getInterpretation()
				),
				attributeElement
			);
			break;

		// Simple float-based data.

		case FloatDataTypeId :
			attr = convertTypedPrimitiveVariable<FloatData>( name, primitiveVariable, attributes, ccl::TypeFloat, attributeElement );
			break;
		case V2fDataTypeId :
			attr = convertTypedPrimitiveVariable<V2fData>( name, primitiveVariable, attributes, ccl::TypeFloat2, attributeElement );
			break;
		case V3fDataTypeId :
			attr = convertTypedPrimitiveVariable<V3fData>(
				name, primitiveVariable, attributes,
				typeFromGeometricDataInterpretation(
					static_cast<const V3fData *>( primitiveVariable.data.get() )->getInterpretation()
				),
				attributeElement
			);
			break;
		case Color3fDataTypeId :
			attr = convertTypedPrimitiveVariable<Color3fData>( name, primitiveVariable, attributes, ccl::TypeColor, attributeElement );
			break;

		// Vectors of float-based data.

		case FloatVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<FloatVectorData>( name, primitiveVariable, attributes, ccl::TypeFloat, attributeElement );
			break;
		case V2fVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<V2fVectorData>( name, primitiveVariable, attributes, ccl::TypeFloat2, attributeElement );
			break;
		case V3fVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<V3fVectorData>(
				name, primitiveVariable, attributes,
				typeFromGeometricDataInterpretation(
					static_cast<const V3fVectorData *>( primitiveVariable.data.get() )->getInterpretation()
				),
				attributeElement
			);
			break;
		case Color3fVectorDataTypeId :
			attr = convertTypedPrimitiveVariable<Color3fVectorData>( name, primitiveVariable, attributes, ccl::TypeColor, attributeElement );
			break;
		default :
			msg(
				Msg::Warning, "IECoreCyles::GeometryAlgo::convertPrimitiveVariable",
				fmt::format(
					"Primitive variable \"{}\" has unsupported type \"{}\".",
					name, primitiveVariable.data->typeName()
				)
			);
	};

	if( !attr )
	{
		return;
	}

	// Tag as a standard attribute if possible. Note that we don't use `AttributeSet::add( AttributeStandard )`
	// because that crashes for certain combinations of geometry type and attribute. But some of those "crashy"
	// combinations are useful - see `RendererTest.testPointsWithNormals` for an example.
	//
	/// \todo Support more standard attributes here. Maybe then the geometry converters could
	/// use `convertPrimitiveVariable()` for most data, instead of having
	/// custom code paths for `P`, `uv` etc?

	if( name == "N" && attr->element == ccl::ATTR_ELEMENT_VERTEX && attr->type == ccl::TypeNormal )
	{
		attr->std = ccl::ATTR_STD_VERTEX_NORMAL;
	}
	else if( name == "N" && attr->element == ccl::ATTR_ELEMENT_FACE && attr->type == ccl::TypeNormal )
	{
		attr->std = ccl::ATTR_STD_FACE_NORMAL;
		attr->name = ccl::Attribute::standard_name( attr->std ); // Cycles calls this `Ng`.
	}
	else if( name == "uv" && attr->type == ccl::TypeFloat2 )
	{
		attr->std = ccl::ATTR_STD_UV;
	}
	else if( name == "uv.tangent_sign" && attr->element == ccl::ATTR_ELEMENT_CORNER && attr->type == ccl::TypeFloat )
	{
		attr->std = ccl::ATTR_STD_UV_TANGENT_SIGN;
	}
	else if( name == "uv.tangent" && attr->element == ccl::ATTR_ELEMENT_CORNER && attr->type == ccl::TypeVector )
	{
		attr->std = ccl::ATTR_STD_UV_TANGENT;
	}
}

void convertVoxelGrids( const IECoreVDB::VDBObject *vdbObject, ccl::Volume *volume, ccl::Scene *scene, int precision )
{
	for( const std::string& gridName : vdbObject->gridNames() )
	{
		openvdb::GridBase::ConstPtr grid = vdbObject->findGrid( gridName );
		ccl::AttributeStandard std = ccl::ATTR_STD_NONE;
		ccl::TypeDesc ctype = ccl::TypeUnknown;

		/// \todo Should we also be checking that grids have an appropriate type before
		/// labelling them with one of the standards?
		if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_DENSITY ) )
		{
			std = ccl::ATTR_STD_VOLUME_DENSITY;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_COLOR ) )
		{
			std = ccl::ATTR_STD_VOLUME_COLOR;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_FLAME ) )
		{
			std = ccl::ATTR_STD_VOLUME_FLAME;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_HEAT ) )
		{
			std = ccl::ATTR_STD_VOLUME_HEAT;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_TEMPERATURE ) )
		{
			std = ccl::ATTR_STD_VOLUME_TEMPERATURE;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_VELOCITY ) )
		{
			std = ccl::ATTR_STD_VOLUME_VELOCITY;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_VELOCITY_X ) )
		{
			std = ccl::ATTR_STD_VOLUME_VELOCITY_X;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_VELOCITY_Y ) )
		{
			std = ccl::ATTR_STD_VOLUME_VELOCITY_Y;
		}
		else if( ccl::ustring( gridName.c_str() ) == ccl::Attribute::standard_name( ccl::ATTR_STD_VOLUME_VELOCITY_Z ) )
		{
			std = ccl::ATTR_STD_VOLUME_VELOCITY_Z;
		}
		else
		{
			if( grid->isType<openvdb::BoolGrid>() )
			{
				ctype = ccl::TypeInt;
			}
			else if( grid->isType<openvdb::DoubleGrid>() )
			{
				ctype = ccl::TypeFloat;
			}
			else if( grid->isType<openvdb::FloatGrid>() )
			{
				ctype = ccl::TypeFloat;
			}
			else if( grid->isType<openvdb::Int32Grid>() )
			{
				ctype = ccl::TypeInt;
			}
			else if( grid->isType<openvdb::Int64Grid>() )
			{
				ctype = ccl::TypeInt;
			}
			else if( grid->isType<openvdb::Vec3DGrid>() )
			{
				ctype = ccl::TypeVector;
			}
			else if( grid->isType<openvdb::Vec3IGrid>() )
			{
				ctype = ccl::TypeVector;
			}
			else if( grid->isType<openvdb::Vec3SGrid>() )
			{
				ctype = ccl::TypeVector;
			}
			else
			{
				IECore::msg(
					IECore::Msg::Warning, "VolumeAlgo",
					fmt::format(
						"Ignoring grid \"{}\" with unsupported type \"{}\"",
						gridName, grid->type()
					)
				);
				continue;
			}
		}

		ccl::Attribute *attr = ( std != ccl::ATTR_STD_NONE ) ?
			volume->attributes.add( std ) :
			volume->attributes.add( ccl::ustring( gridName.c_str() ), ctype, ccl::ATTR_ELEMENT_VOXEL )
		;

		auto loader = std::make_unique<VolumeLoader>( grid, gridName, precision );
		ccl::ImageParams params;
		params.frame = 0.0f;

		std::scoped_lock lock( scene->mutex );
#if ( CYCLES_VERSION_MAJOR * 100 + CYCLES_VERSION_MINOR ) >= 404
		attr->data_voxel() = scene->image_manager->add_image( std::move( loader ), params, false );
#else
		attr->data_voxel() = scene->image_manager->add_image( loader.release(), params, false );
#endif
	}
}

} // namespace GeometryAlgo

} // namespace IECoreCycles
