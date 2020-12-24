//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2020, John Haddon. All rights reserved.
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

#include "GafferSceneUI/SceneDelegate.h"

#include "IECoreUSD/DataAlgo.h"

#include "IECoreScene/MeshPrimitive.h"

IECORE_PUSH_DEFAULT_VISIBILITY
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/matrix4f.h"
IECORE_POP_DEFAULT_VISIBILITY

#include <iostream> // REMOVE ME

using namespace std;
using namespace pxr;
using namespace IECore;
using namespace IECoreUSD;
using namespace GafferScene;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Internal utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

ScenePlug::ScenePath fromUSDWithoutPrefix( const pxr::SdfPath &path, size_t prefixSize )
{
	size_t i = path.GetPathElementCount() - prefixSize;
	ScenePlug::ScenePath result( i );
	pxr::SdfPath p = path;
	while( i )
	{
		result[--i] = p.GetElementString();
		p = p.GetParentPath();
	}
	return result;
}

ScenePlug::ScenePath fromUSD( const pxr::SdfPath &path )
{
	return fromUSDWithoutPrefix( path, 0 );
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// SceneDelegate
//////////////////////////////////////////////////////////////////////////

SceneDelegate::SceneDelegate( const GafferScene::ConstScenePlugPtr &scene, pxr::HdRenderIndex *parentIndex, const pxr::SdfPath &delegateID )
	:	HdSceneDelegate( parentIndex, delegateID ), m_scene( scene )
{
	GetRenderIndex().InsertRprim( HdPrimTypeTokens->mesh, this, SdfPath( "/plane" ) );
}

SceneDelegate::~SceneDelegate()
{

}

// void SceneDelegate::setScene( const GafferScene::ConstScenePlugPtr &scene )
// {
// 	m_scene = scene;
// }

// const GafferScene::ScenePlug *SceneDelegate::getScene() const
// {
// 	return m_scene.get();
// }

pxr::VtValue SceneDelegate::Get( const pxr::SdfPath &id, const pxr::TfToken &key )
{
	std::cerr << "Get " << id << " " << key << std::endl;
 
    // // tasks
    // _ValueCache *vcache = TfMapLookupPtr(_valueCacheMap, id);
    // VtValue ret;
    // if (vcache && TfMapLookup(*vcache, key, &ret)) {
    //     return ret;
    // }

    // // prims
    // if (key == HdTokens->points) {
    //     if(_meshes.find(id) != _meshes.end()) {
    //         return VtValue(_meshes[id].points);
    //     }
    // } else if (key == HdTokens->displayColor) {
    //     if(_meshes.find(id) != _meshes.end()) {
    //         return VtValue(_meshes[id].color);
    //     }
    // } else if (key == HdTokens->displayOpacity) {
    //     if(_meshes.find(id) != _meshes.end()) {
    //         return VtValue(_meshes[id].opacity);
    //     }
    // } else if (key == HdInstancerTokens->scale) {
    //     if (_instancers.find(id) != _instancers.end()) {
    //         return VtValue(_instancers[id].scale);
    //     }
    // } else if (key == HdInstancerTokens->rotate) {
    //     if (_instancers.find(id) != _instancers.end()) {
    //         return VtValue(_instancers[id].rotate);
    //     }
    // } else if (key == HdInstancerTokens->translate) {
    //     if (_instancers.find(id) != _instancers.end()) {
    //         return VtValue(_instancers[id].translate);
    //     }
    // }
    return VtValue();
}

pxr::HdReprSelector SceneDelegate::GetReprSelector( const pxr::SdfPath &id )
{
	std::cerr << "GetReprSelector " << id << std::endl;
	return HdSceneDelegate::GetReprSelector( id );
}

pxr::HdMeshTopology SceneDelegate::GetMeshTopology( const pxr::SdfPath &id )
{
	std::cerr << "get mesh topology " << id << std::endl;

	ConstObjectPtr object = m_scene->object( fromUSD( id ) );
	if( auto mesh = runTimeCast<const IECoreScene::MeshPrimitive>( object.get() ) )
	{
		auto &verticesPerFace = mesh->verticesPerFace()->readable();
		auto &vertexIds = mesh->vertexIds()->readable();
		std::cerr << "    got faces " << verticesPerFace.size() << std::endl;
		return HdMeshTopology(
			PxOsdOpenSubdivTokens->catmullClark,
			HdTokens->rightHanded,
			VtIntArray( verticesPerFace.begin(), verticesPerFace.end() ),
			VtIntArray( vertexIds.begin(), vertexIds.end() ),
			/* refineLevel = */ 0
		);	
	}

	std::cerr << "    no mesh" << std::endl;
	return HdMeshTopology();
}

pxr::GfRange3d SceneDelegate::GetExtent( const pxr::SdfPath &id )
{
	// Would it be better to query this from the object?
	const Imath::Box3f bound = m_scene->bound( fromUSD( id ) );
	std::cerr << "GetExtent " << id << " " << bound.min << " - " << bound.max << std::endl;
	return GfRange3d(
		IECoreUSD::DataAlgo::toUSD( bound.min ),
		IECoreUSD::DataAlgo::toUSD( bound.max )
	);
}

pxr::GfMatrix4d SceneDelegate::GetTransform( const pxr::SdfPath &id )
{
	const Imath::M44f matrix = m_scene->fullTransform( fromUSD( id ) );
	auto r = GfMatrix4d( IECoreUSD::DataAlgo::toUSD( matrix ) );
	std::cerr << "GetTransform " << id << " " << r << std::endl;
	return r;
}

pxr::SdfPath SceneDelegate::GetMaterialId( const pxr::SdfPath &rprimId )
{
	std::cerr << "GetMaterialId " << rprimId << std::endl;
	return SdfPath(); 
}

pxr::HdPrimvarDescriptorVector SceneDelegate::GetPrimvarDescriptors( const pxr::SdfPath &id, pxr::HdInterpolation interpolation )
{
	std::cerr << "GetPrimvarDescriptors " << id << " " << interpolation << std::endl;
	HdPrimvarDescriptorVector result;
	if( interpolation == HdInterpolationVertex )
	{
		std::cerr << "  adding points" << std::endl;
		result.emplace_back( HdTokens->points, interpolation, HdPrimvarRoleTokens->point );
	}
	return result;
}
