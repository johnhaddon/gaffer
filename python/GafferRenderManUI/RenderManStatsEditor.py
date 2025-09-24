##########################################################################
#
#  Copyright (c) 2025, Cinesite VFX Ltd. All rights reserved.
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

import GafferUI

#import importlib

import os
import pathlib
import sys

# TODO : TIDY. CAN't DO THIS IN WRAPPER BECAUSE THE PATH WOULD COME BEFORE OUR OWN SITE-PACKAGES, AND PIXARS IS LITTERED WITH CONFLICTING STUFF
sys.path.append( str( pathlib.Path( os.environ["RMANTREE"] ) / "lib" / "python{}.{}".format( *sys.version_info[:2] ) / "site-packages" ) )

#from Qt import QtWidgets

class RenderManStatsEditor( GafferUI.Editor ) :

	def __init__( self, scriptNode, **kw ) :

		mainColumn = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Vertical, borderWidth = 4, spacing = 4 )

		GafferUI.Editor.__init__( self, mainColumn, scriptNode, **kw )

		from stportal.ui.live.mainwin import LiveStatsMainUI
		from stportal.core.datamanager import DataManager

		self.__manager = DataManager()

		mainColumn.append( GafferUI.Widget( LiveStatsMainUI( parent = None, manager = self.__manager ) ) )

	def __repr__( self ) :

		return "GafferRenderManUI.RenderManStatsEditor( scriptNode )"

GafferUI.Editor.registerType( "RenderManStats", RenderManStatsEditor )
