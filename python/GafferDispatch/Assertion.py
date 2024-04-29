##########################################################################
#
#  Copyright (c) 2024, John Haddon. All rights reserved.
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

import enum
import unittest

import IECore

import Gaffer
import GafferDispatch

class Assertion( GafferDispatch.TaskNode ) :

	Mode = enum.IntEnum(
		"Mode",
		[
			"Equal",
			"NotEqual",
			"Greater",
			"Less",
		],
		start = 0
	)

	def __init__( self, name = "Assertion" ) :

		GafferDispatch.TaskNode.__init__( self, name )

		self["mode"] = Gaffer.IntPlug(
			minValue = 0,
			maxValue = max( self.Mode )
		)
		self["message"] = Gaffer.StringPlug()

	def canSetup( self, prototypePlug ) :

		if "a" in self :
			return False

		return (
			"a" not in self and
			isinstance( prototypePlug, Gaffer.ValuePlug ) and
			hasattr( prototypePlug, "getValue" )
		)

	def setup( self, prototypePlug ) :

		assert( "a" not in self )
		assert( "b" not in self )
		assert( "delta" not in self )

		self.addChild( prototypePlug.createCounterpart( "a", Gaffer.Plug.Direction.In ) )
		self.addChild( prototypePlug.createCounterpart( "b", Gaffer.Plug.Direction.In ) )

		deltaPrototype = None
		if isinstance( prototypePlug, ( Gaffer.IntPlug, Gaffer.FloatPlug ) ) :
			deltaPrototype = prototypePlug
		elif isinstance( prototypePlug, ( Gaffer.V2iPlug, Gaffer.V3iPlug, Gaffer.V3fPlug, Gaffer.Color3fPlug, Gaffer.Color4fPlug ) ) :
			deltaProtype = prototypePlug[0]
		elif isinstance( prototypePlug, ( Gaffer.Box2iPlug, Gaffer.Box3iPlug, Gaffer.Box2fPlug, Gaffer.Box3fPlug ) ) :
			deltaPrototype = prototypePlug["min"][0]

		if deltaPrototype is not None :
			self.addChild( deltaPrototype.createCounterpart( "delta", Gaffer.Plug.Direction.In ) )

		## TODO, SERIALISABLE, NONDYNAMIC

	def hash( self, context ) :

		h = GafferDispatch.TaskNode.hash( self, context )
		# Avoid potentially expensive hashing of our input
		# plugs so that dispatch is quick even if execution
		# performs expensive queries from scenes or images.
		h.append( context.hash() )
		return h

	def execute( self ) :

		test = unittest.TestCase()

		a = self["a"].getValue()
		b = self["b"].getValue()
		#delate = self["delta"].getValue()
		message = self["message"].getValue() or None

		## TODO : HOW TO HAVE THIS BE RECOGNISED AS A FAILURE AND NOT AN ERROR???

		match self["mode"].getValue() :

			case self.Mode.Equal :
				test.assertEqual( a, b, message )
			case self.Mode.NotEqual :
				test.assertNotEqual( a, b, message )
			case self.Mode.Greater :
				test.assertGreater( a, b, message, delta = delta )
			case self.Mode.Less :
				test.assertLess( a, b, message, delta = delta )

IECore.registerRunTimeTyped( Assertion, typeName = "GafferDispatch::Assertion" )

class AssertionSerialiser( Gaffer.NodeSerialiser ) :

	def postConstructor( self, node, identifier, serialisation ) :

		result = Gaffer.NodeSerialiser.postConstructor( self, node, identifier, serialisation )

		if "a" not in node :
			return result

		if result :
			result += "\n"

		plugSerialiser = Gaffer.Serialisation.acquireSerialiser( node["a"] )
		prototypeConstructor = plugSerialiser.constructor( node["a"], serialisation )
		result += f"{identifier}.setup( {prototypeConstructor} )\n"

		return result

	def childNeedsSerialisation( self, child, serialisation ) :

		if child.getName() in ( "a", "b", "delta" ) :
			return True

		return Gaffer.NodeSerialiser.childNeedsSerialisation( self, child, serialisation )

	def childNeedsConstruction( self, child, serialisation ) :

		if child.getName() in ( "a", "b", "delta" ) :
			# We'll make these via a `setup()` call.
			return False

		return Gaffer.NodeSerialiser.childNeedsConstruction( self, child, serialisation )

Gaffer.Serialisation.registerSerialiser( Assertion, AssertionSerialiser() )