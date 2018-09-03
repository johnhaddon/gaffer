##########################################################################
#
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

import unittest

import IECore

import Gaffer
import GafferTest

class GraphComponentPathTest( GafferTest.TestCase ) :

	def testRelative( self ) :

		r = Gaffer.GraphComponent()
		r["d"] = Gaffer.GraphComponent()
		r["d"]["e"] = Gaffer.GraphComponent()

		p = Gaffer.GraphComponentPath( r, "d" )
		self.assertEqual( str( p ), "d" )
		self.assertEqual( p.root(), "" )
		self.assertEqual( [ str( c ) for c in p.children() ], [ "d/e" ] )

		p2 = p.copy()
		self.assertEqual( str( p2 ), "d" )
		self.assertEqual( p2.root(), "" )
		self.assertEqual( [ str( c ) for c in p2.children() ], [ "d/e" ] )

	def testProperties( self ) :

		r = Gaffer.GraphComponent()
		r["d"] = Gaffer.GraphComponent()

		p = Gaffer.GraphComponentPath( r, "/d" )
		self.assertEqual( p.propertyNames(), [ "name", "fullName", "graphComponent:graphComponent" ] )
		self.assertEqual( p.property( "name" ), "d" )
		self.assertEqual( p.property( "fullName" ), "/d" )
		self.assertEqual( p.property( "graphComponent:graphComponent" ), r["d"] )

		p = Gaffer.GraphComponentPath( r, "/" )
		self.assertEqual( p.propertyNames(), [ "name", "fullName", "graphComponent:graphComponent" ] )
		self.assertEqual( p.property( "name" ), "" )
		self.assertEqual( p.property( "fullName" ), "/" )
		self.assertEqual( p.property( "graphComponent:graphComponent" ), r )

		p[:] = [ "non", "existent" ]
		self.assertEqual( p.property( "graphComponent:graphComponent" ), None )

	def testChildrenInheritFilter( self ) :

		r = Gaffer.GraphComponent()
		r["d"] = Gaffer.GraphComponent()
		r["d"]["e"] = Gaffer.GraphComponent()

		p = Gaffer.GraphComponentPath( r, "/d", filter = Gaffer.PathFilter() )
		child = p.children()[0]

		self.assertTrue( isinstance( child.getFilter(), Gaffer.PathFilter ) )

if __name__ == "__main__":
	unittest.main()
