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

import glob
import os
import pathlib
import sys
import unittest
import warnings

import IECore
import Gaffer
import GafferDispatch
import GafferTest

class test( Gaffer.Application ) :

	def __init__( self ) :

		Gaffer.Application.__init__(
			self,
			"""
			Runs the unit tests for all of Gaffer's python
			modules and libraries. These are run automatically
			as part of Gaffer's build and review process, but it
			is useful to run them manually when developing for
			Gaffer or troubleshooting an installation.

			Run all the tests :

			```
			gaffer test
			```

			Run all the tests for the scene module :

			```
			gaffer test GafferScene
			```

			Repeat a specific test 10 times :

			```
			gaffer test -repeat 10 GafferImageTest.ImageNodeTest.testCacheThreadSafety

			```
			"""
		)

		self.parameters().addParameters(

			[
				IECore.StringVectorParameter(
					name = "testCases",
					description = "A list of names of specific test cases to run. If unspecified then all test cases are run.",
					defaultValue = IECore.StringVectorData( self.__allTestModules() ),
				),

				IECore.IntParameter(
					name = "repeat",
					description = "The number of times to repeat the tests.",
					defaultValue = 1,
				),

				## \todo Rename to `categories` (breaking change).
				IECore.StringParameter(
					name = "category",
					description = "Only runs tests matching certain categories. Accepts a space-separated list of categories, optionally containing wildcards. "
						"Use `-showCategories` to see a list of available categories. Use the `TestRunner.Categories` decorator to assign categories.",
					defaultValue = "*",
				),

				IECore.StringParameter(
					name = "excludedCategories",
					description = "Excludes tests matching certain categories. Accepts a space-separated list of categories, optionally containing wildcards.",
					defaultValue = "",
				),

				IECore.BoolParameter(
					name = "showCategories",
					description = "Prints a list of available test categories to `stdout`.",
					defaultValue = False,
				),

				IECore.FileNameParameter(
					name = "outputFile",
					description = "The name of a JSON file that the results are written to.",
					defaultValue = "",
					allowEmptyString = True,
					extensions = "json",
				),

				IECore.FileNameParameter(
					name = "previousOutputFile",
					description = "The name of a JSON file containing the results of a previous test run. "
						"This will be used to detect and report performance regressions.",
					defaultValue = "",
					allowEmptyString = True,
					extensions = "json",
				),

				IECore.BoolParameter(
					name = "stopOnFailure",
					description = "Stops on the first failure, instead of running the remaining tests.",
					defaultValue = False,
				)
			]

		)

		self.parameters().userData()["parser"] = IECore.CompoundObject(
			{
				"flagless" : IECore.StringVectorData( [ "testCases" ] )
			}
		)

	def _run( self, args ) :

		import unittest

		for i in range( 0, args["repeat"].value ) :

			testSuite = unittest.TestSuite()
			for name in args["testCases"] :
				testCase = _TestLoader().loadTestsFromName( name )
				testSuite.addTest( testCase )

			if args["showCategories"].value :
				print( " ".join( sorted( GafferTest.TestRunner.categories( testSuite ) ) ) )
				return 0

			GafferTest.TestRunner.filterCategories( testSuite, args["category"].value, args["excludedCategories"].value )

			testRunner = GafferTest.TestRunner( previousResultsFile = args["previousOutputFile"].value )
			if args["stopOnFailure"].value :
				testRunner.failfast = True

			with warnings.catch_warnings() :
				warnings.simplefilter( "error", DeprecationWarning )
				testResult = testRunner.run( testSuite )

			if args["outputFile"].value :
				testResult.save( args["outputFile"].value )

			if not testResult.wasSuccessful() :
				return 1

		return 0

	@staticmethod
	def __allTestModules() :

		result = set()
		for path in sys.path :
			modules = glob.glob( os.path.join( path, "Gaffer*Test" ) ) + glob.glob( os.path.join( path, "IECore*Test" ) )
			for m in modules :
				result.add( os.path.basename( m ) )

		return sorted( result )

IECore.registerRunTimeTyped( test )

class _TestCase( GafferTest.TestCase ) :

	def __init__( self, script, node ) :

		unittest.TestCase.__init__( self )

		self.__script = script
		self.__node = node

	def __str__( self ) :

		scriptPath = pathlib.Path( self.__script["fileName"].getValue() )
		for path in sys.path :
			try :
				scriptPath = scriptPath.relative_to( pathlib.Path( path ) )
				break
			except :
				pass

		return "{} ({})".format( self.__node.getName(), scriptPath )

	def runTest( self ) :

		dispatcher = GafferDispatch.LocalDispatcher()
		dispatcher["tasks"][0].setInput( self.__node["task"] )
		dispatcher["jobsDirectory"].setValue( self.temporaryDirectory() )

		## TODO : FAILURES NOT ERRORS
		dispatcher["task"].execute()

## TODO : MOVE TO GAFFERTEST MODULE?? NO, I DON'T THINK SO.
## TODO : MAKE DISPATCH DEPENDENCY SOFT (ONLY IMPORT WHEN RUN)
## TODO : OR MOVE TO GAFFERDISPATCHTEST??
##        - THAT IS PURER BUT FORCES TEST.PY TO LOAD THE DISPATCH MODULE ALWAYS
class _TestLoader( unittest.TestLoader ) :

	def loadTestsFromName( self, name ) :

		baseSuite = unittest.TestLoader.loadTestsFromName( self, name )
		taskBasedSuite = self.__loadTaskBasedSuite( name )

		if taskBasedSuite is not None :
			if all( test.__class__.__name__ == "_FailedTest" for test in baseSuite ) :
				return taskBasedSuite
			else :
				return unittest.TestSuite( [ baseSuite, taskBasedSuite ] )
		else :
			return baseSuite

	def __loadTaskBasedSuite( self, name ) :

		nodePath = []
		path = pathlib.Path( *name.split( "." ) )
		while path.parts :

			for dir in sys.path :

				scriptPath = dir / path.with_suffix( ".gfr" )
				if scriptPath.is_file() :
					return self.__makeTaskBasedSuite( scriptPath, nodePath )

				dirPath = dir / path
				if dirPath.is_dir() :

					suite = unittest.TestSuite()
					for scriptPath in dirPath.glob( "**/*Test.gfr" ) :
						suite.addTest( self.__makeTaskBasedSuite( scriptPath, [] ) )

			nodePath.insert( 0, path.name )
			path = path.parent

		return None

	def __makeTaskBasedSuite( self, scriptPath, nodePath ) :

		script = Gaffer.ScriptNode()
		script["fileName"].setValue( scriptPath )
		script.load()

		if nodePath :
			node = script.descendant( ".".join( nodePath ) )
			if node is None :
				return None
		else :
			node = script

		suite = unittest.TestSuite()

		if isinstance( node, GafferDispatch.TaskNode ) :
			suite.addTest( _TestCase( script, node ) )
		else :
			for node in GafferDispatch.TaskNode.RecursiveRange( node ) :
				if node.getName().startswith( "test" ) :
					suite.addTest( _TestCase( script, node ) )

		return suite