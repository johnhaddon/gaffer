#########################################################################
#
#  Copyright (c) 2021, Cinesite VFX Ltd. All rights reserved.
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
import GafferScene
import GafferSceneTest

def randomName( gen, mnc, mxc ):

	from string import ascii_lowercase

	return ''.join( gen.choice( ascii_lowercase )
		for _ in range( gen.randrange( mnc, mxc ) ) )

def addAttr( parent, name, data ):

	parent.addChild( Gaffer.NameValuePlug( name, data, True, name, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )

def addAttrs( parent ):

	addAttr( parent, "b", IECore.BoolData() )
	addAttr( parent, "f", IECore.FloatData() )
	addAttr( parent, "i", IECore.IntData() )
	addAttr( parent, "bv", IECore.BoolVectorData() )
	addAttr( parent, "fv", IECore.FloatVectorData() )
	addAttr( parent, "iv", IECore.IntVectorData() )
	addAttr( parent, "s", IECore.StringData() )
	addAttr( parent, "sv", IECore.StringVectorData() )
	addAttr( parent, "isv", IECore.InternedStringVectorData() )
	addAttr( parent, "c4f", IECore.Color4fData() )
	addAttr( parent, "c3f", IECore.Color3fData() )
	addAttr( parent, "v3f", IECore.V3fData() )
	addAttr( parent, "v2f", IECore.V2fData() )
	addAttr( parent, "v3i", IECore.V3iData() )
	addAttr( parent, "v2i", IECore.V2iData() )
	addAttr( parent, "b3f", IECore.Box3fData() )
	addAttr( parent, "b2f", IECore.Box2fData() )
	addAttr( parent, "b3i", IECore.Box3iData() )
	addAttr( parent, "b2i", IECore.Box2iData() )
	addAttr( parent, "o", IECore.M44fData() )

def addUserPlugs( parent, direction = Gaffer.Plug.Direction.In ):

	parent["user"].addChild( Gaffer.BoolPlug( "b", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.FloatPlug( "f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.IntPlug( "i", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.BoolVectorDataPlug( "bv", direction = direction, defaultValue = IECore.BoolVectorData(), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.FloatVectorDataPlug( "fv", direction = direction, defaultValue = IECore.FloatVectorData(), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.IntVectorDataPlug( "iv", direction = direction, defaultValue = IECore.IntVectorData(), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.StringPlug( "s", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.StringVectorDataPlug( "sv", direction = direction, defaultValue = IECore.StringVectorData(), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.InternedStringVectorDataPlug( "isv", direction = direction, defaultValue = IECore.InternedStringVectorData(), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.Color3fPlug( "c3f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.Color4fPlug( "c4f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.V3fPlug( "v3f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.V3iPlug( "v3i", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.V2fPlug( "v2f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.V2iPlug( "v2i", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.Box3fPlug( "b3f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.Box3iPlug( "b3i", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.Box2fPlug( "b2f", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.Box2iPlug( "b2i", direction = direction, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )
	parent["user"].addChild( Gaffer.ObjectPlug( "o", direction = direction, defaultValue = IECore.NullObject(), flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic ) )

class AttributeQueryTest( GafferSceneTest.SceneTestCase ):

	def testDefault( self ):

		q = GafferScene.AttributeQuery()

		self.assertEqual( q["location"].getValue(), "" )
		self.assertEqual( q["attribute"].getValue(), "" )
		self.assertFalse( q["inherit"].getValue() )
		self.assertFalse( q["exists"].getValue() )
		self.assertFalse( q.isSetup() )
		self.assertRaises( KeyError, lambda: q["default"] )
		self.assertRaises( KeyError, lambda: q["value"] )

	def testNoScene( self ):

		from random import Random
		from datetime import datetime

		r = Random( datetime.now() )

		loc = randomName( r, 5, 10 )
		name = randomName( r, 5, 10 )

		q = GafferScene.AttributeQuery()

		q["location"].setValue( "" )
		q["attribute"].setValue( "" )
		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( name )
		q["inherit"].setValue( False )
		self.assertFalse( q["exists"].getValue() )

		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

		q["location"].setValue( loc )
		q["attribute"].setValue( "" )
		q["inherit"].setValue( False )
		self.assertFalse( q["exists"].getValue() )

		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( name )
		q["inherit"].setValue( False )
		self.assertFalse( q["exists"].getValue() )

		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

	def testSceneNoSetupNoAttr( self ):

		from random import Random
		from datetime import datetime

		r = Random( datetime.now() )

		loc = randomName( r, 5, 10 )
		name = randomName( r, 5, 10 )

		s = GafferScene.Sphere()
		s["name"].setValue( loc )
		a = GafferScene.CustomAttributes()
		a["in"].setInput( s["out"] )
		q = GafferScene.AttributeQuery()
		q["scene"].setInput( a["out"] )

		q["location"].setValue( "" )
		q["attribute"].setValue( "" )
		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( name )
		q["inherit"].setValue( False )
		self.assertFalse( q["exists"].getValue() )

		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

		q["location"].setValue( loc )
		q["attribute"].setValue( "" )
		q["inherit"].setValue( False )
		self.assertFalse( q["exists"].getValue() )

		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( name )
		q["inherit"].setValue( False )
		self.assertFalse( q["exists"].getValue() )

		q["inherit"].setValue( True )
		self.assertFalse( q["exists"].getValue() )

	def testSceneNoSetupAttr( self ):

		from random import Random
		from datetime import datetime

		r = Random( datetime.now() )
		loc = randomName( r, 5, 10 )

		s = GafferScene.Sphere()
		s["name"].setValue( loc )
		a = GafferScene.CustomAttributes()
		a["in"].setInput( s["out"] )
		q = GafferScene.AttributeQuery()
		q["scene"].setInput( a["out"] )

		addAttrs( a["attributes"] )

		q["location"].setValue( "" )
		q["inherit"].setValue( False )
		q["attribute"].setValue( "" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "b" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "i" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "bv" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "fv" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "iv" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "s" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "is" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "sv" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "isv" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "c4f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "c3f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "v3f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "v2f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "v3i" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "v2i" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "b3f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "b2f" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "b3i" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "b2i" )
		self.assertFalse( q["exists"].getValue() )

		q["location"].setValue( loc )
		q["attribute"].setValue( "" )
		self.assertFalse( q["exists"].getValue() )

		q["attribute"].setValue( "b" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "i" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "bv" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "fv" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "iv" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "s" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "sv" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "isv" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "c4f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "c3f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "v3f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "v2f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "v3i" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "v2i" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "b3f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "b2f" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "b3i" )
		self.assertTrue( q["exists"].getValue() )

		q["attribute"].setValue( "b2i" )
		self.assertTrue( q["exists"].getValue() )

	def testCanSetup( self ):

		q = GafferScene.AttributeQuery()

		# plug with unsupported type cannot be used to setup

		self.assertFalse( q.canSetup( Gaffer.ValuePlug() ) )

		# plugs with in direction can be used to setup

		self.assertTrue( q.canSetup( Gaffer.BoolPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.FloatPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.IntPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.BoolVectorDataPlug( defaultValue = IECore.BoolVectorData() ) ) )
		self.assertTrue( q.canSetup( Gaffer.FloatVectorDataPlug( defaultValue = IECore.FloatVectorData() ) ) )
		self.assertTrue( q.canSetup( Gaffer.IntVectorDataPlug( defaultValue = IECore.IntVectorData() ) ) )
		self.assertTrue( q.canSetup( Gaffer.StringPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.StringVectorDataPlug( defaultValue = IECore.StringVectorData() ) ) )
		self.assertTrue( q.canSetup( Gaffer.InternedStringVectorDataPlug( defaultValue = IECore.InternedStringVectorData() ) ) )
		self.assertTrue( q.canSetup( Gaffer.Color3fPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.Color4fPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.V3fPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.V3iPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.V2fPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.V2iPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.Box3fPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.Box3iPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.Box2fPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.Box2iPlug() ) )
		self.assertTrue( q.canSetup( Gaffer.ObjectPlug( defaultValue = IECore.NullObject() ) ) )

		# plugs with out direction can be used to setup

		self.assertTrue( q.canSetup( Gaffer.BoolPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.FloatPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.IntPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.BoolVectorDataPlug( defaultValue = IECore.BoolVectorData(), direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.FloatVectorDataPlug( defaultValue = IECore.FloatVectorData(), direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.IntVectorDataPlug( defaultValue = IECore.IntVectorData(), direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.StringPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.StringVectorDataPlug( defaultValue = IECore.StringVectorData(), direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.InternedStringVectorDataPlug( defaultValue = IECore.InternedStringVectorData(), direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.Color3fPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.Color4fPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.V3fPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.V3iPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.V2fPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.V2iPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.Box3fPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.Box3iPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.Box2fPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.Box2iPlug( direction = Gaffer.Plug.Direction.Out ) ) )
		self.assertTrue( q.canSetup( Gaffer.ObjectPlug( defaultValue = IECore.NullObject(), direction = Gaffer.Plug.Direction.Out ) ) )

		# plugs on another node can be used to setup

		qo = GafferScene.AttributeQuery()
		addUserPlugs( qo )

		self.assertTrue( q.canSetup( qo["user"]["b"] ) )
		self.assertTrue( q.canSetup( qo["user"]["f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["i"] ) )
		self.assertTrue( q.canSetup( qo["user"]["bv"] ) )
		self.assertTrue( q.canSetup( qo["user"]["fv"] ) )
		self.assertTrue( q.canSetup( qo["user"]["iv"] ) )
		self.assertTrue( q.canSetup( qo["user"]["s"] ) )
		self.assertTrue( q.canSetup( qo["user"]["sv"] ) )
		self.assertTrue( q.canSetup( qo["user"]["isv"] ) )
		self.assertTrue( q.canSetup( qo["user"]["c3f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["c4f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["v3f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["v3i"] ) )
		self.assertTrue( q.canSetup( qo["user"]["v2f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["v2i"] ) )
		self.assertTrue( q.canSetup( qo["user"]["b3f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["b3i"] ) )
		self.assertTrue( q.canSetup( qo["user"]["b2f"] ) )
		self.assertTrue( q.canSetup( qo["user"]["b2i"] ) )
		self.assertTrue( q.canSetup( qo["user"]["o"] ) )

		# plugs on same node can be used to setup

		addUserPlugs( q )

		self.assertTrue( q.canSetup( q["user"]["b"] ) )
		self.assertTrue( q.canSetup( q["user"]["f"] ) )
		self.assertTrue( q.canSetup( q["user"]["i"] ) )
		self.assertTrue( q.canSetup( q["user"]["bv"] ) )
		self.assertTrue( q.canSetup( q["user"]["fv"] ) )
		self.assertTrue( q.canSetup( q["user"]["iv"] ) )
		self.assertTrue( q.canSetup( q["user"]["s"] ) )
		self.assertTrue( q.canSetup( q["user"]["sv"] ) )
		self.assertTrue( q.canSetup( q["user"]["isv"] ) )
		self.assertTrue( q.canSetup( q["user"]["c3f"] ) )
		self.assertTrue( q.canSetup( q["user"]["c4f"] ) )
		self.assertTrue( q.canSetup( q["user"]["v3f"] ) )
		self.assertTrue( q.canSetup( q["user"]["v3i"] ) )
		self.assertTrue( q.canSetup( q["user"]["v2f"] ) )
		self.assertTrue( q.canSetup( q["user"]["v2i"] ) )
		self.assertTrue( q.canSetup( q["user"]["b3f"] ) )
		self.assertTrue( q.canSetup( q["user"]["b3i"] ) )
		self.assertTrue( q.canSetup( q["user"]["b2f"] ) )
		self.assertTrue( q.canSetup( q["user"]["b2i"] ) )
		self.assertTrue( q.canSetup( q["user"]["o"] ) )

	def testSetup( self ):

		q = GafferScene.AttributeQuery()

		# plug with unsupported type cannot be used to setup

		self.assertRaises( IECore.Exception, lambda: q.setup( Gaffer.ValuePlug() ) )
		self.assertFalse( q.isSetup() )
		self.assertRaises( KeyError, lambda: q["default"] )
		self.assertRaises( KeyError, lambda: q["value"] )

		# plugs with supported types can be used to setup

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.BoolPlug() )
		self.assertTrue( q.isSetup() )
		self.assertIsInstance( q["default"], Gaffer.Plug )
		self.assertIsInstance( q["value"], Gaffer.Plug )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.FloatPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.IntPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.BoolVectorDataPlug( defaultValue = IECore.BoolVectorData() ) )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.FloatVectorDataPlug( defaultValue = IECore.FloatVectorData() ) )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.IntVectorDataPlug( defaultValue = IECore.IntVectorData() ) )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.StringPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.StringVectorDataPlug( defaultValue = IECore.StringVectorData() ) )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.InternedStringVectorDataPlug( defaultValue = IECore.InternedStringVectorData() ) )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.Color3fPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.Color4fPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.V3fPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.V3iPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.V2fPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.V2iPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.Box3fPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.Box2fPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.Box3iPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.Box2iPlug() )
		self.assertTrue( q.isSetup() )

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.ObjectPlug( defaultValue = IECore.NullObject() ) )
		self.assertTrue( q.isSetup() )

		# once setup calling setup again raises an exception

		q = GafferScene.AttributeQuery()
		q.setup( Gaffer.ObjectPlug( defaultValue = IECore.NullObject() ) )
		self.assertTrue( q.isSetup() )
		self.assertFalse( q.canSetup( Gaffer.ObjectPlug( defaultValue = IECore.NullObject() ) ) )
		self.assertRaises( IECore.Exception, lambda: q.setup( Gaffer.ObjectPlug( defaultValue = IECore.NullObject() ) ) )

	def testSceneSetupAttr( self ):

		from random import Random
		from datetime import datetime

		r = Random( datetime.now() )
		loc = randomName( r, 5, 10 )

		s = GafferScene.Sphere()
		s["name"].setValue( loc )
		a = GafferScene.CustomAttributes()
		a["in"].setInput( s["out"] )
		addAttrs( a["attributes"] )

		a["attributes"]["b"]["value"].setValue( bool( r.randint( 0, 1 ) ) )
		a["attributes"]["f"]["value"].setValue( r.uniform( -100.0, 100.0 ) )
		a["attributes"]["i"]["value"].setValue( r.randint( -100, 100 ) )
		a["attributes"]["bv"]["value"].setValue( IECore.BoolVectorData( [ bool( r.randint( 0, 1 ) ) for _ in range( 10 ) ] ) )
		a["attributes"]["fv"]["value"].setValue( IECore.FloatVectorData( [ r.uniform( -100.0, 100.0 ) for _ in range( 10 ) ] ) )
		a["attributes"]["iv"]["value"].setValue( IECore.IntVectorData( [ r.randint( -100, 100 ) for _ in range( 10 ) ] ) )
		a["attributes"]["s"]["value"].setValue( randomName( r, 4, 5 ) )
		a["attributes"]["sv"]["value"].setValue( IECore.StringVectorData( [ randomName( r, 4, 5 ) for _ in range( 10 ) ] ) )
		a["attributes"]["isv"]["value"].setValue( IECore.InternedStringVectorData( [ IECore.InternedString( randomName( r, 4, 5 ) ) for _ in range( 10 ) ] ) )
		a["attributes"]["c4f"]["value"].setValue( imath.Color4f( r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ) ) )
		a["attributes"]["c3f"]["value"].setValue( imath.Color3f( r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ) ) )
		a["attributes"]["v3f"]["value"].setValue( imath.V3f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) )
		a["attributes"]["v2f"]["value"].setValue( imath.V2f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) )
		a["attributes"]["v3i"]["value"].setValue( imath.V3i( r.randint( -100, 100 ), r.randint( -100, 100 ), r.randint( -100, 100 ) ) )
		a["attributes"]["v2i"]["value"].setValue( imath.V2i( r.randint( -100, 100 ), r.randint( -100, 100 ) ) )
		a["attributes"]["b3f"]["value"].setValue( imath.Box3f(
			imath.V3f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ),
			imath.V3f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) ) )
		a["attributes"]["b2f"]["value"].setValue( imath.Box2f(
			imath.V2f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ),
			imath.V2f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) ) )
		a["attributes"]["b3i"]["value"].setValue( imath.Box3i(
			imath.V3i( r.randint( -100, 100 ), r.randint( -100, 100 ), r.randint( -100, 100 ) ),
			imath.V3i( r.randint( -100, 100 ), r.randint( -100, 100 ), r.randint( -100, 100 ) ) ) )
		a["attributes"]["b2i"]["value"].setValue( imath.Box2i(
			imath.V2i( r.randint( -100, 100 ), r.randint( -100, 100 ) ),
			imath.V2i( r.randint( -100, 100 ), r.randint( -100, 100 ) ) ) )

		n = Gaffer.Node()
		addUserPlugs( n )

		qb = GafferScene.AttributeQuery()
		qb["scene"].setInput( a["out"] )
		qb["location"].setValue( loc )
		qb.setup( n["user"]["b"] )
		self.assertTrue( qb.isSetup() )
		qb["default"].setValue( bool( r.randint( 0, 1 ) ) )
		self.assertEqual( qb["value"].getValue(), qb["default"].getValue() )
		qb["attribute"].setValue("b")
		self.assertEqual( qb["value"].getValue(), a["attributes"]["b"]["value"].getValue() )
		qb["attribute"].setValue("f")
		self.assertEqual( qb["value"].getValue(), bool( a["attributes"]["f"]["value"].getValue() ) )
		qb["attribute"].setValue("i")
		self.assertEqual( qb["value"].getValue(), bool( a["attributes"]["i"]["value"].getValue() ) )
		qb["attribute"].setValue("o")
		self.assertEqual( qb["value"].getValue(), qb["default"].getValue() )

		qf = GafferScene.AttributeQuery()
		qf["scene"].setInput( a["out"] )
		qf["location"].setValue( loc )
		qf.setup( n["user"]["f"] )
		self.assertTrue( qf.isSetup() )
		qf["default"].setValue( r.uniform( -100.0, 100.0 ) )
		self.assertEqual( qf["value"].getValue(), qf["default"].getValue() )
		qf["attribute"].setValue("b")
		self.assertEqual( qf["value"].getValue(), float( a["attributes"]["b"]["value"].getValue() ) )
		qf["attribute"].setValue("f")
		self.assertEqual( qf["value"].getValue(), a["attributes"]["f"]["value"].getValue() )
		qf["attribute"].setValue("i")
		self.assertEqual( qf["value"].getValue(), int( a["attributes"]["i"]["value"].getValue() ) )
		qf["attribute"].setValue("o")
		self.assertEqual( qf["value"].getValue(), qf["default"].getValue() )

		qi = GafferScene.AttributeQuery()
		qi["scene"].setInput( a["out"] )
		qi["location"].setValue( loc )
		qi.setup( n["user"]["i"] )
		self.assertTrue( qi.isSetup() )
		qi["default"].setValue( r.randint( -100, 100 ) )
		self.assertEqual( qi["value"].getValue(), qi["default"].getValue() )
		qi["attribute"].setValue("b")
		self.assertEqual( qi["value"].getValue(), int( a["attributes"]["b"]["value"].getValue() ) )
		qi["attribute"].setValue("f")
		self.assertEqual( qi["value"].getValue(), int( a["attributes"]["f"]["value"].getValue() ) )
		qi["attribute"].setValue("i")
		self.assertEqual( qi["value"].getValue(), a["attributes"]["i"]["value"].getValue() )
		qi["attribute"].setValue("o")
		self.assertEqual( qi["value"].getValue(), qi["default"].getValue() )

		qbv = GafferScene.AttributeQuery()
		qbv["scene"].setInput( a["out"] )
		qbv["location"].setValue( loc )
		qbv.setup( n["user"]["bv"] )
		self.assertTrue( qbv.isSetup() )
		qbv["default"].setValue( IECore.BoolVectorData( [ bool( r.randint( 0, 1 ) ) for _ in range( 10 ) ] ) )
		self.assertEqual( qbv["value"].getValue(), qbv["default"].getValue() )
		qbv["attribute"].setValue("bv")
		self.assertEqual( qbv["value"].getValue(), a["attributes"]["bv"]["value"].getValue() )
		qbv["attribute"].setValue("o")
		self.assertEqual( qbv["value"].getValue(), qbv["default"].getValue() )

		qfv = GafferScene.AttributeQuery()
		qfv["scene"].setInput( a["out"] )
		qfv["location"].setValue( loc )
		qfv.setup( n["user"]["fv"] )
		self.assertTrue( qfv.isSetup() )
		qfv["default"].setValue( IECore.FloatVectorData( [ r.uniform( -100.0, 100.0 ) for _ in range( 10 ) ] ) )
		self.assertEqual( qfv["value"].getValue(), qfv["default"].getValue() )
		qfv["attribute"].setValue("fv")
		self.assertEqual( qfv["value"].getValue(), a["attributes"]["fv"]["value"].getValue() )
		qfv["attribute"].setValue("o")
		self.assertEqual( qfv["value"].getValue(), qfv["default"].getValue() )

		qiv = GafferScene.AttributeQuery()
		qiv["scene"].setInput( a["out"] )
		qiv["location"].setValue( loc )
		qiv.setup( n["user"]["iv"] )
		self.assertTrue( qiv.isSetup() )
		qiv["default"].setValue( IECore.IntVectorData( [ r.randint( -100, 100 ) for _ in range( 10 ) ] ) )
		self.assertEqual( qiv["value"].getValue(), qiv["default"].getValue() )
		qiv["attribute"].setValue("iv")
		self.assertEqual( qiv["value"].getValue(), a["attributes"]["iv"]["value"].getValue() )
		qiv["attribute"].setValue("o")
		self.assertEqual( qiv["value"].getValue(), qiv["default"].getValue() )

		qs = GafferScene.AttributeQuery()
		qs["scene"].setInput( a["out"] )
		qs["location"].setValue( loc )
		qs.setup( n["user"]["s"] )
		self.assertTrue( qs.isSetup() )
		qs["default"].setValue( randomName( r, 4, 5 ) )
		self.assertEqual( qs["value"].getValue(), qs["default"].getValue() )
		qs["attribute"].setValue("s")
		self.assertEqual( qs["value"].getValue(), a["attributes"]["s"]["value"].getValue() )
		qs["attribute"].setValue("o")
		self.assertEqual( qs["value"].getValue(), qs["default"].getValue() )

		qsv = GafferScene.AttributeQuery()
		qsv["scene"].setInput( a["out"] )
		qsv["location"].setValue( loc )
		qsv.setup( n["user"]["sv"] )
		self.assertTrue( qsv.isSetup() )
		qsv["default"].setValue( IECore.StringVectorData( [ randomName( r, 4, 5 ) for _ in range( 10 ) ] ) )
		self.assertEqual( qsv["value"].getValue(), qsv["default"].getValue() )
		qsv["attribute"].setValue("sv")
		self.assertEqual( qsv["value"].getValue(), a["attributes"]["sv"]["value"].getValue() )
		qsv["attribute"].setValue("o")
		self.assertEqual( qsv["value"].getValue(), qsv["default"].getValue() )

		qisv = GafferScene.AttributeQuery()
		qisv["scene"].setInput( a["out"] )
		qisv["location"].setValue( loc )
		qisv.setup( n["user"]["isv"] )
		self.assertTrue( qisv.isSetup() )
		qisv["default"].setValue( IECore.InternedStringVectorData( [ IECore.InternedString( randomName( r, 4, 5 ) ) for _ in range( 10 ) ] ) )
		self.assertEqual( qisv["value"].getValue(), qisv["default"].getValue() )
		qisv["attribute"].setValue("isv")
		self.assertEqual( qisv["value"].getValue(), a["attributes"]["isv"]["value"].getValue() )
		qisv["attribute"].setValue("o")
		self.assertEqual( qisv["value"].getValue(), qisv["default"].getValue() )

		qc4f = GafferScene.AttributeQuery()
		qc4f["scene"].setInput( a["out"] )
		qc4f["location"].setValue( loc )
		qc4f.setup( n["user"]["c4f"] )
		self.assertTrue( qc4f.isSetup() )
		qc4f["default"].setValue( imath.Color4f( r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ) ) )
		self.assertEqual( qc4f["value"].getValue(), qc4f["default"].getValue() )
		qc4f["attribute"].setValue("c4f")
		self.assertEqual( qc4f["value"].getValue(), a["attributes"]["c4f"]["value"].getValue() )
		qc4f["attribute"].setValue("c3f")
		v = a["attributes"]["c3f"]["value"].getValue()
		self.assertEqual( qc4f["value"].getValue(), imath.Color4f( v.x, v.y, v.z, 1.0 ) )
		qc4f["attribute"].setValue("v3f")
		v = a["attributes"]["v3f"]["value"].getValue()
		self.assertEqual( qc4f["value"].getValue(), imath.Color4f( v.x, v.y, v.z, 1.0 ) )
		qc4f["attribute"].setValue("v2f")
		v = a["attributes"]["v2f"]["value"].getValue()
		self.assertEqual( qc4f["value"].getValue(), imath.Color4f( v.x, v.y, 0.0, 1.0 ) )
		qc4f["attribute"].setValue("f")
		v = a["attributes"]["f"]["value"].getValue()
		self.assertEqual( qc4f["value"].getValue(), imath.Color4f( v, v, v, 1.0 ) )
		qc4f["attribute"].setValue("i")
		v = float( a["attributes"]["i"]["value"].getValue() )
		self.assertEqual( qc4f["value"].getValue(), imath.Color4f( v, v, v, 1.0 ) )
		qc4f["attribute"].setValue("o")
		self.assertEqual( qc4f["value"].getValue(), qc4f["default"].getValue() )

		qc3f = GafferScene.AttributeQuery()
		qc3f["scene"].setInput( a["out"] )
		qc3f["location"].setValue( loc )
		qc3f.setup( n["user"]["c3f"] )
		self.assertTrue( qc3f.isSetup() )
		qc3f["default"].setValue( imath.Color3f( r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ), r.uniform( 0.0, 1.0 ) ) )
		self.assertEqual( qc3f["value"].getValue(), qc3f["default"].getValue() )
		qc3f["attribute"].setValue("c4f")
		v = a["attributes"]["c4f"]["value"].getValue()
		self.assertEqual( qc3f["value"].getValue(), imath.Color3f( v.r, v.g, v.b ) )
		qc3f["attribute"].setValue("c3f")
		self.assertEqual( qc3f["value"].getValue(), a["attributes"]["c3f"]["value"].getValue() )
		qc3f["attribute"].setValue("v3f")
		self.assertEqual( qc3f["value"].getValue(), a["attributes"]["v3f"]["value"].getValue() )
		qc3f["attribute"].setValue("v2f")
		v = a["attributes"]["v2f"]["value"].getValue()
		self.assertEqual( qc3f["value"].getValue(), imath.Color3f( v.x, v.y, 0.0 ) )
		qc3f["attribute"].setValue("f")
		v = a["attributes"]["f"]["value"].getValue()
		self.assertEqual( qc3f["value"].getValue(), imath.Color3f( v, v, v ) )
		qc3f["attribute"].setValue("i")
		v = float( a["attributes"]["i"]["value"].getValue() )
		self.assertEqual( qc3f["value"].getValue(), imath.Color3f( v, v, v ) )
		qc3f["attribute"].setValue("o")
		self.assertEqual( qc3f["value"].getValue(), qc3f["default"].getValue() )

		qv3f = GafferScene.AttributeQuery()
		qv3f["scene"].setInput( a["out"] )
		qv3f["location"].setValue( loc )
		qv3f.setup( n["user"]["v3f"] )
		self.assertTrue( qv3f.isSetup() )
		qv3f["default"].setValue( imath.V3f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) )
		self.assertEqual( qv3f["value"].getValue(), qv3f["default"].getValue() )
		qv3f["attribute"].setValue("c3f")
		self.assertEqual( qv3f["value"].getValue(), a["attributes"]["c3f"]["value"].getValue() )
		qv3f["attribute"].setValue("v3f")
		self.assertEqual( qv3f["value"].getValue(), a["attributes"]["v3f"]["value"].getValue() )
		qv3f["attribute"].setValue("v3i")
		v = a["attributes"]["v3i"]["value"].getValue()
		self.assertEqual( qv3f["value"].getValue(), imath.V3f( float( v.x ), float( v.y ), float( v.z ) ) )
		qv3f["attribute"].setValue("v2f")
		v = a["attributes"]["v2f"]["value"].getValue()
		self.assertEqual( qv3f["value"].getValue(), imath.V3f( v.x, v.y, 0.0 ) )
		qv3f["attribute"].setValue("v2i")
		v = a["attributes"]["v2i"]["value"].getValue()
		self.assertEqual( qv3f["value"].getValue(), imath.V3f( float( v.x ), float( v.y ), 0.0 ) )
		qv3f["attribute"].setValue("f")
		v = a["attributes"]["f"]["value"].getValue()
		self.assertEqual( qv3f["value"].getValue(), imath.V3f( v, v, v ) )
		qv3f["attribute"].setValue("i")
		v = float( a["attributes"]["i"]["value"].getValue() )
		self.assertEqual( qv3f["value"].getValue(), imath.V3f( v, v, v ) )
		qv3f["attribute"].setValue("o")
		self.assertEqual( qv3f["value"].getValue(), qv3f["default"].getValue() )

		qv2f = GafferScene.AttributeQuery()
		qv2f["scene"].setInput( a["out"] )
		qv2f["location"].setValue( loc )
		qv2f.setup( n["user"]["v2f"] )
		self.assertTrue( qv2f.isSetup() )
		qv2f["default"].setValue( imath.V2f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) )
		self.assertEqual( qv2f["value"].getValue(), qv2f["default"].getValue() )
		qv2f["attribute"].setValue("v2f")
		self.assertEqual( qv2f["value"].getValue(), a["attributes"]["v2f"]["value"].getValue() )
		qv2f["attribute"].setValue("v2i")
		v = a["attributes"]["v2i"]["value"].getValue()
		self.assertEqual( qv2f["value"].getValue(), imath.V2f( float( v.x ), float( v.y ) ) )
		qv2f["attribute"].setValue("f")
		v = a["attributes"]["f"]["value"].getValue()
		self.assertEqual( qv2f["value"].getValue(), imath.V2f( v, v ) )
		qv2f["attribute"].setValue("i")
		v = float( a["attributes"]["i"]["value"].getValue() )
		self.assertEqual( qv2f["value"].getValue(), imath.V2f( v, v ) )
		qv2f["attribute"].setValue("o")
		self.assertEqual( qv2f["value"].getValue(), qv2f["default"].getValue() )

		qv3i = GafferScene.AttributeQuery()
		qv3i["scene"].setInput( a["out"] )
		qv3i["location"].setValue( loc )
		qv3i.setup( n["user"]["v3i"] )
		self.assertTrue( qv3i.isSetup() )
		qv3i["default"].setValue( imath.V3i( r.randint( -100, 100 ), r.randint( -100, 100 ), r.randint( -100, 100 ) ) )
		self.assertEqual( qv3i["value"].getValue(), qv3i["default"].getValue() )
		qv3i["attribute"].setValue("v3f")
		v = a["attributes"]["v3f"]["value"].getValue()
		self.assertEqual( qv3i["value"].getValue(), imath.V3i( int( v.x ), int( v.y ), int( v.z ) ) )
		qv3i["attribute"].setValue("v2f")
		v = a["attributes"]["v2f"]["value"].getValue()
		self.assertEqual( qv3i["value"].getValue(), imath.V3i( int( v.x ), int( v.y ), 0 ) )
		qv3i["attribute"].setValue("v3i")
		self.assertEqual( qv3i["value"].getValue(), a["attributes"]["v3i"]["value"].getValue() )
		qv3i["attribute"].setValue("v2i")
		v = a["attributes"]["v2i"]["value"].getValue()
		self.assertEqual( qv3i["value"].getValue(), imath.V3i( v.x, v.y, 0 ) )
		qv3i["attribute"].setValue("f")
		v = int( a["attributes"]["f"]["value"].getValue() )
		self.assertEqual( qv3i["value"].getValue(), imath.V3i( v, v, v ) )
		qv3i["attribute"].setValue("i")
		v = a["attributes"]["i"]["value"].getValue()
		self.assertEqual( qv3i["value"].getValue(), imath.V3i( v, v, v ) )
		qv3i["attribute"].setValue("o")
		self.assertEqual( qv3i["value"].getValue(), qv3i["default"].getValue() )

		qv2i = GafferScene.AttributeQuery()
		qv2i["scene"].setInput( a["out"] )
		qv2i["location"].setValue( loc )
		qv2i.setup( n["user"]["v2i"] )
		self.assertTrue( qv2i.isSetup() )
		qv2i["default"].setValue( imath.V2i( r.randint( -100, 100 ), r.randint( -100, 100 ) ) )
		self.assertEqual( qv2i["value"].getValue(), qv2i["default"].getValue() )
		qv2i["attribute"].setValue("v2f")
		v = a["attributes"]["v2f"]["value"].getValue()
		self.assertEqual( qv2i["value"].getValue(), imath.V2i( int( v.x ), int( v.y ) ) )
		qv2i["attribute"].setValue("v2i")
		self.assertEqual( qv2i["value"].getValue(), a["attributes"]["v2i"]["value"].getValue() )
		qv2i["attribute"].setValue("f")
		v = int( a["attributes"]["f"]["value"].getValue() )
		self.assertEqual( qv2i["value"].getValue(), imath.V2i( v, v ) )
		qv2i["attribute"].setValue("i")
		v = a["attributes"]["i"]["value"].getValue()
		self.assertEqual( qv2i["value"].getValue(), imath.V2i( v, v ) )
		qv2i["attribute"].setValue("o")
		self.assertEqual( qv2i["value"].getValue(), qv2i["default"].getValue() )

		qb3f = GafferScene.AttributeQuery()
		qb3f["scene"].setInput( a["out"] )
		qb3f["location"].setValue( loc )
		qb3f.setup( n["user"]["b3f"] )
		self.assertTrue( qb3f.isSetup() )
		qb3f["default"].setValue( imath.Box3f(
			imath.V3f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ),
			imath.V3f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) ) )
		self.assertEqual( qb3f["value"].getValue(), qb3f["default"].getValue() )
		qb3f["attribute"].setValue("b3f")
		self.assertEqual( qb3f["value"].getValue(), a["attributes"]["b3f"]["value"].getValue() )
		qb3f["attribute"].setValue("b3i")
		v = a["attributes"]["b3i"]["value"].getValue()
		self.assertEqual( qb3f["value"].getValue(), imath.Box3f(
			imath.V3f( float( v.min().x ), float( v.min().y ), float( v.min().z ) ),
			imath.V3f( float( v.max().x ), float( v.max().y ), float( v.max().z ) ) ) )
		qb3f["attribute"].setValue("o")
		self.assertEqual( qb3f["value"].getValue(), qb3f["default"].getValue() )

		qb2f = GafferScene.AttributeQuery()
		qb2f["scene"].setInput( a["out"] )
		qb2f["location"].setValue( loc )
		qb2f.setup( n["user"]["b2f"] )
		self.assertTrue( qb2f.isSetup() )
		qb2f["default"].setValue( imath.Box2f(
			imath.V2f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ),
			imath.V2f( r.uniform( -100.0, 100.0 ), r.uniform( -100.0, 100.0 ) ) ) )
		self.assertEqual( qb2f["value"].getValue(), qb2f["default"].getValue() )
		qb2f["attribute"].setValue("b2f")
		self.assertEqual( qb2f["value"].getValue(), a["attributes"]["b2f"]["value"].getValue() )
		qb2f["attribute"].setValue("b2i")
		v = a["attributes"]["b2i"]["value"].getValue()
		self.assertEqual( qb2f["value"].getValue(), imath.Box2f(
			imath.V2f( float( v.min().x ), float( v.min().y ) ),
			imath.V2f( float( v.max().x ), float( v.max().y ) ) ) )
		qb2f["attribute"].setValue("o")
		self.assertEqual( qb2f["value"].getValue(), qb2f["default"].getValue() )

		qb3i = GafferScene.AttributeQuery()
		qb3i["scene"].setInput( a["out"] )
		qb3i["location"].setValue( loc )
		qb3i.setup( n["user"]["b3i"] )
		self.assertTrue( qb3i.isSetup() )
		qb3i["default"].setValue( imath.Box3i(
			imath.V3i( r.randint( -100, 100 ), r.randint( -100, 100 ), r.randint( -100, 100 ) ),
			imath.V3i( r.randint( -100, 100 ), r.randint( -100, 100 ), r.randint( -100, 100 ) ) ) )
		self.assertEqual( qb3i["value"].getValue(), qb3i["default"].getValue() )
		qb3i["attribute"].setValue("b3f")
		v = a["attributes"]["b3f"]["value"].getValue()
		self.assertEqual( qb3i["value"].getValue(), imath.Box3i(
			imath.V3i( int( v.min().x ), int( v.min().y ), int( v.min().z ) ),
			imath.V3i( int( v.max().x ), int( v.max().y ), int( v.max().z ) ) ) )
		qb3i["attribute"].setValue("b3i")
		self.assertEqual( qb3i["value"].getValue(), a["attributes"]["b3i"]["value"].getValue() )
		qb3i["attribute"].setValue("o")
		self.assertEqual( qb3i["value"].getValue(), qb3i["default"].getValue() )

		qb2i = GafferScene.AttributeQuery()
		qb2i["scene"].setInput( a["out"] )
		qb2i["location"].setValue( loc )
		qb2i.setup( n["user"]["b2i"] )
		self.assertTrue( qb2i.isSetup() )
		qb2i["default"].setValue( imath.Box2i(
			imath.V2i( r.randint( -100, 100 ), r.randint( -100, 100 ) ),
			imath.V2i( r.randint( -100, 100 ), r.randint( -100, 100 ) ) ) )
		self.assertEqual( qb2i["value"].getValue(), qb2i["default"].getValue() )
		qb2i["attribute"].setValue("b2f")
		v = a["attributes"]["b2f"]["value"].getValue()
		self.assertEqual( qb2i["value"].getValue(), imath.Box2i(
			imath.V2i( int( v.min().x ), int( v.min().y ) ),
			imath.V2i( int( v.max().x ), int( v.max().y ) ) ) )
		qb2i["attribute"].setValue("b2i")
		self.assertEqual( qb2i["value"].getValue(), a["attributes"]["b2i"]["value"].getValue() )
		qb2i["attribute"].setValue("o")
		self.assertEqual( qb2i["value"].getValue(), qb2i["default"].getValue() )

		qo = GafferScene.AttributeQuery()
		qo["scene"].setInput( a["out"] )
		qo["location"].setValue( loc )
		qo.setup( n["user"]["o"] )
		self.assertTrue( qo.isSetup() )
		qo["default"].setValue( { randomName( r, 4, 5 ) : IECore.NullObject() } )
		self.assertEqual( qo["value"].getValue(), qo["default"].getValue() )
		qo["attribute"].setValue("o")
		self.assertTrue( qo["value"].getValue(), a["attributes"]["o"]["value"].getValue() )

if __name__ == "__main__":
	unittest.main()
