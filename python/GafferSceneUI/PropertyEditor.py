##########################################################################
#
#  Copyright (c) 2020, Cinesite VFX Ltd. All rights reserved.
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

import collections

import imath

import IECore

import Gaffer
import GafferUI
import GafferScene
import GafferSceneUI

_parameters = [ "exposure", "color", "radius" ]

_Value = collections.namedtuple( "_Value", [ "path", "value" ] )

def _formatValue( value ) :

	if isinstance( value, ( int, float ) ) :
		return GafferUI.NumericWidget.valueToString( value )
	elif isinstance( value, ( imath.V3f, imath.V2f, imath.V3i, imath.V2i ) ) :
		return " ".join( GafferUI.NumericWidget.valueToString( x ) for x in value )
	else :
		return str( value )

class PropertyEditor( GafferUI.Widget ) :

	def __init__( self, sceneView ) :

		frame = GafferUI.Frame()
		GafferUI.Widget.__init__( self, frame )

		self.__sceneView = sceneView

		with frame :

			with GafferUI.ListContainer( spacing = 8 ) :

				with GafferUI.ListContainer(
					orientation = GafferUI.ListContainer.Orientation.Horizontal,
					spacing = 4
				) :
					GafferUI.Label( "<h4>Properties</h4>" )
					GafferUI.Spacer( imath.V2i( 1 ) )
					self.__busyWidget = GafferUI.BusyWidget( size = 20, busy = False )

				GafferUI.Divider()

				# with GafferUI.Collapsible( "Arnold" ) :
				# 	GafferUI.TextWidget( "Camera visibility" )

				with GafferUI.Collapsible( "Light" ) :
					with GafferUI.ListContainer( spacing = 4 ) :
						self.__parameterWidgets = {}
						for parameter in _parameters :
							with GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Horizontal, spacing = 4 ) :
								GafferUI.Button( image = "edit.png", hasFrame = False )
								GafferUI.Label( "<h5>" + IECore.CamelCase.toSpaced( parameter ) + "</h5>" )
							self.__parameterWidgets[parameter] = GafferUI.Label( "" )

		sceneView.plugDirtiedSignal().connect( Gaffer.WeakMethod( self.__plugDirtied ), scoped = False )
		sceneView.getContext().changedSignal().connect( Gaffer.WeakMethod( self.__contextChanged ), scoped = False )

	def __plugDirtied( self, plug ) :

		if plug == self.__sceneView["in"] :
			self.__updateLazily()

	def __contextChanged( self, context, name ) :

		if GafferSceneUI.ContextAlgo.affectsSelectedPaths( name ) or not name.startswith( "ui:" ) :
			self.__updateLazily()

	@GafferUI.LazyMethod()
	def __updateLazily( self ) :

		with self.__sceneView.getContext() :
			self.__backgroundUpdate()

	@GafferUI.BackgroundMethod()
	def __backgroundUpdate( self ) :

		selectedPaths = GafferSceneUI.ContextAlgo.getSelectedPaths( Gaffer.Context.current() )
		parameters = {}

		for path in selectedPaths.paths() :
			attributes = self.__sceneView["in"].attributes( path )
			if "ai:light" in attributes :
				shader = attributes["ai:light"].outputShader()
				for parameter in _parameters :
					value = shader.parameters.get( parameter )
					if value is not None :
						parameters.setdefault( parameter, [] ).append(
							_Value( path, value.value )
						)

		return parameters

	@__backgroundUpdate.plug
	def __backgroundUpdatePlug( self ) :

		return self.__sceneView["in"]

	@__backgroundUpdate.preCall
	def __backgroundUpdatePreCall( self ) :

		self.__busyWidget.setBusy( True )
		#self.__menuButton().setEnabled( False )

	@__backgroundUpdate.postCall
	def __backgroundUpdatePostCall( self, backgroundResult ) :

		if isinstance( backgroundResult, IECore.Cancelled ) :
			self.__lazyBackgroundUpdate()
		elif isinstance( backgroundResult, Exception ) :
			# Computation error. Rest of the UI can deal with this.
			pass
		else :
			# Success.
			self.__foregroundUpdate( backgroundResult )

		self.__busyWidget.setBusy( False )

	def __foregroundUpdate( self, parameterValues ) :

		for parameter, widget in self.__parameterWidgets.items() :
			values = parameterValues.get( parameter )
			text = ""
			if values is not None :
				if all( v.value == values[0].value for v in values ) :
					text = _formatValue( values[0].value )
				else :
					text = "--"

			widget.setText( text )

Gaffer.Metadata.registerValue( GafferSceneUI.SceneView, "nodeToolbar:right:type", "GafferUI.StandardNodeToolbar.right" )
Gaffer.Metadata.registerValue( GafferSceneUI.SceneView,	"toolbarLayout:customWidget:PropertyEditor:widgetType", "GafferSceneUI.PropertyEditor" )
Gaffer.Metadata.registerValue( GafferSceneUI.SceneView,	"toolbarLayout:customWidget:PropertyEditor:section", "Right" )

#Gaffer.Metadata.registerValue( GafferSceneUI.SceneView, "toolbarLayout:customWidget:BottomSpacer:widgetType", "GafferSceneUI.SceneViewUI._Spacer" )
#Gaffer.Metadata.registerValue( GafferSceneUI.SceneView, "toolbarLayout:customWidget:BottomSpacer:section", "Right" )
#Gaffer.Metadata.registerValue( GafferSceneUI.SceneView, "toolbarLayout:customWidget:BottomSpacer:index", 1 )
