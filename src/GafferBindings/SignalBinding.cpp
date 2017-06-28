//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2011, Image Engine Design Inc. All rights reserved.
//  Copyright (c) 2012, John Haddon. All rights reserved.
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
#include "boost/signals2.hpp"

#include "IECorePython/ScopedGILLock.h"

#include "GafferBindings/SignalBinding.h"

using namespace boost::python;
using namespace GafferBindings;

namespace
{

// A wrapper class presenting the boost::signal::slot_call_iterator
// as an iterable range in python.
template<typename SlotCallIterator>
struct SlotCallRange
{

	SlotCallRange( const SlotCallRange &other )
		: current( other.current), last( other.last )
	{
	}

	SlotCallRange( SlotCallIterator f, SlotCallIterator l )
		:	current( f ), last( l )
	{
	}

	SlotCallRange &iter()
	{
		return *this;
	}

	object next()
	{
		if( current == last )
		{
			PyErr_SetString( PyExc_StopIteration, "No more results" );
        	throw_error_already_set();
		}
		else
		{
			object result = *current;
			++current;
			return result;
		}
		return object(); // shouldn't get here
	}

	SlotCallIterator current;
	SlotCallIterator last;

};

// A little wrapper class allowing a python callable to be used as a result
// combiner for a signal.
struct PythonResultCombiner
{

	typedef object result_type;

	PythonResultCombiner()
		:	combiner( object() )
	{
	}

	PythonResultCombiner( object c )
		:	combiner( c )
	{
	}

	template<typename SlotCallIterator>
	result_type operator()( SlotCallIterator first, SlotCallIterator last ) const
	{
		if( !combiner )
		{
			// no custom python combiner, so just emulate default behaviour
			object result;
			while( first != last )
			{
				result = *first;
				++first;
			}
			return result;
		}
		else
		{
			// we have a custom combiner, so use that
			IECorePython::ScopedGILLock gilLock;
			SlotCallRange<SlotCallIterator> range( first, last );
			return combiner( range );
		}
	}

	object combiner;

};

template<typename Signal>
Signal *construct( object combiner )
{
	return new Signal( PythonResultCombiner( combiner ) );
}

template<typename Signal>
void bind( const char *name )
{

	// bind using the standard SignalClass, and add a constructor allowing a custom
	// result combiner to be passed.
	scope s = SignalClass<Signal>( name )
		.def( "__init__", make_constructor( &construct<Signal>, default_call_policies() ) )
	;

	// bind the appropriate result range type so the custom result combiner can get the slot results.
	typedef SlotCallRange<typename Signal::slot_call_iterator> Range;
	boost::python::class_<Range>( "__SignalResultRange", no_init )
		.def( "__iter__", &Range::iter, return_self<>() )
		.def( "next", &Range::next )
	;

}

} // namespace

namespace GafferBindings
{

namespace Detail
{

template<>
void extractSlotResult( boost::python::object o )
{
	if( o.ptr() != Py_None )
	{
		PyErr_SetString( PyExc_TypeError, "Expected None" );
		throw_error_already_set();
	}
}

boost::python::object pythonConnection( const boost::signals2::connection &connection, bool scoped )
{
	if( scoped )
	{
		// Simply returning `object( scoped_connection( connection ) )`
		// doesn't work - somehow the scoped_connection dies and the
		// connection is disconnected before we get into python. So
		// we construct via the python-bound copy constructor which
		// avoids the problem.
		PyTypeObject *type = boost::python::converter::registry::query(
			boost::python::type_info( typeid( boost::signals2::scoped_connection ) )
		)->get_class_object();

		boost::python::object oType( boost::python::handle<>( boost::python::borrowed( type ) ) );
		return oType( boost::python::object( connection ) );
	}
	else
	{
		return boost::python::object( connection );
	}
}

} // namespace Detail

void bindSignal()
{

	typedef boost::signals2::signal<object (), PythonResultCombiner > Signal0;
	typedef boost::signals2::signal<object ( object ), PythonResultCombiner > Signal1;
	typedef boost::signals2::signal<object ( object, object ), PythonResultCombiner > Signal2;
	typedef boost::signals2::signal<object ( object, object, object ), PythonResultCombiner > Signal3;

	bind<Signal0>( "Signal0" );
	bind<Signal1>( "Signal1" );
	bind<Signal2>( "Signal2" );
	bind<Signal3>( "Signal3" );

}

} // namespace GafferBindings
