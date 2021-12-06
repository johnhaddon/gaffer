//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2021, John Haddon. All rights reserved.
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

#include "IECore/Export.h"

#include <ai.h>

AI_SHADER_NODE_EXPORT_METHODS( GafferCryptomatteMethods );

enum class Parameters
{
   ObjectAOV
};

node_parameters
{
   AiParameterStr( "objectAOV", "crypto_object" );
}

node_initialize
{
}

node_update
{
}

node_finish
{
}

shader_evaluate
{

   if( sg->Rt & AI_RAY_CAMERA && sg->sc == AI_CONTEXT_SURFACE )
   {
      float hash = 0.5;
        //CryptomatteData* data = reinterpret_cast<CryptomatteData*>(AiNodeGetLocalData(node));
        //data->do_cryptomattes(sg);
      AiAOVSetFlt( sg, AiShaderEvalParamStr( (int)Parameters::ObjectAOV ), hash );
      sg->out.RGBA() = AI_RGBA_ZERO;
   }
   sg->out.RGBA() = AI_RGBA_ZERO;
}

IECORE_EXPORT void loadCryptomatteNode( AtNodeLib *node  )
{
   node->methods = GafferCryptomatteMethods;
   node->output_type = AI_TYPE_RGB;
   node->name = "GafferCryptomatte";
   node->node_type = AI_NODE_SHADER;
   strcpy( node->version, AI_VERSION );
}