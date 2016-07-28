//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2014, Image Engine Design Inc. All rights reserved.
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

#include "tbb/spin_mutex.h"
#include "tbb/task.h"
#include "tbb/parallel_for.h"

#include "boost/algorithm/string/predicate.hpp"

#include "IECore/MatrixMotionTransform.h"
#include "IECore/Camera.h"
#include "IECore/CoordinateSystem.h"
#include "IECore/ClippingPlane.h"
#include "IECore/NullObject.h"
#include "IECore/VisibleRenderable.h"

#include "Gaffer/Context.h"

#include "GafferScene/SceneAlgo.h"
#include "GafferScene/Filter.h"
#include "GafferScene/ScenePlug.h"
#include "GafferScene/PathMatcher.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

bool GafferScene::exists( const ScenePlug *scene, const ScenePlug::ScenePath &path )
{
	ContextPtr context = new Context( *Context::current(), Context::Borrowed );
	Context::Scope scopedContext( context.get() );

	ScenePlug::ScenePath p; p.reserve( path.size() );
	for( ScenePlug::ScenePath::const_iterator it = path.begin(), eIt = path.end(); it != eIt; ++it )
	{
		context->set( ScenePlug::scenePathContextName, p );
		ConstInternedStringVectorDataPtr childNamesData = scene->childNamesPlug()->getValue();
		const vector<InternedString> &childNames = childNamesData->readable();
		if( find( childNames.begin(), childNames.end(), *it ) == childNames.end() )
		{
			return false;
		}
		p.push_back( *it );
	}

	return true;
}

bool GafferScene::visible( const ScenePlug *scene, const ScenePlug::ScenePath &path )
{
	ContextPtr context = new Context( *Context::current(), Context::Borrowed );
	Context::Scope scopedContext( context.get() );

	ScenePlug::ScenePath p; p.reserve( path.size() );
	for( ScenePlug::ScenePath::const_iterator it = path.begin(), eIt = path.end(); it != eIt; ++it )
	{
		p.push_back( *it );
		context->set( ScenePlug::scenePathContextName, p );

		ConstCompoundObjectPtr attributes = scene->attributesPlug()->getValue();
		const BoolData *visibilityData = attributes->member<BoolData>( "scene:visible" );
		if( visibilityData && !visibilityData->readable() )
		{
			return false;
		}
	}

	return true;
}

namespace
{

struct ThreadablePathAccumulator
{
	ThreadablePathAccumulator( GafferScene::PathMatcher &result): m_result( result ){}

	bool operator()( const GafferScene::ScenePlug *scene, const GafferScene::ScenePlug::ScenePath &path )
	{
		tbb::spin_mutex::scoped_lock lock( m_mutex );
		m_result.addPath( path );
		return true;
	}

	tbb::spin_mutex m_mutex;
	GafferScene::PathMatcher &m_result;

};

} // namespace

void GafferScene::matchingPaths( const Filter *filter, const ScenePlug *scene, PathMatcher &paths )
{
	matchingPaths( filter->outPlug(), scene, paths );
}

void GafferScene::matchingPaths( const Gaffer::IntPlug *filterPlug, const ScenePlug *scene, PathMatcher &paths )
{
	ThreadablePathAccumulator f( paths );
	GafferScene::filteredParallelTraverse( scene, filterPlug, f );
}

void GafferScene::matchingPaths( const PathMatcher &filter, const ScenePlug *scene, PathMatcher &paths )
{
	ThreadablePathAccumulator f( paths );
	GafferScene::filteredParallelTraverse( scene, filter, f );
}

IECore::ConstCompoundObjectPtr GafferScene::globalAttributes( const IECore::CompoundObject *globals )
{
	static const std::string prefix( "attribute:" );

	CompoundObjectPtr result = new CompoundObject;

	CompoundObject::ObjectMap::const_iterator it, eIt;
	for( it = globals->members().begin(), eIt = globals->members().end(); it != eIt; ++it )
	{
		if( !boost::starts_with( it->first.c_str(), "attribute:" ) )
		{
			continue;
		}
		// Cast is justified because we don't modify the data, and will return it
		// as const from this function.
		result->members()[it->first.string().substr( prefix.size() )] = boost::const_pointer_cast<Object>(
			it->second
		);
	}

	return result;
}

Imath::V2f GafferScene::shutter( const IECore::CompoundObject *globals )
{
	const BoolData *cameraBlurData = globals->member<BoolData>( "option:render:cameraBlur" );
	const bool cameraBlur = cameraBlurData ? cameraBlurData->readable() : false;

	const BoolData *transformBlurData = globals->member<BoolData>( "option:render:transformBlur" );
	const bool transformBlur = transformBlurData ? transformBlurData->readable() : false;

	const BoolData *deformationBlurData = globals->member<BoolData>( "option:render:deformationBlur" );
	const bool deformationBlur = deformationBlurData ? deformationBlurData->readable() : false;

	V2f shutter( Context::current()->getFrame() );
	if( cameraBlur || transformBlur || deformationBlur )
	{
		const V2fData *shutterData = globals->member<V2fData>( "option:render:shutter" );
		const V2f relativeShutter = shutterData ? shutterData->readable() : V2f( -0.25, 0.25 );
		shutter += relativeShutter;
	}

	return shutter;
}

IECore::TransformPtr GafferScene::transform( const ScenePlug *scene, const ScenePlug::ScenePath &path, const Imath::V2f &shutter, bool motionBlur )
{
	int numSamples = 1;
	if( motionBlur )
	{
		ConstCompoundObjectPtr attributes = scene->fullAttributes( path );
		const IntData *transformBlurSegmentsData = attributes->member<IntData>( "gaffer:transformBlurSegments" );
		numSamples = transformBlurSegmentsData ? transformBlurSegmentsData->readable() + 1 : 2;

		const BoolData *transformBlurData = attributes->member<BoolData>( "gaffer:transformBlur" );
		if( transformBlurData && !transformBlurData->readable() )
		{
			numSamples = 1;
		}
	}

	MatrixMotionTransformPtr result = new MatrixMotionTransform();
	ContextPtr transformContext = new Context( *Context::current(), Context::Borrowed );
	Context::Scope scopedContext( transformContext.get() );
	for( int i = 0; i < numSamples; i++ )
	{
		float frame = lerp( shutter[0], shutter[1], (float)i / std::max( 1, numSamples - 1 ) );
		transformContext->setFrame( frame );
		result->snapshots()[frame] = scene->fullTransform( path );
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Camera algo
//////////////////////////////////////////////////////////////////////////

void GafferScene::applyCameraGlobals( IECore::Camera *camera, const IECore::CompoundObject *globals )
{

	// apply the resolution, aspect ratio and crop window

	V2i resolution( 640, 480 );

	const Box2fData *cropWindowData = globals->member<Box2fData>( "option:render:cropWindow" );
	const V2iData *resolutionOverrideData = camera->parametersData()->member<V2iData>( "resolutionOverride" );
	if( resolutionOverrideData )
	{
		// We allow a parameter on the camera to override the resolution from the globals - this
		// is useful when defining secondary cameras for doing texture projections.
		/// \todo Consider how this might fit in as part of a more comprehensive camera setup.
		/// Perhaps we might actually want a specific Camera subclass for such things?
		resolution = resolutionOverrideData->readable();
	}
	else
	{
		if( const V2iData *resolutionData = globals->member<V2iData>( "option:render:resolution" ) )
		{
			resolution = resolutionData->readable();
		}

		if( const FloatData *resolutionMultiplierData = globals->member<FloatData>( "option:render:resolutionMultiplier" ) )
		{
			resolution.x = int((float)resolution.x * resolutionMultiplierData->readable());
			resolution.y = int((float)resolution.y * resolutionMultiplierData->readable());
		}

		const FloatData *pixelAspectRatioData = globals->member<FloatData>( "option:render:pixelAspectRatio" );
		if( pixelAspectRatioData )
		{
			camera->parameters()["pixelAspectRatio"] = pixelAspectRatioData->copy();
		}

		if( cropWindowData )
		{
			camera->parameters()["cropWindow"] = cropWindowData->copy();
		}
	}

	camera->parameters()["resolution"] = new V2iData( resolution );

	// calculate an appropriate screen window

	camera->addStandardParameters();

	// apply overscan


	Box2i renderRegion( V2i( 0 ), resolution - V2i( 1 ) );

	const BoolData *overscanData = globals->member<BoolData>( "option:render:overscan" );
	if( overscanData && overscanData->readable() )
	{
		// get offsets for each corner of image (as a multiplier of the image width)
		V2f minOffset( 0.0 ), maxOffset( 0.0 );
		if( const FloatData *overscanValueData = globals->member<FloatData>( "option:render:overscanLeft" ) )
		{
			minOffset.x = overscanValueData->readable();
		}
		if( const FloatData *overscanValueData = globals->member<FloatData>( "option:render:overscanRight" ) )
		{
			maxOffset.x = overscanValueData->readable();
		}
		if( const FloatData *overscanValueData = globals->member<FloatData>( "option:render:overscanBottom" ) )
		{
			minOffset.y = overscanValueData->readable();
		}
		if( const FloatData *overscanValueData = globals->member<FloatData>( "option:render:overscanTop" ) )
		{
			maxOffset.y = overscanValueData->readable();
		}

		// convert those offsets into pixel values
		renderRegion.min -= V2i(
			int(minOffset.x * (float)resolution.x),
			int(minOffset.y * (float)resolution.y)
		);

		renderRegion.max += V2i(
			int(maxOffset.x * (float)resolution.x),
			int(maxOffset.y * (float)resolution.y)
		);
	}

	
	if( cropWindowData )
	{
		Box2f cameraCropWindow = camera->parametersData()->member<Box2fData>( "cropWindow" )->readable();
		Box2i cropRegion(
			V2i( (int)( round( resolution.x * cameraCropWindow.min.x ) ),
			     (int)( round( resolution.y * cameraCropWindow.min.y ) ) ),
			V2i( (int)( round( resolution.x * cameraCropWindow.max.x ) ) - 1,
			     (int)( round( resolution.y * cameraCropWindow.max.y ) ) - 1 ) );

		renderRegion.min.x = std::max( renderRegion.min.x, cropRegion.min.x );
		renderRegion.max.x = std::min( renderRegion.max.x, cropRegion.max.x );
		renderRegion.min.y = std::max( renderRegion.min.y, cropRegion.min.y );
		renderRegion.max.y = std::min( renderRegion.max.y, cropRegion.max.y );
			
	}

	if( renderRegion.isEmpty() )
	{
		// OpenEXR does not permit empty data windows, so it is unlikely that any renderer
		// we support would do anything reasonable with an empty render region.
		//
		// Currently returning a one pixel region in the corner of the screen in this case
		//
		// The only other reasonable option I can think of in this case is to throw an exception
		// and abort rendering
		renderRegion = Box2i( V2i( 0 ), V2i( 0 ) );
	}

	camera->parameters()["renderRegion"] = new Box2iData( renderRegion );

	// apply the shutter

	camera->parameters()["shutter"] = new V2fData( shutter( globals ) );

}

IECore::CameraPtr GafferScene::camera( const ScenePlug *scene, const IECore::CompoundObject *globals )
{
	ConstCompoundObjectPtr computedGlobals;
	if( !globals )
	{
		computedGlobals = scene->globalsPlug()->getValue();
		globals = computedGlobals.get();
	}

	const StringData *cameraPathData = globals->member<StringData>( "option:render:camera" );
	if( cameraPathData && !cameraPathData->readable().empty() )
	{
		ScenePlug::ScenePath cameraPath;
		ScenePlug::stringToPath( cameraPathData->readable(), cameraPath );
		return camera( scene, cameraPath, globals );
	}
	else
	{
		CameraPtr defaultCamera = new IECore::Camera();
		applyCameraGlobals( defaultCamera.get(), globals );
		return defaultCamera;
	}
}

IECore::CameraPtr GafferScene::camera( const ScenePlug *scene, const ScenePlug::ScenePath &cameraPath, const IECore::CompoundObject *globals )
{
	ConstCompoundObjectPtr computedGlobals;
	if( !globals )
	{
		computedGlobals = scene->globalsPlug()->getValue();
		globals = computedGlobals.get();
	}

	std::string cameraName;
	ScenePlug::pathToString( cameraPath, cameraName );

	if( !exists( scene, cameraPath ) )
	{
		throw IECore::Exception( "Camera \"" + cameraName + "\" does not exist" );
	}

	IECore::ConstCameraPtr constCamera = runTimeCast<const IECore::Camera>( scene->object( cameraPath ) );
	if( !constCamera )
	{
		std::string path; ScenePlug::pathToString( cameraPath, path );
		throw IECore::Exception( "Location \"" + cameraName + "\" is not a camera" );
	}

	IECore::CameraPtr camera = constCamera->copy();
	camera->setName( cameraName );

	const BoolData *cameraBlurData = globals->member<BoolData>( "option:render:cameraBlur" );
	const bool cameraBlur = cameraBlurData ? cameraBlurData->readable() : false;
	camera->setTransform( transform( scene, cameraPath, shutter( globals ), cameraBlur ) );

	applyCameraGlobals( camera.get(), globals );
	return camera;
}

//////////////////////////////////////////////////////////////////////////
// Sets Algo
//////////////////////////////////////////////////////////////////////////

bool GafferScene::setExists( const ScenePlug *scene, const IECore::InternedString &setName )
{
	IECore::ConstInternedStringVectorDataPtr setNamesData = scene->setNamesPlug()->getValue();
	const std::vector<IECore::InternedString> &setNames = setNamesData->readable();
	return std::find( setNames.begin(), setNames.end(), setName ) != setNames.end();
}

namespace
{

struct Sets
{

	Sets( const ScenePlug *scene, const std::vector<InternedString> &names, std::vector<GafferScene::ConstPathMatcherDataPtr> &sets )
		:	m_scene( scene ), m_names( names ), m_sets( sets )
	{
	}

	void operator()( const tbb::blocked_range<size_t> &r ) const
	{
		for( size_t i=r.begin(); i!=r.end(); ++i )
		{
			m_sets[i] = m_scene->set( m_names[i] );
		}
	}

	private :

		const ScenePlug *m_scene;
		const std::vector<InternedString> &m_names;
		std::vector<GafferScene::ConstPathMatcherDataPtr> &m_sets;

} ;

} // namespace

IECore::ConstCompoundDataPtr GafferScene::sets( const ScenePlug *scene )
{
	ConstInternedStringVectorDataPtr setNamesData = scene->setNamesPlug()->getValue();
	std::vector<GafferScene::ConstPathMatcherDataPtr> setsVector;
	setsVector.resize( setNamesData->readable().size(), NULL );

	Sets setsCompute( scene, setNamesData->readable(), setsVector );
	parallel_for( tbb::blocked_range<size_t>( 0, setsVector.size() ), setsCompute );

	CompoundDataPtr result = new CompoundData;
	for( size_t i = 0, e = setsVector.size(); i < e; ++i )
	{
		// The const_pointer_cast is ok because we're just using it to put the set into
		// a container that will be const on return - we never modify the set itself.
		result->writable()[setNamesData->readable()[i]] = boost::const_pointer_cast<GafferScene::PathMatcherData>( setsVector[i] );
	}
	return result;
}

Imath::Box3f GafferScene::bound( const IECore::Object *object )
{
	if( const IECore::VisibleRenderable *renderable = IECore::runTimeCast<const IECore::VisibleRenderable>( object ) )
	{
		return renderable->bound();
	}
	else if( object->isInstanceOf( IECore::Camera::staticTypeId() ) )
	{
		return Imath::Box3f( Imath::V3f( -0.5, -0.5, 0 ), Imath::V3f( 0.5, 0.5, 2.0 ) );
	}
	else if( object->isInstanceOf( IECore::CoordinateSystem::staticTypeId() ) )
	{
		return Imath::Box3f( Imath::V3f( 0 ), Imath::V3f( 1 ) );
	}
	else if( object->isInstanceOf( IECore::ClippingPlane::staticTypeId() ) )
	{
		return Imath::Box3f( Imath::V3f( -0.5, -0.5, 0 ), Imath::V3f( 0.5 ) );
	}
	else if( !object->isInstanceOf( IECore::NullObject::staticTypeId() ) )
	{
		return Imath::Box3f( Imath::V3f( -0.5 ), Imath::V3f( 0.5 ) );
	}
	else
	{
		return Imath::Box3f();
	}
}
