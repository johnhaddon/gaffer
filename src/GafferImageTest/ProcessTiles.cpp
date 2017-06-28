//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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

#include "boost/bind.hpp"

#include "Gaffer/Node.h"

#include "GafferImage/ImagePlug.h"
#include "GafferImage/ImageAlgo.h"
#include "GafferImageTest/ProcessTiles.h"

using namespace std;
using namespace IECore;
using namespace Gaffer;
using namespace GafferImage;

namespace
{

struct TilesEvaluateFunctor
{
	bool operator()( const GafferImage::ImagePlug *imagePlug, const std::string &channelName, const Imath::V2i &tileOrigin )
	{
		imagePlug->channelDataPlug()->getValue();
		return true;
	}
};

void processTilesOnDirty( const Gaffer::Plug *dirtiedPlug, ConstImagePlugPtr image )
{
	if( dirtiedPlug == image.get() )
	{
		GafferImageTest::processTiles( image.get() );
	}
}

} // namespace

namespace GafferImageTest
{

void processTiles( const GafferImage::ImagePlug *imagePlug )
{
	TilesEvaluateFunctor f;
	ImageAlgo::parallelProcessTiles( imagePlug, imagePlug->channelNamesPlug()->getValue()->readable(), f );
}

boost::signals2::connection connectProcessTilesToPlugDirtiedSignal( GafferImage::ConstImagePlugPtr image )
{
	const Node *node = image->node();
	if( !node )
	{
		throw IECore::Exception( "Plug does not belong to a node." );
	}

	return const_cast<Node *>( node )->plugDirtiedSignal().connect( boost::bind( &processTilesOnDirty, ::_1, image ) );
}

} // namespace GafferImageTest
