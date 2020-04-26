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

	# MAKE BASE CLASS
	class PythonCompleter( object ) :

		__searchPrefix = r"(?:^|(?<=[\s,(]))"

		def __init__( self, namespace ) :

			self.__namespace = namespace

		def __call__( self, text ) :

			return sorted( set(
				self.__attrAndItemMatches( text ) +
				self.__globalMatches( text )
			) )

		def __globalMatches( self, text ) :

			globalVariable = r"{searchPrefix}([a-zA-Z0-9]+)$".format( searchPrefix = self.__searchPrefix )

			match = re.search( globalVariable, text )
			if not match :
				return []

			word = match.group( 0 )
			prefix = text[:-len(word)]

			namespace = __builtins__.copy()
			namespace.update( self.__namespace )

			result = []
			for name in namespace.keys() :
				if name.startswith( word ) and name != word :
					result.append( prefix + name )

			return result

		def __attrAndItemMatches( self, text ) :

			word = r"[a-zA-Z0-9]+"
			optionalWord = r"[a-zA-Z0-9]*"
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
				names = set( dir( rootObject ) )
				namePrefix = "."
				nameSuffix = ""
			else :
				# Item
				try :
					names = rootObject.keys()
				except :
					return []
				quote = partial[1] if len( partial ) > 1 else '"'
				namePrefix = "[" + quote
				nameSuffix = quote + "]"

			partialName = partial[len(namePrefix):]

			result = []
			for name in names :
				if name.startswith( partialName ) and ( name != partialName or nameSuffix ):
					result.append( prefix + namePrefix + name + nameSuffix )

			return sorted( result )

	def __keyPress( self, widget, event ) :

		if self.__completer is None or event.key != "Tab" :
			return False

		text = self.getText()
		cursor = self.getCursorPosition()
		lineStart = max( text.rfind( "\n", 0, cursor ), 0 )
		line = text[lineStart:cursor]

		if not line.strip() :
			return False

		completions = self.__completer( line )
		if not completions :
			return False

		commonPrefix = os.path.commonprefix( completions )
		if len( commonPrefix ) > len( line ) :
			self.insertText( commonPrefix[len(line):] )
		else :
			menuDefinition = IECore.MenuDefinition()
			for i, c in enumerate( completions ) :
				menuDefinition.append(
					"/{}".format( i ),
					{
						"label" : c,
						"command" : functools.partial(
							Gaffer.WeakMethod( self.insertText ), c[len(line):]
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


class Highlighter( object ) :

	Type = IECore.Enum.create(
		"String", "Number", "Comment",
		"Keyword", "ControlFlow", "Braces",
		"Operator", "Call",
	)

	# Specifies a highlight type to be used for the characters
	# between `start` and `end`. End may be `None` to signal that
	# the last highlight on the line continues to the next line.
	Highlight = collections.namedtuple( "Highlight", [ "start", "end", "type" ] )

	# Must be implemented to return a list of Highlight
	# objects for the text in `line`. If the last highlight
	# on the previous line had `end == None`, its type is passed
	# as `previousType`, allowing the highlighting to be continued
	# on this line.
	def highlights( self, line, previousType ) :

		raise NotImplementedError

## todo ; support single triple quotes multi-line
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

	def highlights( self, line, previousType ) :

		if previousType == self.Type.String :
			# Continuation of multi-line string
			highlights = self.highlights( '"""' + line, None )
			return [
				self.Highlight( max( 0, h.start - 3 ), h.end - 3 if h.end is not None else None, h.type )
				for h in highlights
			]

		result = []

		pendingName = None
		try :
			for t in tokenize.generate_tokens( io.StringIO( line ).readline ) :
				highlightType = None
				if t[0] == token.NAME :
					if t[1] in self.__controlFlowKeywords :
						highlightType = self.Type.ControlFlow
					elif keyword.iskeyword( t[1] ) or t[1] == "self" :
						highlightType = self.Type.Keyword
					else :
						pendingName = t
						continue
				elif t[0] == token.STRING :
					highlightType = self.Type.String
				elif t[0] == tokenize.COMMENT :
					highlightType = self.Type.Comment
				elif t[0] == token.NUMBER :
					highlightType = self.Type.Number
				elif t[0] == token.OP :
					if t[1] in self.__parenthesis :
						highlightType = self.Type.Braces
						if t[1] == "(" :
							if pendingName is not None :
								result.append( self.Highlight( pendingName[2][1], pendingName[3][1], self.Type.Call ) )
					elif t[1] in self.__operators :
						highlightType = self.Type.Operator

				if highlightType is not None  :
					result.append( self.Highlight( t[2][1], t[3][1], highlightType ) )

				pendingName = None
		except tokenize.TokenError as e :
			if e.args[0] == "EOF in multi-line string" :
				result.append( self.Highlight( e.args[1][1], end = None, type = self.Type.String ) )

		return result

CodeWidget.Highlighter = Highlighter
CodeWidget.PythonHighlighter = PythonHighlighter

class _QtHighlighter( QtGui.QSyntaxHighlighter ) :

	def __init__( self, document ) :

		QtGui.QSyntaxHighlighter.__init__( self, document )

		self.__highlighter = None

		def format( color ) :

			f = QtGui.QTextCharFormat()
			f.setForeground( color )
			return f

		self.__formats = {
			Highlighter.Type.String : format( QtGui.QColor( 216, 146, 115 ) ),
			Highlighter.Type.Number : format( QtGui.QColor( 174, 208, 164 ) ),
			Highlighter.Type.Comment : format( QtGui.QColor( 90, 156, 76 ) ),
			Highlighter.Type.Keyword : format( QtGui.QColor( 64, 156, 219 ) ),
			Highlighter.Type.ControlFlow : format( QtGui.QColor( 207, 128, 195 ) ),
			Highlighter.Type.Braces : format( QtGui.QColor( 255, 255, 0 ) ),
			Highlighter.Type.Operator : format( QtGui.QColor( 201, 0, 28 ) ),
			Highlighter.Type.Call : format( QtGui.QColor( 219, 221, 164 ) ),
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
