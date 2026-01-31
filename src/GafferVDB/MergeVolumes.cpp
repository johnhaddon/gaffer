//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2026, John Haddon. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "GafferVDB/MergeVolumes.h"

#include "IECoreVDB/VDBObject.h"
#include "IECore/NullObject.h"

#include "openvdb/tools/Merge.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace IECoreScene;
using namespace IECoreVDB;
using namespace Gaffer;
using namespace GafferScene;
using namespace GafferVDB;

// template<typename GridOrTreeT>
// void
// csgUnion(GridOrTreeT& a, GridOrTreeT& b, bool prune, bool pruneCancelledTiles)
// {
//     using Adapter = TreeAdapter<GridOrTreeT>;
//     using TreeT = typename Adapter::TreeType;
//     TreeT &aTree = Adapter::tree(a), &bTree = Adapter::tree(b);
//     composite::validateLevelSet(aTree, "A");
//     composite::validateLevelSet(bTree, "B");
//     CsgUnionOp<TreeT> op(bTree, Steal());
//     op.setPruneCancelledTiles(prune && pruneCancelledTiles);
//     tree::DynamicNodeManager<TreeT> nodeManager(aTree);
//     nodeManager.foreachTopDown(op);
//     if (prune) tools::pruneLevelSet(aTree);
// }

namespace
{

VDBObjectPtr mergeVolumes( const vector<const VDBObject *> &volumes )
{
	// HOUDINI CODE USES DEQUE, BUT IT'S NOT NECESSARY
	//std::deque<tools::TreeToMerge<TreeT>> trees;

	return nullptr;
}

} // namespace

GAFFER_NODE_DEFINE_TYPE( MergeVolumes );

size_t MergeVolumes::g_firstPlugIndex = 0;

MergeVolumes::MergeVolumes( const std::string &name )
	:	MergeObjects( name, "/mergedVolume" )
{
	storeIndexOfNextChild( g_firstPlugIndex );
}

MergeVolumes::~MergeVolumes()
{
}

IECore::ConstObjectPtr MergeVolumes::computeMergedObject( const std::vector<std::pair<IECore::ConstObjectPtr, Imath::M44f>> &sources, const Gaffer::Context *context ) const
{
	vector<const VDBObject *> vdbObjects;
	for( const auto &[object, transform] : sources )
	{
		const auto vdbObject = IECore::runTimeCast<const VDBObject>( object.get() );
		if( !vdbObject )
		{
			continue;
		}
		vdbObjects.push_back( vdbObject );
	}

	if( vdbObjects.empty() )
	{
		return IECore::NullObject::defaultNullObject();
	}
	else if( vdbObjects.size() == 1 )
	{
		return vdbObjects.front();
	}

	return mergeVolumes( vdbObjects );
}
