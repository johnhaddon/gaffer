##########################################################################
#
#  Copyright (c) 2026, Cinesite VFX Ltd. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#      * Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      * Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials provided with
#        the distribution.
#
#      * Neither the name of John Haddon nor the names of
#        any other contributors to this software may be used to endorse or
#        promote products derived from this software without specific prior
#        written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import Gaffer
import GafferScene

import GafferUI

Gaffer.Metadata.registerNode(

	GafferScene.TemporalFilter,

	"description",
	"""
	Applies a temporal filter to primitive variables by sampling them across a frame range
	and combining the samples using the chosen filter.
	""",

	"layout:activator:variableSampling", lambda node : node["samplingMode"].getValue() == GafferScene.TemporalFilter.SamplingMode.Variable,
	"layout:activator:fixedSampling", lambda node : node["samplingMode"].getValue() == GafferScene.TemporalFilter.SamplingMode.Fixed,
	"layout:activator:rampFilter", lambda node : node["filterType"].getValue() == GafferScene.TemporalFilter.Filter.Ramp,

	plugs = {

		"primitiveVariables" : {

			"description" :
			"""
			The primitive variables to filter, specified as a space-separated list of names
			or wildcards.
			""",

		},

		"start" : {

			"description" :
			"""
			The first frame of the filter range, specified relative to the current frame or
			as an absolute value.
			""",

			"plugValueWidget:type" : "GafferUI.LayoutPlugValueWidget",
			"layoutPlugValueWidget:orientation" : "horizontal",

		},

		"start.mode" : {

			"description" :
			"""
			Controls whether `start.frame` is relative to the current frame or an absolute value.
			""",

			"preset:Relative" : GafferScene.TemporalFilter.FrameMode.Relative,
			"preset:Absolute" : GafferScene.TemporalFilter.FrameMode.Absolute,

			"plugValueWidget:type" : "GafferUI.PresetsPlugValueWidget",
			"layout:label" : "",

		},

		"start.frame" : {

			"description" :
			"""
			The first frame of the filter range.
			""",

			"layout:label" : "",

		},

		"end" : {

			"description" :
			"""
			The last frame of the filter range, specified relative to the current frame or
			as an absolute value.
			""",

			"plugValueWidget:type" : "GafferUI.LayoutPlugValueWidget",
			"layoutPlugValueWidget:orientation" : "horizontal",

		},

		"end.mode" : {

			"description" :
			"""
			Controls whether `end.frame` is relative to the current frame or an absolute value.
			""",

			"preset:Relative" : GafferScene.TemporalFilter.FrameMode.Relative,
			"preset:Absolute" : GafferScene.TemporalFilter.FrameMode.Absolute,

			"plugValueWidget:type" : "GafferUI.PresetsPlugValueWidget",
			"layout:label" : "",

		},

		"end.frame" : {

			"description" :
			"""
			The last frame of the filter range.
			""",

			"layout:label" : "",

		},

		"samplingMode" : {

			"description" :
			"""
			Use "Fixed" mode for a constant number of samples across the range.

			Use "Variable" mode for samples at regular `step` intervals.
			""",

			"preset:Variable" : GafferScene.TemporalFilter.SamplingMode.Variable,
			"preset:Fixed" : GafferScene.TemporalFilter.SamplingMode.Fixed,

			"plugValueWidget:type" : "GafferUI.PresetsPlugValueWidget",

		},

		"step" : {

			"description" :
			"""
			The sampling interval in frames between `start.frame` and `end.frame`.

			> Note : `start.frame` and `end.frame` will always be sampled
			even if the `step` does not exactly fit the range. TODO : IS THIS ACTUALLY TRUE??
			""",

			"layout:activator" : "variableSampling",

		},

		"samples" : {

			"description" :
			"""
			The exact number of samples (including `start.frame` and `end.frame`) when using
			a "Fixed" `samplingMode`.
			""",

			"layout:activator" : "fixedSampling",

		},

		"filterType" : {

			"description" :
			"""
			The filter used to combine samples across the frame range.

			- "Box" : Equal weight for all samples.
			- "Gaussian" : Samples weighted by a Gaussian curve centred on the midpoint of
			  the range.
			- "Min" : The minimum value across all samples.
			- "Max" : The maximum value across all samples.
			- "Ramp" : Samples weighted by a user-defined ramp curve.
			""",

			"preset:Box" : GafferScene.TemporalFilter.Filter.Box,
			"preset:Gaussian" : GafferScene.TemporalFilter.Filter.Gaussian,
			"preset:Min" : GafferScene.TemporalFilter.Filter.Min,
			"preset:Max" : GafferScene.TemporalFilter.Filter.Max,
			"preset:Ramp" : GafferScene.TemporalFilter.Filter.Ramp,

			"plugValueWidget:type" : "GafferUI.PresetsPlugValueWidget",

		},

		"ramp" : {

			"description" :
			"""
			A ramp curve defining the weight for each sample position. The horizontal axis
			maps from `start.frame` (0) to `end.frame` (1).
			""",

			"layout:activator" : "rampFilter",

		},

	}

)
