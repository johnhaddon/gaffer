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
			"True_",
			"False_",
		],
		start = 0
	)

	def __init__( self, name = "Assertion" ) :

		GafferDispatch.TaskNode.__init__( self, name )

		self["mode"] = Gaffer.IntPlug(
			minValue = 0,
			maxValue = max( self.Mode )
		)
		self["a"] = Gaffer.IntPlug()
		self["b"] = Gaffer.IntPlug()
		self["delta"] = Gaffer.FloatPlug( defaultValue = 0.0001, minValue = 0 )
		self["message"] = Gaffer.StringPlug()

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
		delate = self["delta"].getValue()
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
			case self.Mode.True_ :
				test.assertTrue( a, message )
			case self.Mode.False_ :
				test.assertFalse( a, message )

IECore.registerRunTimeTyped( Assertion, typeName = "GafferDispatch::Assertion" )
