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

import json
import socket
import sys
import urllib.error
import urllib.request

import IECore

import Gaffer
import GafferDispatch

class FlamencoDispatcher( GafferDispatch.Dispatcher ) :

	def __init__( self, name = "FlamencoDispatcher" ) :

		GafferDispatch.Dispatcher.__init__( self, name )

		self["managerURL"] = Gaffer.StringPlug( defaultValue = "" )
		self["priority"] = Gaffer.IntPlug( defaultValue = 50, minValue = 0 )
		self["workerTag"] = Gaffer.StringPlug()
		self["startPaused"] = Gaffer.BoolPlug()

	## Searches for a Flamenco manager on the local network, returning
	# a URL if one is found, or None otherwise.
	@staticmethod
	def discoverManagerURL() :

		import socket

		broadcastAddress = "239.255.255.250"
		broadcastPort = 1900

		import inspect

		request = (
			"M-SEARCH * HTTP/1.1\r\n" +
			f"HOST: {broadcastAddress}:{broadcastPort}\r\n" +
			'MAN: "ssdp:discover"\r\n' +
			"MX: 1\r\n" +
			"ST: urn:flamenco:manager:0\r\n"
		)

		request = request.replace( "\n", "\r\n" )
		request = request.lstrip()

		print( repr( request ) )

		#LOCATION_REGEX = re.compile("LOCATION: {}_{}://[ ]*(.+)\r\n".format(self.protocol, self.networkid), re.IGNORECASE)
		sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )

		sock.sendto( request.encode( "ASCII" ), ( broadcastAddress, broadcastPort ) )
		sock.settimeout( 3 )

		data, _ = sock.recvfrom( 1024 )
		print( data.decode( "ASCII" ), addr )

	def _doDispatch( self, rootBatch ) :

		# As far as I can tell, Flamenco doesn't have a generic
		# file format for scripted job submissions. So we just
		# build a simple list of tasks, which our Gaffer job type
		# translates to a Flamenco job during submission.

		batchesToTasks = {}
		for batch in rootBatch.preTasks() :
			self.__buildTasksWalk( batch, batchesToTasks )

		# Submit a job to run the tasks. There is a Flamenco Python
		# API we could use for this part, but the protocol is simple
		# enough that we can just do it with `urllib`. So in the
		# interests of simplicity, that's what we do.

		job = {

			"name" : self["jobName"].getValue(),
			"type" : "gaffer",
			"priority" : self["priority"].getValue(),
			"submitter_platform" : sys.platform, ## \todo Check
			"settings" : {
				"tasks" : list( batchesToTasks.values() ),
			},

			"worker_tag" : self["workerTag"].getValue(), ## NEEDS TO BE A UUID!!
			"initial_status" : "paused" if self["startPaused"].getValue() else "active",

		}

		print( job )

		managerURL = self["managerURL"].getValue().rstrip( "/" )
		request = urllib.request.Request( f"{managerURL}/api/v3/jobs" )
		request.add_header( 'Content-Type', 'application/json; charset=utf-8' )

		try :
			urllib.request.urlopen( request, json.dumps( job ).encode( "utf-8" ) )
		except urllib.error.HTTPError as e :
			raise Exception( f"Failed to submit job. Is the `gaffer.js` job type installed`?" ) from None
		except urllib.error.URLError as e :
			raise Exception( f"Failed to connect to manager. Is the manager running at {managerURL}?" ) from None

	def __buildTasksWalk( self, batch, batchesToTasks ) :

		# If we've already visited this batch, then return
		# the name of the task we made already.

		task = batchesToTasks.get( batch )
		if task is not None :
			return task["name"]

		# Otherwise make a new task. Start by getting a unique
		# task name.

		nodeName = batch.node().relativeName( batch.node().scriptNode() )
		frames = str( IECore.frameListFromList( [ int( x ) for x in batch.frames() ] ) )

		taskName = "{nodeName}-{hash}{frames}".format(
			nodeName = nodeName, hash = hash( batch ),
			frames = f"-{frames}" if frames else ""
		)

		task = { "name" : taskName }
		if frames :

			# Add a `gaffer execute` command to the task.

			args = [
				"execute",
				"-script", batch.context()["dispatcher:scriptFileName"],
				"-nodes", nodeName,
				"-frames", frames,
			]

			scriptContext = batch.node().scriptNode().context()
			contextArgs = []
			for entry in batch.context().keys() :
				if entry not in scriptContext.keys() or batch.context()[entry] != scriptContext[entry] :
					contextArgs.extend( [ "-" + entry, IECore.repr( batch.context()[entry] ) ] )

			if contextArgs :
				args.extend( [ "-context" ] + contextArgs )

			task["commandArgs"] = args

		# Recurse to upstream batches, adding them as dependencies of this task.

		for upstreamBatch in batch.preTasks() :
			task.setdefault( "dependencies", [] ).append( self.__buildTasksWalk( upstreamBatch, batchesToTasks ) )

		batchesToTasks[batch] = task
		return taskName

	@staticmethod
	def _setupPlugs( parentPlug ) :

		## \todo Allow task type to be specified
		return

IECore.registerRunTimeTyped( FlamencoDispatcher, typeName = "GafferFlamenco::FlamencoDispatcher" )

GafferDispatch.Dispatcher.registerDispatcher( "Flamenco", FlamencoDispatcher, FlamencoDispatcher._setupPlugs )
