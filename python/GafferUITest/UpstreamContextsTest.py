##########################################################################
#
#  Copyright (c) 2024, Cinesite VFX Ltd. All rights reserved.
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
import GafferUI
import GafferUITest

class UpstreamContextsTest( GafferUITest.TestCase ) :

	def testSimpleNodes( self ) :

		add1 = GafferTest.AddNode()
		add2 = GafferTest.AddNode()
		add3 = GafferTest.AddNode()
		add4 = GafferTest.AddNode()
		unconnected = GafferTest.AddNode()

		add3["op1"].setInput( add1["sum"] )
		add3["op2"].setInput( add2["sum"] )
		add4["op1"].setInput( add3["sum"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( add4, context )

		for node in [ add1, add2, add3, add4 ] :
			self.assertEqual( contexts.context( node ), context )
			self.assertEqual( contexts.context( node["op1"] ), context )
			self.assertEqual( contexts.context( node["op2"] ), context )
			self.assertEqual( contexts.context( node["sum"] ), context )

		self.assertIsNone( contexts.context( unconnected ) )
		self.assertIsNone( contexts.context( unconnected["op1"] ) )
		self.assertIsNone( contexts.context( unconnected["op2"] ) )
		self.assertIsNone( contexts.context( unconnected["sum"] ) )

	def testSwitch( self ) :

		# Static case - switch will have internal pass-through connections.

		add1 = GafferTest.AddNode()
		add2 = GafferTest.AddNode()

		switch = Gaffer.Switch()
		switch.setup( add1["sum"] )
		switch["in"][0].setInput( add1["sum"] )
		switch["in"][1].setInput( add2["sum"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( switch, context )

		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( switch["out"] ), context )
		self.assertEqual( contexts.context( switch["in"][0] ), context )
		#self.assertIsNone( contexts.context( switch["in"][1] ), context ) TODO
		self.assertEqual( contexts.context( add1 ), context )
		self.assertIsNone( contexts.context( add2 ) )

		# TODO : ASSERT THAT PER-PLUG CONTEXT IS NULL FOR UNUSED INPUT ^ AND V

		switch["index"].setValue( 1 )

		self.assertEqual( contexts.context( switch ), context )
		self.assertIsNone( contexts.context( add1 ) )
		self.assertEqual( contexts.context( add2 ), context )

		switch["enabled"].setValue( False )

		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertIsNone( contexts.context( add2 ) )

		# Dynamic case - switch will compute input on the fly.

		add3 = GafferTest.AddNode()
		switch["index"].setInput( add3["sum"] )
		switch["enabled"].setValue( True )

		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertIsNone( contexts.context( add2 ) )

		add3["op1"].setValue( 1 )

		self.assertEqual( contexts.context( switch ), context )
		self.assertIsNone( contexts.context( add1 ) )
		self.assertEqual( contexts.context( add2 ), context )

		## TODO : MAKE SURE WE CAN HAVE BOTH INPUTS BE ACTIVE AT ONCE
		# IF THE SWITCH IS EVALUATED IN TWO CONTEXTS.

	def testNameSwitch( self ) :

		# Static case - switch will have internal pass-through connections.

		add1 = GafferTest.AddNode()
		add2 = GafferTest.AddNode()

		switch = Gaffer.NameSwitch()
		switch.setup( add1["sum"] )
		switch["in"][0]["value"].setInput( add1["sum"] )
		switch["in"][1]["value"].setInput( add2["sum"] )
		switch["in"][1]["name"].setValue( "add2" )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( switch, context )

		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertIsNone( contexts.context( add2 ) )

		# TODO : ASSERT THAT PER-PLUG CONTEXT IS NULL FOR UNUSED INPUT

		switch["selector"].setValue( "add2" )

		self.assertEqual( contexts.context( switch ), context )
		self.assertIsNone( contexts.context( add1 ) )
		self.assertEqual( contexts.context( add2 ), context )

		switch["enabled"].setValue( False )

		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertIsNone( contexts.context( add2 ) )

		# Dynamic case - switch will compute input on the fly.

		stringNode = GafferTest.StringInOutNode()
		switch["selector"].setInput( stringNode["out"] )
		switch["enabled"].setValue( True )

		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertIsNone( contexts.context( add2 ) )

		stringNode["in"].setValue( "add2" )

		self.assertEqual( contexts.context( switch ), context )
		self.assertIsNone( contexts.context( add1 ) )
		self.assertEqual( contexts.context( add2 ), context )

	def testMultipleActiveNameSwitchBranches( self ) :

		add1 = GafferTest.AddNode( "add1" )
		add2 = GafferTest.AddNode( "add2" )

		switch = Gaffer.NameSwitch()
		switch.setup( add1["sum"] )
		switch["in"][0]["value"].setInput( add1["sum"] )
		switch["in"][1]["value"].setInput( add2["sum"] )
		switch["in"][1]["name"].setValue( "add2" )
		switch["selector"].setValue( "${selector}" )

		contextVariables = Gaffer.ContextVariables()
		contextVariables.setup( switch["out"]["value"] )
		contextVariables["in"].setInput( switch["out"]["value"] )
		contextVariables["variables"].addChild( Gaffer.NameValuePlug( "selector", "add2" ) )

		add3 = GafferTest.AddNode( "add3" )
		add3["op1"].setInput( switch["out"]["value"] )
		add3["op2"].setInput( contextVariables["out"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( add3, context )

		self.assertEqual( contexts.context( add3 ), context )
		self.assertEqual( contexts.context( switch ), context )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertEqual( contexts.context( add2 ), contextVariables.inPlugContext() )

	def testContextProcessors( self ) :

		add = GafferTest.AddNode()

		contextVariables = Gaffer.ContextVariables()
		contextVariables.setup( add["sum"] )
		contextVariables["in"].setInput( add["sum"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( contextVariables, context )

		self.assertEqual( contexts.context( contextVariables ), context )
		self.assertEqual( contexts.context( add ), context )

		contextVariables["variables"].addChild( Gaffer.NameValuePlug( "test", 2 ) )

		self.assertEqual( contexts.context( contextVariables ), context )
		self.assertEqual( contexts.context( add ), contextVariables.inPlugContext() )

		contextVariables["enabled"].setValue( False )

		self.assertEqual( contexts.context( contextVariables ), context )
		self.assertEqual( contexts.context( add ), context )

	def testPlugWithoutNode( self ) :

		plug = Gaffer.IntPlug()
		node = GafferTest.AddNode()
		node["op1"].setInput( plug )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( node, context )

		self.assertEqual( contexts.context( node ), context )

		# TODO : ASSERT WE HAVE A CONTEXT FOR THE PLUG? OR EXPLAIN WHY WE DON'T

	def testLoop( self ) :

		loopSource = GafferTest.AddNode()
		loopBody = GafferTest.AddNode()

		loop = Gaffer.Loop()
		loop.setup( loopSource["sum"] )
		loop["in"].setInput( loopSource["sum"] )

		loopBody["op1"].setInput( loop["previous"] )
		loopBody["op2"].setValue( 2 )
		loop["next"].setInput( loopBody["sum"] )

		loop["iterations"].setValue( 10 )
		self.assertEqual( loop["out"].getValue(), 20 )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( loop, context )

		self.assertEqual( contexts.context( loop ), context )
		self.assertEqual( contexts.context( loopSource ), context )

		self.assertEqual( contexts.context( loopBody ), loop.nextIterationContext() )


	## TODO :
	#
	# - test connection to plug without node

if __name__ == "__main__":
	unittest.main()
