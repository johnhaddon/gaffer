##########################################################################
#
#  Copyright (c) 2020, John Haddon. All rights reserved.
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

import imath

import IECore

import GafferScene
import GafferSceneTest

class ClosestPointSamplerTest( GafferSceneTest.SceneTestCase ) :

	def test( self ) :

		# Build network to perform closest point queries
		# from a plane, against a copy of the same plane
		# converted to a points primitive. Closest points
		# should be exact vertices.

		plane = GafferScene.Plane()
		plane["dimensions"].setValue( imath.V2f( 2 ) )

		planeFilter = GafferScene.PathFilter()
		planeFilter["paths"].setValue( IECore.StringVectorData( [ "/plane" ] ) )

		planeTransform = GafferScene.Transform()
		planeTransform["in"].setInput( plane["out"] )
		planeTransform["filter"].setInput( planeFilter["out"] )

		points = GafferScene.MeshToPoints()
		points["in"].setInput( plane["out"] )
		points["filter"].setInput( planeFilter["out"] )

		pointsTransform = GafferScene.Transform()
		pointsTransform["in"].setInput( points["out"] )
		pointsTransform["filter"].setInput( planeFilter["out"] )

		sampler = GafferScene.ClosestPointSampler()
		sampler["in"].setInput( planeTransform["out"] )
		sampler["source"].setInput( pointsTransform["out"] )
		sampler["filter"].setInput( planeFilter["out"] )
		sampler["sourceLocation"].setValue( "/plane" )
		sampler["prefix"].setValue( "sampled:" )
		self.assertScenesEqual( sampler["out"], plane["out"] )

		# Identical transforms. Closest point should
		# be the same as the query point.

		sampler["primitiveVariables"].setValue( "P" )
		self.assertSceneValid( sampler["out"] )

		inMesh = sampler["in"].object( "/plane" )
		outMesh = sampler["out"].object( "/plane" )
		self.assertEqual( set( outMesh.keys() ), set( inMesh.keys() + [ "sampled:P" ] ) )
		self.assertEqual( outMesh["sampled:P"], inMesh["P"] )

		# Translate source off to one side. A single
		# point is the closest for all query points.

		pointsTransform["transform"]["translate"].setValue( imath.V3f( 5, 5, 0 ) )
		outMesh = sampler["out"].object( "/plane" )
		self.assertEqual(
			outMesh["sampled:P"].data,
			IECore.V3fVectorData(
				[ imath.V3f( 4, 4, 0 ) ] * 4,
				IECore.GeometricData.Interpretation.Point
			)
		)

		# Translate the plane too. Sampled results should
		# be adjusted so that they are relative to the local
		# space of the plane.

		planeTransform["transform"]["translate"].setValue( imath.V3f( -1, 0, 0 ) )
		outMesh = sampler["out"].object( "/plane" )
		self.assertEqual(
			outMesh["sampled:P"].data,
			IECore.V3fVectorData(
				[ imath.V3f( 5, 4, 0 ) ] * 4,
				IECore.GeometricData.Interpretation.Point
			)
		)

if __name__ == "__main__":
	unittest.main()
