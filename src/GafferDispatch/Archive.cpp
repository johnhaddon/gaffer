//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2025, Cinesite VFX Ltd. All rights reserved.
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

#include "GafferDispatch/Archive.h"

extern "C"
{
#include "archive.h"
#include "archive_entry.h"
}

#include <filesystem>
#include <fstream>

namespace
{

void handleArchiveError( archive *a, int error )
{
	switch( error )
	{
		case ARCHIVE_OK :
			return;
		case ARCHIVE_WARN :
			msg( IECore::Msg::Level::Warning, "Archive", archive_error_string( a ) );
			return;
		default :
			throw IECore::Exception( archive_error_string( a ) );
	}
}

} // namespace

using namespace std;
using namespace IECore;
using namespace Gaffer;
using namespace GafferDispatch;

GAFFER_NODE_DEFINE_TYPE( Archive );

size_t Archive::g_firstPlugIndex = 0;

Archive::Archive( const std::string &name )
	:	TaskNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringVectorDataPlug( "files" ) );
	addChild( new StringPlug( "archive" ) );
}

Archive::~Archive()
{
}

Gaffer::StringVectorDataPlug *Archive::filesPlug()
{
	return getChild<StringVectorDataPlug>( g_firstPlugIndex );
}

const Gaffer::StringVectorDataPlug *Archive::filesPlug() const
{
	return getChild<StringVectorDataPlug>( g_firstPlugIndex );
}

Gaffer::StringPlug *Archive::archivePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::StringPlug *Archive::archivePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

IECore::MurmurHash Archive::hash( const Gaffer::Context *context ) const
{
	ConstStringVectorDataPtr filesData = filesPlug()->getValue();
	const std::string archive = archivePlug()->getValue();
	if( filesData->readable().empty() || archive.empty() )
	{
		return IECore::MurmurHash();
	}

	IECore::MurmurHash h = TaskNode::hash( context );
	filesData->hash( h );
	h.append( archive );
	return h;
}

void Archive::execute() const
{
	ConstStringVectorDataPtr filesData = filesPlug()->getValue();
	filesystem::path archivePath = archivePlug()->getValue();
	if( archivePath.empty() )
	{
		return;
	}

	using ArchivePtr = std::unique_ptr<archive, decltype( &archive_free )>;
	ArchivePtr writer( archive_write_new(), archive_free );
	//ArchivePtr reader( archive_read_disk_new(), archive_free );

	int error =	archive_write_set_format_filter_by_ext( writer.get(), archivePath.c_str() );
	handleArchiveError( writer.get(), error );
	error = archive_write_open_filename( writer.get(), archivePath.c_str() );
	handleArchiveError( writer.get(), error );

	using ArchiveEntryPtr = std::unique_ptr<archive_entry, decltype( &archive_entry_free )>;
	ArchiveEntryPtr entry( archive_entry_new(), archive_entry_free );

	//char buff[8192];
	struct stat st;

	for( const auto &file : filesData->readable() )
	{
		//archive_entry_set_pathname_utf8( entry.get(), file.c_str() );
		//error = archive_read_disk_entry_from_file( reader.get(), entry.get(), 0, nullptr );
		//handleArchiveError( reader.get(), error );

		 stat(file.c_str(), &st);
		 archive_entry_copy_stat( entry.get(), &st );

		const string fileName = filesystem::path( file ).filename();
		archive_entry_set_pathname_utf8( entry.get(), fileName.c_str() );

		std::cerr << "FILE " << file << std::endl;
		std::cerr << "SIZE " << archive_entry_size( entry.get() ) << std::endl;

		error = archive_write_header( writer.get(), entry.get() );
		handleArchiveError( writer.get(), error );

		std::ifstream f( file.c_str(), std::ios::binary );
		f.exceptions(std::ios::failbit | std::ios::badbit);
		f.seekg(0, std::ios::end);
		std::streamsize n = f.tellg();
		f.seekg(0, std::ios::beg);
		std::vector<char> data(static_cast<std::size_t>(n));
		f.read(data.data(), n);

		archive_write_data( writer.get(), data.data(), data.size() );

		// auto fd = open( file.c_str(), O_RDONLY );
   		// len = read( fd, buff, sizeof( buff ) );
		// while( len > 0 ) {
		// 	archive_write_data( a, buff, len );
		// 	len = read(fd, buff, sizeof(buff) );
		// }
    	// close( fd );

		archive_entry_clear( entry.get() );
	}

	error = archive_write_close( writer.get() );
	handleArchiveError( writer.get(), error );

	// filesystem::path destinationPath( destination );
	// filesystem::create_directories( destinationPath );

	// const bool deleteSource = deleteSourcePlug()->getValue();
	// const bool overwrite = overwritePlug()->getValue();

	// filesystem::copy_options options = filesystem::copy_options::recursive;
	// if( overwrite )
	// {
	// 	options |= filesystem::copy_options::overwrite_existing;
	// }

	// for( const auto &file : filesData->readable() )
	// {
	// 	const filesystem::path filePath = file;
	// 	const filesystem::path destinationFilePath = destinationPath / filePath.filename();
	// 	if( deleteSource && ( overwrite || !filesystem::exists( destinationFilePath ) ) )
	// 	{
	// 		try
	// 		{
	// 			// If we can simply rename the file, then that will be faster.
	// 			std::filesystem::rename( file, destinationFilePath );
	// 			continue;
	// 		}
	// 		catch( const filesystem::filesystem_error &e )
	// 		{
	// 			// Couldn't rename. This could be because source and destination
	// 			// are on different filesystems, in which case we suppress the
	// 			// exception and fall through to copy/remove which should succeed.
	// 			// Or it could be due to another problem such as permissions, in
	// 			// which case we still suppress and fall through, expecting to
	// 			// throw again.
	// 		}
	// 	}
	// 	filesystem::copy( filePath, destinationFilePath, options );
	// 	if( deleteSource )
	// 	{
	// 		std::filesystem::remove_all( filePath );
	// 	}
	// }

}
