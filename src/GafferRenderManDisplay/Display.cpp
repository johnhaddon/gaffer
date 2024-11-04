//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2010-2013, Image Engine Design Inc. All rights reserved.
//  Copyright (c) 2023, Cinesite VFX Ltd. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
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

#include "IECoreImage/DisplayDriver.h"

#include "IECore/MessageHandler.h"
#include "IECore/SimpleTypedData.h"
#include "IECore/VectorTypedData.h"

#include "ndspy.h"

#include "fmt/format.h"

#include <vector>

using namespace std;
using namespace Imath;
using namespace IECore;

/// TODO : THIS IS PRACTICALLY IDENTICAL TO THE 3DELIGHT DRIVER. DO WE TRY TO SHARE THE CODE?
/// MAYBE WE CAN COMPILE THE SAME FILE INTO DIFFERENT LIBRARIES?

extern "C"
{

// Implementation
// ==============

PtDspyError DspyImageOpen( PtDspyImageHandle *image, const char *driverName, const char *fileName, int width, int height, int paramcount, const UserParameter *parameters, int formatCount, PtDspyDevFormat *format, PtFlagStuff *flags )
{
	*image = nullptr;

	// get channel names

	vector<string> channels;

	if( formatCount == 1 )
	{
		channels.push_back( "R" );
	}
	else if( formatCount == 3 )
	{
		channels.push_back( "R" );
		channels.push_back( "G" );
		channels.push_back( "B" );
	}
	else if( formatCount == 4 )
	{
		channels.push_back( "R" );
		channels.push_back( "G" );
		channels.push_back( "B" );
		channels.push_back( "A" );
	}
	else
	{
		msg( Msg::Error, "Dspy::imageOpen", "Invalid number of channels!" );
		return PkDspyErrorBadParams;
	}
	for( int i = 0; i < formatCount; i++ )
	{
		format[i].type = PkDspyFloat32 | PkDspyByteOrderNative;
	}

	// Process the parameter list. We use some of the parameters to help determine
	// the display and data windows, and the others we convert ready to passed to
	// `DisplayDriver::create()`.

	V2i originalSize( width, height );
	V2i origin( 0 );

	CompoundDataPtr convertedParameters = new CompoundData;

	for( int p = 0; p < paramcount; p++ )
	{
		std::cerr << "P " << p << " " << parameters[p].name << std::endl;

		if ( !strcmp( parameters[p].name, "OriginalSize" ) && parameters[p].vtype == (char)'i' && parameters[p].vcount == (char)2 && parameters[p].nbytes == (int) (parameters[p].vcount * sizeof(int)) )
		{
			originalSize.x = static_cast<const int *>(parameters[p].value)[0];
			originalSize.y = static_cast<const int *>(parameters[p].value)[1];
		}
		else if ( !strcmp( parameters[p].name, "origin" ) && parameters[p].vtype == (char)'i' && parameters[p].vcount == (char)2 && parameters[p].nbytes == (int)(parameters[p].vcount * sizeof(int)) )
		{
			origin.x = static_cast<const int *>(parameters[p].value)[0];
			origin.y = static_cast<const int *>(parameters[p].value)[1];
		}
		else if( 0 == strcmp( parameters[p].name, "layername" ) && parameters[p].vtype == 's' )
		{
			const string layerName = *(const char **)(parameters[p].value);
			std::cerr << "LAYER NAME " << layerName << std::endl;
			if( !layerName.empty() )
			{
				if( channels.size() == 1 )
				{
					// I'm not sure what the semantics of 3Delight's `layername`
					// actually are, but this gets the naming matching Arnold
					// for our all-important OutputBuffer outputs used in the
					// Viewer.
					/// \todo We're overdue a reckoning were we define our own
					/// standard semantics for all the little details of outputs,
					/// and implement them to match across all renderers.
					channels[0] = layerName;
				}
				else
				{
					for( auto &channel : channels )
					{
						channel = layerName + "." + channel;
					}
				}
			}
		}
		else
		{
			DataPtr newParam;

			if ( !parameters[p].nbytes )
			{
				continue;
			}

			const int *pInt;
			const float *pFloat;
			char const **pChar;

			// generic converter
			switch( parameters[p].vtype )
			{
			case 'i':
				// sanity check
				if ( parameters[p].nbytes / parameters[p].vcount != sizeof(int) )
				{
					msg( Msg::Error, "Dspy::imageOpen", "Invalid int data size" );
					continue;
				}
				pInt = static_cast<const int *>(parameters[p].value);
				if ( parameters[p].vcount == 1 )
				{
					newParam = new IntData( pInt[0] );
				}
				else
				{
					std::vector< int > newVec( pInt, pInt + parameters[p].vcount );
					newParam = new IntVectorData( newVec );
				}
				break;
			case 'f':
				if ( parameters[p].nbytes / parameters[p].vcount != sizeof(float) )
				{
					msg( Msg::Error, "Dspy::imageOpen", "Invalid float data size" );
					continue;
				}
				pFloat = static_cast<const float *>(parameters[p].value);
				if ( parameters[p].vcount == 1 )
				{
					newParam = new FloatData( pFloat[0] );
				}
				else
				{
					std::vector< float > newVec( pFloat, pFloat + parameters[p].vcount );
					newParam = new FloatVectorData( newVec );
				}
				break;
			case 's':
				pChar = (const char **)(parameters[p].value);
				if ( parameters[p].vcount == 1 )
				{
					newParam = new StringData( pChar[0] );
					std::cerr << "   " << pChar[0] << std::endl;
				}
				else
				{
					StringVectorDataPtr newStringVec = new StringVectorData();
					for ( int s = 0; s < parameters[p].vcount; s++ )
					{
						newStringVec->writable().push_back( pChar[s] );
						std::cerr << "   " << pChar[s] << std::endl;
					}
					newParam = newStringVec;
				}
				break;
			default :
				// We shouldn't ever get here...
				break;
			}
			if( newParam )
			{
				convertedParameters->writable()[ parameters[p].name ] = newParam;
			}
		}
	}

	convertedParameters->writable()[ "fileName" ] = new StringData( fileName );

	// Calculate display and data windows

	Box2i displayWindow(
		V2i( 0 ),
		originalSize - V2i( 1 )
	);

	Box2i dataWindow(
		origin,
		origin + V2i( width - 1, height - 1)
	);

	// Create the display driver

	IECoreImage::DisplayDriverPtr dd = nullptr;
	try
	{
		const StringData *driverType = convertedParameters->member<StringData>( "driverType", true /* throw if missing */ );
		dd = IECoreImage::DisplayDriver::create( driverType->readable(), displayWindow, dataWindow, channels, convertedParameters );
	}
	catch( std::exception &e )
	{
		msg( Msg::Error, "Dspy::imageOpen", e.what() );
		return PkDspyErrorUnsupported;
	}

	if( !dd )
	{
		msg( Msg::Error, "Dspy::imageOpen", "DisplayDriver::create returned 0." );
		return PkDspyErrorUnsupported;
	}

	// Update flags and return

	if( dd->scanLineOrderOnly() )
	{
		flags->flags |= PkDspyFlagsWantsScanLineOrder;
	}

	dd->addRef(); // This will be removed in imageClose()
	*image = (PtDspyImageHandle)dd.get();
	return PkDspyErrorNone;

}


PtDspyError DspyImageQuery( PtDspyImageHandle image, PtDspyQueryType type, int size, void *data )
{
	IECoreImage::DisplayDriver *dd = static_cast<IECoreImage::DisplayDriver *>( image );

	if( type == PkRedrawQuery )
	{
		if( (!dd->scanLineOrderOnly()) && dd->acceptsRepeatedData() )
		{
			((PtDspyRedrawInfo *)data)->redraw = 1;
		}
		else
		{
			((PtDspyRedrawInfo *)data)->redraw = 0;
		}
		return PkDspyErrorNone;
	}

	// TODO : 3Delight Only. Is there a PRMan equivalent?
	//
	// if( type == PkProgressiveQuery )
	// {
	// 	if( (!dd->scanLineOrderOnly()) && dd->acceptsRepeatedData() )
	// 	{
	// 		((PtDspyProgressiveInfo *)data)->acceptProgressive = 1;
	// 	}
	// 	else
	// 	{
	// 		((PtDspyProgressiveInfo *)data)->acceptProgressive = 0;
	// 	}
	// 	return PkDspyErrorNone;
	// }

	return PkDspyErrorUnsupported;
}

PtDspyError DspyImageData( PtDspyImageHandle image, int xMin, int xMaxPlusOne, int yMin, int yMaxPlusOne, int entrySize, const unsigned char *data )
{
	IECoreImage::DisplayDriver *dd = static_cast<IECoreImage::DisplayDriver *>( image );
	Box2i dataWindow = dd->dataWindow();

	// Convert coordinates from cropped image to original image coordinates.
	Box2i box( V2i( xMin + dataWindow.min.x, yMin + dataWindow.min.y ), V2i( xMaxPlusOne - 1 + dataWindow.min.x, yMaxPlusOne - 1 + dataWindow.min.y ) );
	int channels = dd->channelNames().size();
	int blockSize = (xMaxPlusOne - xMin) * (yMaxPlusOne - yMin);
	int bufferSize = channels * blockSize;

	if( entrySize % sizeof(float) )
	{
		msg( Msg::Error, "Dspy::imageData", "The entry size is not multiple of sizeof(float)!" );
		return PkDspyErrorUnsupported;
	}

	const float *buffer;
	vector<float> bufferStorage;

	// TODO : integer ID support

	if( entrySize == (int)(channels*sizeof(float)) )
	{
		// This is the case we like - we can just send the data as-is.
		buffer = (const float *)data;
	}
	else
	{
		// PRMan seems to pad pixels sometimes for unknown reasons, and we need
		// to unpad them before sending. This is a pity.
		/// \todo Figure out why this is happening, and see if we can avoid it.
		bufferStorage.reserve( bufferSize );
		auto source = (const float *)data;
		const size_t stride = entrySize / sizeof( float );
		for( int i = 0; i < blockSize; ++i )
		{
			for( int c = 0; c < channels; ++c )
			{
				bufferStorage.push_back( source[c] );
			}
			source += stride;
		}
		buffer = bufferStorage.data();
	}

	try
	{
		dd->imageData( box, buffer, bufferSize );
	}
	catch( std::exception &e )
	{
		if( strcmp( e.what(), "stop" ) == 0 )
		{
			/// \todo Is this even used?
			return PkDspyErrorUndefined;
		}
		else
		{
			msg( Msg::Error, "Dspy::imageData", e.what() );
			return PkDspyErrorUndefined;
		}
	}

	return PkDspyErrorNone;
}

PtDspyError DspyImageClose( PtDspyImageHandle image )
{
	if ( !image )
	{
		return PkDspyErrorNone;
	}

	IECoreImage::DisplayDriver *dd = static_cast<IECoreImage::DisplayDriver*>( image );
	try
	{
		dd->imageClose();
	}
	catch( std::exception &e )
	{
		msg( Msg::Error, "Dspy::imageClose", e.what() );
	}

	try
	{
		dd->removeRef();
	}
	catch( std::exception &e )
	{
		msg( Msg::Error, "DspyImageData", e.what() );
		return PkDspyErrorBadParams;
	}

	return PkDspyErrorNone;
}

} // extern "C"
