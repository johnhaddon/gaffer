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

#pragma once

#include "IECoreScene/ShaderNetwork.h"

#include "IECore/RefCounted.h"

#include "Session.h"

#include "tbb/concurrent_hash_map.h"

namespace IECoreRenderMan
{

// A reference counted material.
class Material : public IECore::RefCounted
{

	public :

		Material( const IECoreScene::ShaderNetwork *network, const ConstSessionPtr &session );
		~Material() override;

		const riley::MaterialId &id() const;

	private :

		ConstSessionPtr m_session;
		riley::MaterialId m_id;

};

IE_CORE_DECLAREPTR( Material )

class MaterialCache : public IECore::RefCounted
{

	public :

		MaterialCache( const ConstSessionPtr &session );

		// Can be called concurrently with other calls to `get()`
		ConstMaterialPtr get( const IECoreScene::ShaderNetwork *network );

		// Must not be called concurrently with anything.
		void clearUnused();

	private :

		ConstSessionPtr m_session;

		using Cache = tbb::concurrent_hash_map<IECore::MurmurHash, ConstMaterialPtr>;
		Cache m_cache;

};

IE_CORE_DECLAREPTR( MaterialCache )

/// \todo Is there a better home for this? Should we have a LightShader class like the Material class?
riley::LightShaderId convertLightShaderNetwork( const IECoreScene::ShaderNetwork *network, riley::Riley *riley );

} // namespace IECoreRenderMan