##########################################################################
#
#  Copyright (c) 2024, Cinesite VFX Ltd. All rights reserved.
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

import pathlib
import unittest

import imath
import OpenImageIO

import IECore

import Gaffer
import GafferUI
from GafferUI.ColorChooser import _tmiToRGB
from GafferUI.ColorChooser import _rgbToTMI
from GafferUI.ColorChooserPlugValueWidget import saveDefaultOptions
import GafferUITest

class ColorChooserTest( GafferUITest.TestCase ) :

	def testTMI( self ) :
		# Load a precomputed image generated by Houdini to test our TMI <-> RGB calculation.
		# The image is 200px x 20px. Each 20px square varies the temperature from -1.0 to 1.0
		# on the X axis and magenta from -1.0 to 1.0 on the Y-axis. Each of the 10 squares, from
		# left to right, varies the intensity from 0.0 to 0.9.
		tileWidth = 20
		tileCount = 10
		tmiTargetImage = OpenImageIO.ImageBuf( ( pathlib.Path( __file__ ).parent / "images" / "tmi.exr" ).as_posix() )

		for tile in range ( 0, tileCount ) :
			for y in range( 0, tileWidth ) :
				for x in range( 0, tileWidth ) :
					tmiOriginal = imath.Color3f(
						( float( x ) / tileWidth ) * 2.0 - 1.0,
						( float( y ) / tileWidth ) * 2.0 - 1.0,
						float( tile ) / tileCount
					)

					rgbConverted = _tmiToRGB( tmiOriginal )
					tmiTarget = tmiTargetImage.getpixel( x + ( tile * tileWidth ), y )
					self.assertAlmostEqual( tmiTarget[0], rgbConverted.r, places = 6 )
					self.assertAlmostEqual( tmiTarget[1], rgbConverted.g, places = 6 )
					self.assertAlmostEqual( tmiTarget[2], rgbConverted.b, places = 6 )

					tmiConverted = _rgbToTMI( rgbConverted )
					self.assertAlmostEqual( tmiConverted.r, tmiOriginal.r, places = 6 )
					self.assertAlmostEqual( tmiConverted.g, tmiOriginal.g, places = 6 )
					self.assertAlmostEqual( tmiConverted.b, tmiOriginal.b, places = 6 )

	def testTMIAlpha( self ) :

		rgb = imath.Color4f( 0.5 )
		tmi = _rgbToTMI( rgb )
		self.assertEqual( tmi.a, rgb.a )

		rgbConverted = _tmiToRGB( tmi )
		self.assertEqual( rgbConverted.a, rgb.a )

	def __colorChooserFromWidget( self, widget ) :

		return widget._ColorPlugValueWidget__colorChooser._ColorChooserPlugValueWidget__colorChooser

	def __sliderFromWidget( self, widget, component ) :

		c = self.__colorChooserFromWidget( widget )
		return c._ColorChooser__sliders[component]

	def __setVisibleComponents( self, widget, channels ) :

		c = self.__colorChooserFromWidget( widget )
		c.setVisibleComponents( channels )

	def __setStaticComponent( self, widget, component ) :

		c = self.__colorChooserFromWidget( widget )
		c.setColorFieldStaticComponent( component )

	def __getStaticComponent( self, widget ) :

		c = self.__colorChooserFromWidget( widget )
		return c.getColorFieldStaticComponent()

	def __setColorFieldVisibility( self, widget, visible ) :

		c = self.__colorChooserFromWidget( widget )
		c.setColorFieldVisible( visible )

	def __getColorFieldVisibility( self, widget ) :

		c = self.__colorChooserFromWidget( widget )
		return c.getColorFieldVisible()

	def __setDynamicSliderBackgrounds( self, widget, dynamic ) :

		c = self.__colorChooserFromWidget( widget )
		c.setDynamicSliderBackgrounds( dynamic )

	def __getDynamicSliderBackgrounds( self, widget ) :

		c = self.__colorChooserFromWidget( widget )
		return c.getDynamicSliderBackgrounds()

	def testMetadata( self ) :

		script = Gaffer.ScriptNode()

		script["node"] = Gaffer.Node()
		script["node"]["rgbPlug1"] = Gaffer.Color3fPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		script["node"]["rgbPlug2"] = Gaffer.Color3fPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		widget = GafferUI.ColorPlugValueWidget( script["node"]["rgbPlug1"] )
		widget.setColorChooserVisible( True )

		# Default state

		for c in "rgbhsvtmi" :
			self.assertTrue( self.__sliderFromWidget( widget, c ).getVisible() )
		self.assertEqual( self.__getStaticComponent( widget ), "v" )
		self.assertTrue( self.__getColorFieldVisibility( widget ) )

		for p in [ "rgbPlug1", "rgbPlug2" ] :
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:visibleComponents" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:staticComponent" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:colorFieldVisible" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:dynamicSliderBackgrounds" ) )

		# Modify widget

		self.__setVisibleComponents( widget, "rgbtmi" )
		self.__setStaticComponent( widget, "t" )
		self.__setColorFieldVisibility( widget, False )
		self.__setDynamicSliderBackgrounds( widget, False )

		for c in "rgbtmi" :
			self.assertTrue( self.__sliderFromWidget( widget, c ).getVisible() )
		for c in "hsv" :
			self.assertFalse( self.__sliderFromWidget( widget, c ).getVisible() )
		self.assertEqual( self.__getStaticComponent( widget ), "t" )
		self.assertFalse( self.__getColorFieldVisibility( widget ) )
		self.assertFalse( self.__getDynamicSliderBackgrounds( widget ) )

		for p in [ "rgbPlug2" ] :
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:visibleComponents" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:staticComponent" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:colorFieldVisible" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:dynamicSliderBackgrounds" ) )

		self.assertEqual( set( Gaffer.Metadata.value( script["node"]["rgbPlug1"], "colorChooser:inline:visibleComponents" ) ), set( "rgbtmi" ) )
		self.assertEqual( Gaffer.Metadata.value( script["node"]["rgbPlug1"], "colorChooser:inline:staticComponent" ), "t" )
		self.assertFalse( Gaffer.Metadata.value( script["node"]["rgbPlug1"], "colorChooser:inline:colorFieldVisible" ) )
		self.assertFalse( Gaffer.Metadata.value( script["node"]["rgbPlug1"], "colorChooser:inline:dynamicSliderBackgrounds" ) )

		# Recreate widget and should have the same state

		del widget
		widget = GafferUI.ColorPlugValueWidget( script["node"]["rgbPlug1"] )
		widget.setColorChooserVisible( True )

		for c in "rgbtmi" :
			self.assertTrue( self.__sliderFromWidget( widget, c ).getVisible() )
		for c in "hsv" :
			self.assertFalse( self.__sliderFromWidget( widget, c ).getVisible() )
		self.assertEqual( self.__getStaticComponent( widget ), "t" )
		self.assertFalse( self.__getColorFieldVisibility( widget ) )
		self.assertFalse( self.__getDynamicSliderBackgrounds( widget ) )

		# We haven't saved the defaults, so a widget for a second plug
		# gets the original defaults.

		widget2 = GafferUI.ColorPlugValueWidget( script["node"]["rgbPlug2"] )
		widget2.setColorChooserVisible( True )

		for c in "rgbhsvtmi" :
			self.assertTrue( self.__sliderFromWidget( widget2, c ).getVisible() )
		self.assertEqual( self.__getStaticComponent( widget2 ), "v" )
		self.assertTrue( self.__getColorFieldVisibility( widget2 ) )
		self.assertTrue( self.__getDynamicSliderBackgrounds( widget2 ) )

		for p in [ "rgbPlug2" ] :
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:visibleComponents" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:staticComponent" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:colorFieldVisible" ) )
			self.assertIsNone( Gaffer.Metadata.value( script["node"][p], "colorChooser:inline:dynamicSliderBackgrounds" ) )

		# Don't serialize state

		del widget

		script2 = Gaffer.ScriptNode()
		script2.execute( script.serialise() )

		widget = GafferUI.ColorPlugValueWidget( script2["node"]["rgbPlug1"] )
		widget.setColorChooserVisible( True )

		for c in "rgbhsvtmi" :
			self.assertTrue( self.__sliderFromWidget( widget, c ).getVisible() )
		self.assertEqual( self.__getStaticComponent( widget ), "v" )
		self.assertTrue( self.__getColorFieldVisibility( widget ) )
		self.assertTrue( self.__getDynamicSliderBackgrounds( widget ) )

		for p in [ "rgbPlug1", "rgbPlug2" ] :
			self.assertIsNone( Gaffer.Metadata.value( script2["node"][p], "colorChooser:inline:visibleComponents" ) )
			self.assertIsNone( Gaffer.Metadata.value( script2["node"][p], "colorChooser:inline:staticComponent" ) )
			self.assertIsNone( Gaffer.Metadata.value( script2["node"][p], "colorChooser:inline:colorFieldVisible" ) )
			self.assertIsNone( Gaffer.Metadata.value( script2["node"][p], "colorChooser:inline:dynamicSliderBackgrounds" ) )

	def testSaveDefaultOptions( self ) :

		script = Gaffer.ScriptNode()

		script["node"] = Gaffer.Node()
		script["node"]["rgbPlug"] = Gaffer.Color3fPlug()
		script["node"]["rgbaPlug"] = Gaffer.Color4fPlug()
		script["node"]["rgbaPlug"].setValue( imath.Color4f( 0.1 ) )

		rgbWidget = GafferUI.ColorPlugValueWidget( script["node"]["rgbPlug"] )
		rgbWidget.setColorChooserVisible( True )
		rgbaWidget = GafferUI.ColorPlugValueWidget( script["node"]["rgbaPlug"] )
		rgbaWidget.setColorChooserVisible( True )

		GafferUITest.PlugValueWidgetTest.waitForUpdate( rgbWidget._ColorPlugValueWidget__colorChooser )
		GafferUITest.PlugValueWidgetTest.waitForUpdate( rgbaWidget._ColorPlugValueWidget__colorChooser )

		# Default state
		for c in "rgbhsvtmi" :
			self.assertTrue( self.__sliderFromWidget( rgbWidget, c ).getVisible() )
			self.assertTrue( self.__sliderFromWidget( rgbaWidget, c ).getVisible() )
		self.assertTrue( self.__sliderFromWidget( rgbaWidget, "a" ).getVisible() )
		self.assertEqual( self.__getStaticComponent( rgbWidget ), "v" )
		self.assertEqual( self.__getStaticComponent( rgbaWidget ), "v" )
		self.assertTrue( self.__getColorFieldVisibility( rgbWidget ) )
		self.assertTrue( self.__getColorFieldVisibility( rgbaWidget ) )
		self.assertTrue( self.__getDynamicSliderBackgrounds( rgbWidget ) )
		self.assertTrue( self.__getDynamicSliderBackgrounds( rgbaWidget ) )

		# Modify `rgbWidget`

		self.__setVisibleComponents( rgbWidget, "rgbhsv" )
		self.__setStaticComponent( rgbWidget, "g" )
		self.__setColorFieldVisibility( rgbWidget, False )
		self.__setDynamicSliderBackgrounds( rgbWidget, False )

		# Save defaults
		colorChooser = self.__colorChooserFromWidget( rgbWidget )
		saveDefaultOptions( colorChooser, "colorChooser:inline:" )

		del rgbWidget
		del rgbaWidget

		# Both color types get the same value
		rgbWidget = GafferUI.ColorPlugValueWidget( script["node"]["rgbPlug"] )
		rgbWidget.setColorChooserVisible( True )
		rgbaWidget = GafferUI.ColorPlugValueWidget( script["node"]["rgbaPlug"] )
		rgbaWidget.setColorChooserVisible( True )

		GafferUITest.PlugValueWidgetTest.waitForUpdate( rgbWidget._ColorPlugValueWidget__colorChooser )
		GafferUITest.PlugValueWidgetTest.waitForUpdate( rgbaWidget._ColorPlugValueWidget__colorChooser )

		for c in "rgbhsv" :
			self.assertTrue( self.__sliderFromWidget( rgbWidget, c ).getVisible() )
			self.assertTrue( self.__sliderFromWidget( rgbaWidget, c ).getVisible() )
		for c in "tmi" :
			self.assertFalse( self.__sliderFromWidget( rgbWidget, c ).getVisible() )
			self.assertFalse( self.__sliderFromWidget( rgbaWidget, c ).getVisible() )
		self.assertTrue( self.__sliderFromWidget( rgbaWidget, "a" ).getVisible() )
		self.assertEqual( self.__getStaticComponent( rgbWidget ), "g" )
		self.assertEqual( self.__getStaticComponent( rgbaWidget ), "g" )
		self.assertFalse( self.__getColorFieldVisibility( rgbWidget ) )
		self.assertFalse( self.__getColorFieldVisibility( rgbaWidget ) )
		self.assertFalse( self.__getDynamicSliderBackgrounds( rgbWidget ) )
		self.assertFalse( self.__getDynamicSliderBackgrounds( rgbaWidget ) )

if __name__ == "__main__" :
	unittest.main()
