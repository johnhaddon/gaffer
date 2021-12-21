##########################################################################
#
#  Copyright (c) 2021, John Haddon. All rights reserved.
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

import inspect

import IECore
import IECoreScene

import Gaffer
import GafferScene
import GafferArnold

class ArnoldCryptomatteOutputs( GafferScene.SceneProcessor ) :

	def __init__( self, name = "ArnoldCryptomatteOutputs" ) :

		GafferScene.SceneProcessor.__init__( self, name )

		self["renderType"] = Gaffer.IntPlug(
			defaultValue = GafferScene.Private.IECoreScenePreview.Renderer.RenderType.Batch,
			minValue = GafferScene.Private.IECoreScenePreview.Renderer.RenderType.Batch,
			maxValue = GafferScene.Private.IECoreScenePreview.Renderer.RenderType.Interactive
		)
		self["fileName"] = Gaffer.StringPlug()
		self["depth"] = Gaffer.IntPlug( minValue = 1, defaultValue = 6 )

		self["__shader"] = GafferArnold.ArnoldShader()
		self["__shader"].loadShader( "GafferCryptomatte" )

		self["__aov"] = GafferArnold.ArnoldAOVShader()
		self["__aov"]["in"].setInput( self["in"] )
		self["__aov"]["shader"].setInput( self["__shader"]["out"] )
		self["__aov"]["optionSuffix"].setValue( "cryptomatte" )

		self["__enabler"] = Gaffer.Switch()
		self["__enabler"].setup( self["in"] )
		self["__enabler"]["in"][0].setInput( self["in"] )
		self["__enabler"]["in"][1].setInput( self["in"] )
		self["__enabler"]["index"].setInput( self["enabled"] )

		self["__expression"] = Gaffer.Expression()
		self["__expression"].setExpression( inspect.cleandoc(
			"""
			import GafferArnold
			parent["__enabler"]["in"]["in1"]["globals"] = GafferArnold.ArnoldCryptomatteOutputs._ArnoldCryptomatteOutputs__computeGlobals(
				parent["__aov"]["out"]["globals"],
				parent["renderType"],
				parent["fileName"],
				parent["depth"]
			)
			"""
		) )

		self["out"].setInput( self["__enabler" ]["out"] )

	@staticmethod
	def __computeGlobals( inGlobals, renderType, fileName, depth ) :

		# Remove existing cryptomatte outputs

		prefix = "output:{}/Cryptomatte/".format(
			GafferScene.Private.IECoreScenePreview.Renderer.RenderType.values[renderType]
		)

		result = IECore.CompoundObject( {
			k : v for k, v in inGlobals.items()
			if not k.startswith( prefix )
		} )

		# Make a template output

		metadataPrefix = "header:" + GafferScene.CryptomatteAlgo.metadataPrefix( "crypto_object" )

		outputTemplate = IECoreScene.Output(
			fileName,
			"exr",
			"crypto_object FLOAT",
			{
				"{}name".format( metadataPrefix ) : "crypto_object",
				"{}hash".format( metadataPrefix ) : "MurmurHash3_32",
				"{}conversion".format( metadataPrefix ) : "uint32_to_float32",
			}
		)

		if renderType == GafferScene.Private.IECoreScenePreview.Renderer.RenderType.Batch :
			outputTemplate.parameters()["filter"] = IECore.StringData( "cryptomatte" )
		else :
			# The `cryptomatte_filter` isn't optimised for progressive rendering,
			# so we avoid it. A single layer with the `nearest` filter is sufficient
			# for interactive picking (but not matte extraction with anti-aliased edges).
			depth = 1
			outputTemplate.parameters()["filter"] = IECore.StringData( "nearest" )

		# Add the outputs we want

		for d in range( 0, depth, 2 ) :

			depthSuffix = "{:02}".format( d / 2 )

			output = outputTemplate.copy()
			output.setName(
				output.getName().replace(
					"<depth>", depthSuffix
				)
			)
			output.parameters()["filterrank"] = d

			result["{}Object{}".format( prefix, depthSuffix )] = output

		return result

IECore.registerRunTimeTyped( ArnoldCryptomatteOutputs, typeName = "GafferArnold::ArnoldCryptomatteOutputs" )
