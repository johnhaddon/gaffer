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

import inspect
import unittest

import IECore

import Gaffer
import GafferScene
import GafferSceneTest

class SceneStatsTest( GafferSceneTest.SceneTestCase ) :

	def testAddAndRemoveQuery( self ) :

		stats = GafferScene.SceneStats()

		self.assertEqual( len( stats["queries"] ), 0 )
		self.assertEqual( len( stats["out"] ), 0 )

		stats.addQuery( Gaffer.IntPlug(), "test" )

		self.assertEqual( len( stats["queries"] ), 1 )
		self.assertEqual( len( stats["out"] ), 1 )
		self.assertIsInstance( stats["queries"]["test"], Gaffer.OptionalValuePlug )
		self.assertEqual( stats["queries"]["test"]["enabled"].getValue(), True )
		self.assertEqual( stats["queries"]["test"]["enabled"].defaultValue(), True )
		self.assertIsInstance( stats["queries"]["test"]["value"], Gaffer.IntPlug )
		self.assertIsInstance( stats["out"]["test"]["sum"], Gaffer.IntPlug )

		self.assertTrue( stats.outPlugFromQuery( stats["queries"]["test"] ).isSame( stats["out"]["test"] ) )
		self.assertTrue( stats.queryPlug( stats["out"]["test"] ).isSame( stats["queries"]["test"] ) )

		stats.removeQuery( stats["queries"]["test"] )

		self.assertEqual( len( stats["queries"] ), 0 )
		self.assertEqual( len( stats["out"] ), 0 )

	def testQueryPlugFromDescendant( self ) :

		stats = GafferScene.SceneStats()
		stats.addQuery( Gaffer.V3fPlug(), "position" )

		# queryPlug() should work when called with a component of a compound output
		self.assertTrue(
			stats.queryPlug( stats["out"]["position"]["sum"]["x"] ).isSame( stats["queries"]["position"] )
		)

	def testStats( self ) :

		# /group
		#    /sphere
		#    /cube
		#    /plane

		sphere = GafferScene.Sphere()
		cube = GafferScene.Cube()
		plane = GafferScene.Plane()

		group = GafferScene.Group()
		group["in"][0].setInput( sphere["out"] )
		group["in"][1].setInput( cube["out"] )
		group["in"][2].setInput( plane["out"] )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/*/*" ] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( group["out"] )
		stats["filter"].setInput( pathFilter["out"] )

		stats.addQuery( Gaffer.IntPlug(), "pInPath" )
		stats.addQuery( Gaffer.IntPlug(), "hInPath" )

		stats["expression"] = Gaffer.Expression()
		stats["expression"].setExpression(
			inspect.cleandoc(
				"""
				import GafferScene
				path = GafferScene.ScenePlug.pathToString( context["scene:path"] )
				parent["queries"]["pInPath"]["value"] = path.count( "p" )
				parent["queries"]["hInPath"]["value"] = path.count( "h" )
				"""
			)
		)

		self.assertEqual( stats["out"]["pInPath"]["count"].getValue(), 3 )
		self.assertEqual( stats["out"]["pInPath"]["sum"].getValue(), 5 )
		self.assertEqual( stats["out"]["pInPath"]["min"].getValue(), 1 )
		self.assertEqual( stats["out"]["pInPath"]["max"].getValue(), 2 )

		self.assertEqual( stats["out"]["hInPath"]["count"].getValue(), 3 )
		self.assertEqual( stats["out"]["hInPath"]["sum"].getValue(), 1 )
		self.assertEqual( stats["out"]["hInPath"]["min"].getValue(), 0 )
		self.assertEqual( stats["out"]["hInPath"]["max"].getValue(), 1 )

		stats["queries"]["pInPath"]["enabled"].setValue( False )

		self.assertEqual( stats["out"]["pInPath"]["count"].getValue(), 0 )
		self.assertEqual( stats["out"]["pInPath"]["sum"].getValue(), 0 )
		self.assertEqual( stats["out"]["pInPath"]["min"].getValue(), 0 )
		self.assertEqual( stats["out"]["pInPath"]["max"].getValue(), 0 )

	def testAllInputTypes( self ) :

		sphere = GafferScene.Sphere()
		group = GafferScene.Group()
		group["in"][0].setInput( sphere["out"] )

		allFilter = GafferScene.PathFilter()
		allFilter["paths"].setValue( IECore.StringVectorData( [ "/*/..." ] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( group["out"] )
		stats["filter"].setInput( allFilter["out"] )

		for plugType in [
			Gaffer.BoolPlug,
			Gaffer.IntPlug, Gaffer.FloatPlug,
			Gaffer.V2iPlug, Gaffer.V3iPlug,
			Gaffer.V2fPlug, Gaffer.V3fPlug,
			Gaffer.Color3fPlug, Gaffer.Color4fPlug,
		] :

			query = stats.addQuery( plugType() )
			query["value"].setValue( plugType.ValueType( 1 ) )

			output = stats.outPlugFromQuery( query )
			self.assertEqual( output["sum"].getValue(), query["value"].getValue() * 2 )

	def testFilterLimitsContributions( self ) :

		sphere = GafferScene.Sphere()
		cube = GafferScene.Cube()

		group = GafferScene.Group()
		group["in"][0].setInput( sphere["out"] )
		group["in"][1].setInput( cube["out"] )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/group/sphere" ] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( group["out"] )
		stats["filter"].setInput( pathFilter["out"] )

		stats.addQuery( Gaffer.IntPlug(), "test" )
		stats["queries"]["test"]["value"].setValue( 2 )

		self.assertEqual( stats["out"]["test"]["count"].getValue(), 1 )
		self.assertEqual( stats["out"]["test"]["sum"].getValue(), 2 )

		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/group", "/group/sphere" ] ) )

		self.assertEqual( stats["out"]["test"]["count"].getValue(), 2 )
		self.assertEqual( stats["out"]["test"]["sum"].getValue(), 4 )

	def testRenameInput( self ) :

		sphere = GafferScene.Sphere()

		allFilter = GafferScene.PathFilter()
		allFilter["paths"].setValue( IECore.StringVectorData( [ "/*" ] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( sphere["out"] )
		stats["filter"].setInput( allFilter["out"] )

		stats.addQuery( Gaffer.IntPlug( defaultValue = 1 ), "foo" )
		self.assertEqual( stats["out"]["foo"]["sum"].getValue(), 1 )

		stats["queries"]["foo"].setName( "bar" )
		self.assertEqual( stats["out"].keys(), [ "bar" ] )
		self.assertEqual( stats["out"]["bar"]["sum"].getValue(), 1 )

		stats["queries"]["bar"].setValue( 2 )
		self.assertEqual( stats["out"]["bar"]["sum"].getValue(), 2 )

	def testIdenticalStatsWithDifferentNames( self ) :

		sphere = GafferScene.Sphere()

		allFilter = GafferScene.PathFilter()
		allFilter["paths"].setValue( IECore.StringVectorData( [ "/*" ] ) )

		for name in ( "toto", "tata" ) :

			with self.subTest( name = name ) :

				stats = GafferScene.SceneStats()
				stats["scene"].setInput( sphere["out"] )
				stats["filter"].setInput( allFilter["out"] )

				stats.addQuery( Gaffer.IntPlug( defaultValue = 1 ), name )
				self.assertEqual( stats["out"][name]["sum"].getValue(), 1 )

	def testNoMatches( self ) :

		stats = GafferScene.SceneStats()

		stats.addQuery( Gaffer.IntPlug(), "test" )
		stats["queries"]["test"]["value"].setValue( 1 )

		self.assertEqual( stats["out"]["test"]["count"].getValue(), 0 )
		self.assertEqual( stats["out"]["test"]["sum"].getValue(), 0 )
		self.assertEqual( stats["out"]["test"]["min"].getValue(), 0 )
		self.assertEqual( stats["out"]["test"]["max"].getValue(), 0 )

	def testPrimitiveQuery( self ) :

		cube = GafferScene.Cube()
		plane = GafferScene.Plane()

		group = GafferScene.Group()
		group["in"][0].setInput( cube["out"] )
		group["in"][1].setInput( plane["out"] )

		primitiveQuery = GafferScene.PrimitiveQuery()
		primitiveQuery["scene"].setInput( group["out"] )
		primitiveQuery["location"].setValue( "${scene:path}" )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/group/cube", "/group/plane" ] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( group["out"] )
		stats["filter"].setInput( pathFilter["out"] )

		stats.addQuery( Gaffer.IntPlug(), "vertexCount" )
		stats["queries"]["vertexCount"]["value"].setInput( primitiveQuery["vertex"] )

		self.assertEqual( stats["out"]["vertexCount"]["count"].getValue(), 2 )
		self.assertEqual( stats["out"]["vertexCount"]["sum"].getValue(), 12 )

if __name__ == "__main__" :
	unittest.main()
