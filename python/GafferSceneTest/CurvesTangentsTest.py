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
import IECoreScene

import GafferScene
import GafferSceneTest
import GafferTest

class CurvesTangentsTest( GafferSceneTest.SceneTestCase ) :

	def makeFilteredNode( self, upstream ) :

		# Stored on self to prevent the PathFilter being garbage collected,
		# which would disconnect the filter plug.
		self._pathFilter = GafferScene.PathFilter()
		self._pathFilter["paths"].setValue( IECore.StringVectorData( ["/object"] ) )

		node = GafferScene.CurvesTangents()
		node["in"].setInput( upstream["out"] )
		node["filter"].setInput( self._pathFilter["out"] )

		return node

	def testLinearNonPeriodic( self ) :

		curves = IECoreScene.CurvesPrimitive(
			IECore.IntVectorData( [ 2, 2 ] ), IECore.CubicBasisf.linear(), False,
			IECore.V3fVectorData( [
				imath.V3f( 0, 0, 0 ), imath.V3f( 1, 0, 0 ),  # curve 0 along X
				imath.V3f( 0, 0, 0 ), imath.V3f( 0, 1, 0 ),  # curve 1 along Y
			] )
		)

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/object" ] ) )

		curveTangents = GafferScene.CurvesTangents()
		curveTangents["in"].setInput( objectToScene["out"] )
		curveTangents["filter"].setInput( pathFilter["out"] )

		curves = curveTangents["out"].object( "/object" )
		self.assertTrue( curves.arePrimitiveVariablesValid() )
		self.assertTrue( "tangent" in curves )
		self.assertEqual( curves["tangent"].interpolation, IECoreScene.PrimitiveVariable.Interpolation.Vertex )

		self.assertEqual(
			curves["tangent"].data,
			IECore.V3fVectorData(
				[
					imath.V3f( 1, 0, 0 ), imath.V3f( 1, 0, 0 ),
					imath.V3f( 0, 1, 0 ), imath.V3f( 0, 1, 0 ),
				],
				IECore.GeometricData.Interpretation.Vector
			)
		)

	def testCatmullRomPeriodic( self ) :

		# Circle in XY plane
		curves = IECoreScene.CurvesPrimitive(
			IECore.IntVectorData( [ 4 ] ), IECore.CubicBasisf.catmullRom(), True,
			IECore.V3fVectorData( [
				imath.V3f( -1, 0, 0 ), imath.V3f( 0, 1, 0 ),
				imath.V3f( 1, 0, 0 ), imath.V3f( 0, -1, 0 ),
			] )
		)

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )

		pathFilter = GafferScene.PathFilter()
		pathFilter["paths"].setValue( IECore.StringVectorData( [ "/object" ] ) )

		curveTangents = GafferScene.CurvesTangents()
		curveTangents["in"].setInput( objectToScene["out"] )
		curveTangents["filter"].setInput( pathFilter["out"] )

		curves = curveTangents["out"].object( "/object" )
		self.assertTrue( curves.arePrimitiveVariablesValid() )
		self.assertTrue( "tangent" in curves )
		self.assertEqual( curves["tangent"].interpolation, IECoreScene.PrimitiveVariable.Interpolation.Vertex )
		self.assertEqual( curves["tangent"].data.getInterpretation(), IECore.GeometricData.Interpretation.Vector )

		print( curves["tangent"].data )
		self.assertEqual(
			curves["tangent"].data,
			IECore.V3fVectorData(
				[
					imath.V3f( 0, 1, 0 ), imath.V3f( 1, 0, 0 ),
					imath.V3f( 0, -1, 0 ), imath.V3f( -1, 0, 0 ),
				],
				IECore.GeometricData.Interpretation.Vector
			)
		)

	def testTangentName( self ) :

		scene = self.makeStraightCurveScene()
		node = self.makeFilteredNode( scene )
		node["tangent"].setValue( "myTangent" )

		curves = node["out"].object( "/object" )
		self.assertIn( "myTangent", curves )
		self.assertNotIn( "tangent", curves )

	def testAlternativePosition( self ) :

		vertsPerCurve = IECore.IntVectorData( [3] )
		p = IECore.V3fVectorData( [imath.V3f( 0, 0, 0 ), imath.V3f( 0, 1, 0 ), imath.V3f( 0, 2, 0 )] )
		pref = IECore.V3fVectorData( [imath.V3f( 0, 0, 0 ), imath.V3f( 1, 0, 0 ), imath.V3f( 2, 0, 0 )] )

		curves = IECoreScene.CurvesPrimitive( vertsPerCurve, IECore.CubicBasisf.linear(), False, p )
		curves["Pref"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Vertex, pref
		)

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )
		node = self.makeFilteredNode( objectToScene )
		node["position"].setValue( "Pref" )

		obj = node["out"].object( "/object" )
		tangentVar = obj["tangent"]
		self.assertEqual( len( tangentVar.data ), 3 )

		for t in tangentVar.data :
			self.assertEqualWithAbsError( t.normalized(), imath.V3f( 1, 0, 0 ), 0.000001 )

	def testNonCurvesPassThrough( self ) :

		cube = GafferScene.Cube()

		cubeFilter = GafferScene.PathFilter()
		cubeFilter["paths"].setValue( IECore.StringVectorData( [ "cube" ] ) )

		node = GafferScene.CurvesTangents()
		node["in"].setInput( cube["out"] )
		node["filter"].setInput( cubeFilter["out"] )

		self.assertScenesEqual( node["out"], node["in"] )

	def testMissingPositionPassThrough( self ) :

		grid = GafferScene.Grid()

		allFilter = GafferScene.PathFilter()
		allFilter["paths"].setValue( IECore.StringVectorData( [ "..." ] ) )

		node = GafferScene.CurvesTangents()
		node["in"].setInput( grid["out"] )
		node["filter"].setInput( allFilter["out"] )
		node["position"].setValue( "nonexistentP" )

		self.assertScenesEqual( node["out"], node["in"] )

	def testPreservesExistingVariables( self ) :

		scene = self.makeStraightCurveScene()
		node = self.makeFilteredNode( scene )

		obj = node["out"].object( "/object" )
		self.assertIn( "P", obj.keys() )

	def testHashChangesWithPlugs( self ) :

		scene = self.makeStraightCurveScene()
		node = self.makeFilteredNode( scene )

		h1 = node["out"].objectHash( "/object" )

		node["tangent"].setValue( "renamed" )
		h2 = node["out"].objectHash( "/object" )
		self.assertNotEqual( h1, h2 )

		node["tangent"].setValue( "tangent" )
		self.assertEqual( node["out"].objectHash( "/object" ), h1 )

		node["position"].setValue( "Pref" )
		h3 = node["out"].objectHash( "/object" )
		self.assertNotEqual( h1, h3 )

	@GafferTest.TestRunner.PerformanceTestMethod()
	def testPerformance( self ) :

		# 10,000 curves of 100 vertices each = 1M vertices total.
		numCurves = 10000
		numVertsPerCurve = 100

		vertsPerCurve = IECore.IntVectorData( [numVertsPerCurve] * numCurves )
		p = IECore.V3fVectorData(
			[
				imath.V3f( c, float(v) / float(numVertsPerCurve - 1), 0 )
				for c in range( numCurves )
				for v in range( numVertsPerCurve )
			]
		)
		curves = IECoreScene.CurvesPrimitive( vertsPerCurve, IECore.CubicBasisf.linear(), False, p )

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )

		self._pathFilter = GafferScene.PathFilter()
		self._pathFilter["paths"].setValue( IECore.StringVectorData( ["/object"] ) )

		node = GafferScene.CurvesTangents()
		node["in"].setInput( objectToScene["out"] )
		node["filter"].setInput( self._pathFilter["out"] )

		# Precache input so it is not included in the measurement.
		node["in"].object( "/object" )

		with GafferTest.TestRunner.PerformanceScope() :
			node["out"].object( "/object" )

if __name__ == "__main__" :
	unittest.main()
