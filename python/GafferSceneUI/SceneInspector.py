##########################################################################
#
#  Copyright (c) 2012, John Haddon. All rights reserved.
#  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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

import enum

import imath

import IECore

import Gaffer
import GafferScene
import GafferUI
import GafferSceneUI

from . import _GafferSceneUI

class SceneInspector( GafferSceneUI.SceneEditor ) :

	class Settings( GafferSceneUI.SceneEditor.Settings ) :

		def __init__( self ) :

			GafferSceneUI.SceneEditor.Settings.__init__( self, numInputs = 2 )

			self["editScope"] = Gaffer.Plug()
			self["__switchedIn"] = GafferScene.ScenePlug()

			self["__switchIndexQuery"] = Gaffer.ContextQuery()
			self["__switchIndexQuery"].addQuery( Gaffer.IntPlug(), "__sceneInspector:inputIndex" )

			self["__switch"] = Gaffer.Switch()
			self["__switch"].setup( self["in"][0] )
			self["__switch"]["in"][0].setInput( self["in"][0] )
			self["__switch"]["in"][1].setInput( self["in"][1] )
			self["__switch"]["index"].setInput( self["__switchIndexQuery"]["out"][0]["value"] )
			self["__switch"]["deleteContextVariables"].setValue( "__sceneInspector:inputIndex" )
			self["__switchedIn"].setInput( self["__switch"]["out"] )

	IECore.registerRunTimeTyped( Settings, typeName = "GafferSceneUI::SceneInspector::Settings" )

	def __init__( self, scriptNode, **kw ) :

		mainColumn = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, borderWidth = 4, spacing = 4 )

		GafferSceneUI.SceneEditor.__init__( self, mainColumn, scriptNode, **kw )

		self.__standardColumns = [
			GafferUI.StandardPathColumn( "Name", "name" ),
			GafferSceneUI.Private.InspectorColumn( "inspector:inspector", headerData = GafferUI.PathColumn.CellData( value = "Value" ) ),
		]

		self.__diffColumns = [
			GafferUI.StandardPathColumn( "Name", "name" ),
			_GafferSceneUI._SceneInspector.InspectorDiffColumn( _GafferSceneUI._SceneInspector.InspectorDiffColumn.DiffContext.A ),
			_GafferSceneUI._SceneInspector.InspectorDiffColumn( _GafferSceneUI._SceneInspector.InspectorDiffColumn.DiffContext.B ),
		]

		with mainColumn :

			GafferUI.PlugLayout(
				self.settings(),
				orientation = GafferUI.ListContainer.Orientation.Horizontal,
				rootSection = "Settings",
			)

			with GafferUI.TabbedContainer() :

				with GafferUI.ListContainer( spacing = 4, borderWidth = 4, parenting = { "label" : "Selection" } ) :

					self.__selectionPathListing = GafferUI.PathListingWidget(
						Gaffer.DictPath( {}, "/" ),
						columns = self.__standardColumns,
						selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
						displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
						sortable = False,
					)

				with GafferUI.ListContainer( spacing = 4, borderWidth = 4, parenting = { "label" : "Globals" } ) :

					self.__globalsPathListing = GafferUI.PathListingWidget(
						Gaffer.DictPath( {}, "/" ),
						columns = self.__standardColumns,
						selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
						displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
						sortable = False,
					)

		GafferSceneUI.ScriptNodeAlgo.selectedPathsChangedSignal( scriptNode ).connect(
			Gaffer.WeakMethod( self.__selectedPathsChanged )
		)

		self.settings().plugInputChangedSignal().connect(
			Gaffer.WeakMethod( self.__settingsInputChanged )
		)

		self._updateFromSet()
		self.__updateLazily()

	def __repr__( self ) :

		return "GafferSceneUI.SceneInspector( scriptNode )"

	def _updateFromContext( self, modifiedItems ) :

		for item in modifiedItems :
			if not item.startswith( "ui:" ) :
				self.__updateLazily()
				break

	def __selectedPathsChanged( self, scriptNode ) :

		self.__updateLazily()

	@GafferUI.LazyMethod( deferUntilPlaybackStops = True )
	def __updateLazily( self ) :

		self.__update()

	def __update( self ) :

		inputContexts = []
		for inputIndex in range( len( self.settings()["in"] ) ) :
			context = Gaffer.Context( self.context() )
			context["__sceneInspector:inputIndex"] = inputIndex if self.settings()["in"][inputIndex].getInput() is not None else 0
			inputContexts.append( context )

		self.__globalsPathListing.setPath(
			_GafferSceneUI._SceneInspector.InspectorPath(
				self.settings()["__switchedIn"],
				inputContexts,
				self.settings()["editScope"],
				"/Globals"
			)
		)

		self.__globalsPathListing.setColumns(
			self.__standardColumns if inputContexts[0] == inputContexts[1] else self.__diffColumns
		)

		selectionContexts = None
		lastSelectedPath = GafferSceneUI.ScriptNodeAlgo.getLastSelectedPath( self.scriptNode() )
		if lastSelectedPath :

			selectionContexts = [ Gaffer.Context( c ) for c in inputContexts ]
			selectionContexts[0]["scene:path"] = GafferScene.ScenePlug.stringToPath( lastSelectedPath )

			selectedPaths = GafferSceneUI.ScriptNodeAlgo.getSelectedPaths( self.scriptNode() ).paths()
			if len( selectedPaths ) > 1 and inputContexts[0] == inputContexts[1] :
				selectionContexts[1]["scene:path"] = GafferScene.ScenePlug.stringToPath( next( p for p in selectedPaths if p != lastSelectedPath ) )
			else :
				selectionContexts[1]["scene:path"] = GafferScene.ScenePlug.stringToPath( lastSelectedPath )

		if selectionContexts :

			self.__selectionPathListing.setPath(
				_GafferSceneUI._SceneInspector.InspectorPath(
					self.settings()["__switchedIn"],
					selectionContexts,
					self.settings()["editScope"],
					"/Selection"
				)
			)

			self.__selectionPathListing.setColumns(
				self.__standardColumns if selectionContexts[0] == selectionContexts[1] else self.__diffColumns
			)

		else :

			self.__selectionPathListing.setPath( Gaffer.DictPath( {}, "/" ) )
			self.__selectionPathListing.setColumns( self.__standardColumns )

	def __settingsInputChanged( self, plug ) :

		if plug == self.settings()["in"][1] :
			# When the comparison scene is connected or disconnected, the
			# contexts we give to our InspectorPath need to be updated.
			self.__updateLazily()

GafferUI.Editor.registerType( "SceneInspector", SceneInspector )

##########################################################################
# Settings metadata
##########################################################################

Gaffer.Metadata.registerNode(

	SceneInspector.Settings,

	## \todo Doing spacers with custom widgets is tedious, and we're doing it
	# in all the View UIs. Maybe we could just attach metadata to the plugs we
	# want to add space around, in the same way we use `divider` to add a divider?
	# "layout:customWidget:spacer:widgetType", "GafferSceneUI.RenderPassEditor._Spacer",
	# "layout:customWidget:spacer:section", "Settings",
	# "layout:customWidget:spacer:index", 3,

	plugs = {

		"*" : [

			"label", "",

		],

		"in" : [ "plugValueWidget:type", "" ],

		"editScope" : [

			"plugValueWidget:type", "GafferUI.EditScopeUI.EditScopePlugValueWidget",
			"layout:width", 130,

		],

	}

)

##########################################################################
# Node section
##########################################################################

# class __NodeSection( Section ) :

# 	def __init__( self ) :

# 		Section.__init__( self, collapsed = None )

# 		with self._mainColumn() :
# 			with Row().listContainer() :

# 				label = GafferUI.Label(
# 					"Node",
# 					horizontalAlignment = GafferUI.Label.HorizontalAlignment.Right,
# 				)
# 				label._qtWidget().setFixedWidth( 150 )

# 				self.__diff = SideBySideDiff()
# 				for i in range( 0, 2 ) :
# 					label = GafferUI.NameLabel( None )
# 					label._qtWidget().setSizePolicy( QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Fixed )
# 					self.__diff.setValueWidget( i, label )

# 				GafferUI.Spacer( imath.V2i( 0 ), parenting = { "expand" : True } )

# 	def update( self, targets ) :

# 		Section.update( self, targets )

# 		values = [ target.scene.node() for target in targets ]
# 		backgrounds = None
# 		if len( values ) == 0 :
# 			values.append( "Select a node to inspect" )
# 			backgrounds = [ SideBySideDiff.Background.Other, SideBySideDiff.Background.Other ]
# 		elif len( values ) == 1 :
# 			values.append( "Select a second node to inspect differences" )
# 			backgrounds = [ SideBySideDiff.Background.AB, SideBySideDiff.Background.Other ]

# 		self.__diff.update( values, backgrounds = backgrounds )

# 		for index, value in enumerate( values ) :
# 			widget = self.__diff.getValueWidget( index )
# 			if isinstance( value, Gaffer.Node ) :
# 				widget.setFormatter( lambda x : ".".join( [ n.getName() for n in x ] ) )
# 				widget.setGraphComponent( value )
# 				widget.setEnabled( True )
# 			else :
# 				widget.setFormatter( lambda x : value )
# 				widget.setGraphComponent( None )
# 				widget.setEnabled( False )

# SceneInspector.registerSection( __NodeSection, tab = None )

# ##########################################################################
# # Path section
# ##########################################################################

# class __PathSection( LocationSection ) :

# 	def __init__( self ) :

# 		LocationSection.__init__( self, collapsed = None )

# 		with self._mainColumn() :
# 			with Row().listContainer() :

# 				label = GafferUI.Label(
# 					"Location",
# 					horizontalAlignment = GafferUI.Label.HorizontalAlignment.Right,
# 				)
# 				label._qtWidget().setFixedWidth( 150 )

# 				self.__diff = TextDiff( highlightDiffs = False )

# 				GafferUI.Spacer( imath.V2i( 0 ), parenting = { "expand" : True } )

# 	def update( self, targets ) :

# 		LocationSection.update( self, targets )

# 		numValidPaths = len( set( [ t.path for t in targets if t.path is not None ] ) )
# 		backgrounds = None
# 		if numValidPaths == 0 :
# 			labels = [ "Select a location to inspect" ]
# 			backgrounds = [ SideBySideDiff.Background.Other, SideBySideDiff.Background.Other ]
# 		else :
# 			labels = [ t.path if t.path is not None else "Invalid" for t in targets ]
# 			if numValidPaths == 1 and len( targets ) == 1 :
# 				labels.append( "Select a second location to inspect differences" )
# 				backgrounds = [ SideBySideDiff.Background.AB, SideBySideDiff.Background.Other ]

# 		self.__diff.update( labels, backgrounds = backgrounds )

# 		for i in range( 0, 2 ) :
# 			self.__diff.getValueWidget( i ).setEnabled(
# 				len( labels ) > i and labels[i].startswith( "/" )
# 			)

# SceneInspector.registerSection( __PathSection, tab = "Selection" )
