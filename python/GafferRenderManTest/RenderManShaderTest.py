##########################################################################
#
#  Copyright (c) 2018, John Haddon. All rights reserved.
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

import unittest

import imath

import Gaffer
import GafferScene
import GafferSceneTest
import GafferOSL
import GafferRenderMan

class RenderManShaderTest( GafferSceneTest.SceneTestCase ) :

	def testBasicLoading( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrConstant" )

		self.assertEqual( shader["name"].getValue(), "PxrConstant" )
		self.assertEqual( shader["type"].getValue(), "ri:surface" )

		self.assertEqual( shader["parameters"].keys(), [ "emitColor", "presence" ] )

		self.assertIsInstance( shader["parameters"]["emitColor"], Gaffer.Color3fPlug )
		self.assertIsInstance( shader["parameters"]["presence"], Gaffer.FloatPlug )

		self.assertEqual( shader["parameters"]["emitColor"].defaultValue(), imath.Color3f( 1 ) )
		self.assertEqual( shader["parameters"]["presence"].defaultValue(), 1.0 )
		self.assertEqual( shader["parameters"]["presence"].minValue(), 0.0 )
		self.assertEqual( shader["parameters"]["presence"].maxValue(), 1.0 )

	def testLoadParametersInsidePages( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrVolume" )

		self.assertIn( "extinctionDistance", shader["parameters"] )
		self.assertIn( "densityColor", shader["parameters"] )

	def testLoadRemovesUnnecessaryParameters( self ) :

		for keepExisting in ( True, False ) :

			shader = GafferRenderMan.RenderManShader()
			shader.loadShader( "PxrDiffuse" )
			self.assertIn( "diffuseColor", shader["parameters"] )

			shader.loadShader( "PxrConstant", keepExistingValues = keepExisting )
			self.assertNotIn( "color1", shader["parameters"] )
			self.assertIn( "emitColor", shader["parameters"] )

	def testLoadOutputs( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrSeExpr" )

		self.assertIn( "resultRGB", shader["out"] )
		self.assertIsInstance( shader["out"]["resultRGB"], Gaffer.Color3fPlug )

		self.assertIn( "resultR", shader["out"] )
		self.assertIsInstance( shader["out"]["resultR"], Gaffer.FloatPlug )

		self.assertIn( "resultG", shader["out"] )
		self.assertIsInstance( shader["out"]["resultG"], Gaffer.FloatPlug )

		self.assertIn( "resultB", shader["out"] )
		self.assertIsInstance( shader["out"]["resultB"], Gaffer.FloatPlug )

	## IECoreUSD isn't round-tripping the shader type correctly yet.
	@unittest.expectedFailure
	def testUSDRoundTrip( self ) :

		texture = GafferOSL.OSLShader( "PxrTexture" )
		texture.loadShader( "PxrTexture" )
		texture["parameters"]["filename"].setValue( "test.tx" )

		surface = GafferRenderMan.RenderManShader( "PxrSurface" )
		surface.loadShader( "PxrSurface" )
		surface["parameters"]["diffuseColor"].setInput( texture["out"]["resultRGB"] )
		surface["parameters"]["diffuseGain"].setValue( 0.5 )

		plane = GafferScene.Plane()

		shaderAssignment = GafferScene.ShaderAssignment()
		shaderAssignment["in"].setInput( plane["out"] )
		shaderAssignment["shader"].setInput( surface["out"] )

		sceneWriter = GafferScene.SceneWriter()
		sceneWriter["in"].setInput( shaderAssignment["out"] )
		sceneWriter["fileName"].setValue( self.temporaryDirectory() / "test.usda" )
		sceneWriter["task"].execute()

		sceneReader = GafferScene.SceneReader()
		sceneReader["fileName"].setInput( sceneWriter["fileName"] )

		self.assertShaderNetworksEqual(
			sceneReader["out"].attributes( "/plane" )["ri:surface"],
			sceneWriter["in"].attributes( "/plane" )["ri:surface"]
		)

	def testKeepExistingValues( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrSurface" )
		defaultValues = { p.getName() : p.getValue() for p in shader["parameters"] if hasattr( p, "getValue" ) }

		shader["parameters"]["diffuseGain"].setValue( 0.25 )
		shader["parameters"]["diffuseColor"].setValue( imath.Color3f( 1, 2, 3 ) )
		shader["parameters"]["specularDoubleSided"].setValue( True )
		shader["parameters"]["volumeAggregateName"].setValue( "test" )
		modifiedValues = { p.getName() : p.getValue() for p in shader["parameters"] if hasattr( p, "getValue" ) }

		shader.loadShader( "PxrSurface", keepExistingValues = True )
		self.assertEqual(
			{ p.getName() : p.getValue() for p in shader["parameters"] if hasattr( p, "getValue" ) },
			modifiedValues
		)

		shader.loadShader( "PxrSurface", keepExistingValues = False )
		self.assertEqual(
			{ p.getName() : p.getValue() for p in shader["parameters"] if hasattr( p, "getValue" ) },
			defaultValues
		)

	def testDisplacementShaderType( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrDisplace" )
		self.assertEqual( shader["type"].getValue(), "ri:displacement" )

		shader = GafferOSL.OSLShader()
		shader.loadShader( "PxrDisplace" )
		self.assertEqual( shader["type"].getValue(), "osl:displacement" )

	def testPatternShaderType( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrBakePointCloud" )
		self.assertEqual( shader["type"].getValue(), "ri:shader" )

	def testUtilityPatternArray( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrSurface" )

		self.assertIn( "utilityPattern", shader["parameters"] )
		self.assertIsInstance( shader["parameters"]["utilityPattern"], Gaffer.ArrayPlug )
		self.assertIsInstance( shader["parameters"]["utilityPattern"].elementPrototype(), Gaffer.IntPlug )

	def testBXDFParameters( self ) :

		dielectric = GafferRenderMan.RenderManShader()
		dielectric.loadShader( "LamaDielectric" )
		self.assertEqual( dielectric["out"].keys(), [ "bxdf_out" ] )
		self.assertIsInstance( dielectric["out"]["bxdf_out"], GafferRenderMan.BXDFPlug )

		surface = GafferRenderMan.RenderManShader()
		surface.loadShader( "LamaSurface" )
		for parameter in ( "materialFront", "materialBack" ) :
			self.assertIn( parameter, surface["parameters"] )
			self.assertIsInstance( surface["parameters"][parameter], GafferRenderMan.BXDFPlug )

		surface["parameters"]["materialFront"].setInput( dielectric["out"]["bxdf_out"] )

	def testMinAndMaxValues( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrSurface" )

		self.assertTrue( shader["parameters"]["diffuseGain"].hasMinValue() )
		self.assertEqual( shader["parameters"]["diffuseGain"].minValue(), 0.0 )

		self.assertFalse( shader["parameters"]["diffuseExponent"].hasMinValue() )
		self.assertFalse( shader["parameters"]["diffuseExponent"].hasMaxValue() )
		self.assertFalse( shader["parameters"]["diffuseColor"].hasMinValue() )
		self.assertFalse( shader["parameters"]["diffuseColor"].hasMaxValue() )

		shader.loadShader( "PxrDisney" )

		self.assertTrue( shader["parameters"]["subsurfaceColor"].hasMinValue() )
		self.assertEqual( shader["parameters"]["subsurfaceColor"].minValue(), imath.Color3f( 0 ) )

		self.assertTrue( shader["parameters"]["subsurfaceColor"].hasMaxValue() )
		self.assertEqual( shader["parameters"]["subsurfaceColor"].maxValue(), imath.Color3f( 1 ) )

	def testManifoldConnections( self ) :

		# This is testing OSLShader and USDScene more than RenderManShader,
		# but it is testing stuff essential to the operation of the Pxr
		# shaders.

		manifold = GafferOSL.OSLShader( "manifold" )
		manifold.loadShader( "PxrManifold2D" )

		texture = GafferOSL.OSLShader( "texture" )
		texture.loadShader( "PxrTexture" )
		texture["parameters"]["manifold"].setInput( manifold["out"]["result"] )

		constant = GafferRenderMan.RenderManShader( "constant" )
		constant.loadShader( "PxrConstant" )
		constant["parameters"]["emitColor"].setInput( texture["out"]["resultRGB"] )

		plane = GafferScene.Plane()

		shaderAssignment = GafferScene.ShaderAssignment()
		shaderAssignment["in"].setInput( plane["out"] )
		shaderAssignment["shader"].setInput( constant["out"]["bxdf_out"] )

		# Our generic OSL support allows individual struct fields to have values
		# and connections, but USD just treats structs as atomic types that can
		# be connected as a whole but don't really have child fields. Check that
		# we have a top-level connection that works in that world.

		network = shaderAssignment["out"].attributes( "/plane" )["ri:surface"]
		self.assertEqual( network.input( ( "texture", "manifold" ) ), ( "manifold", "result" ) )

		# Write to USD and read back. Check that we still have the top-level
		# connection.

		sceneWriter = GafferScene.SceneWriter()
		sceneWriter["in"].setInput( shaderAssignment["out"] )
		sceneWriter["fileName"].setValue( self.temporaryDirectory() / "test.usda" )
		sceneWriter["task"].execute()

		sceneReader = GafferScene.SceneReader()
		sceneReader["fileName"].setInput( sceneWriter["fileName"] )

		network = sceneReader["out"].attributes( "/plane" )["ri:surface"]
		self.assertEqual( network.input( ( "texture", "manifold" ) ), ( "manifold", "result" ) )

	def testShaderTweakProxy( self ) :

		proxy = GafferScene.ShaderTweakProxy()
		proxy.loadShader( "ri:PxrConstant" )
		self.assertIn( "bxdf_out", proxy["out"] )

	def testDot( self ) :

		script = Gaffer.ScriptNode()

		script["shaderAssignment"] = GafferScene.ShaderAssignment()

		script["shader"] = GafferRenderMan.RenderManShader()
		script["shader"].loadShader( "PxrSurface" )

		script["dot"] = Gaffer.Dot()
		script["dot"].setup( script["shader"]["out"]["bxdf_out"] )
		script["dot"]["in"].setInput( script["shader"]["out"]["bxdf_out"] )

		script["shaderAssignment"]["shader"].setInput( script["dot"]["out"] )

		script2 = Gaffer.ScriptNode()
		script2.execute( script.serialise() )
		self.assertTrue( script["shaderAssignment"]["shader"].getInput().isSame( script["dot"]["out"] ) )
		self.assertTrue( script["shaderAssignment"]["shader"].source().isSame( script["shader"]["out"]["bxdf_out"] ) )

if __name__ == "__main__":
	unittest.main()
