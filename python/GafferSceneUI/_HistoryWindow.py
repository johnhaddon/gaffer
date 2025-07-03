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

import imath

import Gaffer
import GafferUI
import GafferScene
import GafferSceneUI

class _OperationIconColumn( GafferUI.PathColumn ) :

	def __init__( self, title, property ) :

		GafferUI.PathColumn.__init__( self )

		self.__title = title
		self.__property = property

	def cellData( self, path, canceller = None ) :

		cellValue = path.property( self.__property )

		data = self.CellData()

		data.icon = {
			Gaffer.TweakPlug.Mode.Replace : "replaceSmall.png",
			Gaffer.TweakPlug.Mode.Add : "plusSmall.png",
			Gaffer.TweakPlug.Mode.Subtract : "minusSmall.png",
			Gaffer.TweakPlug.Mode.Multiply : "multiplySmall.png",
			Gaffer.TweakPlug.Mode.Remove : "removeSmall.png",
			Gaffer.TweakPlug.Mode.Create : "createSmall.png",
			Gaffer.TweakPlug.Mode.CreateIfMissing : "createIfMissingSmall.png",
			Gaffer.TweakPlug.Mode.Min : "lessThanSmall.png",
			Gaffer.TweakPlug.Mode.Max : "greaterThanSmall.png",
			Gaffer.TweakPlug.Mode.ListAppend : "listAppendSmall.png",
			Gaffer.TweakPlug.Mode.ListPrepend : "listPrependSmall.png",
			Gaffer.TweakPlug.Mode.ListRemove : "listRemoveSmall.png",
		}.get( cellValue, "errorSmall.png" )

		return data

	def headerData( self, canceller = None ) :

		return self.CellData( self.__title )

class _NodeNameColumn( GafferUI.PathColumn ) :

	def __init__( self, title, property ) :

		GafferUI.PathColumn.__init__( self )

		self.__title = title
		self.__property = property

	def cellData( self, path, canceller = None ) :

		node = path.property( self.__property )
		return self.CellData( node.relativeName( node.scriptNode() ) )

	def headerData( self, canceller = None ) :

		return self.CellData( self.__title )

# \todo This duplicates logic from (in this case) `_GafferSceneUI._LightEditorInspectorColumn`.
# Refactor to allow calling `_GafferSceneUI.InspectorColumn.cellData()` from `_HistoryWindow` to
# remove this duplication for columns that customize their value presentation.
# DO WE WANT TO USE THE EXACT SAME COLUMN AS WE GOT THE HISTORY FROM? WOULD IT BE ODD TO GET THE
# DIFF BACKGROUND IN THE TRACEBACK? MAYBE IT WOULD BE USEFUL, TO SEE WHERE THE THING HAD DIVERGED?
# HOW DO WE GET THE DIFF CONTEXT INTO THE HISTORY PATH ANYHOW?
#
# I'M FAIRLY SURE WE JUST WANT TO USE A REGULAR INSPECTORCOLUMN, AND NOT A FANCY DIFF COLUMN OR
# ANYTHING LIKE THAT. AND I THINK CUSTOM VALUE PRESENTATION SHOULD BE DONE BY INSPECTORCOLUMN
# ANYWAY. SO THE MAIN ISSUE IS GETTING THE RIGHT CONTEXT INTO THE PATH. IT WOULD BE KINDOF CONVENIENT
# JUST TO USE THE ORIGINAL PATH AND GET THE CONTEXT UPDATES AUTOMATICALLY (BECAUSE THEY ARE BEING
# MADE BY THE ORIGINAL EDITOR). I DON'T THINK THAT IS POSSIBLE THOUGH, BECAUSE THE CELL PATH IS
# NOT THE SAME AS THE ROOT PATH. OK, SO ATTRIBUTEPATH.SETCONTEXT() NEEDS TO MERGE IN ITS OWN BITS
# APPROPRIATELY.
class _ValueColumn( GafferUI.PathColumn ) :

	def __init__( self, title, property, fallbackProperty ) :

		GafferUI.PathColumn.__init__( self )

		self.__title = title
		self.__property = property
		self.__fallbackProperty = fallbackProperty

	def cellData( self, path, canceller = None ) :

		cellValue = path.property( self.__property )
		fallbackValue = path.property( self.__fallbackProperty )

		data = self.CellData()

		if cellValue is not None :
			data.value = cellValue
		elif fallbackValue is not None :
			data.value = fallbackValue
			data.foreground = imath.Color4f( 0.64, 0.64, 0.64, 1.0 )

		if isinstance( data.value, ( imath.Color3f, imath.Color4f ) ) :
			data.icon = data.value

		return data

	def headerData( self, canceller = None ) :

		return self.CellData( self.__title )

class _HistoryWindow( GafferUI.Window ) :

	def __init__( self, inspector, inspectionPath, title=None, **kw ) :

		if title is None :
			title = "History"

		GafferUI.Window.__init__( self, title, **kw )

		self.__inspector = inspector
		self.__inspectionPath = inspectionPath

		with self :
			self.__pathListingWidget = GafferUI.PathListingWidget(
				Gaffer.DictPath( {}, "/" ),
				columns = (
					_NodeNameColumn( "Node", "history:node" ),
					_ValueColumn( "Value", "history:value", "history:fallbackValue" ),
					_OperationIconColumn( "Operation", "history:operation" ),
				),
				sortable = False,
				horizontalScrollMode = GafferUI.ScrollMode.Automatic,
				selectionMode = GafferUI.PathListingWidget.SelectionMode.Cell
			)

		self.__pathListingWidget.setDragPointer( "values" )

		self.__nameColumnIndex = 0
		self.__valueColumnIndex = 1
		self.__operationColumnIndex = 2

		self.__pathListingWidget.buttonDoubleClickSignal().connectFront( Gaffer.WeakMethod( self.__buttonDoubleClick ) )
		self.__pathListingWidget.keyPressSignal().connectFront( Gaffer.WeakMethod( self.__keyPress ) )
		self.__pathListingWidget.dragBeginSignal().connectFront( Gaffer.WeakMethod( self.__dragBegin ) )
		self.__pathListingWidget.updateFinishedSignal().connectFront( Gaffer.WeakMethod( self.__updateFinished ) )

		inspector.dirtiedSignal().connect( Gaffer.WeakMethod( self.__inspectorDirtied ) )

		## \todo We want to make the inspection framework scene-agnostic. We could add an `Inspector::plug()` method
		# to provide a scene-agnostic way of querying what is being inspected, and use it here.
		self.__contextTracker = GafferUI.ContextTracker.acquireForFocus( self.__inspectionPath.getScene() )
		self.__contextTracker.changedSignal().connect( Gaffer.WeakMethod( self.__contextChanged ) )
		self.__updatePath()

	def __updatePath( self ) :

		## TODO : ASSUMES PATH HAS SETCONTEXT METHOD, WHICH WE DON'T REALLY WANT. ALSO ASSUMES IT IS THREADSAFE,
		# WHICH IT IS NOT. IF WE ACCEPT WE NEED SETCONTEXT, WE COULD COPY PATH TO DEAL WITH THREAD-SAFETY?
		# AND SCENEINSPECTORPATH CAN SPLIT THE CONTEXT IN TWO AND ADD THE INPUT INDEX TO EACH.
		# COULD USE CANCELLATIONSUBJECT INSTEAD OF GETSCENE, SO THIS ISN'T SCENE-SPECIFIC?
		self.__inspectionPath.setContext( self.__contextTracker.context( self.__inspectionPath.getScene() ) )
		with self.__inspectionPath.property( "inspector:context" ) : # NOT RIGHT FOR DIFF COLUMNS
			# TODO : ADD INSPECTIONCONTEXT METHOD TO COLUMN? OR HISTORYPATH METHOD?
			self.__path = self.__inspector.historyPath()

		self.__pathChangedConnection = self.__path.pathChangedSignal().connect( Gaffer.WeakMethod( self.__pathChanged ), scoped = True )
		self.__pathListingWidget.setPath( self.__path )

	def __pathChanged( self, path ) :

		## \todo This invokes computes on the UI thread, but the point of using
		# a Path and PathListingWidget was to never block the UI.
		if len( path.children() ) == 0 :
			self.close()

	def __buttonDoubleClick( self, pathListing, event ) :

		if pathListing.pathAt( event.line.p0 ) is None :
			return False

		if event.buttons == event.Buttons.Left :
			self.__editSelectedCell( pathListing )

			return True

		return False

	def __keyPress( self, pathListing, event ) :

		if event.key == "Return" or event.key == "Enter" :
			self.__editSelectedCell( pathListing )

			return True

		return False

	def __editSelectedCell( self, pathListing ) :

		selection = pathListing.getSelection()

		selectedPath, selectedColumn = self.__selectionData( selection )

		if selectedColumn == self.__nameColumnIndex :
			GafferUI.NodeEditor.acquire(
				selectedPath.property( "history:node" ),
				floating = True
			)
		elif (
			( selectedColumn == self.__valueColumnIndex or selectedColumn == self.__operationColumnIndex ) and
			not isinstance( self.__inspector, GafferSceneUI.Private.SetMembershipInspector )
		) :
			editPlug = selectedPath.property( "history:source" )
			self.__popup = GafferUI.PlugPopup(
				[ editPlug ], warning = selectedPath.property( "history:editWarning" )
			)
			if isinstance( self.__popup.plugValueWidget(), GafferUI.TweakPlugValueWidget ) :
				self.__popup.plugValueWidget().setNameVisible( False )

			self.__popup.popup( parent = self )

	def __dragBegin( self, pathListing, event ) :

		selection = pathListing.getSelection()

		selectedPath, selectedColumn = self.__selectionData( selection )

		if selectedColumn == self.__nameColumnIndex :
			GafferUI.Pointer.setCurrent( "nodes" )

			return selectedPath.property( "history:node" )

		elif selectedColumn == self.__operationColumnIndex :
			GafferUI.Pointer.setCurrent( "values" )

			return selectedPath.property( "history:operation" )

		# Value column works by default

	def __selectionData( self, selection ) :

		# Return a tuple of (selectedPath, selectedColumnIndex )

		columnIndex = None

		for i in range( 0, len( selection ) ) :
			if selection[i].size() > 0 :
				selectedPathString = selection[i].paths()[0]
				columnIndex = i

		for path in self.__path.children() :
			if str( path ) == selectedPathString :
				return (path, columnIndex )

		return None

	def __inspectorDirtied( self, inspector ) :

		self.__path._emitPathChanged()

	def __contextChanged( self, contextTracker ) :

		self.__updatePath()

	def __updateFinished( self, pathListing ) :

		self.__nodeNameChangedSignals = {}

		## \todo calling `children()` here can cause computations/inspections
		# to occur _now_ on the UI thread. But the point of using
		# HistoryPath/PathListingWidget is to make sure all that happens in the
		# background. Can we do better?
		for path in self.__path.children() :

			# The node and all of its parents up to the script node
			# contribute to the path name.

			node = path.property( "history:node" )
			while node is not None and not isinstance( node, Gaffer.ScriptNode ) :
				if node not in self.__nodeNameChangedSignals :
					self.__nodeNameChangedSignals[node] = node.nameChangedSignal().connect(
						Gaffer.WeakMethod( self.__nodeNameChanged ),
						scoped = True
					)

				node = node.parent()

	def __nodeNameChanged( self, node, oldName ) :

		nameColumn = self.__pathListingWidget.getColumns()[0]
		nameColumn.changedSignal()( nameColumn )
