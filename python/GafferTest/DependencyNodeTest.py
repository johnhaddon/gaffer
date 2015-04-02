##########################################################################
#
#  Copyright (c) 2011-2012, John Haddon. All rights reserved.
#  Copyright (c) 2011-2013, Image Engine Design Inc. All rights reserved.
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
import threading
import collections

import IECore

import Gaffer
import GafferTest

class DependencyNodeTest( GafferTest.TestCase ) :

	def testDirtyOnDisconnect( self ) :

		n1 = GafferTest.AddNode( "n1" )
		n2 = GafferTest.AddNode( "n2" )

		n1["op1"].setValue( 2 )
		n1["op2"].setValue( 3 )

		dirtied = GafferTest.CapturingSlot( n2.plugDirtiedSignal() )
		set = GafferTest.CapturingSlot( n2.plugSetSignal() )
		n2["op1"].setInput( n1["sum"] )

		self.assertEqual( len( set ), 0 )
		self.assertEqual( len( dirtied ), 2 )
		self.failUnless( dirtied[0][0].isSame( n2["op1"] ) )
		self.failUnless( dirtied[1][0].isSame( n2["sum"] ) )

		n2["op1"].setInput( None )

		self.assertEqual( len( set ), 1 )
		self.failUnless( set[0][0].isSame( n2["op1"] ) )
		self.assertEqual( len( dirtied ), 4 )
		self.failUnless( dirtied[2][0].isSame( n2["op1"] ) )
		self.failUnless( dirtied[3][0].isSame( n2["sum"] ) )

	def testDirtyPropagationForCompoundPlugs( self ) :

		class CompoundOut( Gaffer.DependencyNode ) :

			def __init__( self, name="CompoundOut" ) :

				Gaffer.DependencyNode.__init__( self, name )

				self["in"] = Gaffer.IntPlug()
				self["out"] = Gaffer.CompoundPlug( direction = Gaffer.Plug.Direction.Out )
				self["out"]["one"] = Gaffer.IntPlug( direction = Gaffer.Plug.Direction.Out )
				self["out"]["two"] = Gaffer.IntPlug( direction = Gaffer.Plug.Direction.Out )

				self["behaveBadly"] = Gaffer.BoolPlug( defaultValue = False )

			def affects( self, input ) :

				outputs = Gaffer.DependencyNode.affects( self, input )

				if input.isSame( self["in"] ) :
					if self["behaveBadly"].getValue() :
						# we're not allowed to return a CompoundPlug in affects() - we're
						# just doing it here to make sure we can see that the error is detected.
						outputs.append( self["out"] )
					else :
						# to behave well we must list all leaf level children explicitly.
						outputs.extend( self["out"].children() )

				return outputs

		class CompoundIn( Gaffer.DependencyNode ) :

			def __init__( self, name="CompoundIn" ) :

				Gaffer.DependencyNode.__init__( self, name )

				self["in"] = Gaffer.CompoundPlug()
				self["in"]["one"] = Gaffer.IntPlug()
				self["in"]["two"] = Gaffer.IntPlug()

				self["out"] = Gaffer.IntPlug( direction = Gaffer.Plug.Direction.Out )

			def affects( self, input ) :

				# affects should never be called with a CompoundPlug - only
				# leaf level plugs.
				assert( not input.isSame( self["in"] ) )

				if self["in"].isAncestorOf( input ) :
					return [ self["out"] ]

				return []

		src = CompoundOut()
		dst = CompoundIn()

		dst["in"].setInput( src["out"] )

		srcDirtied = GafferTest.CapturingSlot( src.plugDirtiedSignal() )
		dstDirtied = GafferTest.CapturingSlot( dst.plugDirtiedSignal() )

		src["behaveBadly"].setValue( True )
		self.assertEqual( len( srcDirtied ), 1 )
		self.assertTrue( srcDirtied[0][0].isSame( src["behaveBadly"] ) )
		with IECore.CapturingMessageHandler() as mh :
			src["in"].setValue( 10 )

		self.assertEqual( src["in"].getValue(), 10 )
		self.assertEqual( len( mh.messages ), 1 )
		self.assertEqual( mh.messages[0].level, IECore.Msg.Level.Error )
		self.assertEqual( mh.messages[0].context, "CompoundOut::affects()" )
		self.assertEqual( mh.messages[0].message, "Non-leaf plug out returned by affects()" )

		src["behaveBadly"].setValue( False )
		del srcDirtied[:]

		src["in"].setValue( 20 )

		srcDirtiedNames = set( [ x[0].fullName() for x in srcDirtied ] )

		self.assertEqual( len( srcDirtiedNames ), 4 )

		self.assertTrue( "CompoundOut.in" in srcDirtiedNames )
		self.assertTrue( "CompoundOut.out.one" in srcDirtiedNames )
		self.assertTrue( "CompoundOut.out.two" in srcDirtiedNames )
		self.assertTrue( "CompoundOut.out" in srcDirtiedNames )

		dstDirtiedNames = set( [ x[0].fullName() for x in dstDirtied ] )

		self.assertEqual( len( dstDirtiedNames ), 4 )
		self.assertTrue( "CompoundIn.in.one" in dstDirtiedNames )
		self.assertTrue( "CompoundIn.in.two" in dstDirtiedNames )
		self.assertTrue( "CompoundIn.in" in dstDirtiedNames )
		self.assertTrue( "CompoundIn.out" in dstDirtiedNames )

	def testAffectsRejectsCompoundPlugs( self ) :

		n = GafferTest.CompoundPlugNode()

		self.assertRaises( RuntimeError, n.affects, n["p"] )

	def testAffectsWorksWithPlugs( self ) :

		# check that we can propagate dirtiness for simple Plugs, and
		# not just ValuePlugs.

		class SimpleDependencyNode( Gaffer.DependencyNode ) :

			def __init__( self, name="PassThrough", inputs={}, dynamicPlugs=() ) :

				Gaffer.DependencyNode.__init__( self, name )

				self.addChild( Gaffer.Plug( "in", Gaffer.Plug.Direction.In ) )
				self.addChild( Gaffer.Plug( "out", Gaffer.Plug.Direction.Out ) )

			def affects( self, input ) :

				if input.isSame( self["in"] ) :
					return [ self["out"] ]

				return []

		s1 = SimpleDependencyNode()
		s2 = SimpleDependencyNode()

		cs = GafferTest.CapturingSlot( s2.plugDirtiedSignal() )

		s2["in"].setInput( s1["out"] )

		self.assertEqual( len( cs ), 2 )
		self.assertTrue( cs[0][0].isSame( s2["in"] ) )
		self.assertTrue( cs[1][0].isSame( s2["out"] ) )

	def testEnableBehaviour( self ) :

		n = Gaffer.DependencyNode()
		self.assertEqual( n.enabledPlug(), None )

		m = GafferTest.MultiplyNode()
		self.assertEqual( m.enabledPlug(), None )
		self.assertEqual( m.correspondingInput( m["product"] ), None )

		class EnableAbleNode( Gaffer.DependencyNode ) :

			def __init__( self, name = "EnableAbleNode" ) :

				Gaffer.DependencyNode.__init__( self, name )

				self.addChild( Gaffer.BoolPlug( "enabled", Gaffer.Plug.Direction.In, True ) )
				self.addChild( Gaffer.IntPlug( "aIn" ) )
				self.addChild( Gaffer.IntPlug( "bIn" ) )
				self.addChild( Gaffer.IntPlug( "aOut", Gaffer.Plug.Direction.Out ) )
				self.addChild( Gaffer.IntPlug( "bOut", Gaffer.Plug.Direction.Out ) )
				self.addChild( Gaffer.IntPlug( "cOut", Gaffer.Plug.Direction.Out ) )

			def enabledPlug( self ) :

				return self["enabled"]

			def correspondingInput( self, output ) :

				if output.isSame( self["aOut"] ) :

					return self["aIn"]

				elif output.isSame( self["bOut"] ) :

					return self["bIn"]

				return None

		e = EnableAbleNode()
		self.assertTrue( e.enabledPlug().isSame( e["enabled"] ) )
		self.assertTrue( e.correspondingInput( e["aOut"] ).isSame( e["aIn"] ) )
		self.assertTrue( e.correspondingInput( e["bOut"] ).isSame( e["bIn"] ) )
		self.assertEqual( e.correspondingInput( e["enabled"] ), None )
		self.assertEqual( e.correspondingInput( e["aIn"] ), None )
		self.assertEqual( e.correspondingInput( e["bIn"] ), None )
		self.assertEqual( e.correspondingInput( e["cOut"] ), None )

	def testNoDirtiedSignalDuplicates( self ) :

		a1 = GafferTest.AddNode()
		a2 = GafferTest.AddNode()
		a2["op1"].setInput( a1["sum"] )
		a2["op2"].setInput( a1["sum"] )

		cs = GafferTest.CapturingSlot( a2.plugDirtiedSignal() )

		a1["op1"].setValue( 21 )

		self.assertEqual( len( cs ), 3 )
		self.assertTrue( cs[0][0].isSame( a2["op1"] ) )
		self.assertTrue( cs[1][0].isSame( a2["op2"] ) )
		self.assertTrue( cs[2][0].isSame( a2["sum"] ) )

	def testSettingValueAlsoSignalsDirtiness( self ) :

		a = GafferTest.AddNode()
		cs = GafferTest.CapturingSlot( a.plugDirtiedSignal() )

		a["op1"].setValue( 21 )

		self.assertEqual( len( cs ), 2 )
		self.assertTrue( cs[0][0].isSame( a["op1"] ) )
		self.assertTrue( cs[1][0].isSame( a["sum"] ) )

	def testDirtyPropagationThreading( self ) :

		def f() :

			n1 = GafferTest.AddNode()
			n2 = GafferTest.AddNode()
			n3 = GafferTest.AddNode()

			n2["op1"].setInput( n1["sum"] )
			n2["op2"].setInput( n1["sum"] )

			n3["op1"].setInput( n2["sum"] )

			for i in range( 1, 100 ) :

				cs = GafferTest.CapturingSlot( n3.plugDirtiedSignal() )

				n1["op1"].setValue( i )

				self.assertEqual( len( cs ), 2 )
				self.assertTrue( cs[0][0].isSame( n3["op1"] ) )
				self.assertTrue( cs[1][0].isSame( n3["sum"] ) )

		threads = []
		for i in range( 0, 10 ) :

			t = threading.Thread( target = f )
			t.start()
			threads.append( t )

		for t in threads :
			t.join()

	def testParentDirtinessSignalledAfterAllChildren( self ) :

		n = Gaffer.DependencyNode()
		n["i"] = Gaffer.FloatPlug()
		n["o"] = Gaffer.V3fPlug( direction = Gaffer.Plug.Direction.Out )

		for c in n["o"].children() :
			c.setInput( n["i"] )

		cs = GafferTest.CapturingSlot( n.plugDirtiedSignal() )
		n["i"].setValue( 10 )

		self.assertEqual( len( cs ), 5 )
		self.assertTrue( cs[0][0].isSame( n["i"] ) )
		self.assertTrue( cs[1][0].isSame( n["o"]["x"] ) )
		self.assertTrue( cs[2][0].isSame( n["o"]["y"] ) )
		self.assertTrue( cs[3][0].isSame( n["o"]["z"] ) )
		self.assertTrue( cs[4][0].isSame( n["o"] ) )

	def testEfficiency( self ) :

		class FanTest( Gaffer.DependencyNode ) :

			def __init__( self, name = "FanTest" ) :

				Gaffer.DependencyNode.__init__( self, name )

				self["in"] = Gaffer.CompoundPlug()
				self["out"] = Gaffer.CompoundPlug( direction = Gaffer.Plug.Direction.Out )

				for i in range( 0, 10 ) :

					self["in"].addChild( Gaffer.IntPlug( "i%d" % i ) )
					self["out"].addChild( Gaffer.IntPlug( "o%d" % i, direction = Gaffer.Plug.Direction.Out ) )

			def affects( self, input ) :

				result = Gaffer.DependencyNode.affects( self, input )

				if input.parent().isSame( self["in"] ) :
					result.extend( self["out"].children() )

				return result

		f1 = FanTest( "f1" )
		f2 = FanTest( "f2" )
		f3 = FanTest( "f3" )
		f4 = FanTest( "f4" )
		f5 = FanTest( "f5" )
		f6 = FanTest( "f6" )

		f2["in"].setInput( f1["out"] )
		f3["in"].setInput( f2["out"] )
		f4["in"].setInput( f3["out"] )
		f5["in"].setInput( f4["out"] )
		f6["in"].setInput( f5["out"] )

		f1["in"][0].setValue( 10 )

	def testDirtyPropagationScoping( self ) :

		s = Gaffer.ScriptNode()
		s["n"] = GafferTest.AddNode()

		cs = GafferTest.CapturingSlot( s["n"].plugDirtiedSignal() )

		with Gaffer.UndoContext( s ) :

			s["n"]["op1"].setValue( 20 )
			s["n"]["op2"].setValue( 21 )

		# Even though we made two changes, we only want
		# dirtiness to have been signalled once, because
		# we grouped them logically in an UndoContext.

		self.assertEqual( len( cs ), 3 )
		self.assertTrue( cs[0][0].isSame( s["n"]["op1"] ) )
		self.assertTrue( cs[1][0].isSame( s["n"]["op2"] ) )
		self.assertTrue( cs[2][0].isSame( s["n"]["sum"] ) )

		# Likewise, when we undo.

		del cs[:]
		s.undo()

		self.assertEqual( len( cs ), 3 )
		self.assertTrue( cs[0][0].isSame( s["n"]["op2"] ) )
		self.assertTrue( cs[1][0].isSame( s["n"]["op1"] ) )
		self.assertTrue( cs[2][0].isSame( s["n"]["sum"] ) )

		# And when we redo.

		del cs[:]
		s.redo()

		self.assertEqual( len( cs ), 3 )
		self.assertTrue( cs[0][0].isSame( s["n"]["op1"] ) )
		self.assertTrue( cs[1][0].isSame( s["n"]["op2"] ) )
		self.assertTrue( cs[2][0].isSame( s["n"]["sum"] ) )

	def testDirtyPropagationScopingForCompoundPlugInputChange( self ) :

		n1 = GafferTest.CompoundPlugNode()
		n2 = GafferTest.CompoundPlugNode()
		n3 = GafferTest.CompoundPlugNode()

		# We never want to be signalled at a point
		# when the child connections are not in
		# a consistent state.

		InputState = collections.namedtuple( "InputState", ( "p", "s", "f" ) )

		inputStates = []
		def plugDirtied( p ) :

			# We can't use self.assert*() in here,
			# because exceptions are caught in plugDirtiedSignal()
			# handling. So we just record the state to check later
			# in assertStatesValid().
			inputStates.append(
				InputState(
					n3["p"].getInput(),
					n3["p"]["s"].getInput(),
					n3["p"]["f"].getInput()
				)
			)

		def assertStatesValid() :

			for state in inputStates :
				if state.p is not None :
					self.assertTrue( state.s is not None )
					self.assertTrue( state.f is not None )
					self.assertTrue( state.p.isSame( state.f.parent() ) )
				else :
					self.assertTrue( state.s is None )
					self.assertTrue( state.f is None )

		c = n3.plugDirtiedSignal().connect( plugDirtied )

		n3["p"].setInput( n1["o"] )
		assertStatesValid()

		n3["p"].setInput( n2["o"] )
		assertStatesValid()

		n3["p"].setInput( None )
		assertStatesValid()

	def testDirtyOnPlugAdditionAndRemoval( self ) :

		class DynamicAddNode( Gaffer.ComputeNode ) :

			def __init__( self, name = "DynamicDependencies" ) :

				Gaffer.ComputeNode.__init__( self, name )

				self["out"] = Gaffer.IntPlug( direction = Gaffer.Plug.Direction.Out )

			def affects( self, input ) :

				result = Gaffer.DependencyNode.affects( self, input )

				if input in self.__inputs() :
					result.append( self["out"] )

				return result

			def hash( self, output, context, h ) :

				assert( output.isSame( self["out"] ) )

				for plug in self.__inputs() :
					plug.hash( h )

			def compute( self, output, context ) :

				result = 0
				for plug in self.__inputs() :
					result += plug.getValue()

				output.setValue( result )

			def __inputs( self ) :

				return [ p for p in self.children( Gaffer.IntPlug ) if p.direction() == p.Direction.In ]

		valuesWhenDirtied = []

		def plugDirtied( plug ) :

			if plug.isSame( n["out"] ) :
				valuesWhenDirtied.append( plug.getValue() )

		n = DynamicAddNode()
		c = n.plugDirtiedSignal().connect( plugDirtied )

		n["in"] = Gaffer.IntPlug( defaultValue = 1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		self.assertEqual( valuesWhenDirtied, [ 1 ] )

		del valuesWhenDirtied[:]
		del n["in"]
		self.assertEqual( valuesWhenDirtied, [ 0 ] )

	def testThrowInAffects( self ) :
		# Dirty propagation is a secondary process that
		# is triggered by primary operations like adding
		# plugs, setting values, and changing inputs.
		# We don't want errors that occur during dirty
		# propagation to prevent the original operation
		# from succeeding, so that although dirtiness is
		# not propagated fully, the graph itself is in
		# an intact state.

		node = GafferTest.BadNode()

		with IECore.CapturingMessageHandler() as mh :
			node["in3"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		# We want the addition of the child to have succeeded.
		self.assertTrue( "in3" in node )
		# And to have been informed of the bug in BadNode.
		self.assertEqual( len( mh.messages ), 1 )
		self.assertEqual( mh.messages[0].level, mh.Level.Error )
		self.assertEqual( mh.messages[0].context, "BadNode::affects()" )
		self.assertTrue( "BadNode is bad" in mh.messages[0].message )

		with IECore.CapturingMessageHandler() as mh :
			del node["in3"]

		# We want the removal of the child to have succeeded.
		self.assertTrue( "in3" not in node )
		# And to have been informed of the bug in BadNode.
		self.assertEqual( len( mh.messages ), 1 )
		self.assertEqual( mh.messages[0].level, mh.Level.Error )
		self.assertEqual( mh.messages[0].context, "BadNode::affects()" )
		self.assertTrue( "BadNode is bad" in mh.messages[0].message )

		# And after all that, we still want dirty propagation to work properly.
		cs = GafferTest.CapturingSlot( node.plugDirtiedSignal() )
		node["in1"].setValue( 10 )
		self.assertTrue( node["out1"] in [ c[0] for c in cs ] )

if __name__ == "__main__":
	unittest.main()
