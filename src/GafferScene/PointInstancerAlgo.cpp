//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2026, Cinesite VFX Ltd. All rights reserved.
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

#include "GafferScene/Private/PointInstancerAlgo.h"

#include "GafferScene/SceneAlgo.h"

#include "IECore/NullObject.h"

#include "Imath/ImathMatrixAlgo.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

namespace
{

ScenePlug::ScenePath fullPrototypePath( const std::string &prototypePath, const ScenePlug::ScenePath &pointInstancerPath )
{
	if( prototypePath.empty() )
	{
		throw IECore::Exception( "Prototype path empty" );
	}

	if( prototypePath[0] == '/' )
	{
		return ScenePlug::stringToPath( prototypePath );
	}
	else
	{
		ScenePlug::ScenePath result;
		if( prototypePath[0] == '.' && prototypePath.size() >= 2 && prototypePath[1] == '/' )
		{
			ScenePlug::stringToPath( prototypePath.substr( 2 ), result );
		}
		else
		{
			ScenePlug::stringToPath( prototypePath, result );
		}
		result.insert( result.begin(), pointInstancerPath.begin(), pointInstancerPath.end() );
		return result;
	}
}

} // namespace

IECore::MurmurHash Private::PointInstancerAlgo::prototypesHash( const ScenePlug *scene )
{
	MurmurHash result;
	ConstObjectPtr object = scene->objectPlug()->getValue();
	auto pointInstancer = runTimeCast<const PointInstancer>( object.get() );
	if( !pointInstancer )
	{
		return result;
	}

	const auto &currentPath = Context::current()->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName );

	/// TODO : PARALLEL_REDUCE
	auto prototypePaths = pointInstancer->getPrototypes( /* throwIfInvalid = */ true );
	for( size_t prototypeIndex = 0; prototypeIndex < prototypePaths->size(); ++prototypeIndex )
	{
		auto fullPath = fullPrototypePath( (*prototypePaths)[prototypeIndex], currentPath );
		/// TODO : CHECK EXISTENCE
		result.append( SceneAlgo::hierarchyHash( scene, fullPath ) );
	}

	return result;
}

std::vector<IECoreScenePreview::Renderer::Prototype> Private::PointInstancerAlgo::prototypes( const IECoreScene::PointInstancer *instancer, const RendererAlgo::RenderOptions &renderOptions, const ScenePlug *scene, IECoreScenePreview::Renderer *renderer )
{
	const auto &currentPath = Context::current()->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName );

	auto prototypePaths = instancer->getPrototypes( /* throwIfInvalid = */ true );
	std::vector<IECoreScenePreview::Renderer::Prototype> result;
	result.resize( prototypePaths->size() );


	const ThreadState &threadState = ThreadState::current();
	tbb::task_group_context taskGroupContext( tbb::task_group_context::isolated );

	tbb::parallel_for(

		tbb::blocked_range<size_t>( 0, prototypePaths->size() ),

		[&]( const tbb::blocked_range<size_t> &r )
		{
			ScenePlug::PathScope prototypeScope( threadState );
			IECoreScenePreview::Renderer::SampleTimes sampleTimes;

			for( size_t prototypeIndex = r.begin(); prototypeIndex != r.end(); ++prototypeIndex )
			{
				auto fullPath = fullPrototypePath( (*prototypePaths)[prototypeIndex], currentPath );
				prototypeScope.setPath( &fullPath );
				if( !scene->existsPlug()->getValue() )
				{
					IECore::msg(
						IECore::Msg::Warning, "PointInstancer", "Prototype `{}` does not exist for instancer `{}`.",
						ScenePlug::pathToString( fullPath ), ScenePlug::pathToString( currentPath )
					);
					continue;
				}

				// TODO : RENDER PURPOSE

				fmt::print( "Getting prototype from {}\n", ScenePlug::pathToString( fullPath ) );

				ConstCompoundObjectPtr rootAttributes = scene->fullAttributes( fullPath );

				IECoreScenePreview::Renderer::Prototype &prototype = result[prototypeIndex];

				if( scene->childNamesPlug()->getValue()->readable().empty() ) // TODD : GET ON THIS PATH WHEN THERE IS A SINGLE VISIBLE OBJECT (INC PURPOSE) IN THE HIERARCHY
				{
					deformationMotionTimes( renderOptions, rootAttributes.get(), prototype.times );
					auto sampledObject = GafferScene::Private::RendererAlgo::objectSamples( scene->objectPlug(), prototype.times );
					prototype.samples = sampledObject->samples;
					prototype.times = sampledObject->sampleTimes;

				}
				else
				{
					// TODO : POSSIBLY JUST ERROR??
					// CapsulePtr capsule = new Capsule(
					// 	scene, rootPath, *Context::current(),
					// 	SceneAlgo::hierarchyHash( scene, rootPath ),
					// 	Box3f( V3f( -1 ), V3f( 1 ) ) // TODO
					// );
					// capsule->setRenderOptions( renderOptions );

					// prototype.samples = { capsule };
					// prototype.times = { 0.0f }; // TODO
				}

				prototype.attributes = renderer->attributes( rootAttributes.get() );
			}

		},

		taskGroupContext
	);


	return result;
}

namespace
{

struct PrototypeLocation
{
	string path;
	Imath::M44f transform;
	int index = 0;
};

using FlattenedPrototype = vector<PrototypeLocation>;

M44f flattenedTransform( const ScenePlug *scene, ScenePlug::ScenePath path, size_t rootPathSize )
{
	ScenePlug::PathScope pathScope( Context::current() );

	Imath::M44f result;
	while( path.size() >= rootPathSize )
	{
		pathScope.setPath( &path );
		result = result * scene->transformPlug()->getValue();
		path.pop_back();
	}

	return result;
}

FlattenedPrototype flattenedPrototype( const ScenePlug *scene, const string &rootPath, const ScenePlug::ScenePath &fullRootPath )
{
	FlattenedPrototype result;
	SceneAlgo::parallelGatherLocations(
		scene,
		[&]( const ScenePlug *scene, const ScenePlug::ScenePath &path ) -> std::optional<PrototypeLocation> {
			ConstObjectPtr o = scene->objectPlug()->getValue();
			if( runTimeCast<const IECore::NullObject>( o.get() ) )
			{
				return std::nullopt;
			}

			ScenePlug::ScenePath relativePath( path.begin() + fullRootPath.size(), path.end() );
			// Note : `PrototypeLocation::index` is filled later.
			return PrototypeLocation{
				rootPath + ScenePlug::pathToString( relativePath ),
				flattenedTransform( scene, path, fullRootPath.size() )
			};
		},
		[&]( std::optional<PrototypeLocation> location ) {
			if( location )
			{
				result.push_back( *location );
			}
		},
		fullRootPath
	);

	// `parallelGatherLocations()` doesn't guarantee order - sort so we
	// have a deterministic order for testing and debugging.
	std::sort(
		result.begin(), result.end(),
		[]( const PrototypeLocation &a, const PrototypeLocation &b ) {
			return a.path < b.path;
		}
	);

	return result;
}

} // namespace

IECoreScene::PointInstancerPtr Private::PointInstancerAlgo::flatten( const IECoreScene::PointInstancer *instancer, const ScenePlug *scene )
{
	// For each prototype, get a flattened list of descendant locations
	// with non-empty objects.

	const auto &currentPath = Context::current()->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName );
	auto prototypePaths = instancer->getPrototypes( /* throwIfInvalid = */ true );

	vector<FlattenedPrototype> flattenedPrototypes;
	flattenedPrototypes.resize( prototypePaths->size() );

	const ThreadState &threadState = ThreadState::current();
	tbb::task_group_context taskGroupContext( tbb::task_group_context::isolated );

	tbb::parallel_for(

		tbb::blocked_range<size_t>( 0, prototypePaths->size() ),

		[&]( const tbb::blocked_range<size_t> &r )
		{
			ScenePlug::PathScope prototypeScope( threadState );

			for( size_t prototypeIndex = r.begin(); prototypeIndex != r.end(); ++prototypeIndex )
			{
				auto fullPath = fullPrototypePath( (*prototypePaths)[prototypeIndex], currentPath );
				prototypeScope.setPath( &fullPath );
				if( !scene->existsPlug()->getValue() )
				{
					IECore::msg(
						IECore::Msg::Warning, "PointInstancer", "Prototype `{}` does not exist for instancer `{}`.",
						ScenePlug::pathToString( fullPath ), ScenePlug::pathToString( currentPath )
					);
					continue;
				}

				flattenedPrototypes[prototypeIndex] = flattenedPrototype( scene, (*prototypePaths)[prototypeIndex], fullPath );
			}

		},

		taskGroupContext
	);

	// Assign indices to flattened prototypes.

	StringVectorDataPtr flattenedPrototypeRootsData = new StringVectorData;
	auto &flattenedPrototypeRoots = flattenedPrototypeRootsData->writable();
	int offset = 0;
	for( auto &flattenedPrototype : flattenedPrototypes )
	{
		for( auto &location : flattenedPrototype )
		{
			location.index = offset++;
			flattenedPrototypeRoots.push_back( location.path );
		}
	}

	// Figure out how many points we'll have after flattening and allocate
	// vertex data for them.

	fmt::print( "flattened prototypes {}\n", flattenedPrototypes.size() );

	/// TODO : THROW IF CAN'T GET? MAKE ACCESSORS THROW FOR THINGS REQUIRED BY USD?
	auto prototypeIndex = instancer->variableIndexedView<IECore::IntVectorData>( "prototypeIndex", IECoreScene::PrimitiveVariable::Vertex, false ); // TODO : ACCESSOR, THROW IF MISSING

	fmt::print( "indexed view {}\n", (bool)prototypeIndex );

	PointInstancer::VisibilityQuery visibilityQuery( instancer );

	size_t numFlattenedPoints = 0;
	for( size_t pointIndex = 0; pointIndex < prototypeIndex->size(); ++pointIndex )
	{
		fmt::print( "Processing prototype {}\n", (*prototypeIndex)[pointIndex] );
		if( !visibilityQuery.visible( pointIndex ) )
		{
			continue;
		}
		numFlattenedPoints += flattenedPrototypes[(*prototypeIndex)[pointIndex]].size();
	}

	PointInstancerPtr result = new PointInstancer( numFlattenedPoints );
	result->variables["prototypeRoots"] = IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Constant, flattenedPrototypeRootsData ); // TODO : ACCESSOR

	IntVectorDataPtr flattenedPrototypeIndicesData = new IntVectorData;
	result->variables["prototypeIndex"] = IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, flattenedPrototypeIndicesData ); // TODO : ACCESSOR
	auto &flattenedPrototypeIndices = flattenedPrototypeIndicesData->writable();
	flattenedPrototypeIndices.resize( numFlattenedPoints );

	V3fVectorDataPtr flattenedPData = new V3fVectorData;
	result->variables["P"] = IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, flattenedPData ); // TODO : ACCESSOR
	auto &flattenedP = flattenedPData->writable();
	flattenedP.resize( numFlattenedPoints );

	V3fVectorDataPtr flattenedScaleData = new V3fVectorData;
	result->variables["scale"] = IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, flattenedScaleData ); // TODO : ACCESSOR
	auto &flattenedScale = flattenedScaleData->writable();
	flattenedScale.resize( numFlattenedPoints );

	QuatfVectorDataPtr flattenedOrientationData = new QuatfVectorData;
	result->variables["orientation"] = IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, flattenedOrientationData ); // TODO : ACCESSOR
	auto &flattenedOrientation = flattenedOrientationData->writable();
	flattenedOrientation.resize( numFlattenedPoints );

	// Fill the vertex data.

	// TODO : TBB ME
	// TODO : CUSTOM VERTEX PRIMVARS

	PointInstancer::Query query( instancer );

	size_t flattenedPointIndex = 0;
	for( size_t pointIndex = 0; pointIndex < instancer->getNumPoints(); ++pointIndex )
	{
		if( !visibilityQuery.visible( pointIndex ) )
		{
			continue;
		}

		for( const auto &location : flattenedPrototypes[(*prototypeIndex)[pointIndex]] )
		{
			flattenedPrototypeIndices[flattenedPointIndex] = location.index;

			M44f m = location.transform * query.transform( pointIndex );
			flattenedP[flattenedPointIndex] = m.translation();

			V3f discardedShear( 0 );
			extractAndRemoveScalingAndShear( m, flattenedScale[flattenedPointIndex], discardedShear );
			flattenedOrientation[flattenedPointIndex] = extractQuat( m );

			flattenedPointIndex++;
		}
	}

	return result;
}
