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

import Gaffer
import GafferScene
import GafferSceneTest
import GafferTest


class CurvesTangentsTest( GafferSceneTest.SceneTestCase ) :

	def makeStraightCurveScene( self, axis=imath.V3f( 1, 0, 0 ), numCurves=1, numVerts=3 ) :

		vertsPerCurve = IECore.IntVectorData( [numVerts] * numCurves )
		p = IECore.V3fVectorData(
			[ axis * ( float(i) / float(numVerts - 1) ) for _ in range( numCurves ) for i in range( numVerts ) ]
		)
		curves = IECoreScene.CurvesPrimitive( vertsPerCurve, IECore.CubicBasisf.linear(), False, p )

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )

		return objectToScene

	def makeFilteredNode( self, upstream ) :

		# Stored on self to prevent the PathFilter being garbage collected,
		# which would disconnect the filter plug.
		self._pathFilter = GafferScene.PathFilter()
		self._pathFilter["paths"].setValue( IECore.StringVectorData( ["/object"] ) )

		node = GafferScene.CurvesTangents()
		node["in"].setInput( upstream["out"] )
		node["filter"].setInput( self._pathFilter["out"] )

		return node

	def testTangentsAlongX( self ) :

		scene = self.makeStraightCurveScene( axis=imath.V3f( 1, 0, 0 ), numVerts=3 )
		node = self.makeFilteredNode( scene )

		obj = node["out"].object( "/object" )

		self.assertIn( "tangent", obj.keys() )
		tangentVar = obj["tangent"]
		self.assertEqual( tangentVar.interpolation, IECoreScene.PrimitiveVariable.Interpolation.Vertex )
		self.assertEqual( len( tangentVar.data ), 3 )

		for t in tangentVar.data :
			self.assertEqualWithAbsError( t.normalized(), imath.V3f( 1, 0, 0 ), 0.000001 )

	def testTangentsAlongY( self ) :

		scene = self.makeStraightCurveScene( axis=imath.V3f( 0, 1, 0 ), numVerts=4 )
		node = self.makeFilteredNode( scene )

		obj = node["out"].object( "/object" )
		tangentVar = obj["tangent"]
		self.assertEqual( len( tangentVar.data ), 4 )

		for t in tangentVar.data :
			self.assertEqualWithAbsError( t.normalized(), imath.V3f( 0, 1, 0 ), 0.000001 )

	def testMultipleCurves( self ) :

		vertsPerCurve = IECore.IntVectorData( [2, 2] )
		p = IECore.V3fVectorData( [
			imath.V3f( 0, 0, 0 ), imath.V3f( 1, 0, 0 ),  # curve 0 along X
			imath.V3f( 0, 0, 0 ), imath.V3f( 0, 1, 0 ),  # curve 1 along Y
		] )
		curves = IECoreScene.CurvesPrimitive( vertsPerCurve, IECore.CubicBasisf.linear(), False, p )

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )
		node = self.makeFilteredNode( objectToScene )

		obj = node["out"].object( "/object" )
		tangentVar = obj["tangent"]
		self.assertEqual( len( tangentVar.data ), 4 )

		for t in tangentVar.data[:2] :
			self.assertEqualWithAbsError( t.normalized(), imath.V3f( 1, 0, 0 ), 0.000001 )

		for t in tangentVar.data[2:] :
			self.assertEqualWithAbsError( t.normalized(), imath.V3f( 0, 1, 0 ), 0.000001 )

	def testRename( self ) :

		scene = self.makeStraightCurveScene()
		node = self.makeFilteredNode( scene )
		node["tangent"].setValue( "myTangent" )

		obj = node["out"].object( "/object" )
		self.assertIn( "myTangent", obj.keys() )
		self.assertNotIn( "tangent", obj.keys() )
		self.assertEqual( len( obj["myTangent"].data ), 3 )

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

		mesh = IECoreScene.MeshPrimitive.createBox( imath.Box3f( imath.V3f( -1 ), imath.V3f( 1 ) ) )
		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( mesh )

		node = self.makeFilteredNode( objectToScene )

		obj = node["out"].object( "/object" )
		self.assertIsInstance( obj, IECoreScene.MeshPrimitive )
		self.assertNotIn( "tangent", obj.keys() )

	def testMissingPositionPassThrough( self ) :

		scene = self.makeStraightCurveScene()
		node = self.makeFilteredNode( scene )
		node["position"].setValue( "nonexistent" )

		# Should pass through unchanged when the named position variable is absent
		obj = node["out"].object( "/object" )
		self.assertNotIn( "tangent", obj.keys() )

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


	def testOpenCubicPhantomCVs( self ) :

		# 6-point open CatmullRom S-curve. CVs 0 and 5 are phantom hull points
		# not on the curve; their tangents should be copied from the nearest
		# varying position (the curve start and end respectively).
		vertsPerCurve = IECore.IntVectorData( [6] )
		p = IECore.V3fVectorData( [
			imath.V3f( 0, -1, 0 ),
			imath.V3f( 1, -1, 0 ),
			imath.V3f( 2,  0, 0 ),
			imath.V3f( 3,  0, 0 ),
			imath.V3f( 4,  1, 0 ),
			imath.V3f( 5,  1, 0 ),
		] )
		curves = IECoreScene.CurvesPrimitive( vertsPerCurve, IECore.CubicBasisf.catmullRom(), False, p )

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )
		node = self.makeFilteredNode( objectToScene )

		obj = node["out"].object( "/object" )
		tangentVar = obj["tangent"]
		self.assertEqual( tangentVar.interpolation, IECoreScene.PrimitiveVariable.Interpolation.Vertex )
		self.assertEqual( len( tangentVar.data ), 6 )

		tangents = tangentVar.data
		# Phantom CVs should receive the tangent from the nearest curve endpoint.
		self.assertEqual( tangents[0], tangents[1] )
		self.assertEqual( tangents[5], tangents[4] )
		# The S-curve travels in the +X direction throughout, so all tangents
		# should have a positive X component.
		for t in tangents :
			self.assertGreater( t.x, 0 )

	def testPeriodicCubic( self ) :

		# 4-point periodic CatmullRom approximating a circle in the XY plane,
		# going counterclockwise. For periodic curves varying count equals vertex
		# count, so no phantom CV handling is needed.
		vertsPerCurve = IECore.IntVectorData( [4] )
		p = IECore.V3fVectorData( [
			imath.V3f(  1,  0, 0 ),
			imath.V3f(  0,  1, 0 ),
			imath.V3f( -1,  0, 0 ),
			imath.V3f(  0, -1, 0 ),
		] )
		curves = IECoreScene.CurvesPrimitive( vertsPerCurve, IECore.CubicBasisf.catmullRom(), True, p )

		objectToScene = GafferScene.ObjectToScene()
		objectToScene["object"].setValue( curves )
		node = self.makeFilteredNode( objectToScene )

		obj = node["out"].object( "/object" )
		tangentVar = obj["tangent"]
		self.assertEqual( tangentVar.interpolation, IECoreScene.PrimitiveVariable.Interpolation.Vertex )
		self.assertEqual( len( tangentVar.data ), 4 )

		# For CatmullRom the tangent at each CV is proportional to
		# (next - prev), giving the exact circle tangent at each of the
		# 4 symmetric points.
		tangents = tangentVar.data
		self.assertEqualWithAbsError( tangents[0].normalized(), imath.V3f(  0,  1, 0 ), 0.000001 )
		self.assertEqualWithAbsError( tangents[1].normalized(), imath.V3f( -1,  0, 0 ), 0.000001 )
		self.assertEqualWithAbsError( tangents[2].normalized(), imath.V3f(  0, -1, 0 ), 0.000001 )
		self.assertEqualWithAbsError( tangents[3].normalized(), imath.V3f(  1,  0, 0 ), 0.000001 )

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
