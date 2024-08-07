##########################################################################
#
#  Copyright (c) 2015, Image Engine Design Inc. All rights reserved.
#  Copyright (c) 2015, Nvizible Ltd. All rights reserved.
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
import GafferImage

Gaffer.Metadata.registerNode(

	GafferImage.Unpremultiply,

	"description",
	"""
	Divides selected channels by a specified alpha channel.
	If the alpha channel on a pixel is 0, then that pixel will remain
	the same as the input.
	""",

	plugs = {

		"alphaChannel" : [

			"description",
			"""
			The channel to use as the alpha channel.
			The selected channel does not have to be 'A', but whichever
			channel is chosen will act as the alpha for the sake of this
			node.
			This channel will never be divided by itself - it will
			remain the same as the input.
			""",

			"plugValueWidget:type", "GafferImageUI.ChannelPlugValueWidget",

		],

		"ignoreMissingAlpha" : [

			"description",
			"""
			If set, this node will do nothing if the specified `alphaChannel`
			is not found, instead of throwing an error.
			""",

		],

	}

)
