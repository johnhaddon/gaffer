//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

#include "GafferSceneUI/unitTestDelegate.h"

#include "IECoreUSD/DataAlgo.h"

#include "pxr/base/gf/frustum.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/sprim.h"
#include "pxr/imaging/hd/texture.h"
#include "pxr/imaging/hd/textureResource.h"

#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hdSt/drawTarget.h"
#include "pxr/imaging/hdSt/light.h"

#include "pxr/imaging/hdx/drawTargetTask.h"
#include "pxr/imaging/hdx/pickTask.h"
#include "pxr/imaging/hdx/renderTask.h"
#include "pxr/imaging/hdx/selectionTask.h"
#include "pxr/imaging/hdx/simpleLightTask.h"
#include "pxr/imaging/hdx/shadowTask.h"
#include "pxr/imaging/hdx/shadowMatrixComputation.h"

#include "pxr/imaging/pxOsd/tokens.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

// ------------------------------------------------------------------------
Hdx_UnitTestDelegate::Hdx_UnitTestDelegate(HdRenderIndex *index)
    : HdSceneDelegate(index, SdfPath::AbsoluteRootPath())
    , _refineLevel(0)
{
}

//------------------------------------------------------------------------------
//                                  PRIMS
//------------------------------------------------------------------------------
void
Hdx_UnitTestDelegate::AddMesh(SdfPath const &id,
                             GfMatrix4d const &transform,
                             VtVec3fArray const &points,
                             VtIntArray const &numVerts,
                             VtIntArray const &verts,
                             bool guide,
                             SdfPath const &instancerId,
                             TfToken const &scheme,
                             TfToken const &orientation,
                             bool doubleSided)
{
    HdRenderIndex& index = GetRenderIndex();
    index.InsertRprim(HdPrimTypeTokens->mesh, this, id, instancerId);

    _meshes[id] = _Mesh(scheme, orientation, transform,
                        points, numVerts, verts, PxOsdSubdivTags(),
                        /*color=*/VtValue(GfVec3f(1, 1, 0)),
                        /*colorInterpolation=*/HdInterpolationConstant,
                        /*opacity=*/VtValue(1.0f),
                        HdInterpolationConstant,
                        guide, doubleSided);
}

void
Hdx_UnitTestDelegate::AddMesh(SdfPath const &id,
                             GfMatrix4d const &transform,
                             VtVec3fArray const &points,
                             VtIntArray const &numVerts,
                             VtIntArray const &verts,
                             PxOsdSubdivTags const &subdivTags,
                             VtValue const &color,
                             HdInterpolation colorInterpolation,
                             VtValue const &opacity,
                             HdInterpolation opacityInterpolation,
                             bool guide,
                             SdfPath const &instancerId,
                             TfToken const &scheme,
                             TfToken const &orientation,
                             bool doubleSided)
{
    HdRenderIndex& index = GetRenderIndex();
    index.InsertRprim(HdPrimTypeTokens->mesh, this, id, instancerId);

    _meshes[id] = _Mesh(scheme, orientation, transform,
                        points, numVerts, verts, subdivTags,
                        color, colorInterpolation, opacity,
                        opacityInterpolation, guide, doubleSided);
}

void
Hdx_UnitTestDelegate::AddCube(SdfPath const &id, GfMatrix4d const &transform,
                              bool guide, SdfPath const &instancerId,
                              TfToken const &scheme, VtValue const &color,
                              HdInterpolation colorInterpolation,
                              VtValue const &opacity,
                              HdInterpolation opacityInterpolation)
{
    GfVec3f points[] = {
        GfVec3f( 1.0f, 1.0f, 1.0f ),
        GfVec3f(-1.0f, 1.0f, 1.0f ),
        GfVec3f(-1.0f,-1.0f, 1.0f ),
        GfVec3f( 1.0f,-1.0f, 1.0f ),
        GfVec3f(-1.0f,-1.0f,-1.0f ),
        GfVec3f(-1.0f, 1.0f,-1.0f ),
        GfVec3f( 1.0f, 1.0f,-1.0f ),
        GfVec3f( 1.0f,-1.0f,-1.0f ),
    };

    if (scheme == PxOsdOpenSubdivTokens->loop) {
        int numVerts[] = { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
        int verts[] = {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            0, 6, 5, 0, 5, 1,
            4, 7, 3, 4, 3, 2,
            0, 3, 7, 0, 7, 6,
            4, 2, 1, 4, 1, 5,
        };
        AddMesh(
            id,
            transform,
            _BuildArray(points, sizeof(points)/sizeof(points[0])),
            _BuildArray(numVerts, sizeof(numVerts)/sizeof(numVerts[0])),
            _BuildArray(verts, sizeof(verts)/sizeof(verts[0])),
            PxOsdSubdivTags(),
            color,
            colorInterpolation,
            opacity,
            opacityInterpolation,
            guide,
            instancerId,
            scheme);
    } else {
        int numVerts[] = { 4, 4, 4, 4, 4, 4 };
        int verts[] = {
            0, 1, 2, 3,
            4, 5, 6, 7,
            0, 6, 5, 1,
            4, 7, 3, 2,
            0, 3, 7, 6,
            4, 2, 1, 5,
        };
        AddMesh(
            id,
            transform,
            _BuildArray(points, sizeof(points)/sizeof(points[0])),
            _BuildArray(numVerts, sizeof(numVerts)/sizeof(numVerts[0])),
            _BuildArray(verts, sizeof(verts)/sizeof(verts[0])),
            PxOsdSubdivTags(),
            color,
            colorInterpolation,
            opacity,
            opacityInterpolation,
            guide,
            instancerId,
            scheme);
    }
}

GfRange3d
Hdx_UnitTestDelegate::GetExtent(SdfPath const & id)
{
    std::cerr << "GetExtent " << id << std::endl;

    GfRange3d range;
    VtVec3fArray points;
    if(_meshes.find(id) != _meshes.end()) {
        points = _meshes[id].points;
    }
    TF_FOR_ALL(it, points) {
        range.UnionWith(*it);
    }
    return range;
}

GfMatrix4d
Hdx_UnitTestDelegate::GetTransform(SdfPath const & id)
{
    std::cerr << "GetTransform " << id << std::endl;

    if(_meshes.find(id) != _meshes.end()) {
        return _meshes[id].transform;
    }
    return GfMatrix4d(1);
}

bool
Hdx_UnitTestDelegate::GetVisible(SdfPath const& id)
{
     std::cerr << "GetVisible " << id << std::endl;
   return true;
}

HdMeshTopology
Hdx_UnitTestDelegate::GetMeshTopology(SdfPath const& id)
{
     std::cerr << "GetMeshTopology " << id << std::endl;
    HdMeshTopology topology;
    const _Mesh &mesh = _meshes[id];

    return HdMeshTopology(PxOsdOpenSubdivTokens->catmullClark,
                          HdTokens->rightHanded,
                          mesh.numVerts,
                          mesh.verts);
}

VtValue
Hdx_UnitTestDelegate::Get(SdfPath const& id, TfToken const& key)
{
     std::cerr << "Get " << id << " " << key << std::endl;
   
    // prims
    if (key == HdTokens->points) {
        if(_meshes.find(id) != _meshes.end()) {
            return VtValue(_meshes[id].points);
        }
    }
    
    return VtValue();
}

/*virtual*/
VtIntArray
Hdx_UnitTestDelegate::GetInstanceIndices(SdfPath const& instancerId,
                                        SdfPath const& prototypeId)
{

     std::cerr << "GetInstanceIndices " << instancerId << " " << prototypeId << std::endl;


    HD_TRACE_FUNCTION();
    VtIntArray indices(0);
    //
    // XXX: this is very naive implementation for unit test.
    //
    //   transpose prototypeIndices/instances to instanceIndices/prototype
    if (_Instancer *instancer = TfMapLookupPtr(_instancers, instancerId)) {
        size_t prototypeIndex = 0;
        for (; prototypeIndex < instancer->prototypes.size(); ++prototypeIndex) {
            if (instancer->prototypes[prototypeIndex] == prototypeId) break;
        }
        if (prototypeIndex == instancer->prototypes.size()) return indices;

        // XXX use const_ptr
        for (size_t i = 0; i < instancer->prototypeIndices.size(); ++i) {
            if (static_cast<size_t>(instancer->prototypeIndices[i]) == prototypeIndex) {
                indices.push_back(i);
            }
        }
    }
    return indices;
}

/*virtual*/
GfMatrix4d
Hdx_UnitTestDelegate::GetInstancerTransform(SdfPath const& instancerId)
{
      std::cerr << "GetInstancerTransform " << instancerId << " " << std::endl;

   HD_TRACE_FUNCTION();
    if (_Instancer *instancer = TfMapLookupPtr(_instancers, instancerId)) {
        return GfMatrix4d(instancer->rootTransform);
    }
    return GfMatrix4d(1);
}

/*virtual*/
HdDisplayStyle
Hdx_UnitTestDelegate::GetDisplayStyle(SdfPath const& id)
{
       std::cerr << "GetDisplayStyle " << id << " " << std::endl;
   if (_refineLevels.find(id) != _refineLevels.end()) {
        return HdDisplayStyle(_refineLevels[id]);
    }
    return HdDisplayStyle(_refineLevel);
}

HdPrimvarDescriptorVector
Hdx_UnitTestDelegate::GetPrimvarDescriptors(SdfPath const& id,
                                            HdInterpolation interpolation)
{
       std::cerr << "GetPrimvarDescriptors " << id << " " << interpolation << std::endl;
    HdPrimvarDescriptorVector primvars;
    if (interpolation == HdInterpolationVertex) {
        primvars.emplace_back(HdTokens->points, interpolation,
                              HdPrimvarRoleTokens->point);
    }
    if(_meshes.find(id) != _meshes.end()) {
        if (_meshes[id].colorInterpolation == interpolation) {
            primvars.emplace_back(HdTokens->displayColor, interpolation,
                                  HdPrimvarRoleTokens->color);
        }
        if (_meshes[id].opacityInterpolation == interpolation) {
            primvars.emplace_back(HdTokens->displayOpacity, interpolation);
        }
    }
    if (interpolation == HdInterpolationInstance &&
        _instancers.find(id) != _instancers.end()) {
        primvars.emplace_back(HdInstancerTokens->scale, interpolation);
        primvars.emplace_back(HdInstancerTokens->rotate, interpolation);
        primvars.emplace_back(HdInstancerTokens->translate, interpolation);
    }
    return primvars;
}

/*virtual*/
SdfPath
Hdx_UnitTestDelegate::GetMaterialId(SdfPath const &rprimId)
{
    SdfPath materialId;
    TfMapLookup(_materialBindings, rprimId, &materialId);
       std::cerr << "GetMaterialId " << rprimId << " " << materialId << std::endl;
    return materialId;
}

/*virtual*/
VtValue
Hdx_UnitTestDelegate::GetMaterialResource(SdfPath const &materialId)
{
       std::cerr << "GetMaterialResource " << materialId << std::endl;
    if (VtValue *material = TfMapLookupPtr(_materials, materialId)){
        return *material;
    }
    return VtValue();
}

/*virtual*/
VtValue
Hdx_UnitTestDelegate::GetCameraParamValue(SdfPath const &cameraId,
                                          TfToken const &paramName)
{
    std::cerr << "GetCameraParamValue " << cameraId << " " << paramName << std::endl;

    _ValueCache *vcache = TfMapLookupPtr(_valueCacheMap, cameraId);
    VtValue ret;
    if (vcache && TfMapLookup(*vcache, paramName, &ret)) {
        return ret;
    }

    return VtValue();
}

// HdRenderBufferDescriptor
// Hdx_UnitTestDelegate::GetRenderBufferDescriptor(SdfPath const &id)
// {
//     _ValueCache *vcache = TfMapLookupPtr(_valueCacheMap, id);
//     if (!vcache) {
//         return HdRenderBufferDescriptor();
//     }

//     VtValue ret;
//     if (!TfMapLookup(*vcache, _tokens->renderBufferDescriptor, &ret)) {
//         return HdRenderBufferDescriptor();
//     }

//     if (!ret.IsHolding<HdRenderBufferDescriptor>()) {
//         return HdRenderBufferDescriptor();
//     }

//     return ret.UncheckedGet<HdRenderBufferDescriptor>();
// }

// HdTextureResourceSharedPtr
// Hdx_UnitTestDelegate::GetTextureResource(SdfPath const& textureId)
// {
//     return HdTextureResourceSharedPtr();
// }

// HdTextureResource::ID
// Hdx_UnitTestDelegate::GetTextureResourceID(SdfPath const& textureId)
// {
//     return SdfPath::Hash()(textureId);
// }

// TfTokenVector
// Hdx_UnitTestDelegate::GetTaskRenderTags(SdfPath const& taskId)
// {
//     const auto it1 = _valueCacheMap.find(taskId);
//     if (it1 == _valueCacheMap.end()) {
//         return {};
//     }

//     const auto it2 = it1->second.find(HdTokens->renderTags);
//     if (it2 == it1->second.end()) {
//         return {};
//     }

//     return it2->second.Get<TfTokenVector>();
// }


PXR_NAMESPACE_CLOSE_SCOPE

