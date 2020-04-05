##########################################################################
#
#  Copyright (c) 2020, Cinesite VFX Ltd. All rights reserved.
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

Gaffer.Metadata.registerNode(

	GafferScene.ClosestPointSampler,

	"description",
	"""
	Samples primitive variables from the closest point on
	the surface of a source primitive, and transfers the
	values onto new primitive variable on the sampling objects.
	""",

	plugs = {

		"curveIndex" : [

			"description",
			"""
			The name of the primitive variable that specifies the index of
			the curve to be sampled. If this is not specified, the first
			curve will be sampled.
			""",

			"layout:section", "Settings.Input",

		],

		"v" : [

			"description",
			"""
			The name of the primitive variable that specifies the parametric
			position on the curve to be sampled. A value of 0 corresponds to
			the start of the curve, and a value of 1 corresponds to the end.
			If no value is provided for `v`, a value of 0 is used.

			> Note : Values outside the `0-1` range are invalid and cannot
			> be sampled. In this case, the `status` output primitive variable
			> will contain `False` to indicate failure.
			""",

			"layout:section", "Settings.Input",

		],

	}

)
