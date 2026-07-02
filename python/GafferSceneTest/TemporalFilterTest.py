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

import imath

import IECore

import Gaffer
import GafferScene
import GafferSceneTest

class TemporalFilterTest( GafferSceneTest.SceneTestCase ) :

	class __AnimatedSphere( GafferScene.SceneNode ) :

		def __init__( self, name = "__AnimatedSphere" ) :

			GafferScene.SceneNode.__init__( self, name )

			self["__objectToScene"] = GafferScene.ObjectToScene()

			self["__expression"] = Gaffer.Expression()
			self["__expression"].setExpression( inspect.cleandoc(
				"""
				import imath
				import IECore
				import IECoreScene

				sphere = IECoreScene.SpherePrimitive()

				frame = context.getFrame()
				numVertices = sphere.variableSize( IECoreScene.PrimitiveVariable.Interpolation.Vertex )

				sphere["frame"] = IECoreScene.PrimitiveVariable(
					IECoreScene.PrimitiveVariable.Interpolation.Constant,
					IECore.FloatData( frame )
				)
				sphere["vertexFrame"] = IECoreScene.PrimitiveVariable(
					IECoreScene.PrimitiveVariable.Interpolation.Vertex,
					IECore.FloatVectorData( [ frame ] * numVertices )
				)
				sphere["vectorFrame"] = IECoreScene.PrimitiveVariable(
					IECoreScene.PrimitiveVariable.Interpolation.Constant,
					IECore.V3fData( imath.V3f( frame - 1, frame, frame + 1 ) )
				)
				sphere["pulse"] = IECoreScene.PrimitiveVariable(
					IECoreScene.PrimitiveVariable.Interpolation.Constant,
					IECore.FloatData( int( frame ) % 2 )
				)
				sphere["onOne"] = IECoreScene.PrimitiveVariable(
					IECoreScene.PrimitiveVariable.Interpolation.Constant,
					IECore.FloatData( frame == 1 )
				)

				parent["__objectToScene"]["object"] = sphere
				"""
			) )

			self["out"].setInput( self["__objectToScene"]["out"] )

	IECore.registerRunTimeTyped( __AnimatedSphere )

	def testPassThrough( self ) :

		sphere = self.__AnimatedSphere()

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )

		# No primitive variables specified.
		self.assertSceneHashesEqual( temporalFilter["out"], temporalFilter["in"] )
		self.assertScenesEqual( temporalFilter["out"], temporalFilter["in"] )

		# No filter connected.
		temporalFilter["primitiveVariables"].setValue( "pulse" )
		self.assertSceneHashesEqual( temporalFilter["out"], temporalFilter["in"] )
		self.assertScenesEqual( temporalFilter["out"], temporalFilter["in"] )

		# Filter connected. No pass through.
		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		self.assertNotEqual( temporalFilter["out"].object( "/sphere" ), temporalFilter["in"].object( "/sphere" ) )

	def testNonPrimitive( self ) :

		camera = GafferScene.Camera()

		cameraFilter = GafferScene.PathFilter()
		cameraFilter["paths"].setValue( IECore.StringVectorData( [ "/camera" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( camera["out"] )
		temporalFilter["filter"].setInput( cameraFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )

		self.assertEqual( temporalFilter["out"].object( "/camera" ), temporalFilter["in"].object( "/camera" ) )
		self.assertNotEqual( temporalFilter["out"].objectHash( "/camera" ), temporalFilter["in"].objectHash( "/camera" ) )

	def testBoxFilter( self ) :

		sphere = self.__AnimatedSphere()

		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )
		temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )

		with Gaffer.Context() as context :

			context.setFrame( 1 )
			filtered = temporalFilter["out"].object( "/sphere" )

			# `frame` is a linear sequence centred on 1, so averages to 1.
			self.assertAlmostEqual( filtered["frame"].data.value, 1, delta = 0.0001 )
			# `pulse` is on for odd frames, meaning 3 of our 5 samples.
			self.assertAlmostEqual( filtered["pulse"].data.value, 3 / 5, delta = 0.0001 )
			# `onOne` is zero for everything except frame 1.
			self.assertAlmostEqual( filtered["onOne"].data.value, 1 / 5, delta = 0.0001 )

			context.setFrame( 4 )
			filtered = temporalFilter["out"].object( "/sphere" )

			# linear sequence centred on 4, so averages to 4.
			self.assertAlmostEqual( filtered["frame"].data.value, 4, delta = 0.0001 )
			# only 2 odd frames out of our 5 samples.
			self.assertAlmostEqual( filtered["pulse"].data.value, 2 / 5, delta = 0.0001 )
			# zero everywhere in the filter window.
			self.assertEqual( filtered["onOne"].data.value, 0 )

	def testGaussianFilter( self ) :

		sphere = self.__AnimatedSphere()

		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )
		temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Gaussian )

		# Gaussian is symmetric around the midpoint of the filter, and normalised,
		# so averages to the value of `frame`.
		with Gaffer.Context() as context :

			for frame in ( 0, 3, -1 ) :
				context.setFrame( frame )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertAlmostEqual( filtered["frame"].data.value, frame, delta = 0.0001 )

		# Test specific weights by filtering against a value which is only non-zero
		# on frame 1.

		weights = []
		with Gaffer.Context() as context :

			for frame in range( -1, 4 ) :
				context.setFrame( frame )
				filtered = temporalFilter["out"].object( "/sphere" )
				weights.append( filtered["onOne"].data.value )

		self.assertEqual( weights, list( reversed( weights ) ) ) # Symmetric
		self.assertAlmostEqual( sum( weights ), 1, delta = 0.00001 ) # Normalised
		self.assertAlmostEqual( weights[0], 0.05448869 )
		self.assertAlmostEqual( weights[1], 0.24420136 )
		self.assertAlmostEqual( weights[2], 0.40261996 )

	def testMinMaxFilters( self ) :

		sphere = self.__AnimatedSphere()

		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )

		with Gaffer.Context() as context :

			for frame in range( -4, 5 ) :

				context.setFrame( frame )

				temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Min )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertEqual( filtered["frame"].data.value, frame - 2 )

				temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Max )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertEqual( filtered["frame"].data.value, frame + 2 )

	def testRampFilter( self ) :

		sphere = self.__AnimatedSphere()

		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )
		temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Ramp )

		ramp = IECore.Rampff(
			[
				( 0, 0 ),
				( 1, 1 ),
			],
			IECore.RampInterpolation.Linear
		)
		temporalFilter["ramp"].setValue( ramp )

		expectedWeights = [ 0, 0.1, 0.2, 0.3, 0.4 ]

		filtered = temporalFilter["out"].object( "/sphere" )
		self.assertAlmostEqual(
			filtered["frame"].data.value,
			sum( expectedWeights[x] * ( x - 1 ) for x in range( 0, 5 ) ),
			delta = 0.0001
		)

	def testRelativeRange( self ) :

		sphere = self.__AnimatedSphere()

		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )
		temporalFilter["start"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Relative )
		temporalFilter["end"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Relative )
		temporalFilter["start"]["frame"].setValue( -5 )
		temporalFilter["end"]["frame"].setValue( 3 )

		with Gaffer.Context() as context :

			for frame in ( 0, 5 ) :

				context.setFrame( frame )

				temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Min )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertEqual( filtered["frame"].data.value, frame - 5 )

				temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Max )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertEqual( filtered["frame"].data.value, frame + 3 )

	def testAbsoluteRange( self ) :

		sphere = self.__AnimatedSphere()

		sphereFilter = GafferScene.PathFilter()
		sphereFilter["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )

		temporalFilter = GafferScene.TemporalFilter()
		temporalFilter["in"].setInput( sphere["out"] )
		temporalFilter["filter"].setInput( sphereFilter["out"] )
		temporalFilter["primitiveVariables"].setValue( "*" )
		temporalFilter["start"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Absolute )
		temporalFilter["end"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Absolute )
		temporalFilter["start"]["frame"].setValue( 0 )
		temporalFilter["end"]["frame"].setValue( 10 )

		with Gaffer.Context() as context :

			for frame in range( 0, 5 ) :

				context.setFrame( frame )

				temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Min )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertEqual( filtered["frame"].data.value, 0 )

				temporalFilter["filterType"].setValue( GafferScene.TemporalFilter.Filter.Max )
				filtered = temporalFilter["out"].object( "/sphere" )
				self.assertEqual( filtered["frame"].data.value, 10 )

if __name__ == "__main__" :
	unittest.main()
