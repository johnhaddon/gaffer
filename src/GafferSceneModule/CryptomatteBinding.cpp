//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2021, Murray Stevenson. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
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

#include "boost/python.hpp"

#include "CryptomatteBinding.h"

#include "GafferScene/Cryptomatte.h"
#include "GafferScene/CryptomatteAlgo.h"

#include "GafferBindings/DependencyNodeBinding.h"
#include "GafferBindings/PlugBinding.h"

using namespace boost::python;
using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;
using namespace GafferScene;

namespace
{

float hashWrapper( const std::string &s )
{
	/// \todo Figure out how to get a `string_view` that directly
	/// refers to a Python string.
	return CryptomatteAlgo::hash( s );
}

object findWrapper( const ScenePlug &scene, float hash )
{
	boost::optional<ScenePlug::ScenePath> path;
	{
		ScopedGILRelease gilRelease;
		path = CryptomatteAlgo::find( &scene, hash );
	}

	if( path )
	{
		return object( IECore::InternedStringVectorDataPtr(
			new IECore::InternedStringVectorData( *path )
		) );
	}
	else
	{
		return object();
	}
}

} // namespace

void GafferSceneModule::bindCryptomatte()
{
	{
		scope s = GafferBindings::DependencyNodeClass<Cryptomatte>();
		enum_<Cryptomatte::ManifestSource>( "ManifestSource" )
			.value( "None", Cryptomatte::ManifestSource::None )
			.value( "None_", Cryptomatte::ManifestSource::None )
			.value( "Metadata", Cryptomatte::ManifestSource::Metadata )
			.value( "Sidecar", Cryptomatte::ManifestSource::Sidecar )
		;
	}

	{
		object module( borrowed( PyImport_AddModule( "GafferScene.CryptomatteAlgo" ) ) );
		scope().attr( "CryptomatteAlgo" ) = module;
		scope moduleScope( module );

		def( "hash", &hashWrapper );
		def( "metadataPrefix", &CryptomatteAlgo::metadataPrefix );
		def( "find", &findWrapper );
	}
}
