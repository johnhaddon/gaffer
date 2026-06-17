//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2026, Cinesite VFX Ltd. All rights reserved.
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

#include "GafferScene/OutputsQuery.h"

#include "IECoreScene/Output.h"

#include "IECore/VectorTypedData.h"

#include <algorithm>

using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

namespace
{

const std::string g_outputPrefix( "output:" );

} // namespace

GAFFER_NODE_DEFINE_TYPE( OutputsQuery );

size_t OutputsQuery::g_firstPlugIndex = 0;

OutputsQuery::OutputsQuery( const std::string &name )
	: ComputeNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new ScenePlug( "scene" ) );
	addChild( new StringVectorDataPlug( "fileNames", Plug::Out, new StringVectorData() ) );
}

OutputsQuery::~OutputsQuery()
{
}

ScenePlug *OutputsQuery::scenePlug()
{
	return getChild<ScenePlug>( g_firstPlugIndex );
}

const ScenePlug *OutputsQuery::scenePlug() const
{
	return getChild<ScenePlug>( g_firstPlugIndex );
}

StringVectorDataPlug *OutputsQuery::fileNamesPlug()
{
	return getChild<StringVectorDataPlug>( g_firstPlugIndex + 1 );
}

const StringVectorDataPlug *OutputsQuery::fileNamesPlug() const
{
	return getChild<StringVectorDataPlug>( g_firstPlugIndex + 1 );
}

void OutputsQuery::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	ComputeNode::affects( input, outputs );
	if( input == scenePlug()->globalsPlug() )
	{
		outputs.push_back( fileNamesPlug() );
	}
}

void OutputsQuery::hash( const ValuePlug *output, const Context *context, MurmurHash &h ) const
{
	ComputeNode::hash( output, context, h );
	if( output == fileNamesPlug() )
	{
		ScenePlug::GlobalScope globalScope( context );
		scenePlug()->globalsPlug()->hash( h );
	}
}

void OutputsQuery::compute( ValuePlug *output, const Context *context ) const
{
	if( output == fileNamesPlug() )
	{
		StringVectorDataPtr result = new StringVectorData();
		std::vector<std::string> &fileNames = result->writable();

		ScenePlug::GlobalScope globalScope( context );
		ConstCompoundObjectPtr globals = scenePlug()->globalsPlug()->getValue();

		for( const auto &[key, value] : globals->members() )
		{
			const std::string &k = key.string();
			if( k.compare( 0, g_outputPrefix.size(), g_outputPrefix ) == 0 )
			{
				if( const Output *sceneOutput = runTimeCast<const Output>( value.get() ) )
				{
					fileNames.push_back( sceneOutput->getName() );
				}
			}
		}

		std::sort( fileNames.begin(), fileNames.end() );

		static_cast<StringVectorDataPlug *>( output )->setValue( result );
		return;
	}

	ComputeNode::compute( output, context );
}
