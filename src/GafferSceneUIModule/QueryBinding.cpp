//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2021, Cinesite VFX Ltd. All rights reserved.
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

#include "QueryBinding.h"

#include "GafferSceneUI/AttributeQueryUI.h"

#include "IECorePython/ScopedGILRelease.h"

namespace
{
	std::string setupMenuTitle()
	{
		return GafferSceneUI::AttributeQueryUI::setupMenuTitle();
	}

	boost::python::list setupMenuNames()
	{
		const std::vector< std::string >& names = GafferSceneUI::AttributeQueryUI::setupMenuNames();

		boost::python::list list;

		for( std::vector< std::string >::const_iterator it = names.begin(), itEnd = names.end(); it != itEnd; ++it )
		{
			list.append( *it );
		}

		return list;
	}

	bool setupFromMenuName( GafferScene::AttributeQuery& query, const std::string& name )
	{
		IECorePython::ScopedGILRelease gilRelease;
		return GafferSceneUI::AttributeQueryUI::setupFromMenuName( query, name );
	}

} // namespace

void GafferSceneUIModule::bindQueries()
{
	boost::python::def( "__setupMenuTitle", & setupMenuTitle );
	boost::python::def( "__setupMenuNames", & setupMenuNames );
	boost::python::def( "__setupFromMenuName", & setupFromMenuName );
}
