##########################################################################
#
#  Copyright (c) 2024, Image Engine Design Inc. All rights reserved.
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

import Gaffer
import GafferUI
import GafferScene
import GafferSceneUI

import IECore
import IECoreScene

import functools
import imath

Gaffer.Metadata.registerNode(

	GafferScene.ShaderTweakProxy,

	"description",
	"""
	Represents a shader in the shader network that a ShaderTweaks node is modifying. Allows forming
	connections from existing shaders to shaders that are being inserted.
	""",

	"icon", "shaderTweakProxy.png",

	plugs = {

		"name" : [

			"description", "Hardcoded for ShaderTweakProxy nodes.",
			"plugValueWidget:type", "",

		],

		"type" : [

			"description", "Hardcoded for ShaderTweakProxy nodes.",
			"plugValueWidget:type", "",

		],

		"parameters" : [

			"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",

		],

		"parameters.shaderTweakProxySourceNode" : [

			"description",
			"""
			The handle of the upstream shader being fetched by this proxy - or Auto, indicating that
			the original input of the parameter being ShaderTweaked will be used.
			""",
			"label", "Source Node",
			"readOnly", True,
			"nodule:type", "",
			"stringPlugValueWidget:placeholderText", "Auto",

		],

		"out" : [

			"plugValueWidget:type", "",
			"nodule:type", "GafferUI::CompoundNodule"

		],

		"out.*" : [

			"description",
			"""
			The name of the output on the shader we are fetching, or "auto" for an auto proxy.
			""",

		],

	}

)

def __findConnectedShaderTweaks( startShader ):
	shadersScanned = set()
	shadersToScan = [ startShader ]
	shaderTweaks = set()

	while len( shadersToScan ):
		shader = shadersToScan.pop()
		shadersScanned.add( shader )
		if isinstance( shader, GafferScene.ShaderTweaks ):
			shaderTweaks.add( shader )
			continue
		elif not isinstance( shader, GafferScene.Shader ):
			continue
		elif not "out" in shader:
			continue

		possibleOutputs = [ shader["out"] ]

		outputs = []

		while len( possibleOutputs ):
			po = possibleOutputs.pop()
			if po.outputs():
				outputs.append( po )
			else:
				for c in po.children():
					possibleOutputs.append( c )

		while len( outputs ):
			o = outputs.pop()
			if o.outputs():
				outputs += o.outputs()
			else:
				dest = Gaffer.PlugAlgo.findDestination( o, lambda plug : plug if not plug.outputs() else None ).node()
				if not dest in shadersScanned:
					shadersToScan.append( dest )

	return shaderTweaks

def __createShaderTweakProxy( plug, sourceNode, outputNames, outputTypes ):

	with Gaffer.UndoScope( plug.ancestor( Gaffer.ScriptNode ) ):
		result = GafferScene.ShaderTweakProxy( sourceNode, outputNames, outputTypes, name = sourceNode or "Auto" )
		plug.node().parent().addChild( result )

		# See if there are any output plugs on the new proxy which can be connected to this plug
		for p in result["out"].children():
			try:
				plug.setInput( p )
			except:
				continue
			break

		# \todo - It's probably bad that I'm doing this manually, instead of using GraphGadget.setNodePosition
		# ... but it also feels wrong that that is a non-static member of GraphGadget ... it doesn't use
		# any members of GraphGadget, and when creating a new ShaderTweakProxy from the Node Editor, this totally
		# makes sense to do, even if there are no current GraphGadgets
		if "__uiPosition" in plug.node():
			result.addChild( Gaffer.V2fPlug( "__uiPosition", Gaffer.Plug.Direction.In, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
			result["__uiPosition"].setValue( plug.node()["__uiPosition"].getValue() + imath.V2f( -20, 0 ) )

def _shaderAttributes( context, nodes, paths, affectedOnly ) :

	result = {}

	with context :
		for node in nodes:
			useFullAttr = node["localise"].getValue()
			attributeNamePatterns = node["shader"].getValue() if affectedOnly else "*"
			for path in paths :
				if not node["in"].exists( path ):
					continue

				attributes = node["in"].fullAttributes( path ) if useFullAttr else node["in"].attributes( path )
				for name, attribute in attributes.items() :
					if not IECore.StringAlgo.matchMultiple( name, attributeNamePatterns ) :
						continue
					if not isinstance( attribute, IECoreScene.ShaderNetwork ) or not len( attribute ) :
						continue
					result.setdefault( path, {} )[name] = attribute

	return result

def __getOutputs( shaderType, shaderName ) :
	dummyLoader = GafferSceneUI.ShaderTweaksUI.getShaderLoader( shaderType )()
	dummyLoader.loadShader( shaderName )

	# If "out" is a ValuePlug, that means we have a single output
	if isinstance( dummyLoader["out"], Gaffer.ValuePlug ):
		return IECore.CompoundData(
			{ "out" : Gaffer.PlugAlgo.extractDataFromPlug( dummyLoader["out"] ) }
		)

	return ( IECore.StringVectorData( dummyLoader["out"].keys() ), IECore.ObjectVector(
		[ Gaffer.PlugAlgo.extractDataFromPlug( dummyLoader["out"][k] ) for k in dummyLoader["out"].keys() ]
	) )

def __browseShaders( scriptWindow, plug, context, nodes, paths, title ) :

	shaderAttributes = _shaderAttributes( context, nodes, paths, affectedOnly = True )

	uniqueNetworks = { n.hash(): n for a in shaderAttributes.values() for n in a.values() }

	browser = GafferSceneUI.ShaderUI._ShaderDialogue( uniqueNetworks.values(), title )

	shaderHandle = browser.waitForShader( parentWindow = scriptWindow )

	if shaderHandle is not None :
		for n in uniqueNetworks.values():
			if shaderHandle in n.shaders().keys():
				shader = n.shaders()[shaderHandle]
				__createShaderTweakProxy( plug, shaderHandle, *__getOutputs( shader.type, shader.name ) )
				break

def _pathsFromAffected( context, nodes ) :

	pathMatcher = IECore.PathMatcher()
	with context:
		for node in nodes:
			GafferScene.SceneAlgo.matchingPaths( node["filter"], node["in"], pathMatcher )

	return pathMatcher.paths()

def _pathsFromSelection( context ) :

	paths = GafferSceneUI.ContextAlgo.getSelectedPaths( context )
	paths = paths.paths() if paths else []

	return paths


def __browseAffectedShaders( graphEditor, plug, shaderTweaksOverride ) :

	scriptWindow = graphEditor.ancestor( GafferUI.ScriptWindow )
	context = plug.ancestor( Gaffer.ScriptNode ).context()
	shaderTweaks = [ shaderTweaksOverride ] if shaderTweaksOverride else __findConnectedShaderTweaks( plug.node() )

	__browseShaders( scriptWindow, plug, context, shaderTweaks, _pathsFromAffected( context, shaderTweaks ), "Affected Shaders" )

def __browseSelectedShaders( graphEditor, plug, shaderTweaksOverride ) :

	scriptWindow = graphEditor.ancestor( GafferUI.ScriptWindow )
	context = plug.ancestor( Gaffer.ScriptNode ).context()
	shaderTweaks = [ shaderTweaksOverride ] if shaderTweaksOverride else __findConnectedShaderTweaks( plug.node() )
	__browseShaders( scriptWindow, plug, context, shaderTweaks, _pathsFromSelection( context ), "Selected Shaders" )

def _plugContextMenu( graphEditor, plug, shaderTweaks ) :

	menuDefinition = IECore.MenuDefinition()

	# Find the actual node if we're looking at something like a box input
	# NOTE : This could fail if a shader output is connected to 2 things, and the first thing is not a shader,
	# but that seems like a pretty weird case, and we want to get to the early out without doing too much traversal
	destPlug = Gaffer.PlugAlgo.findDestination( plug, lambda plug : plug if not plug.outputs() else None )

	if not ( isinstance( destPlug.node(), GafferScene.Shader ) or isinstance( destPlug.node(), GafferScene.ShaderTweaks ) ):
		return

	# Note the "createCounterpart" here, which is just to work around there not being a more direct way to
	# get the default value as data.
	valueData = Gaffer.PlugAlgo.getValueAsData( plug.createCounterpart( "unused", Gaffer.Plug.Direction.Out ) )

	menuDefinition.append(
		"Auto ( Original Input )",
		{
			"command" : functools.partial( __createShaderTweakProxy,
				plug, "", IECore.StringVectorData( [ "auto" ] ), IECore.ObjectVector( [ valueData ] )
			),
			"active" : not Gaffer.MetadataAlgo.readOnly( plug.node().parent() ),
		}
	)

	menuDefinition.append(
		"From Affected",
		{
			"command" : functools.partial( __browseAffectedShaders, graphEditor, plug, shaderTweaks ),
			"active" : not Gaffer.MetadataAlgo.readOnly( plug.node().parent() ),
		}
	)
	menuDefinition.append(
		"From Selected",
		{
			"command" : functools.partial( __browseSelectedShaders, graphEditor, plug, shaderTweaks ),
			"active" : not Gaffer.MetadataAlgo.readOnly( plug.node().parent() ),
		}
	)

	return menuDefinition

def __plugContextMenuSignal( graphEditor, plug, menuDefinition ) :
	menuDefinition.append( "/Create ShaderTweakProxy",
		{ "subMenu" : functools.partial( _plugContextMenu, graphEditor, plug, None ) }
	)

GafferUI.GraphEditor.plugContextMenuSignal().connect( __plugContextMenuSignal, scoped = False )
