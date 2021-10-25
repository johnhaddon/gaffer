##########################################################################
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
import GafferUI
import GafferScene
import GafferSceneUI
import GafferUITest

class GraphGadgetTest( GafferUITest.TestCase ) :

	def testConstructorGILRelease( self ) :

		script = Gaffer.ScriptNode()

		# Build a contrived scene that will cause `childNames` queries to spawn
		# a threaded compute that will execute a Python expression.

		script["plane"] = GafferScene.Plane()
		script["plane"]["divisions"].setValue( imath.V2i( 50 ) )

		script["planeFilter"] = GafferScene.PathFilter()
		script["planeFilter"]["paths"].setValue( IECore.StringVectorData( [ "/plane" ] ) )

		script["sphere"] = GafferScene.Sphere()

		script["instancer"] = GafferScene.Instancer()
		script["instancer"]["in"].setInput( script["plane"]["out"] )
		script["instancer"]["prototypes"].setInput( script["sphere"]["out"] )
		script["instancer"]["filter"].setInput( script["planeFilter"]["out"] )

		script["instanceFilter"] = GafferScene.PathFilter()
		script["instanceFilter"]["paths"].setValue( IECore.StringVectorData( [ "/plane/instances/*/*" ] ) )

		script["cube"] = GafferScene.Cube()

		script["parent"] = GafferScene.Parent()
		script["parent"]["in"].setInput( script["instancer"]["out"] )
		script["parent"]["children"][0].setInput( script["cube"]["out"] )
		script["parent"]["filter"].setInput( script["instanceFilter"]["out"] )

		script["expression"] = Gaffer.Expression()
		script["expression"].setExpression( 'parent["sphere"]["name"] = "sphere"' )

		# Use an existence query to connect that threaded compute to the
		# `enabled` plug of a node.

		script["query"] = GafferScene.ExistenceQuery()
		script["query"]["scene"].setInput( script["parent"]["out"] )
		script["query"]["location"].setValue( "/plane/instances/sphere/100/cube")

		script["enabledNode"] = GafferScene.Transform()
		script["enabledNode"]["enabled"].setInput( script["query"]["exists"] )
		#print( script["enabledNode"]["enabled"].getValue() )

		# Test that the `GraphGadget` constructor releases the GIL so that the
		# compute doesn't hang when a StandardNodeGadget evaluates
		# `node.enabled`. If we're lucky, the expression executes on the main
		# thread anyway, so loop to give it plenty of chances to fail.

		for i in range( 0, 100 ) :

			graphGadget = GafferUI.GraphGadget( script )
			Gaffer.ValuePlug.clearCache()
			Gaffer.ValuePlug.clearHashCache()

if __name__ == "__main__":
	unittest.main()
