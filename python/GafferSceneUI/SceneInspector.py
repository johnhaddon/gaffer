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

import IECore

import Gaffer
import GafferScene
import GafferUI
import GafferSceneUI

from . import _GafferSceneUI

class SceneInspector( GafferSceneUI.SceneEditor ) :

	class Settings( GafferSceneUI.SceneEditor.Settings ) :

		def __init__( self ) :

			GafferSceneUI.SceneEditor.Settings.__init__( self )

			self["editScope"] = Gaffer.Plug()

			self["compare"] = Gaffer.Plug()
			self["compare"]["enabled"] = Gaffer.BoolPlug()
			self["compare"]["location"] = Gaffer.StringPlug()
			self["compare"]["scene"] = GafferScene.ScenePlug()
			self["compare"]["renderPass"] = Gaffer.StringPlug()

			self["__switchedIn"] = GafferScene.ScenePlug()

			self["__switchIndexQuery"] = Gaffer.ContextQuery()
			self["__switchIndexQuery"].addQuery( Gaffer.IntPlug(), "__sceneInspector:inputIndex" )

			self["__switch"] = Gaffer.Switch()
			self["__switch"].setup( self["in"] )
			self["__switch"]["in"][0].setInput( self["in"] )
			self["__switch"]["in"][1].setInput( self["compare"]["scene"] )
			self["__switch"]["index"].setInput( self["__switchIndexQuery"]["out"][0]["value"] )
			self["__switch"]["deleteContextVariables"].setValue( "__sceneInspector:inputIndex" )
			self["__switchedIn"].setInput( self["__switch"]["out"] )

	IECore.registerRunTimeTyped( Settings, typeName = "GafferSceneUI::SceneInspector::Settings" )

	def __init__( self, scriptNode, **kw ) :

		mainColumn = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, borderWidth = 4, spacing = 4 )

		GafferSceneUI.SceneEditor.__init__( self, mainColumn, scriptNode, **kw )

		self.settings()["compare"]["scene"].setInput( self.settings()["in"] )

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

			with GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Horizontal, spacing = 4 ) :

				GafferUI.PlugLayout(
					self.settings(),
					orientation = GafferUI.ListContainer.Orientation.Horizontal,
					rootSection = "Settings",
				)

			with GafferUI.TabbedContainer() :

				with GafferUI.ListContainer( spacing = 4, borderWidth = 4, parenting = { "label" : "Selection" } ) :

					self.__selectionPathListing = GafferUI.PathListingWidget(
						_GafferSceneUI._SceneInspector.InspectorPath(
							self.settings()["__switchedIn"],
							self.__selectionContexts(),
							self.settings()["editScope"],
							"/Selection"
						),
						columns = self.__standardColumns,
						selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
						displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
						sortable = False,
					)

				with GafferUI.ListContainer( spacing = 4, borderWidth = 4, parenting = { "label" : "Globals" } ) :

					self.__globalsPathListing = GafferUI.PathListingWidget(
						_GafferSceneUI._SceneInspector.InspectorPath(
							self.settings()["__switchedIn"],
							self.__globalsContexts(),
							self.settings()["editScope"],
							"/Globals"
						),
						columns = self.__standardColumns,
						selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
						displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
						sortable = False,
					)

		GafferSceneUI.ScriptNodeAlgo.selectedPathsChangedSignal( scriptNode ).connect(
			Gaffer.WeakMethod( self.__selectedPathsChanged )
		)

		self._updateFromSet()

	def __repr__( self ) :

		return "GafferSceneUI.SceneInspector( scriptNode )"

	def _updateFromContext( self, modifiedItems ) :

		self.__lazyUpdateFromContexts()

	def _updateFromSettings( self, plug ) :

		if plug == self.settings()["compare"]["enabled"] :
			columns = self.__diffColumns if plug.getValue() else self.__standardColumns
			self.__selectionPathListing.setColumns( columns )
			self.__globalsPathListing.setColumns( columns )

	@GafferUI.LazyMethod( deferUntilPlaybackStops = True )
	def __lazyUpdateFromContexts( self ) :

		self.__selectionPathListing.getPath().setContexts( self.__selectionContexts() )
		self.__globalsPathListing.getPath().setContexts( self.__globalsContexts() )

	def __selectedPathsChanged( self, scriptNode ) :

		self.__selectionPathListing.getPath().setContexts( self.__selectionContexts() )

	def __globalsContexts( self ) :

		result = []
		for inputIndex in range( 0, 2 ) :
			context = Gaffer.Context( self.context() )
			context["__sceneInspector:inputIndex"] = inputIndex
			result.append( context )

		if self.settings()["compare"]["enabled"].getValue() :
			renderPass = self.settings()["compare"]["renderPass"].getValue()
			if renderPass :
				result[1]["renderPass"] = renderPass

		return result

	def __selectionContexts( self ) :

		result = self.__globalsContexts()

		lastSelectedPath = GafferSceneUI.ScriptNodeAlgo.getLastSelectedPath( self.scriptNode() )
		if lastSelectedPath :
			result[0]["scene:path"] = GafferScene.ScenePlug.stringToPath( lastSelectedPath )

		if self.settings()["compare"]["enabled"].getValue() :
			comparePath = self.settings()["compare"]["location"].getValue() or lastSelectedPath
			if comparePath :
				result[1]["scene:path"] = GafferScene.ScenePlug.stringToPath( comparePath )

		return result

GafferUI.Editor.registerType( "SceneInspector", SceneInspector )

SceneInspector.registerInspectors = _GafferSceneUI._SceneInspector.registerInspectors

##########################################################################
# Settings metadata
##########################################################################

Gaffer.Metadata.registerNode(

	SceneInspector.Settings,

	## \todo Doing spacers with custom widgets is tedious, and we're doing it
	# in all the View UIs. Maybe we could just attach metadata to the plugs we
	# want to add space around, in the same way we use `divider` to add a divider?
	"layout:customWidget:spacer:widgetType", "GafferSceneUI.RenderPassEditor._Spacer",
	"layout:customWidget:spacer:section", "Settings",
	"layout:customWidget:spacer:index", 1,

	plugs = {

		"*" : [

			"label", "",

		],

		"in" : [ "plugValueWidget:type", "" ], #"GafferSceneUI.SceneInspector._InputWidget" ],

		"compare" : [

			"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",

			"layout:activator:compareIsEnabled", lambda plug : plug["enabled"].getValue(),

		],

		"compare.enabled" : [

			"label", "Enable Comparison",

		],

		"compare.scene" : [

			"plugValueWidget:type", "GafferUI.ConnectionPlugValueWidget",
			"layout:activator", "compareIsEnabled",

		],

		"compare.location" : [

			"plugValueWidget:type", "GafferSceneUI.ScenePathPlugValueWidget",
			"layout:activator", "compareIsEnabled",

		],

		"compare.renderPass" : [

			"plugValueWidget:type", "GafferSceneUI.RenderPassEditor._RenderPassPlugValueWidget",
			"layout:activator", "compareIsEnabled",

		],

		"editScope" : [

			"plugValueWidget:type", "GafferUI.EditScopeUI.EditScopePlugValueWidget",
			"layout:width", 130,
			"layout:index", -1,

		],

	}

)
