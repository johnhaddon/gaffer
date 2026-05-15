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

import functools

import Gaffer
import GafferUI
import GafferScene

##########################################################################
# Metadata
##########################################################################

Gaffer.Metadata.registerNode(

	GafferScene.SceneStats,

	"description",
	"""
	Sums statistics across all scene locations matched by a filter.
	Each query computes the total of that value across all matched locations.
	""",

	plugs = {

		"scene" : {

			"description" :
			"""
			The scene to gather statistics from.
			""",

		},

		"filter" : {

			"description" :
			"""
			The filter controlling which locations are included in the sums.
			""",

			"layout:section" : "Filter",
			"noduleLayout:section" : "right",
			"layout:index" : -3, # Just before the enabled plug,
			"nodule:type" : "GafferUI::StandardNodule",
			"plugValueWidget:type" : "GafferSceneUI.FilterPlugValueWidget",

		},

		"queries" : {

			"description" :
			"""
			The per-location values to be summed. Each child plug provides the
			value contributed by one matched location. The value may be a constant
			or may be connected from a node that queries properties of the current
			location.
			""",

			"plugValueWidget:type" : "GafferUI.LayoutPlugValueWidget",

			"layout:customWidget:footer:widgetType" : "GafferUI.PlugCreationWidget",
			"layout:customWidget:footer:index" : -1,

			"plugCreationWidget:action" : "addQuery",
			"plugCreationWidget:includedTypes" : (
				"Gaffer::BoolPlug Gaffer::IntPlug Gaffer::FloatPlug "
				"Gaffer::V2iPlug Gaffer::V3iPlug Gaffer::V2fPlug Gaffer::V3fPlug "
				"Gaffer::Color3fPlug Gaffer::Color4fPlug"
			),

			"nodule:type" : "",

		},

		"queries.*" : {

			"deletable" : True, # TODO : WE HAVE OUR OWN MENU THAT DOES THIS. MAYBE WE CAN USE CHILDREMOVED AND THE STANDARD MENU?
			"renameable" : True,
			"plugValueWidget:type" : "GafferSceneUI.SceneStatsUI._QueryWidget",

		},

		# TODO : MATCH CAMERA UI, WITH EVERYTHING ON ONE LINE

		"out" : {

			"description" :
			"""
			The parent plug of the summed outputs. Each child corresponds to a
			child of the `queries` plug and holds the total value summed over
			all matched locations.
			""",

			"plugValueWidget:type" : "",

			"layout:section" : "Settings.Outputs",

			"nodule:type" : "GafferUI::CompoundNodule",
			"noduleLayout:spacing" : 0.4,
			"noduleLayout:customGadget:addButton:gadgetType" : "",

		},

		"out.*.sum" : {

			"description" :
			"""
			The sum of this statistic over all matched locations.
			""",

		},

		## TODO DOCS FOR OTHER PLUGS

	}

)


##########################################################################
# Query widget
##########################################################################

class _QueryWidget( GafferUI.PlugValueWidget ) :

	def __init__( self, queryPlugs, **kw ) :

		self.__column = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, spacing = 4 )

		GafferUI.PlugValueWidget.__init__( self, self.__column, queryPlugs )

		self.__plugWidgets = {}
		with self.__column :

			self.__plugWidgets["in"] = GafferUI.PlugWidget( GafferUI.PlugValueWidget.create( queryPlugs, typeMetadata = None ) )

			outPlugs = [ plug.node().outPlugFromQuery( plug ) for plug in self.getPlugs() ]
			for childName in outPlugs[0].keys() :
				widget = GafferUI.PlugWidget( GafferUI.PlugValueWidget.create( { plug[childName] for plug in outPlugs } ) )
				widget.labelPlugValueWidget().setFixedWidth(
					GafferUI.PlugWidget.labelWidth() + 40
				)
				self.__plugWidgets[childName] = widget

		self.dropSignal().connectFront( Gaffer.WeakMethod( self.__drop ) )

	def setPlugs( self, plugs ) :

		GafferUI.PlugValueWidget.setPlugs( self, plugs )

		self.__plugWidgets["in"].setPlugs( plugs )
		outPlugs = [ plug.node().outPlugFromQuery( plug ) for plug in self.getPlugs() ]
		for childName in outPlugs[0].keys()  :
			self.__plugWidgets[childName].setPlugs(
				{ plug[childName] for plug in outPlugs }
			)

	def hasLabel( self ) :

		return True

	def childPlugValueWidget( self, childPlug ) :

		return self.__plugWidgets["in"].plugValueWidget().childPlugValueWidget( childPlug )

	def __drop( self, widget, event ) : ## TODO : NEEDED OR NOT?

		# We dont want to accept plugs or values dropped onto the query row
		# as that would attempt to set the row's query plug, so we always
		# return `True` to block the PlugValueWidget drop handler.
		return True

##########################################################################
# Popup menu
##########################################################################

def __plugPopupMenu( menuDefinition, plugValueWidget ) :

	plug = plugValueWidget.getPlug()
	if plug is None or not isinstance( plug.node(), GafferScene.SceneStats ) :
		return

	queriesPlug = plug.node()["queries"]

	queryPlug = plug
	while queryPlug is not None and queryPlug.parent() != queriesPlug :
		queryPlug = queryPlug.parent()

	if queryPlug is None :
		return

	if len( menuDefinition.items() ) :
		menuDefinition.append( "/DeleteDivider", { "divider" : True } )

	menuDefinition.append(
		"/Delete",
		{
			"command" : functools.partial( __deleteQuery, queryPlug ),
			"active" : not Gaffer.MetadataAlgo.readOnly( queriesPlug ),
		}
	)

def __deleteQuery( queryPlug ) :

	with Gaffer.UndoScope( queryPlug.ancestor( Gaffer.ScriptNode ) ) :
		queryPlug.node().removeQuery( queryPlug )

GafferUI.PlugValueWidget.popupMenuSignal().connect( __plugPopupMenu )
