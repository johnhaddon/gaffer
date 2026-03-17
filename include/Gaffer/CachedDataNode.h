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

#pragma once

#include "Gaffer/ComputeNode.h"
#include "Gaffer/Spreadsheet.h"

#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"

#include "IECore/MurmurHash.h"

#include "boost/algorithm/string/replace.hpp"

#include "tbb/spin_rw_mutex.h"

namespace Gaffer
{

class GAFFER_API CachedDataNode : public ComputeNode
{

	public :

		explicit CachedDataNode( const std::string &name=defaultName<ComputeNode>(), IECore::CompoundDataPtr storedFiles = nullptr );
		~CachedDataNode() override;

		GAFFER_NODE_DECLARE_TYPE( Gaffer::CachedDataNode, CachedDataNodeTypeId, ComputeNode );

		StringPlug *selectorPlug();
		const StringPlug *selectorPlug() const;

		StringPlug *targetDirectoryPlug();
		const StringPlug *targetDirectoryPlug() const;

		AtomicCompoundDataPlug *dataPlug();
		const AtomicCompoundDataPlug *dataPlug() const;

		StringVectorDataPlug *keysPlug();
		const StringVectorDataPlug *keysPlug() const;


		//StringPlug *currentFilePathPlug() const;
		//const StringPlug *currentFilePathPlug() const;

		// TODO - should this be private? Currently the serialiser childNeedsSerialisation needs to see it
		/*AtomicCompoundDataPlug *fileDataPlug();
		const AtomicCompoundDataPlug *fileDataPlug() const;*/

		//void load( const std::string &filePath );
		IECore::ConstCompoundDataPtr save() const;

		// TODO - custom UI to display this, so you can tell what's happening?
		//std::string currentFilePath() const;

		virtual void affects( const Plug *input, AffectedPlugsContainer &outputs ) const override;

		/*class SingleCacheNode : public ComputeNode
		{
		public :

			explicit SingleCacheNode( const std::string &name=defaultName<SingleCacheNode>() );
			~SingleCacheNode() override;

			GAFFER_NODE_DECLARE_TYPE( Gaffer::CachedDataNode::SingleCacheNode, SingleCacheNodeTypeId, ComputeNode );

			AtomicCompoundDataPlug *liveDataPlug();
			const AtomicCompoundDataPlug *liveDataPlug() const;

			StringPlug *filePathPlug();
			const StringPlug *filePathPlug() const;

			AtomicCompoundDataPlug *dataPlug();
			const AtomicCompoundDataPlug *dataPlug() const;

			void affects( const Plug *input, AffectedPlugsContainer &outputs ) const override;

		protected :

			void hash( const ValuePlug *output, const Context *context, IECore::MurmurHash &h ) const override;
			void compute( ValuePlug *output, const Context *context ) const override;

		private :

			static size_t g_firstPlugIndex;
		};*/

		// TODO - should probably be private, with enabledRowNames exposed
		/*Spreadsheet *spreadsheetNode();
		const Spreadsheet *spreadsheetNode() const;

		SingleCacheNode *singleCacheNode();
		const SingleCacheNode *singleCacheNode() const;*/

		void setEntry( const IECore::InternedString &key, IECore::ConstObjectPtr value );

		// TODO throwExceptions naming/spelling
		IECore::ConstObjectPtr getEntry( const IECore::InternedString &key, bool throwExceptions = true ) const;

	protected :
		//bool acceptsInput( const Plug *plug, const Plug *inputPlug ) const override;
		void hash( const ValuePlug *output, const Context *context, IECore::MurmurHash &h ) const override;
		void compute( ValuePlug *output, const Context *context ) const override;

	private :

		IntPlug *refreshCountPlug();
		const IntPlug *refreshCountPlug() const;

		// Evaluate the cache - used by getEntry, and the output plugs
		ObjectPlug *evaluatePlug();
		const ObjectPlug *evaluatePlug() const;

		mutable tbb::spin_rw_mutex m_mutex;

		mutable std::map<IECore::InternedString, IECore::ConstObjectPtr> m_liveData;
		//IECore::CompoundObjectPtr m_liveData;

		struct CacheFile
		{
			std::string path;
			IECore::MurmurHash hash;
		};
		mutable std::map<IECore::InternedString, CacheFile> m_cacheFiles;

		/*static std::map<IECore::InternedString, CacheFile> filePathMapFromCompoundData(
			const IECore::CompoundData *storedFiles
		);*/

		// TODO
		//mutable std::string m_currentFilePath;

		void plugSet( Plug *plug );

        static size_t g_firstPlugIndex;

};

IE_CORE_DECLAREPTR( CachedDataNode );

} // namespace Gaffer
