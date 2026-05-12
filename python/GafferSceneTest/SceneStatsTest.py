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

import unittest

import imath

import IECore

import Gaffer
import GafferScene
import GafferSceneTest

class SceneStatsTest( GafferSceneTest.SceneTestCase ) :

	def testAddAndRemoveQuery( self ) :

		stats = GafferScene.SceneStats()

		self.assertEqual( len( stats["queries"].children() ), 0 )
		self.assertEqual( len( stats["out"].children() ), 0 )

		stats.addQuery( Gaffer.IntPlug(), "count" )

		self.assertEqual( len( stats["queries"].children() ), 1 )
		self.assertEqual( len( stats["out"].children() ), 1 )
		self.assertIsInstance( stats["queries"]["count"], Gaffer.IntPlug )
		self.assertIsInstance( stats["out"]["count"], Gaffer.IntPlug )

		self.assertTrue( stats.outPlugFromQuery( stats["queries"]["count"] ).isSame( stats["out"]["count"] ) )
		self.assertTrue( stats.queryPlug( stats["out"]["count"] ).isSame( stats["queries"]["count"] ) )

		stats.removeQuery( stats["queries"]["count"] )

		self.assertEqual( len( stats["queries"].children() ), 0 )
		self.assertEqual( len( stats["out"].children() ), 0 )

	def testQueryPlugFromDescendant( self ) :

		stats = GafferScene.SceneStats()
		stats.addQuery( Gaffer.V3fPlug(), "position" )

		# queryPlug() should work when called with a component of a compound output
		self.assertTrue(
			stats.queryPlug( stats["out"]["position"]["x"] ).isSame( stats["queries"]["position"] )
		)

	def testSum( self ) :

		sphere = GafferScene.Sphere()
		cube = GafferScene.Cube()
		plane = GafferScene.Plane()

		group = GafferScene.Group()
		group["in"][0].setInput( sphere["out"] )
		group["in"][1].setInput( cube["out"] )
		group["in"][2].setInput( plane["out"] )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [
			"/group/sphere", "/group/cube", "/group/plane"
		] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( group["out"] )
		stats["filter"].setInput( pathFilter["out"] )

		stats.addQuery( Gaffer.IntPlug(), "count" )
		stats["queries"]["count"].setValue( 1 )

		self.assertEqual( stats["out"]["count"].getValue(), 3 )

	def testAllQueryTypes( self ) :

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
			query.setValue( plugType.ValueType( 1 ) )

			output = stats.outPlugFromQuery( query )
			self.assertEqual( output.getValue(), query.getValue() * 2 )

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

		stats.addQuery( Gaffer.IntPlug(), "count" )
		stats["queries"]["count"].setValue( 7 )

		self.assertEqual( stats["out"]["count"].getValue(), 7 )

	def testNoMatches( self ) :

		cube = GafferScene.Cube()

		pathFilter = GafferScene.PathFilter()

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( cube["out"] )
		stats["filter"].setInput( pathFilter["out"] )

		stats.addQuery( Gaffer.IntPlug(), "count" )
		stats["queries"]["count"].setValue( 99 )

		self.assertEqual( stats["out"]["count"].getValue(), 0 )

	def testConnectedQuery( self ) :

		# Verify that a query input connected to another node is evaluated in
		# the context of each matched location. PrimitiveQuery with a fixed
		# location returns the same value regardless of the traversal path, so
		# the sum equals that value multiplied by the number of matched locations.
		cube = GafferScene.Cube()

		group = GafferScene.Group()
		group["in"][0].setInput( cube["out"] )
		group["in"][1].setInput( cube["out"] )

		primitiveQuery = GafferScene.PrimitiveQuery()
		primitiveQuery["scene"].setInput( group["out"] )
		primitiveQuery["location"].setValue( "/group/cube" )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/group/cube", "/group/cube1" ] ) )

		stats = GafferScene.SceneStats()
		stats["scene"].setInput( group["out"] )
		stats["filter"].setInput( pathFilter["out"] )

		stats.addQuery( Gaffer.IntPlug(), "vertexCount" )
		stats["queries"]["vertexCount"].setInput( primitiveQuery["vertex"] )

		self.assertEqual(
			stats["out"]["vertexCount"].getValue(),
			primitiveQuery["vertex"].getValue() * 2
		)

if __name__ == "__main__" :
	unittest.main()
