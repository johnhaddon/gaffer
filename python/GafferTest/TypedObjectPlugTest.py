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
import imath

import IECore

import Gaffer
import GafferTest

class TypedObjectPlugTest( GafferTest.TestCase ) :

	def testSerialisation( self ) :

		s = Gaffer.ScriptNode()
		s["n"] = Gaffer.Node()
		s["n"]["t"] = Gaffer.ObjectPlug( "hello", defaultValue = IECore.IntData( 1 ), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		se = s.serialise()

		s2 = Gaffer.ScriptNode()
		s2.execute( se )

		self.assertTrue( s2["n"]["t"].isInstanceOf( Gaffer.ObjectPlug.staticTypeId() ) )

	def testSerialisationWithConnection( self ) :

		s = Gaffer.ScriptNode()
		s["n"] = Gaffer.Node()
		s["n"]["t"] = Gaffer.ObjectPlug( "hello", defaultValue = IECore.IntData( 0 ), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		s["n2"] = Gaffer.Node()
		s["n2"]["t2"] = Gaffer.ObjectPlug( "hello", defaultValue = IECore.IntData( 0 ), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic, direction=Gaffer.Plug.Direction.Out )

		s["n"]["t"].setInput( s["n2"]["t2"] )

		se = s.serialise()

		s2 = Gaffer.ScriptNode()
		s2.execute( se )

		self.assertTrue( s2["n"]["t"].getInput().isSame( s2["n2"]["t2"] ) )

	def testDefaultValue( self ) :

		v1 = IECore.IntVectorData( [ 1, 2, 3 ] )
		v2 = IECore.IntVectorData( [ 4, 5, 6 ] )

		s = Gaffer.ScriptNode()
		s["n"] = Gaffer.Node()
		s["n"]["user"]["p"] = Gaffer.ObjectPlug( defaultValue = v1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		self.assertEqual( s["n"]["user"]["p"].getDefaultValue(), v1 )
		self.assertEqual( s["n"]["user"]["p"].getValue(), v1 )

		with Gaffer.UndoScope( s ) :
			s["n"]["user"]["p"].setDefaultValue( v2 )
			self.assertEqual( s["n"]["user"]["p"].getDefaultValue(), v2 )
			self.assertEqual( s["n"]["user"]["p"].getValue(), v1 )

		s.undo()
		self.assertEqual( s["n"]["user"]["p"].getDefaultValue(), v1 )
		self.assertEqual( s["n"]["user"]["p"].getValue(), v1 )

		s.redo()
		self.assertEqual( s["n"]["user"]["p"].getDefaultValue(), v2 )
		self.assertEqual( s["n"]["user"]["p"].getValue(), v1 )

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )
		self.assertEqual( s2["n"]["user"]["p"].getDefaultValue(), v2 )
		self.assertEqual( s2["n"]["user"]["p"].getValue(), v1 )

	def testDefaultValueIdentity( self ) :

		p = Gaffer.ObjectPlug( "p", defaultValue = IECore.IntVectorData( [ 1, 2, 3 ] ) )
		self.assertEqual( p.getDefaultValue(), IECore.IntVectorData( [ 1, 2, 3 ] ) )

		self.assertFalse( p.getDefaultValue().isSame( p.getDefaultValue() ) )
		self.assertTrue( p.getDefaultValue( _copy = False ).isSame( p.getDefaultValue( _copy = False ) ) )

		v = IECore.IntVectorData( [ 4, 5, 6 ] )
		p.setDefaultValue( v ) # Should be copied here
		self.assertFalse( p.getDefaultValue( _copy = False ).isSame( v ) )
		p.setDefaultValue( v, _copy = False )
		self.assertFalse( p.getDefaultValue().isSame( v ) ) # Should be copied in `getDefaultValue()`
		self.assertTrue( p.getDefaultValue( _copy = False ).isSame( v ) )

	def testRunTimeTyped( self ) :

		self.assertEqual( IECore.RunTimeTyped.baseTypeId( Gaffer.ObjectPlug.staticTypeId() ), Gaffer.ValuePlug.staticTypeId() )

	def testAcceptsNoneInput( self ) :

		p = Gaffer.ObjectPlug( "hello", Gaffer.Plug.Direction.In, IECore.IntData( 10 ) )
		self.assertTrue( p.acceptsInput( None ) )

	def testBoolVectorDataPlug( self ) :

		p = Gaffer.BoolVectorDataPlug( "p", defaultValue = IECore.BoolVectorData( [ True, False ] ) )

		self.assertEqual( p.getDefaultValue(), IECore.BoolVectorData( [ True, False ] ) )
		self.assertEqual( p.getValue(), IECore.BoolVectorData( [ True, False ] ) )

		p.setValue( IECore.BoolVectorData( [ False ] ) )
		self.assertEqual( p.getValue(), IECore.BoolVectorData( [ False ] ) )

		self.assertRaises( Exception, p.setValue, IECore.IntData( 10 ) )

	def testNullDefaultValue( self ) :

		self.assertRaises( ValueError, Gaffer.ObjectPlug, "hello", defaultValue = None )

	def testNullValue( self ) :

		p = Gaffer.ObjectPlug( "hello", Gaffer.Plug.Direction.In, IECore.IntData( 10 ) )
		self.assertRaises( ValueError, p.setValue, None )

	def testSerialisationWithValueAndDefaultValue( self ) :

		s = Gaffer.ScriptNode()
		s["n"] = Gaffer.Node()
		s["n"]["t"] = Gaffer.ObjectPlug( "hello", flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic, defaultValue = IECore.IntData( 10 ) )
		s["n"]["t"].setValue( IECore.CompoundObject( { "a" : IECore.IntData( 20 ) } ) )

		se = s.serialise()

		s2 = Gaffer.ScriptNode()
		s2.execute( se )

		self.assertTrue( s2["n"]["t"].isInstanceOf( Gaffer.ObjectPlug.staticTypeId() ) )
		self.assertEqual( s2["n"]["t"].getDefaultValue(), IECore.IntData( 10 ) )
		self.assertEqual( s2["n"]["t"].getValue(), IECore.CompoundObject( { "a" : IECore.IntData( 20 ) } ) )

	def testConstructCantSpecifyBothInputAndValue( self ) :

		out = Gaffer.ObjectPlug( "out", direction=Gaffer.Plug.Direction.Out, defaultValue=IECore.StringData( "hi" ) )

		self.assertRaises( Exception, Gaffer.ObjectPlug, "in", input=out, value=IECore.IntData( 10 ) )

	class TypedObjectPlugNode( Gaffer.Node ) :

		def __init__( self, name="TypedObjectPlugNode" ) :

			Gaffer.Node.__init__( self, name )

			self.addChild(
				Gaffer.ObjectPlug( "p", defaultValue = IECore.IntData( 1 ) ),
			)

	IECore.registerRunTimeTyped( TypedObjectPlugNode )

	def testSerialisationOfStaticPlugs( self ) :

		s = Gaffer.ScriptNode()
		s["n"] = self.TypedObjectPlugNode()
		s["n"]["p"].setValue( IECore.IntData( 10 ) )

		se = s.serialise()

		s2 = Gaffer.ScriptNode()
		s2.execute( se )

		self.assertEqual( s2["n"]["p"].getValue(), IECore.IntData( 10 ) )

	def testSetToDefault( self ) :

		defaultValue = IECore.IntVectorData( [ 1, 2, 3 ] )
		plug = Gaffer.ObjectPlug( defaultValue = defaultValue )
		self.assertEqual( plug.getValue(), defaultValue )

		plug.setValue( IECore.StringData( "value" ) )
		self.assertEqual( plug.getValue(), IECore.StringData( "value" ) )

		plug.setToDefault()
		self.assertEqual( plug.getValue(), defaultValue )

	def testValueType( self ) :

		self.assertTrue( Gaffer.ObjectPlug.ValueType is IECore.Object )
		self.assertTrue( Gaffer.BoolVectorDataPlug.ValueType is IECore.BoolVectorData )
		self.assertTrue( Gaffer.IntVectorDataPlug.ValueType is IECore.IntVectorData )
		self.assertTrue( Gaffer.FloatVectorDataPlug.ValueType is IECore.FloatVectorData )
		self.assertTrue( Gaffer.StringVectorDataPlug.ValueType is IECore.StringVectorData )
		self.assertTrue( Gaffer.V3fVectorDataPlug.ValueType is IECore.V3fVectorData )
		self.assertTrue( Gaffer.Color3fVectorDataPlug.ValueType is IECore.Color3fVectorData )
		self.assertTrue( Gaffer.M44fVectorDataPlug.ValueType is IECore.M44fVectorData )
		self.assertTrue( Gaffer.V2iVectorDataPlug.ValueType is IECore.V2iVectorData )
		self.assertTrue( Gaffer.ObjectVectorPlug.ValueType is IECore.ObjectVector )
		self.assertTrue( Gaffer.AtomicCompoundDataPlug.ValueType is IECore.CompoundData )

	def testSetValueCopying( self ) :

		p = Gaffer.ObjectPlug( defaultValue = IECore.IntData( 1 ) )

		i = IECore.IntData( 10 )
		p.setValue( i )
		self.assertFalse( p.getValue( _copy=False ).isSame( i ) )

		i = IECore.IntData( 20 )
		p.setValue( i, _copy=False )
		self.assertTrue( p.getValue( _copy=False ).isSame( i ) )

	def testCreateCounterpart( self ) :

		p = Gaffer.ObjectPlug( defaultValue = IECore.IntData( 20 ) )
		p2 = p.createCounterpart( "c", Gaffer.Plug.Direction.Out )

		self.assertEqual( p2.getName(), "c" )
		self.assertEqual( p2.direction(), Gaffer.Plug.Direction.Out )
		self.assertEqual( p2.getDefaultValue(), p.getDefaultValue() )
		self.assertEqual( p2.getFlags(), p.getFlags() )

	def testNoChildrenAccepted( self ) :

		p1 = Gaffer.ObjectPlug( defaultValue = IECore.IntData( 20 ) )
		p2 = Gaffer.ObjectPlug( defaultValue = IECore.IntData( 20 ) )

		self.assertFalse( p1.acceptsChild( p2 ) )
		self.assertRaises( RuntimeError, p1.addChild, p2 )

	def testSerialisationWithoutRepr( self ) :

		# Check that we can serialise plug values even when the
		# stored `IECore::Object` does not have a suitable
		# implementation of `repr()`.

		v1 = IECore.TransformationMatrixfData(
			IECore.TransformationMatrixf(
				imath.V3f( 1, 2, 3 ),
				imath.Eulerf(),
				imath.V3f( 1, 1, 1 )
			)
		)

		v2 = IECore.TransformationMatrixfData(
			IECore.TransformationMatrixf(
				imath.V3f( 4, 5, 6 ),
				imath.Eulerf(),
				imath.V3f( 2, 2, 2 )
			)
		)

		with self.assertRaises( Exception ) :
			eval( repr( v1 ) )

		s = Gaffer.ScriptNode()
		s["n"] = Gaffer.Node()
		s["n"]["user"]["p1"] = Gaffer.ObjectPlug( defaultValue = v1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["n"]["user"]["p2"] = Gaffer.ObjectPlug( defaultValue = v2, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )
		self.assertEqual( s2["n"]["user"]["p1"].getDefaultValue(), v1 )
		self.assertEqual( s2["n"]["user"]["p2"].getDefaultValue(), v2 )
		self.assertEqual( s2["n"]["user"]["p1"].getValue(), v1 )
		self.assertEqual( s2["n"]["user"]["p2"].getValue(), v2 )

		s["n"]["user"]["p1"].setValue( v2 )
		s["n"]["user"]["p2"].setValue( v1 )

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )
		self.assertEqual( s2["n"]["user"]["p1"].getDefaultValue(), v1 )
		self.assertEqual( s2["n"]["user"]["p2"].getDefaultValue(), v2 )
		self.assertEqual( s2["n"]["user"]["p1"].getValue(), v2 )
		self.assertEqual( s2["n"]["user"]["p2"].getValue(), v1 )

if __name__ == "__main__":
	unittest.main()
