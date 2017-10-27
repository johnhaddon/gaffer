##########################################################################
#
#  Copyright (c) 2017, Image Engine Design Inc. All rights reserved.
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

import math

import Gaffer

Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "type", "spot" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "coneAngleParameter", "coneAngle" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "penumbraAngleParameter", "penumbraAngle" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "penumbraType", "outset" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "intensityParameter", "intensity" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "exposureParameter", "exposure" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/spotLight", "colorParameter", "i_color" )

Gaffer.Metadata.registerValue( "osl:light:maya/osl/pointLight", "type", "point" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/pointLight", "intensityParameter", "intensity" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/pointLight", "exposureParameter", "exposure" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/pointLight", "colorParameter", "i_color" )

Gaffer.Metadata.registerValue( "osl:light:maya/osl/distantLight", "type", "distant" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/distantLight", "intensityParameter", "intensity" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/distantLight", "exposureParameter", "exposure" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/distantLight", "colorParameter", "i_color" )

Gaffer.Metadata.registerValue( "osl:light:maya/osl/environmentLight", "type", "environment" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/environmentLight", "textureNameParameter", "image" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/environmentLight", "intensityParameter", "intensity" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/environmentLight", "exposureParameter", "exposure" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/environmentLight", "colorParameter", "tint" )
Gaffer.Metadata.registerValue( "osl:light:maya/osl/environmentLight", "visualiserOrientation", IECore.M44f().rotate( IECore.V3f( 0, math.pi, 0 ) ) )

