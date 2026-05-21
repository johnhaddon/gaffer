##########################################################################
#
#  Copyright (c) 2026, Cinesite VFX Ltd. All rights reserved.
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

import inspect
import unittest

import imath

import IECore
import IECoreScene

import Gaffer
import GafferScene
import GafferSceneTest

class PointInstancerAlgoTest( GafferSceneTest.SceneTestCase ) :

	class PrototypeGroup( GafferScene.SceneNode ) :

		def __init__( self, name = "PrototypeGroup" ) :

			GafferScene.SceneNode.__init__( self, name )

			self["name"] = Gaffer.StringPlug( defaultValue = "prototype" )
			self["transform"] = Gaffer.TransformPlug()
			self["sphereEnabled"] = Gaffer.BoolPlug( defaultValue = True )
			self["sphereTransform"] = Gaffer.TransformPlug()
			self["cubeEnabled"] = Gaffer.BoolPlug( defaultValue = True )
			self["cubeTransform"] = Gaffer.TransformPlug()
			self["planeEnabled"] = Gaffer.BoolPlug( defaultValue = True )
			self["planeTransform"] = Gaffer.TransformPlug()

			self["__sphere"] = GafferScene.Sphere()
			self["__sphere"]["enabled"].setInput( self["sphereEnabled"] )
			self["__sphere"]["transform"].setInput( self["sphereTransform"] )

			self["__cube"] = GafferScene.Cube()
			self["__cube"]["enabled"].setInput( self["cubeEnabled"] )
			self["__cube"]["transform"].setInput( self["cubeTransform"] )

			self["__plane"] = GafferScene.Plane()
			self["__plane"]["enabled"].setInput( self["planeEnabled"] )
			self["__plane"]["transform"].setInput( self["planeTransform"] )

			self["__group"] = GafferScene.Group()
			self["__group"]["name"].setInput( self["name"] )
			self["__group"]["transform"].setInput( self["transform"] )
			self["__group"]["in"][0].setInput( self["__sphere"]["out"] )
			self["__group"]["in"][1].setInput( self["__cube"]["out"] )
			self["__group"]["in"][2].setInput( self["__plane"]["out"] )

			self["out"].setInput( self["__group"]["out"] )

	def testFlatten( self ) :

		prototype1 = self.PrototypeGroup()
		prototype1["name"].setValue( "prototype1" )
		prototype1["sphereTransform"]["translate"].setValue( imath.V3f( 1, 0, 0 ) )
		prototype2 = self.PrototypeGroup() # TODO : MAKE EM DIFFERENT
		prototype2["name"].setValue( "prototype2" )
		prototype2["planeEnabled"].setValue( False )

		prototypeGroup = GafferScene.Group()
		prototypeGroup["name"].setValue( "prototypes" )
		prototypeGroup["in"][0].setInput( prototype1["out"] )
		prototypeGroup["in"][1].setInput( prototype2["out"] )

		pointInstancer = IECoreScene.PointInstancer( 2 )
		pointInstancer["P"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Vertex,
			IECore.V3fVectorData( [
				imath.V3f( 1, 2, 3 ), imath.V3f( 1, 0, 0 ),
			] )
		)
		pointInstancer["prototypeIndex"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Vertex,
			IECore.IntVectorData( [ 0, 1 ] ),
		)
		pointInstancer["prototypeRoots"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Constant,
			IECore.StringVectorData( [ "./prototypes/prototype1", "./prototypes/prototype2" ] ),
		)

		pointInstancerNode = GafferScene.ObjectToScene()
		pointInstancerNode["object"].setValue( pointInstancer )
		pointInstancerNode["name"].setValue( "instancer" )

		parent = GafferScene.Parent()
		parent["in"].setInput( pointInstancerNode["out"] )
		parent["children"][0].setInput( prototypeGroup["out"] )
		parent["parent"].setValue( "/instancer" )

		# def walk( scene, path ) : # TODO : REMOVE

		# 	print( path )
		# 	for childName in scene.childNames( path ) :
		# 		childPath = "{}{}{}".format( path, "/" if path != "/" else "", str( childName ) )
		# 		walk( scene, childPath )

		# walk( parent["out"], "/" )
		# print( "------" )
		# walk( parent["out"], "" )

		with Gaffer.Context() as context :

			context["scene:path"] = GafferScene.ScenePlug.stringToPath( "/instancer" )
			pointInstancer = parent["out"]["object"].getValue()
			self.assertEqual( pointInstancer, pointInstancerNode["object"].getValue() )

			flattened = GafferScene.Private.PointInstancerAlgo.flatten( pointInstancer, parent["out"] )

			self.assertTrue( isinstance( flattened, IECoreScene.PointInstancer ) )
			self.assertTrue( flattened.arePrimitiveVariablesValid() )
			self.assertEqual( flattened.numPoints, 5 )
			self.assertEqual(
				list( flattened["prototypeRoots"].data ),
				[
					"./prototypes/prototype1/cube",
					"./prototypes/prototype1/plane",
					"./prototypes/prototype1/sphere",
					"./prototypes/prototype2/cube",
					"./prototypes/prototype2/sphere",
				]
			)

			self.assertEqual(
				list( flattened["P"].data ),
				[
					imath.V3f( 1, 2, 3 ),
					imath.V3f( 1, 2, 3 ),
					imath.V3f( 2, 2, 3 ),
					imath.V3f( 1, 0, 0 ),
					imath.V3f( 1, 0, 0 ),
				]
			)

	# TODO : TEST CUSTOM VERTEX PRIMVARS

	def testFlattenIncludesRootTransform( self ) :

		prototype = self.PrototypeGroup()
		prototype["transform"]["scale"].setValue( imath.V3f( 2, 3, 4 ) )
		prototype["sphereTransform"]["translate"].setValue( imath.V3f( 1, 0, 0 ) )
		prototype["cubeEnabled"].setValue( False )
		prototype["planeEnabled"].setValue( False )

		pointInstancer = IECoreScene.PointInstancer( 1 )
		pointInstancer["P"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Vertex,
			IECore.V3fVectorData( [ imath.V3f( 0 ) ] ) #imath.V3f( 1, 2, 3 ) ] ) TODO
		)
		pointInstancer["prototypeIndex"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Vertex,
			IECore.IntVectorData( [ 0 ] ),
		)
		pointInstancer["prototypeRoots"] = IECoreScene.PrimitiveVariable(
			IECoreScene.PrimitiveVariable.Interpolation.Constant,
			IECore.StringVectorData( [ "./prototype" ] ),
		)

		pointInstancerNode = GafferScene.ObjectToScene()
		pointInstancerNode["object"].setValue( pointInstancer )
		pointInstancerNode["name"].setValue( "instancer" )

		parent = GafferScene.Parent()
		parent["in"].setInput( pointInstancerNode["out"] )
		parent["children"][0].setInput( prototype["out"] )
		parent["parent"].setValue( "/instancer" )

		with Gaffer.Context() as context :

			context["scene:path"] = GafferScene.ScenePlug.stringToPath( "/instancer" )
			pointInstancer = parent["out"]["object"].getValue()
			self.assertEqual( pointInstancer, pointInstancerNode["object"].getValue() )

			flattened = GafferScene.Private.PointInstancerAlgo.flatten( pointInstancer, parent["out"] )
			query = IECoreScene.PointInstancer.Query( flattened )
			# Actually want to test relative transform, but can use `fullTransform()`
			self.assertEqual(
				query.transform( 0 ),
				parent["out"].fullTransform( "/instancer/prototype/sphere" )
			)

			# TODO : USE QUERY TO TEST AGAINST RELATIVE TRANSFORM


			# self.assertTrue( isinstance( flattened, IECoreScene.PointInstancer ) )
			# self.assertTrue( flattened.arePrimitiveVariablesValid() )
			# self.assertEqual( flattened.numPoints, 5 )
			# self.assertEqual(
			# 	list( flattened["prototypeRoots"].data ),
			# 	[
			# 		"./prototypes/prototype1/cube",
			# 		"./prototypes/prototype1/plane",
			# 		"./prototypes/prototype1/sphere",
			# 		"./prototypes/prototype2/cube",
			# 		"./prototypes/prototype2/sphere",
			# 	]
			# )

			# self.assertEqual(
			# 	list( flattened["P"].data ),
			# 	[
			# 		imath.V3f( 1, 2, 3 ),
			# 		imath.V3f( 1, 2, 3 ),
			# 		imath.V3f( 2, 2, 3 ),
			# 		imath.V3f( 1, 0, 0 ),
			# 		imath.V3f( 1, 0, 0 ),
			# 	]
			# )

if __name__ == "__main__":
	unittest.main()
