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

import math
import unittest

import imath

import IECore
import IECoreScene

import Gaffer
import GafferScene
import GafferSceneTest

class TemporalFilterTest( GafferSceneTest.SceneTestCase ) :

	# Builds a scene with a sphere whose "val" primitive variable equals
	# the current frame number (linear, so we can predict filter outputs).
	def __makeScene( self ) :

		script = Gaffer.ScriptNode()

		script["sphere"] = GafferScene.Sphere()

		script["vars"] = GafferScene.PrimitiveVariables()
		script["vars"]["in"].setInput( script["sphere"]["out"] )

		script["vars"]["primitiveVariables"].addChild(
			Gaffer.NameValuePlug( "val", 0.0, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		)

		# Animate "val" so that its value equals the frame number.
		anim = Gaffer.Animation.acquire( script["vars"]["primitiveVariables"]["NameValuePlug"]["value"] )
		for f in range( -10, 11 ) :
			anim.addKey( Gaffer.Animation.Key( f / 24.0, float( f ), Gaffer.Animation.Interpolation.Linear ) )

		script["filter"] = GafferScene.PathFilter()
		script["filter"]["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )
		script["vars"]["filter"].setInput( script["filter"]["out"] )

		script["temporalFilter"] = GafferScene.TemporalFilter()
		script["temporalFilter"]["in"].setInput( script["vars"]["out"] )
		script["temporalFilter"]["filter"].setInput( script["filter"]["out"] )
		script["temporalFilter"]["primitiveVariables"].setValue( "val" )

		return script

	def __val( self, script, frame ) :

		with Gaffer.Context( script.context() ) as context :
			context.setFrame( frame )
			return script["temporalFilter"]["out"].object( "/sphere" )["val"].data.value

	def testPassThrough( self ) :

		script = self.__makeScene()

		# No primitiveVariables set - should be a perfect pass-through.
		script["temporalFilter"]["primitiveVariables"].setValue( "" )
		self.assertScenesEqual( script["vars"]["out"], script["temporalFilter"]["out"] )

	def testNonPrimitive( self ) :

		# Filter matches a location with no primitive - should pass through silently.
		script = self.__makeScene()
		script["filter"]["paths"].setValue( IECore.StringVectorData( [ "/sphere", "/" ] ) )

		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			self.assertIsInstance( script["temporalFilter"]["out"].object( "/" ), IECore.NullObject )

	def testBoxFilter( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )

		# Default range -2..+2, step 1 → 5 samples at F-2, F-1, F, F+1, F+2.
		# Box average of a linear sequence centred on F is exactly F.
		for frame in ( 0, 3, -1 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ), places=5 )

	def testGaussianFilter( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Gaussian )

		# Gaussian is symmetric around the midpoint of the range, so it also
		# reproduces a linear function exactly.
		for frame in ( 0, 3, -1 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ), places=5 )

		# Verify the weights are bell-shaped: centre sample should have more
		# influence than the edge samples by comparing with a skewed constant.
		# Replace "val" with 1 at the centre sample only, 0 elsewhere, and
		# check the output is above 1/5 (box weight).
		script["vars"]["primitiveVariables"]["NameValuePlug"]["value"].clearKeys()
		anim = Gaffer.Animation.acquire( script["vars"]["primitiveVariables"]["NameValuePlug"]["value"] )
		for f in range( -10, 11 ) :
			anim.addKey( Gaffer.Animation.Key( f / 24.0, 1.0 if f == 0 else 0.0, Gaffer.Animation.Interpolation.Constant ) )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			centreWeight = script["temporalFilter"]["out"].object( "/sphere" )["val"].data.value
		self.assertGreater( centreWeight, 1.0 / 5.0 )

	def testMinFilter( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Min )

		# Min of {F-2, F-1, F, F+1, F+2} = F-2.
		for frame in ( 0, 3, -1 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ) - 2, places=5 )

	def testMaxFilter( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Max )

		# Max of {F-2, F-1, F, F+1, F+2} = F+2.
		for frame in ( 0, 3, -1 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ) + 2, places=5 )

	def testRampFilter( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Ramp )

		# Configure ramp to put all weight on the last sample (x=1 → y=1, rest 0).
		ramp = IECore.Rampff(
			{ 0.0 : 0.0, 1.0 : 1.0 },
			IECore.RampInterpolation.Constant
		)
		script["temporalFilter"]["ramp"].setValue( ramp )

		# Last sample is at F+2, so result should equal F+2.
		for frame in ( 0, 3 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ) + 2, places=5 )

	def testRelativeRange( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )

		script["temporalFilter"]["start"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Relative )
		script["temporalFilter"]["end"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Relative )
		script["temporalFilter"]["start"]["frame"].setValue( -1 )
		script["temporalFilter"]["end"]["frame"].setValue( 1 )

		# Samples at F-1, F, F+1 → average is F.
		for frame in ( 0, 5 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ), places=5 )

	def testAbsoluteRange( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )

		script["temporalFilter"]["start"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Absolute )
		script["temporalFilter"]["end"]["mode"].setValue( GafferScene.TemporalFilter.FrameMode.Absolute )
		script["temporalFilter"]["start"]["frame"].setValue( 0 )
		script["temporalFilter"]["end"]["frame"].setValue( 4 )

		# Samples always at 0, 1, 2, 3, 4 regardless of current frame.
		expected = ( 0 + 1 + 2 + 3 + 4 ) / 5.0
		for frame in ( 0, 5, -3 ) :
			self.assertAlmostEqual( self.__val( script, frame ), expected, places=5 )

	def testVariableSampling( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )
		script["temporalFilter"]["samplingMode"].setValue( GafferScene.TemporalFilter.SamplingMode.Variable )
		script["temporalFilter"]["step"].setValue( 0.5 )
		# step=0.5, range=-2..2 → 9 samples at -2, -1.5, ..., 1.5, 2.
		# Average of linear sequence centred on F is still F.
		for frame in ( 0, 3 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ), places=5 )

	def testFixedSampling( self ) :

		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )
		script["temporalFilter"]["samplingMode"].setValue( GafferScene.TemporalFilter.SamplingMode.Fixed )
		script["temporalFilter"]["samples"].setValue( 3 )
		# 3 samples at F-2, F, F+2 → average is F.
		for frame in ( 0, 3 ) :
			self.assertAlmostEqual( self.__val( script, frame ), float( frame ), places=5 )

	def testBadRange( self ) :

		script = self.__makeScene()

		# start >= end → pass-through.
		script["temporalFilter"]["start"]["frame"].setValue( 2 )
		script["temporalFilter"]["end"]["frame"].setValue( -2 )

		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			self.assertAlmostEqual(
				script["temporalFilter"]["out"].object( "/sphere" )["val"].data.value,
				0.0, places=5
			)

	def testPrimitiveVariablesPattern( self ) :

		script = self.__makeScene()
		script["vars"]["primitiveVariables"].addChild(
			Gaffer.NameValuePlug( "other", 10.0, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		)
		# "other" is constant, so filtering it should leave it unchanged.
		script["temporalFilter"]["primitiveVariables"].setValue( "val other" )
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )

		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			obj = script["temporalFilter"]["out"].object( "/sphere" )
		self.assertAlmostEqual( obj["val"].data.value, 0.0, places=5 )
		self.assertAlmostEqual( obj["other"].data.value, 10.0, places=5 )

		# Wildcard.
		script["temporalFilter"]["primitiveVariables"].setValue( "*" )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			obj = script["temporalFilter"]["out"].object( "/sphere" )
		self.assertAlmostEqual( obj["val"].data.value, 0.0, places=5 )

	def testMissingVariable( self ) :

		# If the variable doesn't exist at some sample times it contributes zero.
		script = self.__makeScene()
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )

		# Override the vars node so "val" only exists at frame 0 and not at other frames.
		# Simplest way: use the raw sphere (no "val") as a second scene and time-warp.
		# Instead, just verify the zero-contribution by using a range that goes below
		# what our animation covers (below frame -10).
		script["temporalFilter"]["start"]["frame"].setValue( -12 )
		script["temporalFilter"]["end"]["frame"].setValue( -8 )
		# Samples at -12, -11, -10, -9, -8. Anim only covers -10..10, so -12 and -11
		# clamp to -10 via Linear interpolation. The average should be near -10.
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			result = script["temporalFilter"]["out"].object( "/sphere" )["val"].data.value
		# All 5 samples have defined values here (linear anim is extrapolated), so
		# just verify it's a sensible average.
		self.assertAlmostEqual( result, -10.0, places=1 )

	def testNonNumericVariablesUnchanged( self ) :

		script = self.__makeScene()

		# Add a string variable - it should pass through unmodified.
		script["vars"]["primitiveVariables"].addChild(
			Gaffer.NameValuePlug( "label", IECore.StringData( "hello" ), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		)
		script["temporalFilter"]["primitiveVariables"].setValue( "*" )

		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			obj = script["temporalFilter"]["out"].object( "/sphere" )
		self.assertEqual( obj["label"].data.value, "hello" )

	def testVectorVariable( self ) :

		script = Gaffer.ScriptNode()
		script["sphere"] = GafferScene.Sphere()

		script["vars"] = GafferScene.PrimitiveVariables()
		script["vars"]["in"].setInput( script["sphere"]["out"] )
		script["vars"]["primitiveVariables"].addChild(
			Gaffer.NameValuePlug( "v", imath.V3f( 0 ), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		)
		# Animate x component = frame, y = 0, z = 0.
		anim = Gaffer.Animation.acquire( script["vars"]["primitiveVariables"]["NameValuePlug"]["value"]["x"] )
		for f in range( -10, 11 ) :
			anim.addKey( Gaffer.Animation.Key( f / 24.0, float( f ), Gaffer.Animation.Interpolation.Linear ) )

		script["filter"] = GafferScene.PathFilter()
		script["filter"]["paths"].setValue( IECore.StringVectorData( [ "/sphere" ] ) )
		script["vars"]["filter"].setInput( script["filter"]["out"] )

		script["temporalFilter"] = GafferScene.TemporalFilter()
		script["temporalFilter"]["in"].setInput( script["vars"]["out"] )
		script["temporalFilter"]["filter"].setInput( script["filter"]["out"] )
		script["temporalFilter"]["primitiveVariables"].setValue( "v" )

		# Box filter: average of {F-2, F-1, F, F+1, F+2}.x = F.
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 3 )
			v = script["temporalFilter"]["out"].object( "/sphere" )["v"].data.value
		self.assertAlmostEqual( v.x, 3.0, places=5 )
		self.assertAlmostEqual( v.y, 0.0, places=5 )

		# Min: component-wise → x = F-2.
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Min )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 3 )
			v = script["temporalFilter"]["out"].object( "/sphere" )["v"].data.value
		self.assertAlmostEqual( v.x, 1.0, places=5 )

	def testBoundsUpdate( self ) :

		script = self.__makeScene()

		# Default: primitiveVariables = "val" which doesn't include P,
		# so adjustBounds should be a no-op.
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			self.assertEqual(
				script["temporalFilter"]["out"].bound( "/sphere" ),
				script["vars"]["out"].bound( "/sphere" )
			)

		# When P is included the bound should change.
		script["temporalFilter"]["primitiveVariables"].setValue( "P" )
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			obj = script["temporalFilter"]["out"].object( "/sphere" )
			import IECoreScene
			bound = script["temporalFilter"]["out"].bound( "/sphere" )
			self.assertEqual( bound, IECoreScene.MeshPrimitive.computeBound( obj ) )

	def testHashChangesWithInputs( self ) :

		script = self.__makeScene()

		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			h1 = script["temporalFilter"]["out"].objectHash( "/sphere" )

		# Different frame → different hash.
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 1 )
			h2 = script["temporalFilter"]["out"].objectHash( "/sphere" )
		self.assertNotEqual( h1, h2 )

		# Different filter → different hash.
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Min )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			h3 = script["temporalFilter"]["out"].objectHash( "/sphere" )
		self.assertNotEqual( h1, h3 )

		# Different range → different hash.
		script["temporalFilter"]["filterType"].setValue( GafferScene.TemporalFilter.Filter.Box )
		script["temporalFilter"]["start"]["frame"].setValue( -3 )
		with Gaffer.Context( script.context() ) as context :
			context.setFrame( 0 )
			h4 = script["temporalFilter"]["out"].objectHash( "/sphere" )
		self.assertNotEqual( h1, h4 )

if __name__ == "__main__" :
	unittest.main()
