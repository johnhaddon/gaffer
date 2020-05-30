##########################################################################
#
#  Copyright (c) 2020, John Haddon. All rights reserved.
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

import os
import re
import functools
import token
import keyword
import tokenize
import collections
import io

import imath

import IECore

import Gaffer
import GafferUI

from Qt import QtGui

class CodeWidget( GafferUI.MultiLineTextWidget ) :

	def __init__( self, text="", editable=True, fixedLineHeight=None, **kw ) :

		GafferUI.MultiLineTextWidget.__init__( self, text, editable, fixedLineHeight = fixedLineHeight, wrapMode = self.WrapMode.None, role = self.Role.Code, **kw )

		self.__completer = None
		self.__highlighter = _QtHighlighter( self._qtWidget().document() )

		self.keyPressSignal().connect( Gaffer.WeakMethod( self.__keyPress ), scoped = False )

	def setCompleter( self, completer ) :

		self.__completer = completer

	def getCompleter( self ) :

		return self.__completer

	def setHighlighter( self, highlighter ) :

		self.__highlighter.setHighlighter( highlighter )

	def getHighlighter( self ) :

		return self.__highlighter.getHighlighter()

	def __keyPress( self, widget, event ) :

		if event.key == "Tab" :
			return self.__tabPress()
		elif event.modifiers == event.Modifiers.Control and event.key == "BracketLeft" :
			return self.__indentPress( -1 )
		elif event.modifiers == event.Modifiers.Control and event.key == "BracketRight" :
			return self.__indentPress( 1 )

		return False

	def __tabPress( self ) :

		if self.__completer is None :
			return False

		text = self.getText()
		cursor = self.getCursorPosition()
		lineStart = max( text.rfind( "\n", 0, cursor ), 0 )
		line = text[lineStart:cursor]

		if not line.strip() :
			return False

		completions = self.__completer.completions( line )
		if not completions :
			return False

		commonPrefix = os.path.commonprefix( [ c.text for c in completions ] )
		if len( commonPrefix ) > len( line ) :
			self.insertText( commonPrefix[len(line):] )
		else :
			menuDefinition = IECore.MenuDefinition()
			for i, c in enumerate( completions ) :
				menuDefinition.append(
					"/{}".format( i ),
					{
						"label" : c.label,
						"command" : functools.partial(
							Gaffer.WeakMethod( self.insertText ), c.text[len(line):]
						)
					}
				)
			self.__completionMenu = GafferUI.Menu( menuDefinition )
			self.__completionMenu.popup(
				parent = self.ancestor( GafferUI.Window ),
				position = self.cursorBound().max(),
				grabFocus = False
			)

		return True

	def __indentPress( self, indent ) :

		cursor = self._qtWidget().textCursor()
		if not cursor.hasSelection() :
			return False

		startBlock = self._qtWidget().document().findBlock( cursor.position() )
		endBlock = self._qtWidget().document().findBlock( cursor.anchor() )
		if startBlock.blockNumber() > endBlock.blockNumber() :
			startBlock, endBlock = endBlock, startBlock

		try :

			cursor.beginEditBlock()

			while True :

				cursor = QtGui.QTextCursor( startBlock )
				if indent > 0 :
					cursor.insertText( "\t" * indent )
				else :
					for i in range( 0, len( os.path.commonprefix( [ startBlock.text(), "\t" * -indent ] ) ) ) :
						cursor.deleteChar()

				if startBlock == endBlock :
					break
				else :
					startBlock = startBlock.next()

		finally :

			cursor.endEditBlock()

		return True

# Highlighter classes
# ===================

class Highlighter( object ) :

	Type = IECore.Enum.create(
		"SingleQuotedString", "DoubleQuotedString", "Number",
		"Keyword", "ControlFlow", "Braces", "Operator", "Call",
		"Comment", "ReservedWord", "Preprocessor"
	)

	# Specifies a highlight type to be used for the characters
	# between `start` and `end`. End may be `None` to signal that
	# the last highlight on the line continues to the next line.
	Highlight = collections.namedtuple( "Highlight", [ "start", "end", "type" ] )

	# Must be implemented to return a list of Highlight
	# objects for the text in `line`. If the last highlight
	# on the previous line had `end == None`, its type is passed
	# as `previousHighlightType`, allowing the highlighting to be
	# continued on this line.
	def highlights( self, line, previousHighlightType ) :

		raise NotImplementedError

class PythonHighlighter( Highlighter ) :

	__controlFlowKeywords = {
		"if", "elif", "else",
		"try", "except", "finally",
		"for", "while",
		"from", "import",
		"return",
	}

	__parenthesis = {
		"(", ")", "[", "]", "{", "}",
	}

	__operators = {
		"=", "==", "!=",
		"+", "+=", "-", "-=", "*", "*=", "/", "/=", "//", "//=", "%", "%=",
		"|", "|=", "&", "&=", "^", "^=", "~",
		">", ">=", "<", "<=", "**", "<<", ">>",
	}

	def highlights( self, line, previousHighlightType ) :

		if previousHighlightType in ( self.Type.SingleQuotedString, self.Type.DoubleQuotedString ) :
			# Continuation of multi-line string
			openingQuote = '"""' if previousHighlightType == self.Type.DoubleQuotedString else "'''"
			highlights = self.highlights( openingQuote + line, None )
			return [
				self.Highlight( max( 0, h.start - 3 ), h.end - 3 if h.end is not None else None, h.type )
				for h in highlights
			]

		result = []

		pendingName = None
		try :
			for tokenType, string, start, end, _  in tokenize.generate_tokens( io.StringIO( line ).readline ) :
				highlightType = None
				if tokenType == token.NAME :
					if string in self.__controlFlowKeywords :
						highlightType = self.Type.ControlFlow
					elif keyword.iskeyword( string ) or string == "self" :
						highlightType = self.Type.Keyword
					else :
						pendingName = self.Highlight( start[1], end[1], self.Type.Call )
						continue
				elif tokenType == token.OP :
					if string in self.__parenthesis :
						highlightType = self.Type.Braces
						if string == "(" :
							if pendingName is not None :
								result.append( pendingName )
					elif string in self.__operators :
						highlightType = self.Type.Operator
				elif tokenType == token.STRING :
					highlightType = self.__stringType( string[-1] )
				elif tokenType == tokenize.COMMENT :
					highlightType = self.Type.Comment
				elif tokenType == token.NUMBER :
					highlightType = self.Type.Number

				if highlightType is not None  :
					result.append( self.Highlight( start[1], end[1], highlightType ) )

				pendingName = None
		except tokenize.TokenError as e :
			if e.args[0] == "EOF in multi-line string" :
				result.append( self.Highlight( e.args[1][1], None, self.__stringType( line[e.args[1][1]] ) ) )

		return result

	def __stringType( self, quote ) :

		return {
			"'" : self.Type.SingleQuotedString,
			'"' : self.Type.DoubleQuotedString
		}[quote]

CodeWidget.Highlighter = Highlighter
CodeWidget.PythonHighlighter = PythonHighlighter

# QSyntaxHighlighter used to adapt our Highlighter class
# for use with a QTextDocument.
class _QtHighlighter( QtGui.QSyntaxHighlighter ) :

	def __init__( self, document ) :

		QtGui.QSyntaxHighlighter.__init__( self, document )

		self.__highlighter = None

		def format( color ) :

			f = QtGui.QTextCharFormat()
			f.setForeground( color )
			return f

		self.__formats = {
			Highlighter.Type.SingleQuotedString : format( QtGui.QColor( 216, 146, 115 ) ),
			Highlighter.Type.DoubleQuotedString : format( QtGui.QColor( 216, 146, 115 ) ),
			Highlighter.Type.Number : format( QtGui.QColor( 174, 208, 164 ) ),
			Highlighter.Type.Keyword : format( QtGui.QColor( 64, 156, 219 ) ),
			Highlighter.Type.ControlFlow : format( QtGui.QColor( 207, 128, 195 ) ),
			Highlighter.Type.Braces : format( QtGui.QColor( 255, 255, 0 ) ),
			Highlighter.Type.Operator : format( QtGui.QColor( 201, 0, 28 ) ),
			Highlighter.Type.Call : format( QtGui.QColor( 219, 221, 164 ) ),
			Highlighter.Type.Comment : format( QtGui.QColor( 90, 156, 76 ) ),
			Highlighter.Type.ReservedWord : format( QtGui.QColor( 200, 0, 0 ) ),
			Highlighter.Type.Preprocessor : format( QtGui.QColor( 207, 128, 195 ) ),
		}

	# Our methods
	# ===========

	def setHighlighter( self, highlighter ) :

		self.__highlighter = highlighter
		self.rehighlight()

	def getHighlighter( self ) :

		return self.__highlighter

	# Qt methods
	# ==========

	def highlightBlock( self, text ) :

		self.setCurrentBlockState( -1 )
		if self.__highlighter is None :
			return

		previousType = None
		if self.previousBlockState() != -1 :
			previousType = Highlighter.Type( self.previousBlockState() )

		highlights = self.__highlighter.highlights( text, previousType )

		for h in highlights :
			end = h.end
			if end is None :
				self.setCurrentBlockState( int( h.type ) )
				end = len( text )
			self.setFormat( h.start, end - h.start, self.__formats[h.type] )

# Completer classes
# ===================

class Completer( object ) :

	## Specifies completed text and a label suitable
	# for referring to it
	Completion = collections.namedtuple( "Completion", [ "text", "label" ] )

	## Must be implemented to return a list of possible
	# completions for the specified text.
	def completions( self, text ) :

		raise NotImplementedError

class PythonCompleter( Completer ) :

	__searchPrefix = r"(?:^|(?<=[\s,(]))"

	def __init__( self, namespace ) :

		self.__namespace = namespace

	def completions( self, text ) :

		return sorted( set(
			self.__attrAndItemCompletions( text ) +
			self.__globalCompletions( text )
		) )

	def __globalCompletions( self, text ) :

		globalVariable = r"{searchPrefix}([a-zA-Z0-9]+)$".format( searchPrefix = self.__searchPrefix )

		match = re.search( globalVariable, text )
		if not match :
			return []

		word = match.group( 0 )

		namespace = __builtins__.copy()
		namespace.update( self.__namespace )

		return self.__completions( namespace.items(), text[:-len(word)], word )

	def __attrAndItemCompletions( self, text ) :

		word = r"[a-zA-Z0-9_]+"
		optionalWord = r"[a-zA-Z0-9_]*"
		getAttr = r"\.{word}".format( word = word )
		partialGetAttr = r"\.{optionalWord}".format( optionalWord = optionalWord )
		quotedWord = r"""(?:'{word}'|"{word}")""".format( word = word )
		getItem = r"\[{quotedWord}\]".format( quotedWord = quotedWord )
		partialGetItem = r"\[(?:['\"]{optionalWord})?".format( optionalWord = optionalWord )
		path = r"{searchPrefix}({word}(?:{getAttr}|{getItem})*)({partialGetAttr}|{partialGetItem})$".format(
			searchPrefix = self.__searchPrefix, word = word, getAttr = getAttr, getItem = getItem,
			partialGetAttr = partialGetAttr, partialGetItem = partialGetItem
		)

		pathMatch = re.search( path, text )
		if not pathMatch :
			return []

		objectPath, partial = pathMatch.group( 1, 2 )
		try :
			rootObject = eval( objectPath, self.__namespace )
		except :
			return []

		prefix = text[:-len(partial)]

		names = []
		if partial.startswith( "." ) :
			# Attribute
			names = dir( rootObject )
			if len( partial ) < 2 or partial[1] != "_" :
				names = [ n for n in names if not n.startswith( "_" ) ] # MOVE TO __COMPLETIONS
			items = [ ( n, getattr( rootObject, n ) ) for n in names ]
			namePrefix = "."
			nameSuffix = ""
		else :
			# Item
			try :
				items = rootObject.items()
			except :
				return []
			quote = partial[1] if len( partial ) > 1 else '"'
			namePrefix = "[" + quote
			nameSuffix = quote + "]"

		partialName = partial[len(namePrefix):]

		return self.__completions( items, prefix, partialName, namePrefix, nameSuffix )

	## SHOULD BE ABLE TO DROP NAMEPREFIX?
	## NEED TO ADD FUNCTION CALLS
	def __completions( self, items, prefix, partialName, namePrefix = "", nameSuffix = "" ) :

		result = []
		for name, value in items :
			if name.startswith( partialName ) and ( name != partialName or nameSuffix ):
				result.append(
					self.Completion(
						prefix + namePrefix + name + nameSuffix,
						namePrefix + name + nameSuffix
					)
				)

		return sorted( result )

CodeWidget.Completer = Completer
CodeWidget.PythonCompleter = PythonCompleter


