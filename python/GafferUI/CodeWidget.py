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

import IECore

import Gaffer
import GafferUI

class CodeWidget( GafferUI.MultiLineTextWidget ) :

	def __init__( self, text="", editable=True, fixedLineHeight=None, **kw ) :

		GafferUI.MultiLineTextWidget.__init__( self, text, editable, fixedLineHeight = fixedLineHeight, wrapMode = self.WrapMode.None, role = self.Role.Code, **kw )

		self.__completer = None

		self.keyPressSignal().connect( Gaffer.WeakMethod( self.__keyPress ), scoped = False )

	def setCompleter( self, completer ) :

		self.__completer = completer

	def getCompleter( self ) :

		return self.__completer

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

		return True

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

