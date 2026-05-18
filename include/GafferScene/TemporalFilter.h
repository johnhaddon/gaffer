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

#pragma once

#include "GafferScene/Deformer.h"

#include "Gaffer/RampPlug.h"

namespace GafferScene
{

class GAFFERSCENE_API TemporalFilter : public Deformer
{

	public :

		explicit TemporalFilter( const std::string &name = defaultName<TemporalFilter>() );
		~TemporalFilter() override;

		GAFFER_NODE_DECLARE_TYPE( GafferScene::TemporalFilter, TemporalFilterTypeId, Deformer );

		enum class FrameMode
		{
			Relative = 0,
			Absolute
		};

		enum class SamplingMode
		{
			Variable = 0,
			Fixed
		};

		enum class Filter
		{
			Box,
			Gaussian,
			Min,
			Max,
			Ramp
		};

		Gaffer::StringPlug *primitiveVariablesPlug();
		const Gaffer::StringPlug *primitiveVariablesPlug() const;

		Gaffer::IntPlug *startModePlug();
		const Gaffer::IntPlug *startModePlug() const;

		Gaffer::FloatPlug *startFramePlug();
		const Gaffer::FloatPlug *startFramePlug() const;

		Gaffer::IntPlug *endModePlug();
		const Gaffer::IntPlug *endModePlug() const;

		Gaffer::FloatPlug *endFramePlug();
		const Gaffer::FloatPlug *endFramePlug() const;

		Gaffer::IntPlug *samplingModePlug();
		const Gaffer::IntPlug *samplingModePlug() const;

		Gaffer::FloatPlug *stepPlug();
		const Gaffer::FloatPlug *stepPlug() const;

		Gaffer::IntPlug *samplesPlug();
		const Gaffer::IntPlug *samplesPlug() const;

		Gaffer::IntPlug *filterTypePlug();
		const Gaffer::IntPlug *filterTypePlug() const;

		Gaffer::RampffPlug *rampPlug();
		const Gaffer::RampffPlug *rampPlug() const;

	protected :

		bool adjustBounds() const override;
		bool affectsProcessedObject( const Gaffer::Plug *input ) const override;
		void hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const override;
		IECore::ConstObjectPtr computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, const IECore::Object *inputObject ) const override;

	private :

		std::vector<float> sampleFrames( const Gaffer::Context *context ) const;
		std::vector<float> sampleWeights( const std::vector<float> &frames ) const;

		static size_t g_firstPlugIndex;

};

IE_CORE_DECLAREPTR( TemporalFilter )

} // namespace GafferScene