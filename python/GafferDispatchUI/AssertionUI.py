##########################################################################
#
#  Copyright (c) 2014, Image Engine Design Inc. All rights reserved.
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
import GafferUI
import GafferDispatch

Gaffer.Metadata.registerNode(

	GafferDispatch.Assertion,

	# "description",
	# """
	# Runs system commands via a shell.
	# """,

	"noduleLayout:customGadget:setupButton:gadgetType", "GafferDispatchUI.AssertionUI.AssertionPlugAdder",
	"noduleLayout:customGadget:setupButton:section", "left",

	plugs = {

		"mode" : (

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"presetNames", lambda plug : IECore.StringVectorData( IECore.CamelCase.toSpaced( str( x ).partition( ".")[2] ) for x in GafferDispatch.Assertion.Mode ),
			"presetValues", lambda plug : IECore.IntVectorData( int( x ) for x in GafferDispatch.Assertion.Mode ),

		),

		"a" : (

			"nodule:type", "GafferUI::StandardNodule",
			"noduleLayout:section", "left",

		),

		"b" : (

			"nodule:type", "GafferUI::StandardNodule",
			"noduleLayout:section", "left",

		),

		"delta" : (

			"layout:activator", lambda plug : plug.node()["mode"].getValue() == GafferDispatch.Assertion.Mode.Equal,

		),

		"message" : (

			"layout:index", -1, # After the plugs made by `setup()`

			"description",
			"""
			An additional message to be included when the assertion fails.
			""",

			"plugValueWidget:type", "GafferUI.MultiLineStringPlugValueWidget",

		),

	}

)

class AssertionPlugAdder( GafferUI.PlugAdder ) :

	def __init__( self, node ) :

		GafferUI.PlugAdder.__init__( self )

		self.__node = node

		self.__node.childAddedSignal().connect( Gaffer.WeakMethod( self.__updateVisibility ), scoped = False )
		self.__node.childRemovedSignal().connect( Gaffer.WeakMethod( self.__updateVisibility ), scoped = False )

	def canCreateConnection( self, endpoint ) :

		if not GafferUI.PlugAdder.canCreateConnection( self, endpoint ) :
			return False

		return self.__node.canSetup( endpoint )

	def createConnection( self, endpoint ) :

		with Gaffer.UndoScope( self.__node.scriptNode() ) :
			self.__node.setup( endpoint )
			self.__node["a"].setInput( endpoint )

	def __updateVisibility( self, *unused ) :

		self.setVisible( "a" not in self.__node )

GafferUI.NoduleLayout.registerCustomGadget( "GafferDispatchUI.AssertionUI.AssertionPlugAdder", AssertionPlugAdder )

## TODO : ADD A + BUTTON WIDGET AS WELL
