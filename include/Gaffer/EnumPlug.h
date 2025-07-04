//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2011-2012, John Haddon. All rights reserved.
//  Copyright (c) 2013-2014, Image Engine Design Inc. All rights reserved.
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

#include "Gaffer/NumericPlug.h"

namespace Gaffer
{

class IECORE_EXPORT EnumPlug : public IntPlug
{

	public :

		GAFFER_PLUG_DECLARE_TYPE( EnumPlug, IntPlug );

		template<typename EnumType>
		explicit EnumPlug(
			const std::string &name = defaultName<NumericPlug>(),
			EnumType defaultValue,
			unsigned flags = Default
		);
		~EnumPlug() override;

		bool acceptsInput( const Plug *input ) const override;
		PlugPtr createCounterpart( const std::string &name, Direction direction ) const override;

		template<typename EnumType>
		void setValue( EnumType value );

		template<typename EnumType>
		EnumType getValue( const IECore::MurmurHash *precomputedHash = nullptr ) const;

	private :

		// using DataType = IECore::TypedData<T>;
		// using DataTypePtr = typename DataType::Ptr;

		// T m_minValue;
		// T m_maxValue;

};

using FloatPlug = NumericPlug<float>;
using IntPlug = NumericPlug<int>;

IE_CORE_DECLAREPTR( FloatPlug );
IE_CORE_DECLAREPTR( IntPlug );

} // namespace Gaffer

#include "Gaffer/NumericPlug.inl"
