//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2026, Image Engine Design Inc. All rights reserved.
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

#include "Gaffer/CachedDataNode.h"

#include "Gaffer/ScriptNode.h"
#include "Gaffer/ValuePlug.h"

#include "IECore/NullObject.h"
#include "IECore/FileIndexedIO.h"

#include "boost/bind/bind.hpp"

using namespace Gaffer;

namespace {

static const IECore::InternedString g_cacheEvaluationKeyName( "__cacheEvaluationKey" );

} // namespace

GAFFER_NODE_DEFINE_TYPE( CachedDataNode );

size_t CachedDataNode::g_firstPlugIndex = 0;

CachedDataNode::CachedDataNode( const std::string &name, IECore::CompoundDataPtr storedFiles )
	:	ComputeNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

    addChild( new StringPlug( "selector", Plug::In ) );
    addChild( new StringPlug( "targetDirectory", Plug::In, "TODO${fileName}" ) );
    addChild( new AtomicCompoundDataPlug( "data", Plug::Out ) );
    addChild( new StringVectorDataPlug( "keys", Plug::Out ) );
    addChild( new IntPlug( "refreshCount", Plug::In ) );
    addChild( new ObjectPlug( "evaluate", Plug::Out, new IECore::NullObject() ) );

	if( storedFiles )
	{
		for( const auto &it : storedFiles->readable() )
		{
			IECore::StringData *stringVal = IECore::runTimeCast<IECore::StringData>( it.second.get() );
			if( !stringVal )
			{
				throw IECore::Exception( "BAD STRINGVAL TODO" );
			}
			m_cacheFiles[ it.first ] = { stringVal->readable(), IECore::MurmurHash() };

			//std::cerr << "FILE MAP " << it.first << " : " << stringVal->readable() << "\n";
		}
	}

	plugSetSignal().connect( boost::bind( &CachedDataNode::plugSet, this, std::placeholders::_1 ) );
}

CachedDataNode::~CachedDataNode()
{
}

StringPlug *CachedDataNode::selectorPlug()
{
    return getChild<StringPlug>( g_firstPlugIndex + 0 );
}

const StringPlug *CachedDataNode::selectorPlug() const
{
    return getChild<StringPlug>( g_firstPlugIndex + 0 );
}

StringPlug *CachedDataNode::targetDirectoryPlug()
{
    return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const StringPlug *CachedDataNode::targetDirectoryPlug() const
{
    return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

AtomicCompoundDataPlug *CachedDataNode::dataPlug()
{
    return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 2 );
}

const AtomicCompoundDataPlug *CachedDataNode::dataPlug() const
{
    return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 2 );
}

StringVectorDataPlug *CachedDataNode::keysPlug()
{
    return getChild<StringVectorDataPlug>( g_firstPlugIndex + 3 );
}

const StringVectorDataPlug *CachedDataNode::keysPlug() const
{
    return getChild<StringVectorDataPlug>( g_firstPlugIndex + 3 );
}

IntPlug *CachedDataNode::refreshCountPlug()
{
    return getChild<IntPlug>( g_firstPlugIndex + 4 );
}

const IntPlug *CachedDataNode::refreshCountPlug() const
{
    return getChild<IntPlug>( g_firstPlugIndex + 4 );
}

ObjectPlug *CachedDataNode::evaluatePlug()
{
    return getChild<ObjectPlug>( g_firstPlugIndex + 5 );
}

const ObjectPlug *CachedDataNode::evaluatePlug() const
{
    return getChild<ObjectPlug>( g_firstPlugIndex + 5 );
}

/*void CachedDataNode::load( const std::string &filePath )
{
	m_currentFilePath = filePath;
	if( !filePath.size() )
	{
		// No data saved yet
		return;
	}

	IECore::ObjectReader reader( filePath );
	IECore::ConstCompoundDataPtr data = IECore::runTimeCast<const IECore::CompoundData>( reader.read() );
	dataPlug()->setValue( data );
	//fileDataPlug()->setValue( data );
}*/

IECore::ConstCompoundDataPtr CachedDataNode::save() const
{
	// TODO - weird things happen if exceptions occur during serialization

	if( m_liveData.size() )
	{
		const ScriptNode *scriptNode = ancestor<ScriptNode>();
		Context::EditableScope scope( scriptNode->context() );
		std::string fileName = std::filesystem::path( scriptNode->fileNamePlug()->getValue() ).stem();
		scope.set( "fileName", &fileName );

		tbb::spin_rw_mutex::scoped_lock lock( m_mutex, /* write = */ true );
		//std::cerr << "SAVE SIZE " << m_liveData.size() << "\n";
		const std::string targetDir = Context::current()->substitute( targetDirectoryPlug()->getValue(), targetDirectoryPlug()->substitutions() );
		std::filesystem::create_directories( targetDir );

		for( const auto &it : m_liveData )
		{
			std::string key = it.first;
			boost::replace_all( key, "/", "_" );
			//scope.set( "key", &key );

			std::string targetPath = targetDir + "/" + key + ".io";

			// \todo : The "Exclusive" flag to FileIndexedIO is undocumented, and does not appear to do anything.
			// Currently using it to match IECore::ObjectWriter, but long term it should probably be removed
			// from IECore.
			IECore::FileIndexedIOPtr file = new IECore::FileIndexedIO( targetPath, IECore::IndexedIO::rootPath, IECore::IndexedIO::Exclusive | IECore::IndexedIO::Write);

			const IECore::Object* outObj = const_cast<IECore::Object*>( it.second.get() );
			IECore::MurmurHash outHash = outObj->hash();

			IECore::UInt64VectorDataPtr hashData = new IECore::UInt64VectorData( { outHash.h1(), outHash.h2() } );
			//((IECore::Object*)hashData.get())->save( file, "hash" );
			hashData->Object::save( file, "hash" );
			outObj->save( file, "object" );

			m_cacheFiles[it.first] = { targetPath, outHash };
		}

		m_liveData.clear();
	}

	// TODO - do we keep this as part of save, or should save return void, and this be a separate function
	// to get the cache file map as a CompoundData?
	IECore::CompoundDataPtr resultData = new IECore::CompoundData;
	auto &result = resultData->writable();
	for( const auto &it : m_cacheFiles )
	{
		result[it.first] = new IECore::StringData( it.second.path );
	}

	return resultData;
	//m_liveData->members()[key] = const_cast<IECore::Object*>( value.get() );
	/*if( m_currentFilePath != "" )
	{
		// We're already sync'ed to disk
		return;
	}

	std::string targetPath;
	{
		const ScriptNode *scriptNode = ancestor<ScriptNode>();
		Context::EditableScope scope( scriptNode->context() );
		std::string fileName = std::filesystem::path( scriptNode->fileNamePlug()->getValue() ).stem();
		scope.set( "fileName", &fileName );
		targetPath = Context::current()->substitute( targetFilePathPlug()->getValue(), targetFilePathPlug()->substitutions() );
	}
	if( !targetPath.size() )
	{
		throw IECore::Exception( "TODO" );
	}

	IECore::ConstCompoundDataPtr data = dataPlug()->getValue();
	// TODO - ensure path exists

	// TODO - I don't know if this const_cast is safe. Why wouldn't writer take a const?
	IECore::ObjectWriter writer( const_cast<IECore::CompoundData*>( data.get() ), targetPath );
	writer.write();

	m_currentFilePath = targetPath;*/
}

/*std::string CachedDataNode::currentFilePath() const
{
	return m_currentFilePath;
}*/

void CachedDataNode::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	ComputeNode::affects( input, outputs );

	if(
		input == refreshCountPlug()
	)
	{
		outputs.push_back( evaluatePlug() );
	}

	if(
		input == refreshCountPlug()
	)
	{
		outputs.push_back( keysPlug() );
	}

	if(
		input == evaluatePlug()
	)
	{
		outputs.push_back( dataPlug() );
	}
}

void CachedDataNode::setEntry( const IECore::InternedString &key, IECore::ConstObjectPtr value )
{
	tbb::spin_rw_mutex::scoped_lock lock( m_mutex, /* write = */ true );
	if( !m_liveData.count( key ) )
	{
		// If we haven't got an override yet for this key, then there could be a substantial benefit
		// in checking if this value matches the value coming from file, so we don't need to override it.

		auto cacheFileIt = m_cacheFiles.find( key );
		if( cacheFileIt != m_cacheFiles.end() )
		{
			if( cacheFileIt->second.hash == IECore::MurmurHash() )
			{
				IECore::FileIndexedIOPtr file = new IECore::FileIndexedIO( cacheFileIt->second.path, IECore::IndexedIO::rootPath, IECore::IndexedIO::Read );
				IECore::UInt64VectorDataPtr hashData = IECore::runTimeCast<IECore::UInt64VectorData>( IECore::Object::load( file, "hash" ) );
				const auto &hashVec = hashData->readable();
				cacheFileIt->second.hash = IECore::MurmurHash( hashVec[0], hashVec[1] );
			}

			if( value->hash() == cacheFileIt->second.hash )
			{
				// We're trying to set this entry to the same value that's already stored in the file.
				// We can skip this set.
				return;
			}
		}
	}
	m_liveData[key] = value;
	refreshCountPlug()->setValue( refreshCountPlug()->getValue() + 1 );
}

IECore::ConstObjectPtr CachedDataNode::getEntry( const IECore::InternedString &key, bool throwExceptions ) const
{
	try
	{
		// We use an evaluation plug to provide the getEntry() functionality - this means we can
		// use Gaffer's default plug cache to ensure that we don't load a file twice if it is
		// queried both via getEntry and via an output plug.
		Context::EditableScope s( Context::current() );
		s.set( g_cacheEvaluationKeyName, &key );
		return evaluatePlug()->getValue();
	}
	catch( ... )
	{
		if( throwExceptions )
		{
			throw;
		}
		else
		{
			return nullptr;
		}
	}
}

void CachedDataNode::hash( const ValuePlug *output, const Context *context, IECore::MurmurHash &h ) const
{
    if( output == evaluatePlug() )
    {
        ComputeNode::hash( output, context, h );

		const IECore::InternedString &key = context->get<IECore::InternedString>( g_cacheEvaluationKeyName );

		tbb::spin_rw_mutex::scoped_lock lock( m_mutex, /* write = */ false );
		auto liveIt = m_liveData.find( key );
		if( liveIt != m_liveData.end() )
		{
			liveIt->second->hash( h );
			return;
		}

		auto cacheFileIt = m_cacheFiles.find( key );
		if( cacheFileIt != m_cacheFiles.end() )
		{
			if( cacheFileIt->second.hash != IECore::MurmurHash() )
			{
				// If we have a hash value, using it is the most accurate option
				h.append( cacheFileIt->second.hash );
			}
			else
			{
				// If we don't have a hash value, just hashing in the file path should be safe,
				// since the file on disk should not have changed yet.
				h.append( cacheFileIt->second.path );
			}
			return;
		}

		throw IECore::Exception( "Unknown key: " + key.string() );
    }
	else if( output == dataPlug() )
	{
		Context::EditableScope s( context );
		IECore::InternedString select = selectorPlug()->getValue();
		s.set( g_cacheEvaluationKeyName, &select );

		// TODO
		h = IECore::MurmurHash();
		evaluatePlug()->hash( h );
	}
    else if( output == keysPlug() )
    {
        ComputeNode::hash( output, context, h );

		tbb::spin_rw_mutex::scoped_lock lock( m_mutex, /* write = */ false );
		for( const auto &i : m_liveData )
		{
			h.append( i.first );
		}

		for( const auto &i : m_cacheFiles )
		{
			h.append( i.first );
		}
	}


    ComputeNode::hash( output, context, h );
}

void CachedDataNode::compute( ValuePlug *output, const Context *context ) const
{
    if( output == evaluatePlug() )
    {
		IECore::ConstObjectPtr result;
		const IECore::InternedString &key = context->get<IECore::InternedString>( g_cacheEvaluationKeyName );

		tbb::spin_rw_mutex::scoped_lock lock( m_mutex, /* write = */ false );
		auto liveIt = m_liveData.find( key );
		if( liveIt != m_liveData.end() )
		{
			//std::cerr << "EVAL " << key.string() << " from live\n";
			result = liveIt->second;
		}

		if( !result )
		{
			auto cacheFileIt = m_cacheFiles.find( key );
			if( cacheFileIt != m_cacheFiles.end() )
			{
				IECore::FileIndexedIOPtr file = new IECore::FileIndexedIO( cacheFileIt->second.path, IECore::IndexedIO::rootPath, IECore::IndexedIO::Read );
				result = IECore::Object::load( file, "object" );
				//std::cerr << "EVAL " << key.string() << " from " << cacheFileIt->second.path << "\n";
				/*IECore::UInt64VectorDataPtr hashData = IECore::runTimeCast<IECore::UInt64VectorData>( IECore::Object::load( file, "hash" ) );
				const auto &hashVec = hashData->readable();
				IECore::MurmurHash h( hashVec[0], hashVec[1] );
				std::cerr << "HASH " << h << "\n";*/
				/*static_cast<AtomicCompoundDataPlug *>( output )->setValue(
					IECore::runTimeCast<const IECore::CompoundData>( reader.read() )
				);*/
			}
		}

		if( !result )
		{
			throw IECore::Exception( "Unknown key: " + key.string() );
		}

		static_cast<ObjectPlug *>( output )->setValue( result );
		return;

	}
    else if( output == dataPlug() )
    {
		Context::EditableScope s( context );
		IECore::InternedString select = selectorPlug()->getValue();
		s.set( g_cacheEvaluationKeyName, &select );
		static_cast<AtomicCompoundDataPlug *>( output )->setValue(
			IECore::runTimeCast<const IECore::CompoundData>( evaluatePlug()->getValue() )
		);
		/*static_cast<AtomicCompoundDataPlug *>( output )->setValue(
			IECore::runTimeCast<const IECore::CompoundData>( getEntry( select ) )
		);*/
		return;
    }
    else if( output == keysPlug() )
    {
		std::vector<std::string> keys;

		tbb::spin_rw_mutex::scoped_lock lock( m_mutex, /* write = */ false );
		for( const auto &i : m_liveData )
		{
			keys.push_back( i.first );
		}

		for( const auto &i : m_cacheFiles )
		{
			keys.push_back( i.first );
		}

		std::sort( keys.begin(), keys.end() );
		auto last = std::unique( keys.begin(), keys.end() );
		keys.erase( last, keys.end() );


		static_cast<StringVectorDataPlug *>( output )->setValue(
			new IECore::StringVectorData( std::move( keys ) )
		);
		return;
    }

    ComputeNode::compute( output, context );
}

/*bool CachedDataNode::acceptsInput( const Plug *plug, const Plug *inputPlug ) const
{
	if( plug == dataPlug() )
	{
		return false;
	}

	return DependencyNode::acceptsInput( plug, inputPlug );
}*/

void CachedDataNode::plugSet( Plug *plug )
{
	/*if( plug != dataPlug() )
	{
		return;
	}*/

	//m_currentFilePath = "";
	/*if( m_currentFilePath == "" )
	{
		// We already have no file association
		return;
	}*/

	/*IECore::ConstCompoundDataPtr newData = dataPlug()->getValue();
	IECore::ConstCompoundDataPtr oldData = fileDataPlug()->getValue();
	if( newData == oldData )
	{
		// TODO - is this check actually beneficial, or would it be simpler and more predictable
		// to reset the file association on any plugSet?
		return;
	}

	currentFilePathPlug()->setValue( "" );
	fileDataPlug()->setValue( nullptr );
	*/
}

/*std::map<IECore::InternedString, CachedDataNode::CacheFile> CachedDataNode::filePathMapFromCompoundData( const IECore::CompoundData *storedFiles )
{
	std::map<IECore::InternedString, CacheFile> result;
	if( storedFiles )
	{
		for( const auto &it : storedFiles->readable() )
		{
			IECore::StringData *stringVal = IECore::runTimeCast<IECore::StringData>( it.second.get() );
			if( !stringVal )
			{
				throw IECore::Exception( "BAD STRINGVAL TODO" );
			}
			result[ it.first ] = { stringVal->readable(), IECore::MurmurHash() };

			std::cerr << "FILE MAP " << it.first << " : " << stringVal->readable() << "\n";
		}
	}
	return result;
}*/
