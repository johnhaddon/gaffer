//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2024, Cinesite VFX Ltd. All rights reserved.
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

#include "Globals.h"
#include "ParamListAlgo.h"
#include "Transform.h"

#include "IECoreScene/ShaderNetwork.h"

#include "IECore/SimpleTypedData.h"

#include "RixPredefinedStrings.hpp"

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"

#include "fmt/format.h"

using namespace std;
using namespace IECore;
using namespace IECoreScene;
using namespace IECoreRenderMan;

namespace
{

const string g_renderManPrefix( "renderman:" );
const IECore::InternedString g_cameraOption( "camera" );
const IECore::InternedString g_sampleMotionOption( "sampleMotion" );
const IECore::InternedString g_frameOption( "frame" );
const IECore::InternedString g_integratorOption( "renderman:integrator" );

template<typename T>
T *optionCast( const IECore::RunTimeTyped *v, const IECore::InternedString &name )
{
	if( !v )
	{
		return nullptr;
	}

	T *t = IECore::runTimeCast<T>( v );
	if( t )
	{
		return t;
	}

	IECore::msg( IECore::Msg::Warning, "IECoreRenderMan::Renderer", fmt::format( "Expected {} but got {} for option \"{}\".", T::staticTypeName(), v->typeName(), name.c_str() ) );
	return nullptr;
}

} // namespace

Globals::Globals( const SessionPtr &session )
	:	m_session( session ), m_options(),
		m_renderTargetExtent(),
		m_worldBegun( false )
{
	// Initialise `m_integratorToConvert`.
	option( g_integratorOption, nullptr );

	if( char *p = getenv( "RMAN_DISPLAYS_PATH" ) )
	{
		string searchPath = string( p ) + ":@";
		m_options.SetString( Rix::k_searchpath_display, RtUString( searchPath.c_str() ) );
	}

	if( char *p = getenv( "OSL_SHADER_PATHS" ) )
	{
		string searchPath = string( p ) + ":@";
		m_options.SetString( Rix::k_searchpath_shader, RtUString( searchPath.c_str() ) );
	}

	if( session->renderType == IECoreScenePreview::Renderer::Interactive )
	{
		m_options.SetInteger( Rix::k_hider_incremental, 1 );
		m_options.SetString( Rix::k_bucket_order, RtUString( "circle" ) );
	}
}

Globals::~Globals()
{
	pause();
}

void Globals::option( const IECore::InternedString &name, const IECore::Object *value )
{
	if( name == g_integratorOption )
	{
		if( auto *network = optionCast<const ShaderNetwork>( value, name ) )
		{
			m_integratorToConvert = network->outputShader();
		}
		else
		{
			m_integratorToConvert = new Shader( "PxrPathTracer", "renderman:integrator" );
		}
	}
	else if( name == g_cameraOption )
	{
		if( auto *d = optionCast<const StringData>( value, name ) )
		{
			m_cameraOption = d->readable();
		}
	}
	else if( name == g_frameOption )
	{
		if( auto *d = optionCast<const IntData>( value, name ) )
		{
			m_options.SetInteger( RtUString( "Ri:Frame" ), d->readable() );
		}
		else
		{
			m_options.Remove( RtUString( "Ri:Frame" ) );
		}
	}
	else if( name == g_sampleMotionOption )
	{
		if( auto *d = optionCast<const BoolData>( value, name ) )
		{
			m_options.SetInteger( RtUString( "hider:samplemotion" ), d->readable() );
		}
		else
		{
			m_options.Remove( RtUString( "hider:samplemotion" ) );
		}
	}
	else if( boost::starts_with( name.c_str(), g_renderManPrefix.c_str() ) )
	{
		const RtUString renderManName( name.c_str() + g_renderManPrefix.size() );
		if( auto data = optionCast<const Data>( value, name ) )
		{
			ParamListAlgo::convertParameter( renderManName, data, m_options );
		}
		else
		{
			m_options.Remove( renderManName );
		}
	}
	else if( boost::starts_with( name.c_str(), "user:" ) )
	{
		const RtUString renderManName( name.c_str() );
		if( auto data = optionCast<const Data>( value, name ) )
		{
			ParamListAlgo::convertParameter( renderManName, data, m_options );
		}
		else
		{
			m_options.Remove( renderManName );
		}
	}
}

void Globals::output( const IECore::InternedString &name, const Output *output )
{
	if( output )
	{
		m_outputs[name] = output;
	}
	else
	{
		m_outputs.erase( name );
	}

	deleteRenderView();
}

// TODO : UPDATE WHEN YOU FIGURE OUT THE NEW RESTRICTIONS
//
// Despite being designed as a modern edit-anything-at-any-time renderer API,
// in places Riley is still implemented as a veneer over an old RI-like
// state. Except now you have to guess how the API functions map to
// state transitions in the backend.
//
// It turns out that `SetActiveCamera()` is basically `WorldBegin`,
// and you must create _all_ cameras before calling it, and you must
// not create geometry until _after_ calling it. We use `ensureWorld()`
// to make this transition at the latest possible moment, just before we
// are given our first geometry. After we've entered the world, we
// refuse to make any further edits to cameras or outputs.
//
// There are further ordering requirements on top of the above. The
// only workable sequence of operations I've found is this :
//
//   1. CreateCamera().
//   2. CreateIntegrator().
//   3. SetRenderTargetIds().
//   4. SetActiveCamera().
void Globals::ensureWorld()
{
	tbb::spin_mutex::scoped_lock l( m_worldBeginMutex );
	if( m_worldBegun )
	{
		return;
	}

	m_session->setOptions( m_options );
	m_worldBegun = true;
}

void Globals::updateIntegrator()
{
	if( !m_integratorToConvert )
	{
		return;
	}

	if( m_integratorId != riley::IntegratorId::InvalidId() )
	{
		// Note : we update the render view to use the new integrator in
		// `updateRenderView()`, called immediately after `updateIntegrator()`.
		// So far it seems to be OK that the render view has a dangling
		// integrator in the meantime.
		m_session->riley->DeleteIntegrator( m_integratorId );
	}

	RtParamList integratorParamList;
	ParamListAlgo::convertParameters( m_integratorToConvert->parameters(), integratorParamList );

	riley::ShadingNode integratorNode = {
		riley::ShadingNode::Type::k_Integrator,
		RtUString( m_integratorToConvert->getName().c_str() ),
		RtUString( "integrator" ),
		integratorParamList
	};

	m_integratorId = m_session->riley->CreateIntegrator( riley::UserId(), integratorNode );
	m_integratorToConvert = nullptr;
}

void Globals::render()
{
	ensureWorld();
	updateIntegrator();
	updateRenderView();

	/// \todo Is it worth avoiding this work when nothing has changed?
	Session::CameraInfo camera = m_session->getCamera( m_cameraOption );
	m_options.Update( camera.options );
	m_session->setOptions( m_options );

	switch( m_session->renderType )
	{
		case IECoreScenePreview::Renderer::Batch : {
			RtParamList renderOptions;
			renderOptions.SetString( RtUString( "renderMode" ), RtUString( "batch" ) );
			m_session->riley->Render( { 1, &m_renderView }, renderOptions );
			break;
		}
		case IECoreScenePreview::Renderer::Interactive :
			/// \todo Would it reduce latency if we reused the same thread?
			m_interactiveRenderThread = std::thread(
				[this] {
					RtParamList renderOptions;
					renderOptions.SetString( RtUString( "renderMode" ), RtUString( "interactive" ) );
					m_session->riley->Render( { 1, &m_renderView }, renderOptions );
				}
			);
			break;
		case IECoreScenePreview::Renderer::SceneDescription :
			// Protected against in RenderManRenderer constructor
			assert( 0 );
	}
}

void Globals::pause()
{
	if( m_interactiveRenderThread.joinable() )
	{
		m_session->riley->Stop();
		m_interactiveRenderThread.join();
	}
}

bool Globals::worldBegun()
{
	tbb::spin_mutex::scoped_lock l( m_worldBeginMutex );
	return m_worldBegun;
}

void Globals::updateRenderView()
{
	// Find camera.

	Session::CameraInfo camera = m_session->getCamera( m_cameraOption );
	if( camera.id == riley::CameraId::InvalidId() )
	{
		/// \todo Should the Camera and/or Session class be responsible for
		/// providing a default camera?
		if( m_defaultCamera == riley::CameraId::InvalidId() )
		{
			m_defaultCamera = m_session->riley->CreateCamera(
				riley::UserId(),
				RtUString( "ieCoreRenderMan:defaultCamera" ),
				/// \todo Projection? Pointing wrong way?
				{
					riley::ShadingNode::Type::k_Projection, RtUString( "PxrCamera" ),
					RtUString( "projection" ), RtParamList()
				},
				IdentityTransform(),
				RtParamList()
			);
		}
		camera.id = m_defaultCamera;
	}

	riley::Extent extent = { 640, 480, 0 };
	if( auto *resolution = camera.options.GetIntegerArray( Rix::k_Ri_FormatResolution, 2 ) )
	{
		extent.x = resolution[0];
		extent.y = resolution[1];
	}

	// If we still have a render view, then it is valid for
	// `m_outputs`, and all we need to do is update the camera and
	// resolution.

	if( m_renderView != riley::RenderViewId::InvalidId() )
	{
		if( extent.x != m_renderTargetExtent.x || extent.y != m_renderTargetExtent.y )
		{
			// Must only modify this if it has actually changed, because it causes
			// Riley to close and reopen all the display drivers.
			m_session->riley->ModifyRenderTarget(
				m_renderTarget, nullptr, &extent, nullptr, nullptr, nullptr
			);
			m_renderTargetExtent = extent;
		}
		m_session->riley->ModifyRenderView(
			m_renderView, nullptr, &camera.id, &m_integratorId, nullptr, nullptr, nullptr
		);
		return;
	}

	// Otherwise we need to build the render view from out list of outputs.

	struct DisplayDefinition
	{
		RtUString name;
		RtUString driver;
		vector<riley::RenderOutputId> outputs;
		RtParamList driverParamList;
	};

	std::vector<DisplayDefinition> displayDefinitions;

	for( const auto &[name, output] : m_outputs )
	{
		// Render outputs

		const size_t firstRenderOutputIndex = m_renderOutputs.size();

		std::optional<riley::RenderOutputType> type;
		RtUString source;
		if( output->getData() == "rgb" || output->getData() == "rgba" )
		{
			type = riley::RenderOutputType::k_Color;
			source = RtUString( "Ci" );
		}

		if( !type )
		{
			IECore::msg( IECore::Msg::Warning, "RenderManRenderer", fmt::format( "Ignoring unsupported output {}", name.c_str() ) );
			continue;
		}

		const RtUString accumulationRule( "filter" );
		const RtUString filter = Rix::k_gaussian;
		const riley::FilterSize filterSize = { 3.0, 3.0 };
		const float relativePixelVariance = 1.0f;

		m_renderOutputs.push_back(
			m_session->riley->CreateRenderOutput(
				riley::UserId(),
				RtUString( name.c_str() ), *type, source,
				accumulationRule, filter, filterSize, relativePixelVariance,
				RtParamList()
			)
		);

		if( output->getData() == "rgba" )
		{
			const string alphaName = name.string() + "_Alpha";
			m_renderOutputs.push_back(
				m_session->riley->CreateRenderOutput(
					riley::UserId(),
					RtUString( alphaName.c_str() ), riley::RenderOutputType::k_Float, Rix::k_a,
					accumulationRule, filter, filterSize, relativePixelVariance,
					RtParamList()
				)
			);
		}

		// Display

		string driver = output->getType();
		if( driver == "exr" )
		{
			driver = "openexr";
		}

		RtParamList driverParamList;
		ParamListAlgo::convertParameters( output->parameters(), driverParamList );

		displayDefinitions.push_back( {
			RtUString( output->getName().c_str() ),
			RtUString( driver.c_str() ),
			{ m_renderOutputs.begin() + firstRenderOutputIndex, m_renderOutputs.end() },
			driverParamList
		} );
	}

	m_renderTarget = m_session->riley->CreateRenderTarget(
		riley::UserId(),
		{ (uint32_t)m_renderOutputs.size(), m_renderOutputs.data() },
		// Why must the resolution be specified both here _and_ via the
		// `k_Ri_FormatResolution` option? Riley only knows.
		extent,
		RtUString( "importance" ),
		0.015f,
		RtParamList()
	);
	m_renderTargetExtent = extent;

	for( const auto &definition : displayDefinitions )
	{
		m_displays.push_back(
			m_session->riley->CreateDisplay(
				riley::UserId(),
				m_renderTarget,
				definition.name,
				definition.driver,
				{ (uint32_t)definition.outputs.size(), definition.outputs.data() },
				definition.driverParamList
			)
		);
	}

	m_renderView = m_session->riley->CreateRenderView(
		riley::UserId(),
		m_renderTarget,
		camera.id,
		m_integratorId,
		{ 0, nullptr },
		{ 0, nullptr },
		RtParamList()
	);

}

void Globals::deleteRenderView()
{
	if( m_renderView == riley::RenderViewId::InvalidId() )
	{
		return;
	}

	m_session->riley->DeleteRenderView( m_renderView );
	m_renderView = riley::RenderViewId::InvalidId();

	for( const auto &display : m_displays )
	{
		m_session->riley->DeleteDisplay( display );
	}
	m_displays.clear();

	m_session->riley->DeleteRenderTarget( m_renderTarget );
	m_renderTarget = riley::RenderTargetId::InvalidId();

	for( const auto &renderOutput : m_renderOutputs )
	{
		m_session->riley->DeleteRenderOutput( renderOutput );
	}
	m_renderOutputs.clear();
}
