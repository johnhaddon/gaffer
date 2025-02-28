//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2025, Cinesite VFX Ltd. All rights reserved.
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

#include "LightFilter.h"

#include "ShaderNetworkAlgo.h"
#include "Transform.h"

using namespace std;
using namespace Imath;
using namespace IECoreRenderMan;

namespace
{

const RtUString g_name( "name" );

} // namespace

LightFilter::LightFilter( const std::string &name, const Attributes *attributes, Session *session )
	 :	m_session( session ), m_coordinateSystemName( name.c_str() )
{
	std::cerr << "MAKING LIGHT FILTER" << std::endl;
	RtParamList params;
	params.SetString( g_name, m_coordinateSystemName );
	m_coordinateSystem = session->riley->CreateCoordinateSystem(
		riley::UserId(), IdentityTransform(), params
	);

	this->attributes( attributes );
}

LightFilter::~LightFilter()
{
	if( m_session->renderType == IECoreScenePreview::Renderer::Interactive )
	{
		m_session->riley->DeleteCoordinateSystem( m_coordinateSystem );
	}
}

void LightFilter::transform( const Imath::M44f &transform )
{
	/// \todo DO WE NEED TO FLIP IN Z???
	StaticTransform staticTransform( transform );

	const riley::CoordinateSystemResult result = m_session->riley->ModifyCoordinateSystem(
		m_coordinateSystem, &staticTransform, /* attributes = */ nullptr
	);

	std::cerr << "TRANSFORMED" << std::endl;

	if( result != riley::CoordinateSystemResult::k_Success )
	{
		IECore::msg( IECore::Msg::Warning, "IECoreRenderMan::LightFilter::transform", "Unexpected edit failure" );
	}
}

void LightFilter::transform( const std::vector<Imath::M44f> &samples, const std::vector<float> &times )
{
	AnimatedTransform animatedTransform( samples, times );

	const riley::CoordinateSystemResult result = m_session->riley->ModifyCoordinateSystem(
		m_coordinateSystem, &animatedTransform, /* attributes = */ nullptr
	);

	if( result != riley::CoordinateSystemResult::k_Success )
	{
		IECore::msg( IECore::Msg::Warning, "IECoreRenderMan::LightFilter::transform", "Unexpected edit failure" );
	}
}

bool LightFilter::attributes( const IECoreScenePreview::Renderer::AttributesInterface *attributes )
{
	const Attributes *typedAttributes = static_cast<const Attributes *>( attributes );
	m_shader = typedAttributes->lightFilter();

	/// \todo I bet this is going to be a pain - we need to update the fricking linked lights now don't we?
	return true;
}

void LightFilter::link( const IECore::InternedString &type, const IECoreScenePreview::Renderer::ConstObjectSetPtr &objects )
{
}

void LightFilter::assignID( uint32_t id )
{
}

RtUString LightFilter::coordinateSystemName() const
{
	return m_coordinateSystemName;
}

riley::CoordinateSystemId LightFilter::coordinateSystem() const
{
	return m_coordinateSystem;
}

const IECoreScene::ShaderNetwork *LightFilter::shader() const
{
	return m_shader.get();
}
