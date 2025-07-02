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

import GafferSceneUI
import GafferVDB

import IECoreVDB

import imath

import functools

## \todo Cache results of properties such as min/max which
# we are currently computing from the grid each time.

def __grid( objectPlug, gridName ) :

	o = objectPlug.getValue()
	return o.findGrid( gridName ) if isinstance( o, IECoreVDB.VDBObject ) else None

def __valueType( objectPlug, gridName ) :

	grid = __grid( objectPlug, gridName )
	return grid.valueTypeName if grid is not None else None

def __convertValue( value ) :

	if isinstance( value, tuple ) :
		if isinstance( value[0], int ) :
			if len( value ) == 3 :
				return imath.V3i( *value )
			elif len( value ) == 2 :
				return imath.V2i( *value )
		elif isinstance( value[0], float ) :
			if len( value ) == 3 :
				return imath.V3f( *value )
			elif len( value ) == 2 :
				return imath.V2f( *value )

	return value

def __minValue( objectPlug, gridName ) :

	grid = __grid( objectPlug, gridName )
	return __convertValue( grid.evalMinMax()[0] ) if grid is not None else None

def __maxValue( objectPlug, gridName ) :

	grid = __grid( objectPlug, gridName )
	return __convertValue( grid.evalMinMax()[1] ) if grid is not None else None

def __activeVoxels( objectPlug, gridName ) :

	grid = __grid( objectPlug, gridName )
	return grid.activeVoxelCount() if grid is not None else None

def __voxelBound( objectPlug, gridName ) :

	grid = __grid( objectPlug, gridName )
	if grid is None :
		return None

	bound = grid.evalActiveVoxelBoundingBox()
	return imath.Box3i( __convertValue( bound[0] ), __convertValue( bound[1] ) )

def __memoryUsage( objectPlug, gridName ) :

	grid = __grid( objectPlug, gridName )
	return grid.memUsage() if grid is not None else None

def __metadata( objectPlug, gridName, metadataName ) :

	grid = __grid( objectPlug, gridName )
	metadata = grid.metadata.get( metadataName ) if grid is not None else None
	return __convertValue( metadata ) if metadata is not None else None

def __vdbInspectors( scene, editScope ) :

	result = {}

	vdb = scene["object"].getValue()
	if not isinstance( vdb, IECoreVDB.VDBObject ) :
		return result

	for gridName in sorted( vdb.gridNames() ) :

		result[f"Grids/{gridName}/Value Type"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __valueType, gridName = gridName ) )
		result[f"Grids/{gridName}/Min Value"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __minValue, gridName = gridName ) )
		result[f"Grids/{gridName}/Max Value"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __maxValue, gridName = gridName ) )
		result[f"Grids/{gridName}/Active Voxels"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __activeVoxels, gridName = gridName ) )
		result[f"Grids/{gridName}/Voxel Bound"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __voxelBound, gridName = gridName ) )
		result[f"Grids/{gridName}/Memory Usage"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __memoryUsage, gridName = gridName ) )

		metadata = vdb.findGrid( gridName ).metadata
		for key in sorted( metadata.keys() ) :
			result[f"Grids/{gridName}/Metadata/{key}"] = GafferSceneUI.Private.BasicInspector( scene["object"], editScope, functools.partial( __metadata, gridName = gridName, metadataName = key ) )

	return result

GafferSceneUI.SceneInspector.registerInspectors( "Selection/Object", __vdbInspectors )
