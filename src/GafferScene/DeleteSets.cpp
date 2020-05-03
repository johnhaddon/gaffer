//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, Image Engine Design Inc. All rights reserved.
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

#include "GafferScene/DeleteSets.h"

#include "Gaffer/StringPlug.h"

#include "IECore/StringAlgo.h"

using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

GAFFER_GRAPHCOMPONENT_DEFINE_TYPE( DeleteSets );

size_t DeleteSets::g_firstPlugIndex(0);

DeleteSets::DeleteSets( IECore::InternedString name )
	:	SceneProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "names" ) );
	addChild( new BoolPlug( "invertNames" ) );

	// Direct pass-through for stuff we don't touch.
	outPlug()->boundPlug()->setInput( inPlug()->boundPlug() );
	outPlug()->transformPlug()->setInput( inPlug()->transformPlug() );
	outPlug()->attributesPlug()->setInput( inPlug()->attributesPlug() );
	outPlug()->objectPlug()->setInput( inPlug()->objectPlug() );
	outPlug()->childNamesPlug()->setInput( inPlug()->childNamesPlug() );
	outPlug()->globalsPlug()->setInput( inPlug()->globalsPlug() );
}

DeleteSets::~DeleteSets()
{
}


Gaffer::StringPlug *DeleteSets::namesPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *DeleteSets::namesPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

Gaffer::BoolPlug *DeleteSets::invertNamesPlug()
{
	return getChild<BoolPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::BoolPlug *DeleteSets::invertNamesPlug() const
{
	return getChild<BoolPlug>( g_firstPlugIndex + 1 );
}

void DeleteSets::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneProcessor::affects( input, outputs );

	if( input == inPlug()->setNamesPlug() || input == namesPlug() || input == invertNamesPlug() )
	{
		outputs.push_back( outPlug()->setNamesPlug() );
	}
}

void DeleteSets::hashSetNames( const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	SceneProcessor::hashSetNames( context, parent, h );
	inPlug()->setNamesPlug()->hash( h );
	namesPlug()->hash( h );
	invertNamesPlug()->hash( h );
}

IECore::ConstInternedStringVectorDataPtr DeleteSets::computeSetNames( const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ConstInternedStringVectorDataPtr inputSetNamesData = inPlug()->setNamesPlug()->getValue();
	const std::vector<InternedString> &inputSetNames = inputSetNamesData->readable();
	if( inputSetNames.empty() )
	{
		return inputSetNamesData;
	}

	InternedStringVectorDataPtr outputSetNamesData = new InternedStringVectorData;
	std::vector<InternedString> &outputSetNames = outputSetNamesData->writable();

	const std::string names = namesPlug()->getValue();
	const bool invert = invertNamesPlug()->getValue();

	for( std::vector<InternedString>::const_iterator it = inputSetNames.begin(); it != inputSetNames.end(); ++it )
	{
		if( StringAlgo::matchMultiple( *it, names ) != (!invert) )
		{
			outputSetNames.push_back( *it );
		}
	}

	return outputSetNamesData;
}

void DeleteSets::hashSet( const IECore::InternedString &setName, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	const std::string names = namesPlug()->getValue();
	const bool invert = invertNamesPlug()->getValue();
	if( StringAlgo::matchMultiple( setName, names ) != (!invert) )
	{
		h = inPlug()->setPlug()->getValue()->Object::hash();
	}
	else
	{
		h = inPlug()->setPlug()->defaultValue()->Object::hash();
	}
}

IECore::ConstPathMatcherDataPtr DeleteSets::computeSet( const IECore::InternedString &setName, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	const std::string names = namesPlug()->getValue();
	const bool invert = invertNamesPlug()->getValue();
	if( StringAlgo::matchMultiple( setName, names ) != (!invert) )
	{
		return inPlug()->setPlug()->getValue();
	}
	else
	{
		return inPlug()->setPlug()->defaultValue();
	}
}
