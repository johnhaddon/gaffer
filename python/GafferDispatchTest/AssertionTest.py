##########################################################################
#
#  Copyright (c) John Haddon, 2014. All rights reserved.
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

import imath

import Gaffer
import GafferTest
import GafferDispatch

class AssertionTest( GafferTest.TestCase ) :

	def testSetup( self ) :

		assertion = GafferDispatch.Assertion()
		self.assertNotIn( "a", assertion )
		self.assertNotIn( "b", assertion )
		self.assertNotIn( "delta", assertion )

		assertion.setup( Gaffer.IntPlug() )

		self.assertIn( "a", assertion )
		self.assertIn( "b", assertion )
		self.assertIn( "delta", assertion )

		self.assertIsInstance( assertion["a"], Gaffer.IntPlug )
		self.assertIsInstance( assertion["b"], Gaffer.IntPlug )
		self.assertIsInstance( assertion["delta"], Gaffer.IntPlug )

		self.assertRaises( AssertionError, assertion.setup, Gaffer.IntPlug )

	def testNoDeltaForNonNumericPlugs( self ) :

		for plugType in [
			Gaffer.StringPlug,
			Gaffer.BoolPlug,
		] :
			with self.subTest( plugType = plugType ) :
				assertion = GafferDispatch.Assertion()
				assertion.setup( plugType() )
				self.assertNotIn( "delta", assertion )
				self.assertIn( "a", assertion )

	def testDeltaPlugType( self ) :

		for plugType, deltaPlugType in [
			( Gaffer.IntPlug, Gaffer.IntPlug ),
			( Gaffer.FloatPlug, Gaffer.FloatPlug ),
			( Gaffer.V2iPlug, Gaffer.IntPlug ),
			( Gaffer.V3fPlug, Gaffer.FloatPlug ),
			( Gaffer.Box2iPlug, Gaffer.IntPlug ),
		] :
			with self.subTest( plugType = plugType ) :
				assertion = GafferDispatch.Assertion()
				assertion.setup( plugType() )
				self.assertIn( "delta", assertion )
				self.assertIsInstance( assertion["delta"], deltaPlugType )

	def testSerialisation( self ) :

		script = Gaffer.ScriptNode()
		script["assertion"] = GafferDispatch.Assertion()

		serialisation = script.serialise()
		self.assertNotIn( "setup(", serialisation )

		script2 = Gaffer.ScriptNode()
		script2.execute( serialisation )
		self.assertEqual( script2["assertion"].keys(), script["assertion"].keys() )

		script["assertion"].setup( Gaffer.StringPlug() )

		serialisation = script.serialise()
		self.assertIn( "setup(", serialisation )

		script2 = Gaffer.ScriptNode()
		script2.execute( serialisation )
		self.assertEqual( script2["assertion"].keys(), script["assertion"].keys() )

	def testDelta( self ) :

		assertion = GafferDispatch.Assertion()
		assertion.setup( Gaffer.IntPlug() )

		assertion["a"].setValue( 0 )
		assertion["b"].setValue( 1 )

		with self.assertRaisesRegex( Gaffer.ProcessException, "AssertionError: 0 != 1" ) :
			assertion["task"].execute()

		assertion["delta"].setValue( 1 )
		assertion["task"].execute() # Shouldn't throw
		assertion["delta"].setValue( 2 )
		assertion["task"].execute() # Shouldn't throw

		assertion["b"].setValue( 3 )
		with self.assertRaisesRegex( Gaffer.ProcessException, "AssertionError: 0 != 3 within 2 delta" ) :
			assertion["task"].execute()

	def testVectorDelta( self ) :

		assertion = GafferDispatch.Assertion()
		assertion.setup( Gaffer.V3iPlug() )

		assertion["a"].setValue( imath.V3i( 1, 2, 3 ) )
		assertion["b"].setValue( imath.V3i( 2, 2, 3 ) )

		with self.assertRaisesRegex( Gaffer.ProcessException, "AssertionError: V3i(1, 2, 3) != V3i(2, 2, 3)" ) :
			assertion["task"].execute()

if __name__ == "__main__":
	unittest.main()
