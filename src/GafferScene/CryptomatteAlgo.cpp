//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) John Haddon. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//	  * Redistributions of source code must retain the above
//		copyright notice, this list of conditions and the following
//		disclaimer.
//
//	  * Redistributions in binary form must reproduce the above
//		copyright notice, this list of conditions and the following
//		disclaimer in the documentation and/or other materials provided with
//		the distribution.
//
//	  * Neither the name of John Haddon nor the names of
//		any other contributors to this software may be used to endorse or
//		promote products derived from this software without specific prior
//		written permission.
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

#include "GafferScene/CryptomatteAlgo.h"

#include "boost/format.hpp"

using namespace GafferScene;

namespace
{

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

inline uint32_t rotl32( uint32_t x, int8_t r )
{
	return (x << r) | (x >> (32 - r));
}

inline uint32_t fmix( uint32_t h )
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

uint32_t MurmurHash3_x86_32( const void *key, size_t len, uint32_t seed )
{
	const uint8_t *data = (const uint8_t *)key;
	const size_t nblocks = len / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	/* body */

	const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

	for( size_t i = -nblocks; i; i++ )
	{
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = rotl32( k1, 15 );
		k1 *= c2;

		h1 ^= k1;
		h1 = rotl32( h1, 13 );
		h1 = h1 * 5 + 0xe6546b64;
	}

	/* tail */

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

	uint32_t k1 = 0;

	switch( len & 3 )
	{
	case 3:
		k1 ^= tail[2] << 16;
	case 2:
		k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];
		k1 *= c1;
		k1 = rotl32( k1, 15 );
		k1 *= c2;
		h1 ^= k1;
	}

	/* finalization */

	h1 ^= len;

	h1 = fmix( h1 );

	return h1;
}

//-----------------------------------------------------------------------------

} // namespace

float CryptomatteAlgo::hash( const boost::string_view &s )
{
	uint32_t h = MurmurHash3_x86_32( s.data(), s.size(), 0 );
	// Taken from the Cryptomatte specification :
	//
	// https://github.com/Psyop/Cryptomatte/blob/master/specification/cryptomatte_specification.pdf
	//
	// If all exponent bits are 0 (subnormals, +zero, -zero) set exponent to 1.
	// If all exponent bits are 1 (NaNs, +inf, -inf) set exponent to 254.
	const uint32_t exponent = h >> 23 & 255;
	if( exponent == 0 || exponent == 255 )
	{
		h ^= 1 << 23; // toggle bit
	}
	float result;
	std::memcpy( &result, &h, sizeof( uint32_t ) );
	return result;
}

std::string CryptomatteAlgo::metadataPrefix( const std::string &layer )
{
	return "cryptomatte/" + boost::str(
		boost::format( "%08x" ) % MurmurHash3_x86_32( layer.data(), layer.size(), 0 )
	).substr( 0, 7 ) + "/";
}
