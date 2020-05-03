//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
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

#include "GafferImage/Display.h"

#include "GafferImage/FormatPlug.h"

#include "Gaffer/Context.h"
#include "Gaffer/DirtyPropagationScope.h"
#include "Gaffer/ParallelAlgo.h"

#include "IECoreImage/DisplayDriver.h"

#include "IECore/BoxOps.h"
#include "IECore/MessageHandler.h"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind.hpp"
#include "boost/bind/placeholders.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/multi_array.hpp"

#include "tbb/spin_mutex.h"

#include <memory>

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferImage;

//////////////////////////////////////////////////////////////////////////
// Implementation of a DisplayDriver to support the node itself
//////////////////////////////////////////////////////////////////////////

namespace GafferImage
{

static const std::string g_headerPrefix = "header:";

class GafferDisplayDriver : public IECoreImage::DisplayDriver
{

	public :

		IE_CORE_DECLARERUNTIMETYPEDEXTENSION( GafferImage::GafferDisplayDriver, GafferDisplayDriverTypeId, DisplayDriver );

		GafferDisplayDriver( const Imath::Box2i &displayWindow, const Imath::Box2i &dataWindow,
			const vector<string> &channelNames, ConstCompoundDataPtr parameters )
			:	DisplayDriver( displayWindow, dataWindow, channelNames, parameters ),
				m_gafferFormat( displayWindow, 1, /* fromEXRSpace = */ true ),
				m_gafferDataWindow( m_gafferFormat.fromEXRSpace( dataWindow ) )
		{
			const V2i dataWindowMinTileIndex = ImagePlug::tileOrigin( m_gafferDataWindow.min ) / ImagePlug::tileSize();
			const V2i dataWindowMaxTileIndex = ImagePlug::tileOrigin( m_gafferDataWindow.max - Imath::V2i( 1 ) ) / ImagePlug::tileSize();

			m_tiles.resize(
				TileArray::extent_gen()
					[TileArray::extent_range( dataWindowMinTileIndex.x, dataWindowMaxTileIndex.x + 1 )]
					[TileArray::extent_range( dataWindowMinTileIndex.y, dataWindowMaxTileIndex.y + 1 )]
					[channelNames.size()]
			);

			m_parameters = parameters ? parameters->copy() : CompoundDataPtr( new CompoundData );
			CompoundDataPtr metadata = new CompoundData;
			for( const auto &p : m_parameters->readable() )
			{
				if( boost::starts_with( p.first.string(), g_headerPrefix ) )
				{
					metadata->writable()[p.first.string().substr( g_headerPrefix.size() )] = p.second;
				}
			}
			m_metadata = metadata;

			if( const FloatData *pixelAspect = m_parameters->member<FloatData>( "pixelAspect" ) )
			{
				/// \todo Give IECore::Display a Format rather than just
				/// a display window, then we won't need this workaround.
				m_gafferFormat.setPixelAspect( pixelAspect->readable() );
			}

			// This is a bit sketchy. By creating `Ptr( this )` we're adding a reference to ourselves from within
			// our own constructor - if that reference is dropped before we return, we'll be double deleted. We rely
			// on the fact that callOnUIThread() will keep us alive long enough for this not to occur.
			ParallelAlgo::callOnUIThread( boost::bind( &GafferDisplayDriver::emitDriverCreated, Ptr( this ), m_parameters ) );
		}

		GafferDisplayDriver( GafferDisplayDriver &other )
			:	DisplayDriver( other.displayWindow(), other.dataWindow(), other.channelNames(), other.parameters() ),
				m_gafferFormat( other.m_gafferFormat ), m_gafferDataWindow( other.m_gafferDataWindow ),
				m_parameters( other.m_parameters ), m_metadata( other.m_metadata )
		{
			// boost::multi_array has a joke assignment operator that only works
			// if you first resize the target of the assignment to match the
			// destination.
			m_tiles.resize(
				TileArray::extent_gen()
					[TileArray::extent_range( other.m_tiles.index_bases()[0], other.m_tiles.index_bases()[0] + other.m_tiles.shape()[0] )]
					[TileArray::extent_range( other.m_tiles.index_bases()[1], other.m_tiles.index_bases()[1] + other.m_tiles.shape()[1] )]
					[other.m_tiles.shape()[2]]
			);
			tbb::spin_rw_mutex::scoped_lock tileLock( other.m_tileMutex, /* write = */ false );
			m_tiles = other.m_tiles;
		}

		~GafferDisplayDriver() override
		{
		}

		const Format &gafferFormat() const
		{
			return m_gafferFormat;
		}

		const Box2i &gafferDataWindow() const
		{
			return m_gafferDataWindow;
		}

		const CompoundData *parameters() const
		{
			return m_parameters.get();
		}

		const CompoundData *metadata() const
		{
			return m_metadata.get();
		}

		void imageData( const Imath::Box2i &box, const float *data, size_t dataSize ) override
		{
			Box2i gafferBox = m_gafferFormat.fromEXRSpace( box );

			const V2i boxMinTileOrigin = ImagePlug::tileOrigin( gafferBox.min );
			const V2i boxMaxTileOrigin = ImagePlug::tileOrigin( gafferBox.max - Imath::V2i( 1 ) );
			for( int tileOriginY = boxMinTileOrigin.y; tileOriginY <= boxMaxTileOrigin.y; tileOriginY += ImagePlug::tileSize() )
			{
				for( int tileOriginX = boxMinTileOrigin.x; tileOriginX <= boxMaxTileOrigin.x; tileOriginX += ImagePlug::tileSize() )
				{
					for( int channelIndex = 0, numChannels = channelNames().size(); channelIndex < numChannels; ++channelIndex )
					{
						const V2i tileOrigin( tileOriginX, tileOriginY );
						ConstFloatVectorDataPtr tileData = getTile( tileOrigin, channelIndex );
						if( !tileData )
						{
							// we've been sent data outside of the data window
							continue;
						}

						// we must create a new object to hold the updated tile data,
						// because the old one might well have been returned from
						// computeChannelData and be being held in the cache.
						FloatVectorDataPtr updatedTileData = tileData->copy();
						vector<float> &updatedTile = updatedTileData->writable();

						const Box2i tileBound( tileOrigin, tileOrigin + Imath::V2i( GafferImage::ImagePlug::tileSize() ) );
						const Box2i transferBound = IECore::boxIntersection( tileBound, gafferBox );

						for( int y = transferBound.min.y; y<transferBound.max.y; ++y )
						{
							int srcY = m_gafferFormat.toEXRSpace( y );
							size_t srcIndex = ( ( srcY - box.min.y ) * ( box.size().x + 1 ) + ( transferBound.min.x - box.min.x ) ) * numChannels + channelIndex;
							size_t dstIndex = ( y - tileBound.min.y ) * ImagePlug::tileSize() + transferBound.min.x - tileBound.min.x;
							const size_t srcEndIndex = srcIndex + transferBound.size().x * numChannels;
							while( srcIndex < srcEndIndex )
							{
								updatedTile[dstIndex] = data[srcIndex];
								srcIndex += numChannels;
								dstIndex++;
							}
						}

						setTile( tileOrigin, channelIndex, updatedTileData );
					}
				}
			}

			dataReceivedSignal()( this, box );
		}

		void imageClose() override
		{
			imageReceivedSignal()( this );
		}

		bool scanLineOrderOnly() const override
		{
			return false;
		}

		bool acceptsRepeatedData() const override
		{
			return true;
		}

		ConstFloatVectorDataPtr channelData( const Imath::V2i &tileOrigin, const std::string &channelName )
		{
			vector<string>::const_iterator cIt = find( channelNames().begin(), channelNames().end(), channelName );
			if( cIt == channelNames().end() )
			{
				return ImagePlug::blackTile();
			}

			ConstFloatVectorDataPtr tile = getTile( tileOrigin, cIt - channelNames().begin() );
			if( tile )
			{
				return tile;
			}
			else
			{
				return ImagePlug::blackTile();
			}
		}

		typedef boost::signal<void ( GafferDisplayDriver *, const Imath::Box2i & )> DataReceivedSignal;
		DataReceivedSignal &dataReceivedSignal()
		{
			return m_dataReceivedSignal;
		}

		typedef boost::signal<void ( GafferDisplayDriver * )> ImageReceivedSignal;
		ImageReceivedSignal &imageReceivedSignal()
		{
			return m_imageReceivedSignal;
		}

	private :

		static const DisplayDriverDescription<GafferDisplayDriver> g_description;

		static void emitDriverCreated( Ptr driver, IECore::ConstCompoundDataPtr parameters )
		{
			Display::driverCreatedSignal()( driver.get(), parameters.get() );
		}

		ConstFloatVectorDataPtr getTile( const V2i &tileOrigin, size_t channelIndex )
		{
			V2i tileIndex = tileOrigin / ImagePlug::tileSize();

			if(
				tileIndex.x < m_tiles.index_bases()[0] ||
				tileIndex.x >= (int)(m_tiles.index_bases()[0] + m_tiles.shape()[0] ) ||
				tileIndex.y < m_tiles.index_bases()[1] ||
				tileIndex.y >= (int)(m_tiles.index_bases()[1] + m_tiles.shape()[1] )
			)
			{
				// outside data window
				return nullptr;
			}

			tbb::spin_rw_mutex::scoped_lock tileLock( m_tileMutex, false /* read */ );

			ConstFloatVectorDataPtr result = m_tiles[tileIndex.x][tileIndex.y][channelIndex];
			if( !result )
			{
				result = ImagePlug::blackTile();
			}

			return result;
		}

		void setTile( const V2i &tileOrigin, size_t channelIndex, ConstFloatVectorDataPtr tile )
		{
			V2i tileIndex = tileOrigin / ImagePlug::tileSize();
			tbb::spin_rw_mutex::scoped_lock tileLock( m_tileMutex, true /* write */ );
			m_tiles[tileIndex.x][tileIndex.y][channelIndex] = tile;
		}

		// indexed by tileIndexX, tileIndexY, channelIndex.
		typedef boost::multi_array<ConstFloatVectorDataPtr, 3> TileArray;
		TileArray m_tiles;
		tbb::spin_rw_mutex m_tileMutex;

		Format m_gafferFormat;
		Imath::Box2i m_gafferDataWindow;
		IECore::ConstCompoundDataPtr m_parameters;
		IECore::ConstCompoundDataPtr m_metadata;
		DataReceivedSignal m_dataReceivedSignal;
		ImageReceivedSignal m_imageReceivedSignal;

};

const IECoreImage::DisplayDriver::DisplayDriverDescription<GafferDisplayDriver> GafferDisplayDriver::g_description;

} // namespace GafferImage

//////////////////////////////////////////////////////////////////////////
// Implementation of the Display class itself
//////////////////////////////////////////////////////////////////////////

GAFFER_GRAPHCOMPONENT_DEFINE_TYPE( Display );

size_t Display::g_firstPlugIndex = 0;

Display::Display( IECore::InternedString name )
	:	ImageNode( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	// This plug is incremented when a new driver is set, triggering dirty signals
	// on all output plugs and prompting reevaluation in the viewer.
	addChild(
		new IntPlug(
			"__driverCount",
			Plug::In,
			0,
			0,
			Imath::limits<int>::max(),
			Plug::Default & ~Plug::Serialisable
		)
	);

	// This plug is incremented when new data is received, triggering dirty signals
	// on only the channel data plug and prompting reevaluation in the viewer.
	addChild(
		new IntPlug(
			"__channelDataCount",
			Plug::In,
			0,
			0,
			Imath::limits<int>::max(),
			Plug::Default & ~Plug::Serialisable
		)
	);
}

Display::~Display()
{
}

Gaffer::IntPlug *Display::driverCountPlug()
{
	return getChild<IntPlug>( g_firstPlugIndex );
}

const Gaffer::IntPlug *Display::driverCountPlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex );
}

Gaffer::IntPlug *Display::channelDataCountPlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::IntPlug *Display::channelDataCountPlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 1 );
}

void Display::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	ImageNode::affects( input, outputs );

	if( input == driverCountPlug() )
	{
		for( ValuePlugIterator it( outPlug() ); !it.done(); ++it )
		{
			outputs.push_back( it->get() );
		}
	}
	else if( input == channelDataCountPlug() )
	{
		outputs.push_back( outPlug()->channelDataPlug() );
	}
}

Display::DriverCreatedSignal &Display::driverCreatedSignal()
{
	static DriverCreatedSignal s;
	return s;
}

Node::UnaryPlugSignal &Display::imageReceivedSignal()
{
	static UnaryPlugSignal s;
	return s;
}

void Display::setDriver( IECoreImage::DisplayDriverPtr driver, bool copy )
{
	GafferDisplayDriver *gafferDisplayDriver = runTimeCast<GafferDisplayDriver>( driver.get() );
	if( !gafferDisplayDriver )
	{
		throw IECore::Exception( "Expected GafferDisplayDriver" );
	}

	setupDriver( copy ? new GafferDisplayDriver( *gafferDisplayDriver ) : gafferDisplayDriver );

	driverCountPlug()->setValue( driverCountPlug()->getValue() + 1 );
}

IECoreImage::DisplayDriver *Display::getDriver()
{
	return m_driver.get();
}

const IECoreImage::DisplayDriver *Display::getDriver() const
{
	return m_driver.get();
}

void Display::hashFormat( const GafferImage::ImagePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ImageNode::hashFormat( output, context, h );

	Format format;
	if( m_driver )
	{
		format = m_driver->gafferFormat();
	}
	else
	{
		format = FormatPlug::getDefaultFormat( Context::current() );
	}

	h.append( format.getDisplayWindow().min );
	h.append( format.getDisplayWindow().max );
	h.append( format.getPixelAspect() );
}

GafferImage::Format Display::computeFormat( const Gaffer::Context *context, const ImagePlug *parent ) const
{
	Format format;
	if( m_driver )
	{
		format = m_driver->gafferFormat();
	}
	else
	{
		format = FormatPlug::getDefaultFormat( context );
	}

	return format;
}

void Display::hashChannelNames( const GafferImage::ImagePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ImageNode::hashChannelNames( output, context, h );
	if( m_driver )
	{
		h.append(
			&(m_driver->channelNames()[0]),
			m_driver->channelNames().size()
		);
	}
}

IECore::ConstStringVectorDataPtr Display::computeChannelNames( const Gaffer::Context *context, const ImagePlug *parent ) const
{
	if( m_driver )
	{
		return new StringVectorData( m_driver->channelNames() );
	}
	return new StringVectorData();
}

void Display::hashDataWindow( const GafferImage::ImagePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ImageNode::hashDataWindow( output, context, h );
	Box2i dataWindow; // empty
	if( m_driver )
	{
		dataWindow = m_driver->gafferDataWindow();
	}
	h.append( dataWindow );
}

Imath::Box2i Display::computeDataWindow( const Gaffer::Context *context, const ImagePlug *parent ) const
{
	if( m_driver )
	{
		return m_driver->gafferDataWindow();
	}
	return Box2i();
}

void Display::hashMetadata( const GafferImage::ImagePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	const CompoundData *d = m_driver ? m_driver->metadata() : outPlug()->metadataPlug()->defaultValue();
	h = d->Object::hash();
}

IECore::ConstCompoundDataPtr Display::computeMetadata( const Gaffer::Context *context, const ImagePlug *parent ) const
{
	return m_driver ? m_driver->metadata() : outPlug()->metadataPlug()->defaultValue();
}

void Display::hashDeep( const GafferImage::ImagePlug *parent, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	h.append( false );
}

bool Display::computeDeep( const Gaffer::Context *context, const ImagePlug *parent ) const
{
	return false;
}

void Display::hashSampleOffsets( const GafferImage::ImagePlug *parent, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	h = ImagePlug::flatTileSampleOffsets()->Object::hash();
}

IECore::ConstIntVectorDataPtr Display::computeSampleOffsets( const Imath::V2i &tileOrigin, const Gaffer::Context *context, const ImagePlug *parent ) const
{
	return ImagePlug::flatTileSampleOffsets();
}

void Display::hashChannelData( const GafferImage::ImagePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ConstFloatVectorDataPtr channelData = ImagePlug::blackTile();
	if( m_driver )
	{
		channelData = m_driver->channelData(
			context->get<Imath::V2i>( ImagePlug::tileOriginContextName ),
			context->get<std::string>( ImagePlug::channelNameContextName )
		);
	}
	h = channelData->Object::hash();
}

IECore::ConstFloatVectorDataPtr Display::computeChannelData( const std::string &channelName, const Imath::V2i &tileOrigin, const Gaffer::Context *context, const ImagePlug *parent ) const
{
	ConstFloatVectorDataPtr channelData = ImagePlug::blackTile();
	if( m_driver )
	{
		channelData = m_driver->channelData(
			context->get<Imath::V2i>( ImagePlug::tileOriginContextName ),
			context->get<std::string>( ImagePlug::channelNameContextName )
		);
	}
	return channelData;
}

void Display::setupDriver( GafferDisplayDriverPtr driver )
{
	if( m_driver )
	{
		m_driver->dataReceivedSignal().disconnect( boost::bind( &Display::dataReceived, this) );
		m_driver->imageReceivedSignal().disconnect( boost::bind( &Display::imageReceived, this ) );
	}

	m_driver = driver;
	if( m_driver )
	{
		m_driver->dataReceivedSignal().connect( boost::bind( &Display::dataReceived, this ) );
		m_driver->imageReceivedSignal().connect( boost::bind( &Display::imageReceived, this ) );
	}
}

//////////////////////////////////////////////////////////////////////////
// Signalling and update mechanism
//////////////////////////////////////////////////////////////////////////

namespace
{

typedef set<PlugPtr> PlugSet;
typedef std::unique_ptr<PlugSet> PlugSetPtr;

struct PendingUpdates
{

	tbb::spin_mutex mutex;
	PlugSetPtr plugs;

};

PendingUpdates &pendingUpdates()
{
	static PendingUpdates *p = new PendingUpdates;
	return *p;
}

};

// Called on a background thread when data is received on the driver.
// We need to increment `channelDataCountPlug()`, but all graph edits must
// be performed on the UI thread, so we can't do it directly.
void Display::dataReceived()
{
	bool scheduleUpdate = false;
	{
		// To minimise overhead we perform updates in batches by storing
		// a set of plugs which are pending update. If we're the creator
		// of a new batch then we are responsible for scheduling a call
		// to `dataReceivedUI()` to process the batch. Otherwise we just
		// add to the current batch.
		PendingUpdates &pending = pendingUpdates();
		tbb::spin_mutex::scoped_lock lock( pending.mutex );
		if( !pending.plugs.get() )
		{
			scheduleUpdate = true;
			pending.plugs.reset( new PlugSet );
		}
		pending.plugs->insert( outPlug() );
	}
	if( scheduleUpdate )
	{
		ParallelAlgo::callOnUIThread( &Display::dataReceivedUI );
	}
}

// Called on the UI thread after being scheduled by `dataReceived()`.
void Display::dataReceivedUI()
{
	// Get the batch of plugs to trigger updates for. We want to hold
	// g_plugsPendingUpdateMutex for the shortest duration possible,
	// because it causes contention between the background rendering
	// thread and the UI thread, and can significantly affect performance.
	// We do this by "stealing" the current batch, so the background
	// thread will create a new batch and we are safe to iterate our
	// batch without holding the lock.
	PlugSetPtr batch;
	{
		PendingUpdates &pending = pendingUpdates();
		tbb::spin_mutex::scoped_lock lock( pending.mutex );
		batch.reset( pending.plugs.release() );
	}

	// Now increment the update count for the Display nodes
	// that have received data. This gives them a new hash
	// and also propagates dirtiness to the output image.
	{
		// Use a DirtyPropgationScope to batch up dirty propagation
		// for improved performance.
		DirtyPropagationScope dirtyPropagationScope;
		for( set<PlugPtr>::const_iterator it = batch->begin(), eIt = batch->end(); it != eIt; ++it )
		{
			PlugPtr plug = *it;
			// Because `dataReceivedUI()` is deferred to the UI thread,
			// it's possible that the node has actually been deleted by
			// the time we're called, so we must check.
			if( Display *display = runTimeCast<Display>( plug->node() ) )
			{
				display->channelDataCountPlug()->setValue( display->channelDataCountPlug()->getValue() + 1 );
			}
		}
	}
}

void Display::imageReceived()
{
	ParallelAlgo::callOnUIThread( boost::bind( &Display::imageReceivedUI, DisplayPtr( this ) ) );
}

void Display::imageReceivedUI( Ptr display )
{
	imageReceivedSignal()( display->outPlug() );
}
