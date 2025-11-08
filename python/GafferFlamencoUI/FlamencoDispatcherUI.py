##########################################################################
#
#  Copyright (c) 2025, John Haddon. All rights reserved.
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

import urllib.error
import urllib.request

import imath

import Gaffer
import GafferUI
import GafferFlamenco

from GafferUI.PlugValueWidget import sole

Gaffer.Metadata.registerNode(

	GafferFlamenco.FlamencoDispatcher,

	"description",
	"""
	Dispatches tasks to be run by the [Flamenco](https://flamenco.blender.org) render farm
	manager.
	""",

	plugs = {

		"managerURL" : [

			"description",
			"""
			The URL used to connect to the Flamenco manager. Defaults to a manager running
			on the same machine as Gaffer, using the default port.

			> Tip : The Flamenco manager displays its own URL when it is first started.
			""",

			"plugValueWidget:type", "GafferFlamencoUI.FlamencoDispatcherUI._ManagerURLPlugValueWidget",

		],

		"priority" : [

			"description",
			"""
			The priority of the job relative to other jobs.
			""",

		],

		## TODO : DESCRIPTIONS FOR EVERYTHING

		"workerTag" : [

			## TODO : PRESETS FOR TAGS
			#"plugValueWidget:type",

		],

	}

)

class _ManagerURLPlugValueWidget( GafferUI.PlugValueWidget ) :

	def __init__( self, plugs, **kw ) :

		print( "CONSTRUCTING" )

		column = GafferUI.ListContainer( spacing = 4 )
		GafferUI.PlugValueWidget.__init__( self, column, plugs )

		with column :

			GafferUI.StringPlugValueWidget( plugs )

			with GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Horizontal, spacing = 4 ) as self.__statusRow :

				self.__statusIcon = GafferUI.Image( "warningSmall.png" )
				self.__statusLabel = GafferUI.Label( "I am a status" )

				GafferUI.Spacer( imath.V2i( 1 ) )

				refreshButton = GafferUI.Button( hasFrame = False, image = "refresh.png" )
				refreshButton.clickedSignal().connect( Gaffer.WeakMethod( self._requestUpdateFromValues ) )

	def _updateFromValues( self, values, exception ) :

		print( "UPDATING" )

		# Possible code for auto-discovery of manager
		# https://github.com/tw7613781/ssdp_upnp/blob/master/ssdp_upnp/ssdp.py#L121-L138"


		managerURL = sole( values )

		with urllib.request.urlopen( f"{managerURL}/api/v3/jobs/type/gaffer" ) as response :
			print( response.read() )

		# If there was an error getting the plug values, that will be
		# shown by our StringPlugValueWidget. We can't show a status
		# if we don't have a plug value, so just hide the status bar.
		self.__statusRow.setVisible( exception is None )



	# 	request.add_header( 'Content-Type', 'application/json; charset=utf-8' )

	# 	try :
	# 		urllib.request.urlopen( request, json.dumps( job ).encode( "utf-8" ) )

		pass

