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

bool Private::PointInstancerAlgo::prototypesHash( const ScenePlug *scene, IECore::MurmurHash &h )
{
	ConstObjectPtr object = scene->objectPlug()->getValue();
	auto pointInstancer = runTimeCast<const PointInstancer>( object.get() );
	if( !pointInstancer )
	{
		return false;
	}

	const auto &currentPath = Context::current()->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName );

	/// TODO : PARALLEL_REDUCE
	auto prototypePaths = pointInstancer->getPrototypes( /* throwIfInvalid = */ true );
	for( size_t prototypeIndex = 0; prototypeIndex < prototypePaths->size(); ++prototypeIndex )
	{
		auto fullPath = fullPrototypePath( (*prototypePaths)[prototypeIndex], currentPath );
		h.append( SceneAlgo::hierarchyHash( scene, fullPath ) );
	}

	return true;
}
