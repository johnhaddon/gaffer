##########################################################################
#
#  Copyright (c) 2019, Image Engine Design Inc. All rights reserved.
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

import unittest

import Gaffer
import GafferTest

class GraphComponentAlgoTest( GafferTest.TestCase ) :

	def testCommonAncestor( self ) :

		a = Gaffer.ApplicationRoot()
		s = Gaffer.ScriptNode()
		a["scripts"]["one"] = s

		s["n1"] = Gaffer.Node()
		s["n2"] = Gaffer.Node()

		self.assertEqual( Gaffer.GraphComponentAlgo.commonAncestor( s["n1"], s["n2"], Gaffer.ScriptNode ), s )
		self.assertEqual( Gaffer.GraphComponentAlgo.commonAncestor( s["n2"], s["n1"], Gaffer.ScriptNode ), s )

	def testCommonAncestorType( self ) :

		s = Gaffer.ScriptNode()

		s["n"] = Gaffer.Node()
		s["n"]["user"]["p1"] = Gaffer.IntPlug()
		s["n"]["user"]["p2"] = Gaffer.Color3fPlug()

		self.assertEqual( Gaffer.GraphComponentAlgo.commonAncestor( s["n"]["user"]["p1"], s["n"]["user"]["p2"]["r"] ), s["n"]["user"] )
		self.assertEqual( Gaffer.GraphComponentAlgo.commonAncestor( s["n"]["user"]["p1"], s["n"]["user"]["p2"]["r"], Gaffer.Plug ), s["n"]["user"] )
		self.assertEqual( Gaffer.GraphComponentAlgo.commonAncestor( s["n"]["user"]["p1"], s["n"]["user"]["p2"]["r"], Gaffer.Node ), s["n"] )

if __name__ == "__main__":
	unittest.main()
