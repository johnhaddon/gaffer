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

#pragma once

#include "GafferScene/Private/IECoreScenePreview/Renderer.h"

#include "Riley.h"

namespace IECoreRenderMan
{

class Light;
class LightFilter;
/// - DONT'T NEED OWNERSHIP OF LIGHTS OR LIGHTFILTERS
///		- JUST REMOVE FROM DATA STRUCTURES ON DESTRUCTION
///
/// - DOCUMENT THE REASON FOR THIS BALLACHE
///
/// - CAN WE RELY ON UNLINKING TO REMOVE DANGLING POINTERS?
/// 	- OR DO WE NEED TO DEREGISTER IN OBJECTINTERFACE DESTRUCTORS?
///
/// - OPERATIONS
///		- LINK
///			- ADD LIGHT TO MAP KEYED BY OBJECTSETPTR
///		- LIGHTFITER::ATTRIBUTES
///			- DIRTY EVERYTHING AFFECTED BY FILTER
///		- RENDER :
///			- UPDATE LIGHTS
///			- CLEAR SETS THAT WE ARE THE SOLE OWNER OF
///
/// EVEN WHEN WE DON'T _NEED_ THIS, WILL IT STILL BE USEFUL AS A
/// CACHE OF CONVERTED FILTER NODES??? MAYBE THE CONVERSION IS SO FAST WE DON'T CARE?
/// - NAH, BECAUSE WHEN WE DON'T NEED THIS, THE LIGHT FILTER OBJECTS WILL MANAGE THEIR OWN STATE

/// Light filters aren't first-class objects in Riley. Instead they are just
/// extra shaders bolted on to the shader owned by the light. So we need extra
/// tracking to update the lights when the filters are edited.
class LightFilterLinks
{

	public :

		void registerLink( Light *light, const IECoreScenePreview::Renderer::ConstObjectSetPtr &lightFilters );
		//void dirtyLinks( const LightFilter *lightFilter );
		void updateDirtyLinks();

	private :

		struct LinkData
		{
			std::atomic_bool dirty; // OR A SEPARATE CONTAINER OF DIRTY SETS? THAT WOULD AVOID LINEAR WORK IN UPDATEDIRTYLINKS.
			std::vector<Light *> affectedLights; // How to remove???
			//std::vector<riley::CoordinateSystemId>
		};

		using SetsToLights = std::unordered_map<IECoreScenePreview::Renderer::ConstObjectSetPtr, LinkData>;

		/// NEED TO ACTAULLY MAP TO A SET OF SETS
		using FiltersToSets = std::unordered_map<const LightFilter *, IECoreScenePreview::Renderer::ConstObjectSetPtr>;

		// m_filtersToSets;
		// m_setsToLights;

};

} // namespace IECoreRenderMan
