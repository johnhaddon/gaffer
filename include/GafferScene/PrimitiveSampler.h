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

#ifndef GAFFERSCENE_PRIMITIVESAMPLER_H
#define GAFFERSCENE_PRIMITIVESAMPLER_H

#include "GafferScene/Deformer.h"

#include "Gaffer/StringPlug.h"

#include "IECoreScene/PrimitiveEvaluator.h"

namespace GafferScene
{

/// Base class for nodes which use an `IECoreScene::PrimitiveEvaluator`
/// to sample primitive variables from another object.
///
/// \todo
/// - Better error messages for incorrect primvar types (include name)
/// - Avoid transform stuff for non-transforming subclasses
/// - Avoid bounds processing if not sampling P
/// - UI
///		- PathChooser
///		- Menus for primitive variable names
///		- Sections
/// - Threading (call samplingfunction once to initialise evaluator)
class GAFFERSCENE_API PrimitiveSampler : public Deformer
{

	public :

		~PrimitiveSampler() override;

		GAFFER_GRAPHCOMPONENT_DECLARE_TYPE( GafferScene::PrimitiveSampler, PrimitiveSamplerTypeId, ObjectProcessor );

		ScenePlug *sourcePlug();
		const ScenePlug *sourcePlug() const;

		Gaffer::StringPlug *sourceLocationPlug();
		const Gaffer::StringPlug *sourceLocationPlug() const;

		Gaffer::StringPlug *primitiveVariablesPlug();
		const Gaffer::StringPlug *primitiveVariablesPlug() const;

		Gaffer::StringPlug *prefixPlug();
		const Gaffer::StringPlug *prefixPlug() const;

		Gaffer::StringPlug *statusPlug();
		const Gaffer::StringPlug *statusPlug() const;

	protected :

		PrimitiveSampler( const std::string &name = defaultName<PrimitiveSampler>() );

		using SamplingFunction = std::function<bool ( const IECoreScene::PrimitiveEvaluator &, size_t, const Imath::M44f &, IECoreScene::PrimitiveEvaluator::Result & )>;

		virtual bool affectsSamplingFunction( const Gaffer::Plug *input ) const = 0;
		virtual void hashSamplingFunction( IECore::MurmurHash &h ) const = 0;
		virtual SamplingFunction computeSamplingFunction( const IECoreScene::Primitive *primitive, IECoreScene::PrimitiveVariable::Interpolation &interpolation ) const = 0;

	private :

		bool affectsProcessedObject( const Gaffer::Plug *input ) const final;
		void hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const final;
		IECore::ConstObjectPtr computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, const IECore::Object *inputObject ) const final;

		static size_t g_firstPlugIndex;

};

IE_CORE_DECLAREPTR( PrimitiveSampler )

} // namespace GafferScene

#endif // GAFFERSCENE_PRIMITIVESAMPLER_H
