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

		add1 = GafferTest.AddNode( "add1" )
		add2 = GafferTest.AddNode( "add2" )
		add3 = GafferTest.AddNode( "add3" )
		add4 = GafferTest.AddNode( "add4" )
		unconnected = GafferTest.AddNode( "unconnected" )

		add3["op1"].setInput( add1["sum"] )
		add3["op2"].setInput( add2["sum"] )
		add4["op1"].setInput( add3["sum"] )

		context = Gaffer.Context()
		context["id"] = 10 # Distinguish from default context
		contexts = GafferUI.UpstreamContexts( add4, context )

		def assertExpectedContexts() :

			# Inactive nodes and plugs fall back to using the target
			# context, so everything has the same context.

			for node in [ add1, add2, add3, add4, unconnected ] :
				self.assertEqual( contexts.context( node ), context )
				for plug in Gaffer.Plug.RecursiveRange( node ) :
					self.assertEqual( contexts.context( plug ), context )

		assertExpectedContexts( )

		for node in [ add1, add2, add3, add4 ] :
			for graphComponent in [ node, node["op1"], node["op2"], node["sum"], node["enabled"] ] :
				self.assertTrue( contexts.isActive( graphComponent ), graphComponent.fullName() )

		for graphComponent in [ unconnected, unconnected["op1"], unconnected["op2"], unconnected["sum"], unconnected["enabled"] ] :
			self.assertFalse( contexts.isActive( graphComponent ) )

		add3["enabled"].setValue( False )

		assertExpectedContexts( )

		self.assertTrue( contexts.isActive( add4 ) )
		self.assertTrue( contexts.isActive( add3 ) )
		self.assertTrue( contexts.isActive( add3["op1"] ) )
		self.assertFalse( contexts.isActive( add3["op2"] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )

	def testSwitch( self ) :

		# Static case - switch will have internal pass-through connections.

		add1 = GafferTest.AddNode( "add1" )
		add2 = GafferTest.AddNode( "add2" )

		switch = Gaffer.Switch()
		switch.setup( add1["sum"] )
		switch["in"][0].setInput( add1["sum"] )
		switch["in"][1].setInput( add2["sum"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( switch, context )

		def assertExpectedContexts() :

			# Inactive nodes and plugs fall back to using the target
			# context, so everything has the same context.

			for node in [ add1, add2, switch ] :
				self.assertEqual( contexts.context( node ), context, node.fullName() )
				for plug in Gaffer.Plug.RecursiveRange( node ) :
					self.assertEqual( contexts.context( plug ), context, plug.fullName() )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["index"] ) )
		self.assertTrue( contexts.isActive( switch["enabled"] ) )
		self.assertTrue( contexts.isActive( switch["in"][0] ) )
		self.assertFalse( contexts.isActive( switch["in"][1] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )

		switch["index"].setValue( 1 )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["index"] ) )
		self.assertTrue( contexts.isActive( switch["enabled"] ) )
		self.assertFalse( contexts.isActive( switch["in"][0] ) )
		self.assertTrue( contexts.isActive( switch["in"][1] ) )
		self.assertFalse( contexts.isActive( add1 ) )
		self.assertTrue( contexts.isActive( add2 ) )

		switch["enabled"].setValue( False )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertFalse( contexts.isActive( switch["index"] ) )
		self.assertTrue( contexts.isActive( switch["enabled"] ) )
		self.assertTrue( contexts.isActive( switch["in"][0] ) )
		self.assertFalse( contexts.isActive( switch["in"][1] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )

		# Dynamic case - switch will compute input on the fly.

		add3 = GafferTest.AddNode()
		switch["index"].setInput( add3["sum"] )
		switch["enabled"].setValue( True )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["index"] ) )
		self.assertTrue( contexts.isActive( switch["enabled"] ) )
		self.assertTrue( contexts.isActive( switch["in"][0] ) )
		self.assertFalse( contexts.isActive( switch["in"][1] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )

		add3["op1"].setValue( 1 )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["index"] ) )
		self.assertTrue( contexts.isActive( switch["enabled"] ) )
		self.assertFalse( contexts.isActive( switch["in"][0] ) )
		self.assertTrue( contexts.isActive( switch["in"][1] ) )
		self.assertFalse( contexts.isActive( add1 ) )
		self.assertTrue( contexts.isActive( add2 ) )

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

		def assertExpectedContexts() :

			# Inactive nodes and plugs fall back to using the target
			# context, so everything has the same context.

			for node in [ add1, add2, switch ] :
				self.assertEqual( contexts.context( node ), context )
				for plug in Gaffer.Plug.RecursiveRange( node ) :
					self.assertEqual( contexts.context( plug ), context )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["selector"] ) )
		self.assertTrue( contexts.isActive( switch["in"][0] ) )
		self.assertFalse( contexts.isActive( switch["in"][1] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )

		switch["selector"].setValue( "add2" )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["selector"] ) )
		self.assertFalse( contexts.isActive( switch["in"][0] ) )
		self.assertTrue( contexts.isActive( switch["in"][1] ) )
		self.assertFalse( contexts.isActive( add1 ) )
		self.assertTrue( contexts.isActive( add2 ) )

		switch["enabled"].setValue( False )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["selector"] ) )
		self.assertTrue( contexts.isActive( switch["in"][0] ) )
		self.assertFalse( contexts.isActive( switch["in"][1] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )

		# Dynamic case - switch will compute input on the fly.

		stringNode = GafferTest.StringInOutNode()
		switch["selector"].setInput( stringNode["out"] )
		switch["enabled"].setValue( True )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["selector"] ) )
		self.assertTrue( contexts.isActive( switch["in"][0] ) )
		self.assertFalse( contexts.isActive( switch["in"][1] ) )
		self.assertTrue( contexts.isActive( add1 ) )
		self.assertFalse( contexts.isActive( add2 ) )
		self.assertTrue( contexts.isActive( stringNode ) )

		stringNode["in"].setValue( "add2" )

		assertExpectedContexts()

		self.assertTrue( contexts.isActive( switch ) )
		self.assertTrue( contexts.isActive( switch["out"] ) )
		self.assertTrue( contexts.isActive( switch["selector"] ) )
		self.assertFalse( contexts.isActive( switch["in"][0] ) )
		self.assertTrue( contexts.isActive( switch["in"][1] ) )
		self.assertFalse( contexts.isActive( add1 ) )
		self.assertTrue( contexts.isActive( add2 ) )
		self.assertTrue( contexts.isActive( stringNode ) )

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
		self.assertEqual( contexts.context( switch["in"][0] ), context )
		self.assertEqual( contexts.context( switch["in"][1] ), contextVariables.inPlugContext() )
		self.assertEqual( contexts.context( add1 ), context )
		self.assertEqual( contexts.context( add2 ), contextVariables.inPlugContext() )

	def testContextProcessors( self ) :

		add = GafferTest.AddNode()

		contextVariables = Gaffer.ContextVariables()
		contextVariables.setup( add["sum"] )
		contextVariables["in"].setInput( add["sum"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( contextVariables, context )

		self.assertTrue( contexts.isActive( contextVariables ) )
		self.assertTrue( contexts.isActive( contextVariables["enabled"] ) )
		self.assertTrue( contexts.isActive( contextVariables["variables"] ) )
		self.assertEqual( contexts.context( contextVariables ), context )
		self.assertTrue( contexts.isActive( add ) )
		self.assertEqual( contexts.context( add ), context )

		contextVariables["variables"].addChild( Gaffer.NameValuePlug( "test", 2 ) )

		self.assertTrue( contexts.isActive( contextVariables ) )
		self.assertTrue( contexts.isActive( contextVariables["enabled"] ) )
		self.assertTrue( contexts.isActive( contextVariables["variables"] ) )
		self.assertTrue( contexts.isActive( contextVariables["variables"][0] ) )
		self.assertTrue( contexts.isActive( contextVariables["variables"][0]["name"] ) )
		self.assertTrue( contexts.isActive( contextVariables["variables"][0]["value"] ) )
		self.assertEqual( contexts.context( contextVariables ), context )
		self.assertTrue( contexts.isActive( add ) )
		self.assertEqual( contexts.context( add ), contextVariables.inPlugContext() )

		contextVariables["enabled"].setValue( False )

		self.assertTrue( contexts.isActive( contextVariables ) )
		self.assertTrue( contexts.isActive( contextVariables["enabled"] ) )
		self.assertFalse( contexts.isActive( contextVariables["variables"] ) )
		self.assertFalse( contexts.isActive( contextVariables["variables"][0] ) )
		self.assertFalse( contexts.isActive( contextVariables["variables"][0]["name"] ) )
		self.assertFalse( contexts.isActive( contextVariables["variables"][0]["value"] ) )
		self.assertEqual( contexts.context( contextVariables ), context )
		self.assertTrue( contexts.isActive( add ) )
		self.assertEqual( contexts.context( add ), context )

	def testPlugWithoutNode( self ) :

		plug = Gaffer.IntPlug()
		node = GafferTest.AddNode()
		node["op1"].setInput( plug )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( node, context )

		self.assertEqual( contexts.context( node ), context )
		self.assertEqual( contexts.context( node["op1"] ), context )
		self.assertEqual( contexts.context( plug ), context )

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

		self.assertTrue( contexts.isActive( loop ) )
		self.assertEqual( contexts.context( loop ), context )
		self.assertTrue( contexts.isActive( loop["iterations"] ) )
		self.assertEqual( contexts.context( loop["iterations"] ), context )
		self.assertTrue( contexts.isActive( loopSource ) )
		self.assertEqual( contexts.context( loopSource ), context )
		self.assertTrue( contexts.isActive( loop["next"] ) )
		self.assertEqual( contexts.context( loop["next"] ), loop.nextIterationContext() )
		self.assertTrue( contexts.isActive( loopBody ) )
		self.assertEqual( contexts.context( loopBody ), loop.nextIterationContext() )

		def assertDisabledLoop() :

			self.assertTrue( contexts.isActive( loop ) )
			self.assertEqual( contexts.context( loop ), context )
			self.assertFalse( contexts.isActive( loop["iterations"] ) )
			self.assertEqual( contexts.context( loop["iterations"] ), context )
			self.assertTrue( contexts.isActive( loopSource ) )
			self.assertEqual( contexts.context( loopSource ), context )
			self.assertFalse( contexts.isActive( loop["next"] ) )
			self.assertEqual( contexts.context( loop["next"] ), context )
			self.assertFalse( contexts.isActive( loopBody ) )
			self.assertEqual( contexts.context( loopBody ), context )

		loop["enabled"].setValue( False )
		assertDisabledLoop()

		loop["enabled"].setValue( True )
		loop["iterations"].setValue( 0 )
		assertDisabledLoop()

	def testMultiplexedBox( self ) :

		addA = GafferTest.AddNode()
		addB = GafferTest.AddNode()

		box = Gaffer.Box()
		box["addA"] = GafferTest.AddNode()
		box["addB"] = GafferTest.AddNode()

		Gaffer.PlugAlgo.promoteWithName( box["addA"]["op1"], "opA" )
		Gaffer.PlugAlgo.promoteWithName( box["addB"]["op1"], "opB" )
		box["opA"].setInput( addA["sum"] )
		box["opB"].setInput( addB["sum"] )

		Gaffer.PlugAlgo.promoteWithName( box["addA"]["sum"], "sumA" )
		Gaffer.PlugAlgo.promoteWithName( box["addB"]["sum"], "sumB" )
		box["sumA"].setInput( box["addA"]["sum"] )
		box["sumB"].setInput( box["addB"]["sum"] )

		resultA = GafferTest.AddNode()
		resultA["op1"].setInput( box["sumA"] )

		resultB = GafferTest.AddNode()
		resultB["op1"].setInput( box["sumB"] )

		context = Gaffer.Context()
		contexts = GafferUI.UpstreamContexts( resultA, context )

		self.assertTrue( contexts.isActive( resultA ) )
		self.assertFalse( contexts.isActive( resultB ) )
		self.assertTrue( contexts.isActive( box["sumA"] ) )
		self.assertFalse( contexts.isActive( box["sumB"] ) )
		self.assertTrue( contexts.isActive( box ) )
		self.assertTrue( contexts.isActive( box["addA"] ) )
		self.assertFalse( contexts.isActive( box["addB"] ) )
		self.assertTrue( contexts.isActive( box["opA"] ) )
		self.assertFalse( contexts.isActive( box["opB"] ) )
		self.assertTrue( contexts.isActive( addA ) )
		self.assertFalse( contexts.isActive( addB ) )

	def testAcquire( self ) :

		add1 = GafferTest.AddNode()
		add2 = GafferTest.AddNode()

		contexts1 = GafferUI.UpstreamContexts.acquire( add1 )
		self.assertTrue( contexts1.isSame( GafferUI.UpstreamContexts.acquire( add1 ) ) )
		self.assertTrue( contexts1.isActive( add1 ) )
		self.assertFalse( contexts1.isActive( add2 ) )

		contexts2 = GafferUI.UpstreamContexts.acquire( add2 )
		self.assertTrue( contexts2.isSame( GafferUI.UpstreamContexts.acquire( add2 ) ) )
		self.assertTrue( contexts2.isActive( add2 ) )
		self.assertFalse( contexts2.isActive( add1 ) )

	def testAcquireLifetime( self ) :

		node = GafferTest.MultiplyNode()
		nodeSlots = node.plugDirtiedSignal().numSlots()
		nodeRefCount = node.refCount()

		contexts = GafferUI.UpstreamContexts.acquire( node )
		del contexts

		# Indicates that `contexts` was truly destroyed.
		self.assertEqual( node.plugDirtiedSignal().numSlots(), nodeSlots )
		self.assertEqual( node.refCount(), nodeRefCount )

		# Should be a whole new instance.
		contexts = GafferUI.UpstreamContexts.acquire( node )
		self.assertTrue( contexts.isActive( node ) )

	def testAcquireForFocus( self ) :

		script = Gaffer.ScriptNode()

		script["add1"] = GafferTest.AddNode()
		script["add2"] = GafferTest.AddNode()

		contexts = GafferUI.UpstreamContexts.acquireForFocus( script )
		self.assertTrue( contexts.isSame( GafferUI.UpstreamContexts.acquireForFocus( script ) ) )

		self.assertFalse( contexts.isActive( script["add1" ] ) )
		self.assertFalse( contexts.isActive( script["add2" ] ) )

		script.setFocus( script["add1"] )
		self.assertTrue( contexts.isActive( script["add1" ] ) )
		self.assertFalse( contexts.isActive( script["add2" ] ) )

		script.setFocus( script["add2"] )
		self.assertFalse( contexts.isActive( script["add1" ] ) )
		self.assertTrue( contexts.isActive( script["add2" ] ) )

		script.setFocus( None )
		self.assertFalse( contexts.isActive( script["add1" ] ) )
		self.assertFalse( contexts.isActive( script["add2" ] ) )

	def testAcquireForFocusLifetime( self ) :

		script = Gaffer.ScriptNode()
		contextSlots = script.context().changedSignal().numSlots()
		contextRefCount = script.context().refCount()

		contexts = GafferUI.UpstreamContexts.acquireForFocus( script )
		del contexts

		# Indicates that `contexts` was truly destroyed.
		self.assertEqual( script.context().changedSignal().numSlots(), contextSlots )
		self.assertEqual( script.context().refCount(), contextRefCount )

		# Should be a whole new instance.
		script["node"] = GafferTest.MultiplyNode()
		script.setFocus( script["node"] )
		contexts = GafferUI.UpstreamContexts.acquireForFocus( script )
		self.assertTrue( contexts.isActive( script["node"] ) )

if __name__ == "__main__":
	unittest.main()
