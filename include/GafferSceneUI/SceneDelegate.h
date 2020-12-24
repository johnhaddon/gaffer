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

#ifndef GAFFERSCENEUI_SCENEDELEGATE_H
#define GAFFERSCENEUI_SCENEDELEGATE_H

#include "GafferScene/ScenePlug.h"

#include "pxr/imaging/hd/sceneDelegate.h"

namespace GafferSceneUI
{

/// \todo Make completely private? Move to GafferScene? Move to GafferUSD?
class SceneDelegate : public pxr::HdSceneDelegate
{

	public :

		SceneDelegate( const GafferScene::ConstScenePlugPtr &scene, pxr::HdRenderIndex *parentIndex, const pxr::SdfPath &delegateID = pxr::SdfPath::AbsoluteRootPath() );
		~SceneDelegate() override;

		// Our methods
		// ===========

		//void setScene( const GafferScene::ConstScenePlugPtr &scene );
		//const GafferScene::ScenePlug *getScene() const;

		// HdSceneDelegate overrides
		// =========================

		/// Synchronizes the delegate state for the given request vector.
		//virtual void Sync(HdSyncRequestVector* request);

// /// Opportunity for the delegate to clean itself up after
// /// performing parrellel work during sync phase
// virtual void PostSyncCleanup();

//	 // -----------------------------------------------------------------------//
//	 /// \name Options
//	 // -----------------------------------------------------------------------//

//	 /// Returns true if the named option is enabled by the delegate.
	
//	 virtual bool IsEnabled(TfToken const& option) const;


//	 // -----------------------------------------------------------------------//
//	 /// \name Rprim Aspects
//	 // -----------------------------------------------------------------------//

//	 /// Gets the topological mesh data for a given prim.
	
		pxr::HdMeshTopology GetMeshTopology( const pxr::SdfPath &id ) override;

//	 /// Gets the topological curve data for a given prim.
	
//	 virtual HdBasisCurvesTopology GetBasisCurvesTopology(SdfPath const& id);

//	 /// Gets the subdivision surface tags (sharpness, holes, etc).
	
//	 virtual PxOsdSubdivTags GetSubdivTags(SdfPath const& id);


//	 /// Gets the axis aligned bounds of a prim.
//	 /// The returned bounds are in the local space of the prim
//	 /// (transform is yet to be applied) and should contain the
//	 /// bounds of any child prims.
//	 ///
//	 /// The returned bounds does not include any displacement that
//	 /// might occur as the result of running shaders on the prim.
	
		pxr::GfRange3d GetExtent( const pxr::SdfPath &id ) override;

//	 /// Returns the object space transform, including all parent transforms.
	
		pxr::GfMatrix4d GetTransform( const pxr::SdfPath &id ) override;

//	 /// Returns the authored visible state of the prim.
	
//	 virtual bool GetVisible(SdfPath const & id);

//	 /// Returns the doubleSided state for the given prim.
	
//	 virtual bool GetDoubleSided(SdfPath const & id);

//	 /// Returns the cullstyle for the given prim.
	
//	 virtual HdCullStyle GetCullStyle(SdfPath const &id);

//	 /// Returns the shading style for the given prim.
	
//	 virtual VtValue GetShadingStyle(SdfPath const &id);

//	 /// Returns the refinement level for the given prim in the range [0,8].
//	 ///
//	 /// The refinement level indicates how many iterations to apply when
//	 /// subdividing subdivision surfaces or other refinable primitives.
	
//	 virtual HdDisplayStyle GetDisplayStyle(SdfPath const& id);

//	 /// Returns a named value.
	
	pxr::VtValue Get( const pxr::SdfPath &id, const pxr::TfToken &key ) override;

//	 /// Returns the authored repr (if any) for the given prim.
	
	pxr::HdReprSelector GetReprSelector( const pxr::SdfPath &id ) override;

//	 /// Returns the render tag that will be used to bucket prims during
//	 /// render pass bucketing.
	
//	 virtual TfToken GetRenderTag(SdfPath const& id);

//	 /// Returns the prim categories.
	
//	 virtual VtArray<TfToken> GetCategories(SdfPath const& id);

//	 /// Returns the categories for all instances in the instancer.
	
//	 virtual std::vector<VtArray<TfToken>>
//	 GetInstanceCategories(SdfPath const &instancerId);

//	 /// Returns the coordinate system bindings, or a nullptr if none are bound.
	
//	 virtual HdIdVectorSharedPtr GetCoordSysBindings(SdfPath const& id);

//	 // -----------------------------------------------------------------------//
//	 /// \name Motion samples
//	 // -----------------------------------------------------------------------//

//	 /// Store up to \a maxSampleCount transform samples in \a *sampleValues.
//	 /// Returns the union of the authored samples and the boundaries 
//	 /// of the current camera shutter interval. If this number is greater
//	 /// than maxSampleCount, you might want to call this function again 
//	 /// to get all the authored data.
//	 /// Sample times are relative to the scene delegate's current time.
//	 /// \see GetTransform()
	
//	 virtual size_t
//	 SampleTransform(SdfPath const & id, 
//	 size_t maxSampleCount,
//	 float *sampleTimes, 
//	 GfMatrix4d *sampleValues);

//	 /// Convenience form of SampleTransform() that takes an HdTimeSampleArray.
//	 /// This function returns the union of the authored transform samples 
//	 /// and the boundaries of the current camera shutter interval.
//	 template <unsigned int CAPACITY>
//	 void 
//	 SampleTransform(SdfPath const & id,
//	 HdTimeSampleArray<GfMatrix4d, CAPACITY> *sa) {
//	 size_t authoredSamples = 
//	 SampleTransform(id, CAPACITY, sa->times.data(), sa->values.data());
//	 if (authoredSamples > CAPACITY) {
//	 sa->Resize(authoredSamples);
//	 size_t authoredSamplesSecondAttempt = 
//	 SampleTransform(
//	 id, 
//	 authoredSamples, 
//	 sa->times.data(), 
//	 sa->values.data());
//	 // Number of samples should be consisntent through multiple
//	 // invokations of the sampling function.
//	 TF_VERIFY(authoredSamples == authoredSamplesSecondAttempt);
//	 }
//	 sa->count = authoredSamples;
//	 }

//	 /// Store up to \a maxSampleCount transform samples in \a *sampleValues.
//	 /// Returns the union of the authored samples and the boundaries 
//	 /// of the current camera shutter interval. If this number is greater
//	 /// than maxSampleCount, you might want to call this function again 
//	 /// to get all the authored data.
//	 /// Sample times are relative to the scene delegate's current time.
//	 /// \see GetInstancerTransform()
	
//	 virtual size_t
//	 SampleInstancerTransform(SdfPath const &instancerId,
//	 size_t maxSampleCount, 
//	 float *sampleTimes,
//	 GfMatrix4d *sampleValues);

//	 /// Convenience form of SampleInstancerTransform()
//	 /// that takes an HdTimeSampleArray.
//	 /// This function returns the union of the authored samples 
//	 /// and the boundaries of the current camera shutter interval.
//	 template <unsigned int CAPACITY>
//	 void
//	 SampleInstancerTransform(SdfPath const &instancerId,
//	 HdTimeSampleArray<GfMatrix4d, CAPACITY> *sa) {
//	 size_t authoredSamples = 
//	 SampleInstancerTransform(
//	 instancerId, 
//	 CAPACITY, 
//	 sa->times.data(), 
//	 sa->values.data());
//	 if (authoredSamples > CAPACITY) {
//	 sa->Resize(authoredSamples);
//	 size_t authoredSamplesSecondAttempt = 
//	 SampleInstancerTransform(
//	 instancerId, 
//	 authoredSamples, 
//	 sa->times.data(), 
//	 sa->values.data());
//	 // Number of samples should be consisntent through multiple
//	 // invokations of the sampling function.
//	 TF_VERIFY(authoredSamples == authoredSamplesSecondAttempt);
//	 }
//	 sa->count = authoredSamples;
//	 }

//	 /// Store up to \a maxSampleCount primvar samples in \a *samplesValues.
//	 /// Returns the union of the authored samples and the boundaries 
//	 /// of the current camera shutter interval. If this number is greater
//	 /// than maxSampleCount, you might want to call this function again 
//	 /// to get all the authored data.
//	 ///
//	 /// Sample values that are array-valued will have a size described
//	 /// by the HdPrimvarDescriptor as applied to the toplogy.
//	 ///
//	 /// For example, this means that a mesh that is fracturing over time
//	 /// will return samples with the same number of points; the number
//	 /// of points will change as the scene delegate is resynchronzied
//	 /// to represent the scene at a time with different topology.
//	 ///
//	 /// Sample times are relative to the scene delegate's current time.
//	 ///
//	 /// \see Get()
	
//	 virtual size_t
//	 SamplePrimvar(SdfPath const& id, 
//	 TfToken const& key,
//	 size_t maxSampleCount, 
//	 float *sampleTimes, 
//	 VtValue *sampleValues);

//	 /// Convenience form of SamplePrimvar() that takes an HdTimeSampleArray.
//	 /// This function returns the union of the authored samples 
//	 /// and the boundaries of the current camera shutter interval.
//	 template <unsigned int CAPACITY>
//	 void 
//	 SamplePrimvar(SdfPath const &id, 
//	 TfToken const& key,
//	 HdTimeSampleArray<VtValue, CAPACITY> *sa) {
//	 size_t authoredSamples = 
//	 SamplePrimvar(
//	 id, 
//	 key, 
//	 CAPACITY, 
//	 sa->times.data(), 
//	 sa->values.data());
//	 if (authoredSamples > CAPACITY) {
//	 sa->Resize(authoredSamples);
//	 size_t authoredSamplesSecondAttempt = 
//	 SamplePrimvar(
//	 id, 
//	 key, 
//	 authoredSamples, 
//	 sa->times.data(), 
//	 sa->values.data());
//	 // Number of samples should be consisntent through multiple
//	 // invokations of the sampling function.
//	 TF_VERIFY(authoredSamples == authoredSamplesSecondAttempt);
//	 }
//	 sa->count = authoredSamples;
//	 }

//	 // -----------------------------------------------------------------------//
//	 /// \name Instancer prototypes
//	 // -----------------------------------------------------------------------//

//	 /// Gets the extracted indices array of the prototype id used in the
//	 /// instancer.
//	 ///
//	 /// example
//	 ///  instances:  0, 1, 2, 3, 4, 5
//	 ///  protoypes:  A, B, A, A, B, C
//	 ///
//	 ///	GetInstanceIndices(A) : [0, 2, 3]
//	 ///	GetInstanceIndices(B) : [1, 4]
//	 ///	GetInstanceIndices(C) : [5]
//	 ///	GetInstanceIndices(D) : []
//	 ///
	
//	 virtual VtIntArray GetInstanceIndices(SdfPath const &instancerId,
//	 SdfPath const &prototypeId);

//	 /// Returns the instancer transform.
	
//	 virtual GfMatrix4d GetInstancerTransform(SdfPath const &instancerId);

//	 // -----------------------------------------------------------------------//
//	 /// \name Path Translation
//	 // -----------------------------------------------------------------------//

//	 /// Returns the scene address of the prim corresponding to the given
//	 /// rprim/instance index. This is designed to give paths in scene namespace,
//	 /// rather than hydra namespace, so it always strips the delegate ID.
	
//	 virtual SdfPath GetScenePrimPath(SdfPath const& rprimId,
//	 int instanceIndex,
//	 HdInstancerContext *instancerContext = nullptr);

//	 // -----------------------------------------------------------------------//
//	 /// \name Material Aspects
//	 // -----------------------------------------------------------------------//

//	 /// Returns the material ID bound to the rprim \p rprimId.
	
		pxr::SdfPath GetMaterialId( const pxr::SdfPath &rprimId ) override;

//	 // Returns a material resource which contains the information 
//	 // needed to create a material.
	 
//	 virtual VtValue GetMaterialResource(SdfPath const &materialId);

//	 // -----------------------------------------------------------------------//
//	 /// \name Texture Aspects
//	 // -----------------------------------------------------------------------//

//	 ///
//	 /// \deprecated It is now the responsibility of the render delegate to load
//	 /// the texture (using the file path authored on the texture node in a
//	 /// material network).
//	 ///
//	 /// Returns the texture resource ID for a given texture ID.
	
//	 virtual HdTextureResource::ID GetTextureResourceID(SdfPath const& textureId);

//	 ///
//	 /// \deprecated It is now the responsibility of the render delegate to load
//	 /// the texture (using the file path authored on the texture node in a
//	 /// material network).
//	 ///
//	 /// Returns the texture resource for a given texture ID.
	
//	 virtual HdTextureResourceSharedPtr GetTextureResource(SdfPath const& textureId);

//	 // -----------------------------------------------------------------------//
//	 /// \name Renderbuffer Aspects
//	 // -----------------------------------------------------------------------//

//	 /// Returns the allocation descriptor for a given render buffer prim.
	
//	 virtual HdRenderBufferDescriptor GetRenderBufferDescriptor(SdfPath const& id);

//	 // -----------------------------------------------------------------------//
//	 /// \name Light Aspects
//	 // -----------------------------------------------------------------------//

//	 // Returns a single value for a given light and parameter.
	
//	 virtual VtValue GetLightParamValue(SdfPath const &id, 
//	 TfToken const &paramName);

//	 // -----------------------------------------------------------------------//
//	 /// \name Camera Aspects
//	 // -----------------------------------------------------------------------//

//	 /// Returns a single value for a given camera and parameter.
//	 /// See HdCameraTokens for the list of paramters.
	
		pxr::VtValue GetCameraParamValue( const pxr::SdfPath &cameraId, const pxr::TfToken &paramName ) override;

//	 // -----------------------------------------------------------------------//
//	 /// \name Volume Aspects
//	 // -----------------------------------------------------------------------//
	
//	 virtual HdVolumeFieldDescriptorVector
//	 GetVolumeFieldDescriptors(SdfPath const &volumeId);

//	 // -----------------------------------------------------------------------//
//	 /// \name Primitive Variables
//	 // -----------------------------------------------------------------------//

//	 /// Returns descriptors for all primvars of the given interpolation type.
	
		pxr::HdPrimvarDescriptorVector GetPrimvarDescriptors( const pxr::SdfPath &id, pxr::HdInterpolation interpolation ) override;

//	 // -----------------------------------------------------------------------//
//	 /// \name Task Aspects
//	 // -----------------------------------------------------------------------//
	
//	 virtual TfTokenVector GetTaskRenderTags(SdfPath const& taskId);

	private :
	
		GafferScene::ConstScenePlugPtr m_scene;

};

} // namespace GafferSceneUI

#endif // GAFFERSCENEUI_SCENEDELEGATE_H