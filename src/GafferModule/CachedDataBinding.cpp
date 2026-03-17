//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2019, Image Engine Design Inc. All rights reserved.
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

#include "boost/python.hpp"

#include "CachedDataBinding.h"

#include "GafferBindings/DependencyNodeBinding.h"

#include "Gaffer/CachedDataNode.h"

#include "fmt/format.h"

using namespace boost::python;
using namespace IECorePython;
using namespace Gaffer;
using namespace GafferBindings;

namespace
{

class CachedDataNodeSerialiser : public NodeSerialiser
{
	std::string constructor( const Gaffer::GraphComponent *graphComponent, Serialisation &serialisation ) const override
	{
		// TODO - is it safe to modify graph during serialization? Seems not?

		const CachedDataNode *node = IECore::runTimeCast<const CachedDataNode>( graphComponent );

		//const_cast<CachedDataNode*>( node )->save();
		IECore::ConstCompoundDataPtr files = node->save();

		//std::cerr << "TEST SERIAL : " << NodeSerialiser::constructor( graphComponent, serialisation ) << "\n";


		std::vector<std::string> fileTokens;
		for( const auto &it : files->readable() )
		{
			fileTokens.push_back( "\"" + it.first.string() + "\" : \"" + IECore::runTimeCast<IECore::StringData>( it.second.get() )->readable() + "\"" );
		}

		/*std::string mySerial = "Gaffer.CachedDataNode( \"";
		mySerial += node->getName();
		mySerial += "\", IECore.CompoundData( {"

		myS
		mySerial += "} ) )"*/

		std::string mySerial = fmt::format(
			"Gaffer.CachedDataNode( \"{}\", IECore.CompoundData( {{ {} }} ) )",
			node->getName().string(), fmt::join( fileTokens, ", " )
		);

		//std::cerr << "MY SERIAL : " << mySerial << "\n";

		return mySerial;
		//return NodeSerialiser::constructor( graphComponent, serialisation );
		/*std::string result = NodeSerialiser::constructor( graphComponent, serialisation );

		result += "\n" + identifier + ".load(" + node->currentFilePath() + ")\n";

		return result;*/
	}

	/*bool childNeedsSerialisation( const Gaffer::GraphComponent *child, const Serialisation &serialisation ) const override
    {
		const CachedDataNode *node = child->parent<CachedDataNode>();
        if( child == node->dataPlug() ) // || child == node->fileDataPlug() )
        {
            return false;
        }
        return NodeSerialiser::childNeedsSerialisation( child, serialisation );
    }

    std::string postConstructor( const Gaffer::GraphComponent *graphComponent, const std::string &identifier, Serialisation &serialisation ) const override
    {
		std::string result = NodeSerialiser::postConstructor( graphComponent, identifier, serialisation );

		//const CachedDataNode *node = IECore::runTimeCast<const CachedDataNode>( graphComponent );

		//result += identifier + ".load(\"" + node->currentFilePath() + "\")\n";

		return result;
    }*/

};

IECore::ObjectPtr getEntryWrapper( const CachedDataNode &cachedDataNode, const IECore::InternedString &key, bool throwExceptions )
{
	return cachedDataNode.getEntry( key, throwExceptions )->copy();
}

} // namespace

void GafferModule::bindCachedData()
{

	scope s = DependencyNodeClass<CachedDataNode>()
		.def( init<std::string, IECore::CompoundDataPtr>( ( arg( "name" )=GraphComponent::defaultName<CachedDataNode>(), arg( "storedFiles" ) = object() ) ) )
		.def( "save", &CachedDataNode::save )
		.def( "setEntry", &CachedDataNode::setEntry )
		.def( "getEntry", &getEntryWrapper )
		//.def( "currentFilePath", &CachedDataNode::currentFilePath )
	;

	Serialisation::registerSerialiser( Gaffer::CachedDataNode::staticTypeId(), new CachedDataNodeSerialiser );

}
