##########################################################################
#
#  Copyright (c) 2021, John Haddon. All rights reserved.
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

import IECore
import Gaffer
import GafferScene
import GafferSceneTest

class CryptomatteAlgoTest( GafferSceneTest.SceneTestCase ) :

	def testHash( self ) :

		self.assertFloat32Equal(
			GafferScene.CryptomatteAlgo.hash( "/GAFFERBOT/C_torso_GRP/C_head_GRP/C_head_CPT/C_head001_REN" ),
			-2.52222490915e+36,
		)

	def testMetadataPrefix( self ) :

		self.assertEqual(
			GafferScene.CryptomatteAlgo.metadataPrefix( "crypto_object" ),
			"cryptomatte/f834d0a/"
		)

	def testFind( self ) :

		reader = GafferScene.SceneReader()
		reader["fileName"].setValue( "${GAFFER_ROOT}/resources/gafferBot/caches/gafferBot.scc" )

		def walk( path ) :

			h = GafferScene.CryptomatteAlgo.hash( GafferScene.ScenePlug.pathToString( path ) )
			self.assertEqual(
				GafferScene.CryptomatteAlgo.find( reader["out"], h ),
				path
			)

			for childName in reader["out"].childNames( path ) :

				childPath = IECore.InternedStringVectorData( path )
				childPath.append( childName )

				walk( childPath )

		walk( IECore.InternedStringVectorData() )

		self.assertIsNone(
			GafferScene.CryptomatteAlgo.find(
				reader["out"],
				GafferScene.CryptomatteAlgo.hash( "/non/existent/path" )
			)
		)

if __name__ == "__main__":
	unittest.main()
