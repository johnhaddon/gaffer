##########################################################################
#
#  Copyright (c) 2022, Cinesite VFX Ltd. All rights reserved.
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

#import IECore
#import IECoreScene

import Gaffer
import GafferScene
import GafferUI
import GafferSceneUI

## TODO : MAKE A SCENEEDITOR BASE CLASS TO ENCAPSULATE THE LOGIC ABOUT WHAT
# SCENE TO VIEW, AND TO TRACK THE REPARENTING OF THE PLUG.
class SetEditor( GafferUI.NodeSetEditor ) :

	def __init__( self, scriptNode, **kw ) :

		mainColumn = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, borderWidth = 4 )

		GafferUI.NodeSetEditor.__init__( self, mainColumn, scriptNode, nodeSet = scriptNode.focusSet(), **kw )

		with mainColumn :

			self.__pathListing = GafferUI.PathListingWidget(
				Gaffer.DictPath( {}, "/" ), # temp till we make a SetPath
				columns = [ GafferUI.PathListingWidget.defaultNameColumn ],
				selectionMode = GafferUI.PathListingWidget.SelectionMode.Rows,
				displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
			)

		# self.__sections = []

		# if sections is not None :

		# 	for section in sections :
		# 		mainColumn.append( section )
		# 		self.__sections.append( section )

		# 	mainColumn.append( GafferUI.Spacer( imath.V2i( 0 ) ), expand = True )

		# else :

		# 	columns = {}
		# 	with mainColumn :
		# 		columns[None] = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, spacing = 8, borderWidth = 5 )
		# 		tabbedContainer = GafferUI.TabbedContainer()

		# 	for registration in self.__sectionRegistrations :
		# 		section = registration.section()
		# 		column = columns.get( registration.tab )
		# 		if column is None :
		# 			with tabbedContainer :
		# 				with GafferUI.ScrolledContainer( horizontalMode = GafferUI.ScrollMode.Automatic, parenting = { "label" : registration.tab } ) :
		# 					column = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, borderWidth = 4, spacing = 8 )
		# 					columns[registration.tab] = column
		# 		column.append( section )
		# 		self.__sections.append( section )

		# 	for tab, column in columns.items() :
		# 		if tab is not None :
		# 			column.append( GafferUI.Spacer( imath.V2i( 0 ) ), expand = True )

		self._updateFromSet()

	def __repr__( self ) :

		return "GafferSceneUI.SetEditor( scriptNode )"

	def _updateFromSet( self ) :

		GafferUI.NodeSetEditor._updateFromSet( self )

		# Decide what plug we're viewing.
		self.__plug = None
		self.__plugParentChangedConnection = None
		node = self._lastAddedNode()
		if node is not None :
			self.__plug = next( GafferScene.ScenePlug.RecursiveOutputRange( node ), None ) ## TODO : IGNORE __ PREFIXES
			if self.__plug is not None :
				self.__plugParentChangedConnection = self.__plug.parentChangedSignal().connect(
					Gaffer.WeakMethod( self.__plugParentChanged ), scoped = True
				)

		self.__updatePathListingPath()

	def _updateFromContext( self, modifiedItems ) :

		if any( not i.startswith( "ui:" ) for i in modifiedItems ) :
			self.__updatePathListingPath()

	@GafferUI.LazyMethod( deferUntilPlaybackStops = True )
	def __updatePathListingPath( self ) :

		# for f in self.__filter.getFilters() :
		# 	f.setScene( self.__plug )

		if self.__plug is not None :
			# We take a static copy of our current context for use in the SetPath - this prevents the
			# PathListing from updating automatically when the original context changes, and allows us to take
			# control of updates ourselves in _updateFromContext(), using LazyMethod to defer the calls to this
			# function until we are visible and playback has stopped.
			contextCopy = Gaffer.Context( self.getContext() )
			# for f in self.__filter.getFilters() :
			# 	f.setContext( contextCopy )
			self.__pathListing.setPath( GafferScene.SetPath( self.__plug, contextCopy, "/" ) )
		else :
			self.__pathListing.setPath( Gaffer.DictPath( {}, "/" ) )

	# def _titleFormat( self ) :

	# 	return GafferUI.NodeSetEditor._titleFormat( self, _maxNodes = 2, _reverseNodes = True, _ellipsis = False )

	# def __plugDirtied( self, plug ) :

	# 	## TODO : CAN WE BE MORE ACCURATE HERE? JUST TEST AGAINST
	# 	if isinstance( plug, GafferScene.ScenePlug ) and plug.direction() == Gaffer.Plug.Direction.Out :
	# 		self.__updateLazily()

	def __plugParentChanged( self, plug, oldParent ) :

		# if a plug has been removed or moved to another node, then
		# we need to stop viewing it - _updateFromSet() will find the
		# next suitable plug from the current node set.
		self._updateFromSet()

GafferUI.Editor.registerType( "SetEditor", SetEditor )
