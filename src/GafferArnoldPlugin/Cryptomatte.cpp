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

node_loader
{
   if (i > 0)
      return false;
   node->methods     = GafferCryptomatteMethods;
   node->output_type = AI_TYPE_RGB;
   node->name        = "GafferCryptomatte";
   node->node_type   = AI_NODE_SHADER;
   strcpy( node->version, AI_VERSION );
   return true;
}
