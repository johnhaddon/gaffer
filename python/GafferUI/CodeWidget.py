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

		self.keyPressSignal().connect( Gaffer.WeakMethod( self.__keyPress ), scoped = False )

	def setCompleter( self, completer ) :

		self.__completer = completer

	def getCompleter( self ) :

		return self.__completer

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

	# HIDE IMPLEMENTATION
	class PythonHighlighter( QtGui.QSyntaxHighlighter ) :

		#@staticmethod
		def __format( color ) :

			f = QtGui.QTextCharFormat()
			f.setForeground( GafferUI.Widget._qtColor( color ) )
			return f

		__keywordFormat = __format( imath.Color3f( 64 / 255., 156/255., 219/255. ) )
		__stringFormat = __format( imath.Color3f( 216 / 255., 146/255., 115/255. ) )
		__commentFormat = __format( imath.Color3f( 90 / 255., 156/255., 76/255. ) )
		__controlFlowFormat = __format( imath.Color3f( 207 / 255., 128 / 255., 195 / 255. ) )
		__numberFormat = __format( imath.Color3f( 174 / 255., 208 / 255., 164 / 255. ) )
		__callFormat = __format( imath.Color3f( 219 / 255., 221 / 255., 164 / 255. ) )
		__operatorFormat = __format( imath.Color3f( 201 / 255., 0, 28/255. ) )

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

		def highlightBlock( self, text ) :

			f = QtGui.QTextCharFormat()
			f.setForeground( QtGui.QColor( 255, 0, 0 ) )

			g = QtGui.QTextCharFormat()
			g.setForeground( QtGui.QColor( 0, 255, 0 ) )

			print text
			pendingName = None
			with IECore.IgnoredExceptions( tokenize.TokenError ) :
				for t in tokenize.generate_tokens( io.StringIO( text ).readline ) :
					f = None
					if t[0] == token.NAME :
						if t[1] in self.__controlFlowKeywords :
							f = self.__controlFlowFormat
						elif keyword.iskeyword( t[1] ) or t[1] == "self" :
							f = self.__keywordFormat
						else :
							pendingName = t
							continue
					elif t[0] == token.STRING :
						f = self.__stringFormat
					elif t[0] == tokenize.COMMENT :
						f = self.__commentFormat
					elif t[0] == token.NUMBER :
						f = self.__numberFormat
					elif t[0] == token.OP :
						if t[1] in self.__parenthesis :
							f = self.__callFormat
							if t[1] == "(" :
								if pendingName is not None :
									self.setFormat( pendingName[2][1], pendingName[3][1] - pendingName[2][1], self.__callFormat )
						elif t[1] in self.__operators :
							f = self.__operatorFormat

					if f is not None  :
						self.setFormat( t[2][1], t[3][1] - t[2][1], f )

					pendingName = None

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


