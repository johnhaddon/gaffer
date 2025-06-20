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
			self["__switch"]["in"].setInput( self["in"] )
			self["__switch"]["index"].setInput( self["__switchIndexQuery"]["out"][0]["value"] )
			self["__switch"]["deleteContextVariables"].setValue( "__sceneInspector:inputIndex" )
			self["__switchedIn"].setInput( self["__switch"]["out"] )

	IECore.registerRunTimeTyped( Settings, typeName = "GafferSceneUI::SceneInspector::Settings" )

	def __init__( self, scriptNode, **kw ) :

		mainColumn = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, borderWidth = 8 )

		GafferSceneUI.SceneEditor.__init__( self, mainColumn, scriptNode, **kw )

		with mainColumn :

			with GafferUI.TabbedContainer() :

				with GafferUI.ListContainer( spacing = 4, borderWidth = 4, parenting = { "label" : "Selection" } ) :

					self.__selectionPathListing = GafferUI.PathListingWidget(
						Gaffer.DictPath( {}, "/" ),
						columns = [
							GafferUI.StandardPathColumn( "Name", "name" ),
							_InspectorDiffColumn(
								_InspectorDiffColumn.DiffContext.A,
								headerData = GafferUI.PathColumn.CellData( value = "A" )
							),
							_InspectorDiffColumn(
								_InspectorDiffColumn.DiffContext.B,
								headerData = GafferUI.PathColumn.CellData( value = "B" )
							)
						],
						selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
						displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
					)

				with GafferUI.ListContainer( spacing = 4, borderWidth = 4, parenting = { "label" : "Globals" } ) :

					self.__globalsPathListing = GafferUI.PathListingWidget(
						Gaffer.DictPath( {}, "/" ),
						columns = [
							GafferUI.StandardPathColumn( "Name", "name" ),
							_InspectorDiffColumn(
								_InspectorDiffColumn.DiffContext.A,
								headerData = GafferUI.PathColumn.CellData( value = "A" )
							),
							_InspectorDiffColumn(
								_InspectorDiffColumn.DiffContext.B,
								headerData = GafferUI.PathColumn.CellData( value = "B" )
							)
						],
						selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
						displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
					)

		GafferSceneUI.ScriptNodeAlgo.selectedPathsChangedSignal( scriptNode ).connect(
			Gaffer.WeakMethod( self.__selectedPathsChanged )
		)

		self._updateFromSet()

	def __repr__( self ) :

		return "GafferSceneUI.SceneInspector( scriptNode )"

	# def _updateFromSettings( self, plug ) :

		# TODO : ADD FILTERS ETC

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

		scenes = [ s.getInput() for s in self.settings()["in"] if s.getInput() is not None ]

		paths = [ None ]
		lastSelectedPath = GafferSceneUI.ScriptNodeAlgo.getLastSelectedPath( self.scriptNode() )
		if lastSelectedPath :
			paths = [ lastSelectedPath ]
			selectedPaths = GafferSceneUI.ScriptNodeAlgo.getSelectedPaths( self.scriptNode() ).paths()
			if len( selectedPaths ) > 1 :
				paths.insert( 0, next( p for p in selectedPaths if p != lastSelectedPath ) )

		contexts = []
		for i in range( 0, 2 ) :
			context = Gaffer.Context( self.context() )
			context["__sceneInspector:inputIndex"] = i if i < len( scenes ) else 0
			context["scene:path"] = GafferScene.ScenePlug.stringToPath( paths[i] if i < len( paths ) else paths[0] )
			contexts.append( context )

		self.__selectionPathListing.setPath(
			_GafferSceneUI._SceneInspector.InspectorPath(
				self.settings()["__switchedIn"],
				contexts
			)
		)

		# with self.context():



		# 	if len( scenes ) > 1 :
		# 		paths = [ paths[-1] ]

		# 	targets = []
		# 	for sceneIndex, scene in enumerate( scenes ) :
		# 		for path in paths :
		# 			if path is not None and not scene.exists( path ) : ## TODO : DOES THIS BELONG HERE? IT'S IN THE FOREGROUND WHICH IS NOT GREAT. MOVE TO PATH?
		# 				# selection may not be valid for both scenes,
		# 				# and we can't inspect invalid paths.
		# 				path = None
		# 			targetContext = Gaffer.Context( self.context() )
		# 			if path is not None :
		# 				targetContext["scene:path"] = GafferScene.ScenePlug.stringToPath( path )
		# 			targetContext["__sceneInspector:inputIndex"] = sceneIndex
		# 			targets.append( self.Target( scene, path, targetContext ) )

		# 	for section in self.__sections :
		# 		section.update( targets )

GafferUI.Editor.registerType( "SceneInspector", SceneInspector )

##########################################################################
# Inspector
##########################################################################

## Abstract class for a callable which inspects a Target and returns
# a value. Inspectors are key to allowing the UI to perform the same
# query over multiple targets to generate history and inheritance
# queries.
# class Inspector( object ) :

# 	## Must be implemented to return a descriptive name
# 	# for what is being inspected.
# 	def name( self ) :

# 		raise NotImplementedError

# 	## Should return True if the Inspector's results
# 	# can meaningfully be tracked back up through the
# 	# node graph.
# 	def supportsHistory( self ) :

# 		return True

# 	## Should return True if the Inspector's results
# 	# may be inherited from parent locations - this will
# 	# enable inheritance queries for the inspector.
# 	def supportsInheritance( self ) :

# 		return False

# 	## Must be implemented to inspect the target and return
# 	# a value to be displayed. When supportsInheritance()==True,
# 	# this method must accept an ignoreInheritance keyword
# 	# argument (defaulting to False).
# 	def __call__( self, target, **kw ) :

# 		raise NotImplementedError

# 	## May be implemented to return a list of "child" inspectors -
# 	# this is used by the DiffColumn to obtain an inspector per row.
# 	def children( self, target ) :

# 		return []

# 	def _useBackgroundThread( self ) :

# 		return False

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

# ##########################################################################
# # Attributes section
# ##########################################################################

class _InspectorDiffColumn( GafferSceneUI.Private.InspectorColumn ) :

	DiffContext = enum.Enum( "DiffContext", [ "A", "B" ] )

	__backgroundColors = {
		DiffContext.A : imath.Color4f( 0.7, 0.12, 0, 0.3 ),
		DiffContext.B : imath.Color4f( 0.13, 0.62, 0, 0.3 ),
	}

	def __init__( self, diffContext, headerData, sizeMode = GafferUI.PathColumn.SizeMode.Default ) :

		print( diffContext.name )

		GafferSceneUI.Private.InspectorColumn.__init__(
			self, inspector = None, headerData = headerData,
			contextProperty = "inspector:context{}".format( diffContext.name ),
			sizeMode = sizeMode
		)

		otherDiffContext = self.DiffContext.A if diffContext == self.DiffContext.B else self.DiffContext.B
		self.__otherColumn = GafferSceneUI.Private.InspectorColumn.__init__(
			self, inspector = None, headerData = self.CellData(),
			contextProperty = "inspector:context{}".format( otherDiffContext.name ),
		)

class _AttributesPath( Gaffer.Path ) :

	def __init__( self, scene, contexts, path = "/", root = "/", filter = None ) :

		Gaffer.Path.__init__( self, path, root, filter )

		self.__editScope = Gaffer.Plug() ## TODO
		self.__scene = scene
		self.__contexts = contexts

		## TODO : COULD JUST DROP THIS, AND RELY ON THE MAIN UPDATE CALL?
		## MAYBE THAT MAKES THE MOST SENSE IN THE SHORT TERM?
		self.__plugDirtiedConnection = self.__scene.node().plugDirtiedSignal().connect( Gaffer.WeakMethod( self.__plugDirtied ), scoped = True )

	# def isValid( self, canceller ) :

	# 	return True

	# def isLeaf( self, canceller ) :

	# 	return False

	# def propertNames()

	# TODO IMPLEMENT ME

	def property( self, name, canceller = None ) :

		if name == "inspector:inspector" :
			if len( self ) :
				return GafferSceneUI.Private.AttributeInspector( self.__scene, self.__editScope, self[-1] )
		elif name == "inspector:contextA" :
			return self.__contexts[0]
		elif name == "inspector:contextB" :
			return self.__contexts[1] if len( self.__contexts ) > 1 else None

		return Gaffer.Path.property( self, name, canceller )

	def copy( self ) :

		return _AttributesPath( self.__scene, self.__contexts, self[:], self.root(), self.getFilter() )

	def cancellationSubject( self ) :

		return self.__scene

	def _children( self, canceller ) :

		if len( self ) > 0 :
			return []

		names = set()

		for context in self.__contexts :
			if "scene:path" not in context :
				continue
			with Gaffer.Context( context, canceller ) :
				if self.__scene["exists"].getValue() : ## TODO : NOT STRICTLY NECESSARY NOW, BUT MAKES SENSE I THINK
					attributes = self.__scene.fullAttributes( context["scene:path"] )
					names.update( attributes.keys() )

		return [
			_AttributesPath( self.__scene, self.__contexts, self[:] + [ name ], self.root(), self.getFilter() )
			for name in sorted( names )
		]

	def __plugDirtied( self, plug ) :

		if plug.isSame( self.__scene["attributes"] ) :
			self._emitPathChanged()

# class __AttributesSection( LocationSection ) :

# 	def __init__( self ) :

# 		LocationSection.__init__( self, collapsed = True, label = "Attributes" )

# 		with self._mainColumn() :
# 			self.__pathListing = GafferUI.PathListingWidget(
# 				Gaffer.DictPath( {}, "/" ),
# 				columns = [
# 					GafferUI.StandardPathColumn( "Name", "name" ),
# 					_InspectorDiffColumn(
# 						_InspectorDiffColumn.DiffContext.A,
# 						headerData = GafferUI.PathColumn.CellData( value = "A" )
# 					),
# 					_InspectorDiffColumn(
# 						_InspectorDiffColumn.DiffContext.B,
# 						headerData = GafferUI.PathColumn.CellData( value = "B" )
# 					)
# 				],
# 				selectionMode = GafferUI.PathListingWidget.SelectionMode.Cells,
# 				displayMode = GafferUI.PathListingWidget.DisplayMode.Tree,
# 			)

# 	def update( self, targets ) :

# 		LocationSection.update( self, targets )

# 		self.__pathListing.setPath(
# 			_AttributesPath(
# 				self._settings()["__switchedIn"], ## TODO : PLUG MAYBE NOT PRIVATE?
# 				[ t.context for t in targets ]
# 			)
# 		)

# SceneInspector.registerSection( __AttributesSection, tab = "Selection" )
