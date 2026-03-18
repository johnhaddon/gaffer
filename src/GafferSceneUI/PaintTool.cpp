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

// TODO - check copyright notices

#include "GafferSceneUI/PaintTool.h"

#include "GafferSceneUI/SceneView.h"
#include "GafferSceneUI/ScriptNodeAlgo.h"

#include "GafferUI/Pointer.h"
#include "GafferUI/Style.h"

#include "GafferScene/AimConstraint.h"
#include "GafferScene/EditScopeAlgo.h"
//#include "GafferScene/Group.h"
//#include "GafferScene/ObjectSource.h"
#include "GafferScene/PrimitiveVariablePaint.h"
#include "GafferScene/SceneAlgo.h"
//#include "GafferScene/SceneReader.h"
//#include "GafferScene/Transform.h"

#include "Gaffer/Animation.h"
#include "Gaffer/Metadata.h"
#include "Gaffer/MetadataAlgo.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/Spreadsheet.h"

#include "IECoreScene/MeshAlgo.h"
#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/MeshPrimitiveEvaluator.h"

#include "IECore/AngleConversion.h"
#include "IECore/DataAlgo.h"
#include "IECore/LRUCache.h"

#include "IECoreGL/Buffer.h"
#include "IECoreGL/CachedConverter.h"
#include "IECoreGL/MeshPrimitive.h"
#include "IECoreGL/CurvesPrimitive.h"
#include "IECoreGL/ShaderLoader.h"

#include "Imath/ImathMatrixAlgo.h"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/bind/bind.hpp"
#include "boost/unordered_map.hpp"

#include "fmt/format.h"

#include <memory>
#include <unordered_set>
#include <numeric> // TODO

using namespace std;
using namespace boost::placeholders;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferUI;
using namespace GafferScene;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

typedef std::vector<typename V2fTree::Iterator> UglyArray;

CompoundDataPtr newPrimVarComposite( IECore::TypeId variableType, size_t numVerts, bool includeOpacity = true )
{
	CompoundDataPtr result = new CompoundData();

	if( includeOpacity )
	{
		FloatVectorDataPtr opacityData = new FloatVectorData();
		opacityData->writable().resize( numVerts, 0.0f );
		result->writable()["opacity"] = opacityData;
	}

	if( variableType == FloatVectorData::staticTypeId() )
	{
		FloatVectorDataPtr valueData = new FloatVectorData();
		valueData->writable().resize( numVerts, 0.0f );
		result->writable()["value"] = valueData;
	}
	else
	{
		Color3fVectorDataPtr valueData = new Color3fVectorData();
		valueData->writable().resize( numVerts, Color3f( 0.0f ) );
		result->writable()["value"] = valueData;
	}

	return result;
}

// TODO - liking the idea of this taking care of allocating result? Needs flag for whether or not to build opacity
void applyPrimVarComposite( const CompoundData *a, const CompoundData *b, int mode, float opacity, CompoundData *result )
{
	const FloatVectorData *bOpacityData = b->member< FloatVectorData >( "opacity" );
	const vector<float> &bOpacity = bOpacityData->readable();
	FloatVectorData *resultOpacityData = result->member< FloatVectorData >( "opacity" );

	const Data *aValueUntypedData = a->member< Data >( "value" );
	if( !aValueUntypedData )
	{
		// TODO erase mode
		// TODO - does copyFrom preserve COW?
		result->member< Data >( "value" )->Object::copyFrom( b->member<Data>( "value" ) );
	}
	else
	{
		IECore::TypeId typeId = aValueUntypedData->typeId();
		// TODO - template, support more types
		if( typeId == FloatVectorData::staticTypeId() )
		{
			const std::vector<float> &aValue = static_cast<const FloatVectorData*>( aValueUntypedData )->readable();
			const FloatVectorData *bValueData = b->member< FloatVectorData >( "value" );
			if( !bValueData )
			{
				throw IECore::Exception( "TODO Q" );
			}
			const std::vector<float> &bValue = bValueData->readable();

			FloatVectorData *resultValueData = result->member< FloatVectorData >( "value" );
			std::vector<float> &resultValue = resultValueData->writable();

			if( mode == 0 )
			{
				for( size_t i = 0; i < resultValue.size(); i++ )
				{
					resultValue[i] = ( 1.0f - bOpacity[i] * opacity ) * aValue[i];
				}
			}
			else
			{
				for( size_t i = 0; i < resultValue.size(); i++ )
				{
					resultValue[i] = ( 1.0f - bOpacity[i] * opacity ) * aValue[i] + bValue[i] * opacity;
				}
			}
		}
		else
		{
			const std::vector<Color3f> &aValue = static_cast<const Color3fVectorData*>( aValueUntypedData )->readable();
			const Color3fVectorData *bValueData = b->member< Color3fVectorData >( "value" );
			if( !bValueData )
			{
				throw IECore::Exception( "TODO QQ" );
			}
			const std::vector<Color3f> &bValue = bValueData->readable();

			Color3fVectorData *resultValueData = result->member< Color3fVectorData >( "value" );
			std::vector<Color3f> &resultValue = resultValueData->writable();

			if( mode == 0 )
			{
				for( size_t i = 0; i < resultValue.size(); i++ )
				{
					resultValue[i] = ( 1.0f - bOpacity[i] * opacity ) * aValue[i];
				}
			}
			else
			{
				for( size_t i = 0; i < resultValue.size(); i++ )
				{
					resultValue[i] = ( 1.0f - bOpacity[i] * opacity ) * aValue[i] + bValue[i] * opacity;
				}
			}
		}
	}

	if( !resultOpacityData )
	{
		return;
	}

	const FloatVectorData *aOpacityData = a->member< FloatVectorData >( "opacity" );
	if( !aOpacityData )
	{
		// TODO erase mode
		result->member< Data >( "opacity" )->Object::copyFrom( bOpacityData );
	}
	else
	{
		const std::vector<float> &aOpacity = aOpacityData->readable();
		std::vector<float> &resultOpacity = resultOpacityData->writable();

		if( mode == 0 )
		{
			for( size_t i = 0; i < resultOpacity.size(); i++ )
			{
				// TODO - this would be simpler if we stored ( 1 - opacity ), but maybe that's a harder
				// thing to name?
				resultOpacity[i] = ( 1.0f - bOpacity[i] * opacity ) * aOpacity[i];
			}
		}
		else
		{
			for( size_t i = 0; i < resultOpacity.size(); i++ )
			{
				// TODO - this would be simpler if we stored ( 1 - opacity ), but maybe that's a harder
				// thing to name?
				resultOpacity[i] = 1.0f - ( 1.0f - bOpacity[i] * opacity ) * ( 1.0f - aOpacity[i] );
			}
		}
	}
}

// Uniform block structure (std140 layout)
struct UniformBlockColorShader
{
    alignas( 16 ) M44f o2c;
};


// Name of P primitive variable
const std::string g_pName = "P";

const GLuint g_uniformBlockBindingIndex = 0;

#define UNIFORM_BLOCK_COLOR_SHADER_GLSL_SOURCE \
    "layout( std140, row_major ) uniform UniformBlock\n" \
    "{\n" \
    "   mat4 o2c;\n" \
    "} uniforms;\n"

#define ATTRIB_GLSL_LOCATION_PS 0
#define ATTRIB_GLSL_LOCATION_VSX 1
#define ATTRIB_GLSL_LOCATION_VSY 2
#define ATTRIB_GLSL_LOCATION_VSZ 3

#define ATTRIB_COLOR_SHADER_GLSL_SOURCE \
    "layout( location = " BOOST_PP_STRINGIZE( ATTRIB_GLSL_LOCATION_PS ) " ) in vec3 ps;\n" \
    "layout( location = " BOOST_PP_STRINGIZE( ATTRIB_GLSL_LOCATION_VSX ) " ) in float vsx;\n" \
    "layout( location = " BOOST_PP_STRINGIZE( ATTRIB_GLSL_LOCATION_VSY ) " ) in float vsy;\n" \
    "layout( location = " BOOST_PP_STRINGIZE( ATTRIB_GLSL_LOCATION_VSZ ) " ) in float vsz;\n" \


#define INTERFACE_BLOCK_COLOR_SHADER_GLSL_SOURCE( STORAGE, NAME ) \
    BOOST_PP_STRINGIZE( STORAGE ) " InterfaceBlock\n" \
    "{\n" \
    "   smooth vec3 value;\n" \
    "} " BOOST_PP_STRINGIZE( NAME ) ";\n"


// Opengl vertex shader code

const std::string g_colorShaderVertSource(
    "#version 330\n"

    UNIFORM_BLOCK_COLOR_SHADER_GLSL_SOURCE

    ATTRIB_COLOR_SHADER_GLSL_SOURCE

    INTERFACE_BLOCK_COLOR_SHADER_GLSL_SOURCE( out, outputs )

    "void main()\n"
    "{\n"
    "   outputs.value = vec3( vsx, vsy, vsz );\n"
    "   gl_Position = vec4( ps, 1.0 ) * uniforms.o2c;\n"
    "}\n"
);

// Opengl fragment shader code

const std::string g_colorShaderFragSource
(
    "#version 330\n"

    UNIFORM_BLOCK_COLOR_SHADER_GLSL_SOURCE

    INTERFACE_BLOCK_COLOR_SHADER_GLSL_SOURCE( in, inputs )

    "layout( location = 0 ) out vec4 cs;\n"

    "void main()\n"
    "{\n"
    "   cs = vec4( inputs.value, 1.0 );\n"
    "}\n"
);

// The gadget that does the actual opengl drawing of the shaded primitive
class PaintGadget : public Gadget
{

	public :

		explicit PaintGadget( const PaintTool &tool, const std::string &name = defaultName<PaintGadget>() ) :
			Gadget( name ),
			m_tool( &tool ), m_visualiseMode( 0 )
		{
		}

		void resetTool()
		{
			m_tool = nullptr;
		}

		void setVisualiseMode( int visualiseMode )
		{
			m_visualiseMode = visualiseMode;
		}

	protected:

		void renderLayer( Gadget::Layer layer, const Style *style, Gadget::RenderReason reason ) const override
		{
			if(
				( layer != Gadget::Layer::MidFront )
				|| Gadget::isSelectionRender( reason ) // TODO TODO TODO - remove this early exit, actually support selection
			)
			{
				return;
			}

			// Check tool reference valid
			if( m_tool == nullptr )
			{
				return;
			}

			// Get parent viewport gadget
			const ViewportGadget *viewportGadget = ancestor<ViewportGadget>();

			if( layer == Gadget::Layer::MidFront )
			{
				renderColorVisualiser( viewportGadget, reason );
			}
		}

		Box3f renderBound() const override
		{
			// NOTE : for now just return an infinite box

			Box3f b;
			b.makeInfinite();
			return b;
		}

		unsigned layerMask() const override
		{
			return m_tool ? static_cast< unsigned >( Gadget::Layer::MidFront ) : static_cast< unsigned >( 0 );
		}

	private:

		friend PaintTool;

		void buildShader( IECoreGL::ConstShaderPtr &shader, const std::string &vertSource, const std::string &fragSource ) const
		{
			if( !shader )
			{
				shader = IECoreGL::ShaderLoader::defaultShaderLoader()->create(
					vertSource, std::string(), fragSource
				);
				if( shader )
				{
					const GLuint program = shader->program();
					const GLuint blockIndex = glGetUniformBlockIndex( program, "UniformBlock" );
					if( blockIndex != GL_INVALID_INDEX )
					{
						glUniformBlockBinding( program, blockIndex, g_uniformBlockBindingIndex );
					}
				}
			}
		}

		/// Renders the color visualiser for the given `ViewportGadget`. In general, each visualiser
		/// is reponsible for determining if it should be drawn for the given `mode`. Objects may
		/// have different data types for the same variable name, so a visualiser's suitability may
		/// vary per-object.
		void renderColorVisualiser( const ViewportGadget *viewportGadget, Gadget::RenderReason reason ) const
		{
			// Get the name of the primitive variable to visualise
			const std::string name = m_tool->variableNamePlug()->getValue();
			if( name.empty() )
			{
				return;
			}

			// TODO TODO TODO : Support selection
			//if( Gadget::isSelectionRender( reason ) )

			IECore::TypeId variableType = (IECore::TypeId)m_tool->variableTypePlug()->getValue();
			int toolMode = m_tool->modePlug()->getValue();
			float toolOpacity = m_tool->opacityPlug()->getValue();

			buildShader( m_colorShader, g_colorShaderVertSource, g_colorShaderFragSource );

			if( !m_colorShader )
			{
				return;
			}

			// Get the cached converter from IECoreGL, this is used to convert primitive
			// variable data to opengl buffers which will be shared with the IECoreGL renderer
			IECoreGL::CachedConverter *converter = IECoreGL::CachedConverter::defaultCachedConverter();

			GLint uniformBinding;
			glGetIntegerv( GL_UNIFORM_BUFFER_BINDING, &uniformBinding );

			if( !m_colorUniformBuffer )
			{
				GLuint buffer = 0u;
				glGenBuffers( 1, &buffer );
				glBindBuffer( GL_UNIFORM_BUFFER, buffer );
				glBufferData( GL_UNIFORM_BUFFER, sizeof( UniformBlockColorShader ), nullptr, GL_DYNAMIC_DRAW );
				m_colorUniformBuffer.reset( new IECoreGL::Buffer( buffer ) );
			}

			glBindBufferBase( GL_UNIFORM_BUFFER, g_uniformBlockBindingIndex, m_colorUniformBuffer->buffer() );

			UniformBlockColorShader uniforms;

			// Get the world to clip space matrix
			M44f v2c;
			glGetFloatv( GL_PROJECTION_MATRIX, v2c.getValue() );
			M44f modelView;
			glGetFloatv( GL_MODELVIEW_MATRIX, modelView.getValue() );

			const M44f w2c = modelView * v2c;

			const GLboolean depthEnabled = glIsEnabled( GL_DEPTH_TEST );
			if( !depthEnabled )
			{
				glEnable( GL_DEPTH_TEST );
			}

			/*GLint depthFunc;
			glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
			glDepthFunc( GL_LEQUAL );
			*/

			/*GLboolean depthWriteEnabled;
			glGetBooleanv( GL_DEPTH_WRITEMASK, &depthWriteEnabled );
			glDepthMask( GL_TRUE );*/

			const GLboolean blendEnabled = glIsEnabled( GL_BLEND );
			if( !blendEnabled )
			{
				glEnable( GL_BLEND );
			}

			/*const GLboolean polgonOffsetFillEnabled = glIsEnabled( GL_POLYGON_OFFSET_FILL );
			if( !polgonOffsetFillEnabled )
			{
				glEnable( GL_POLYGON_OFFSET_FILL );
			}

			GLfloat polygonOffsetFactor, polygonOffsetUnits;
			glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor );
			glGetFloatv( GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits );
			glPolygonOffset( -1, -100 );
			*/

			// Enable shader program

			GLint shaderProgram;
			glGetIntegerv( GL_CURRENT_PROGRAM, &shaderProgram );
			glUseProgram( m_colorShader->program() );

			// Set opengl vertex attribute array state

			GLint arrayBinding;
			glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &arrayBinding );

			glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );

			glVertexAttribDivisor( ATTRIB_GLSL_LOCATION_PS, 0 );
			glEnableVertexAttribArray( ATTRIB_GLSL_LOCATION_PS );
			glVertexAttribDivisor( ATTRIB_GLSL_LOCATION_VSX, 0 );
			glEnableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSX );
			glVertexAttribDivisor( ATTRIB_GLSL_LOCATION_VSY, 0 );
			glEnableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSY );
			glVertexAttribDivisor( ATTRIB_GLSL_LOCATION_VSZ, 0 );

			// Loop through current selection

			for( const auto &location : m_tool->selection() )
			{
				ScenePlug::PathScope scope( location.context(), &location.path() );

				IECoreScene::ConstMeshPrimitivePtr mesh;
				M44f o2w;
				try
				{
					if( m_visualiseMode == 1 || !location.m_paintEdit )
					{
						// Check path exists
						if( !location.scene()->existsPlug()->getValue() )
						{
							continue;
						}

						// Extract mesh primitive
						mesh = runTimeCast<const IECoreScene::MeshPrimitive>( location.scene()->objectPlug()->getValue() );
					}
					else
					{
						//TODO - should the cache be inside the paint or something?
						const PrimitiveVariablePaint *primVarPaint = IECore::runTimeCast<PrimitiveVariablePaint>( (*location.m_paintEdit->dataPlug()->outputs().begin())->node() );
						if( !primVarPaint )
						{
							throw IECore::Exception( "TODO QQQQQQ" );
						}

						// Check path exists
						if( !primVarPaint->outPlug()->existsPlug()->getValue() )
						{
							continue;
						}
						mesh = IECore::runTimeCast< const IECoreScene::MeshPrimitive>( primVarPaint->outPlug()->object( location.upstreamPath() ) );
					}

					if( !mesh )
					{
						continue;
					}

					// Get the object to world transform
					o2w = location.scene()->fullTransform( location.path() );
				}
				catch( const std::exception & )
				{
					/// \todo Ideally the GL state would be handled by `IECoreGL::State` and related classes
					/// which would restore the GL state via RAII in the case of exceptions.
					/// But those don't handle everything we need like shader attribute block alignment,
					/// `GL_POLYGON_OFFSET` and more, so we use try / catch blocks throughout this tool.
					continue;
				}

				// Find opengl named buffer data
				//
				// NOTE : conversion to IECoreGL mesh may generate vertex attributes (eg. "N")
				//        so check named primitive variable exists on IECore mesh primitive.

				const auto vIt = mesh->variables.find( name );
				ConstDataPtr vData;
				if( vIt != mesh->variables.end() )
				{
					vData = vIt->second.data;
				}


				// Retrieve cached IECoreGL mesh primitive
				auto meshGL = runTimeCast<const IECoreGL::MeshPrimitive>( converter->convert( mesh.get() ) );
				if( !meshGL )
				{
					continue;
				}

				// Find opengl "P" buffer data

				IECoreGL::ConstBufferPtr pBuffer = meshGL->getVertexBuffer( g_pName );
				if( !pBuffer )
				{
					continue;
				}

				GLsizei components = 0;
				GLenum type = GL_FLOAT;


				bool isColor = variableType == Color3fVectorData::staticTypeId();

				int activeVisualiseMode = 1;
				//if( m_visualiseMode == 0 && ( location.m_currentStroke || location.m_initialEditValue ) ) // TODO TODO TODO
				if( m_visualiseMode == 0 && location.m_currentStroke ) // TODO TODO TODO
				{
					activeVisualiseMode = 0;
				}

				IECoreGL::ConstBufferPtr vBuffer;
				if( activeVisualiseMode == 0 && location.m_currentStrokeDirty )
				{
					if( !location.m_triangulatedTemp )
					{
						location.m_triangulatedTemp = IECoreScene::MeshAlgo::triangulate( mesh.get() );
					}

					size_t numVerts = mesh->variableSize( IECoreScene::PrimitiveVariable::Interpolation::Vertex );
					size_t numVertsExpanded = location.m_triangulatedTemp->variableSize( IECoreScene::PrimitiveVariable::Interpolation::FaceVarying );

					// TODO - erase mode doesn't work like this
					applyPrimVarComposite( location.m_composedInputValue.get(), location.m_currentStroke.get(), toolMode, toolOpacity, location.m_composedValue.get() );
					if( isColor )
					{
						components = 3;

						const std::vector<Color3f> &strokeValue = location.m_composedValue->member<Color3fVectorData>( "value" )->readable();
						//location.m_colorValue.resize( numVerts, Imath::Color3f( 0.0f, 0.0f, 0.0f ) );
						location.m_colorValueExpanded.resize( numVertsExpanded, Imath::Color3f( 0.0f, 0.0f, 0.0f ) );

						const std::vector<int> &vertexIds = location.m_triangulatedTemp->vertexIds()->readable();
						for( unsigned int i = 0; i < numVertsExpanded; i++ )
						{
							location.m_colorValueExpanded[i] = strokeValue[ vertexIds[i] ];
						}

						if( !location.m_valueBuffer )
						{
							location.m_valueBuffer = new IECoreGL::Buffer( &location.m_colorValueExpanded[0], sizeof( float ) * 3 * numVertsExpanded );
						}
						glBindBuffer( GL_ARRAY_BUFFER, location.m_valueBuffer->buffer() );
						//glBufferData( GL_ARRAY_BUFFER, location.m_valueBuffer->size(), &location.m_colorValueExpanded[0], GL_DYNAMIC_DRAW );
						glBufferSubData( GL_ARRAY_BUFFER, 0, location.m_valueBuffer->size(), &location.m_colorValueExpanded[0] );
					}
					else
					{
						components = 1;
						location.m_floatValue.resize( numVerts, 1.0f );
						if( !location.m_valueBuffer )
						{
							location.m_valueBuffer = new IECoreGL::Buffer( &location.m_floatValue[0], sizeof( float ) * numVerts );
						}
						glBindBuffer( GL_ARRAY_BUFFER, location.m_valueBuffer->buffer() );
						//glBufferData( GL_ARRAY_BUFFER, location.m_valueBuffer->size(), &location.m_floatValue[0], GL_DYNAMIC_DRAW );
						glBufferSubData( GL_ARRAY_BUFFER, 0, location.m_valueBuffer->size(), &location.m_floatValue[0] );
					}
					location.m_currentStrokeDirty = false;
				}
				else
				{
					if( vData )
					{
						vBuffer = meshGL->getVertexBuffer( name );
						if( vBuffer )
						{
							switch( vData->typeId() )
							{
								case IntVectorDataTypeId:
									type = GL_INT;
									components = 1;
									break;
								case FloatVectorDataTypeId:
									components = 1;
									break;
								case V2fVectorDataTypeId:
									components = 2;
									break;
								case Color3fVectorDataTypeId:
									components = 3;
									break;
								case V3fVectorDataTypeId:
									components = 3;
									break;
								default:
									continue;
							}
						}
					}
				}


				// Compute object to clip matrix
				uniforms.o2c = o2w * w2c;

				// Upload opengl uniform block data

				glBufferData( GL_UNIFORM_BUFFER, sizeof( UniformBlockColorShader ), &uniforms, GL_DYNAMIC_DRAW );

				// Draw primitive
				glBindBuffer( GL_ARRAY_BUFFER, pBuffer->buffer() );
				glVertexAttribPointer( ATTRIB_GLSL_LOCATION_PS, 3, GL_FLOAT, GL_FALSE, 0, nullptr );

				if( activeVisualiseMode == 0 )
				{
					glBindBuffer( GL_ARRAY_BUFFER, location.m_valueBuffer->buffer() );
				}
				else
				{
					if( vBuffer )
					{
						glBindBuffer( GL_ARRAY_BUFFER, vBuffer->buffer() );
					}
				}

				if( components != 0 )
				{
					glEnableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSX );
					glVertexAttribPointer( ATTRIB_GLSL_LOCATION_VSX, 1, type, GL_FALSE, components * sizeof( GLfloat ), nullptr );
				}
				else
				{
					glDisableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSX );
					glVertexAttrib1f( ATTRIB_GLSL_LOCATION_VSX, 0.f );
				}

				if( components != 0 )
				{
					glEnableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSX );
					glVertexAttribPointer(
						ATTRIB_GLSL_LOCATION_VSY,
						1,
						type,
						GL_FALSE,
						components * sizeof( GLfloat ),
						( void const *)( ( components != 1 ? 1 : 0 ) * sizeof( GLfloat ) )
					);
				}
				else
				{
					glDisableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSY );
					glVertexAttrib1f( ATTRIB_GLSL_LOCATION_VSY, 0.f );
				}

				if( components == 1 || components == 3 )
				{
					glEnableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSZ );
					glVertexAttribPointer(
						ATTRIB_GLSL_LOCATION_VSZ,
						1,
						type,
						GL_FALSE,
						components * sizeof( GLfloat ),
						( void const *)( ( components != 1 ? 2 : 0 ) * sizeof( GLfloat ) )
					);
				}
				else
				{
					glDisableVertexAttribArray( ATTRIB_GLSL_LOCATION_VSZ );
					glVertexAttrib1f( ATTRIB_GLSL_LOCATION_VSZ, 0.f );
				}

				meshGL->renderInstances( 1 );
			}

			// Restore opengl state

			glPopClientAttrib();
			glBindBuffer( GL_ARRAY_BUFFER, arrayBinding );
			glBindBuffer( GL_UNIFORM_BUFFER, uniformBinding );

			//glDepthFunc( depthFunc );
			//glPolygonOffset( polygonOffsetFactor, polygonOffsetUnits );

			if( !depthEnabled )
			{
				glDisable( GL_DEPTH_TEST );
			}
			/*if( depthWriteEnabled )
			{
				glDepthMask( GL_TRUE );
			}*/
			glUseProgram( shaderProgram );
		}

		const PaintTool *m_tool;
		mutable IECoreGL::ConstShaderPtr m_colorShader;
		mutable IECoreGL::ConstBufferPtr m_colorUniformBuffer;
		int m_visualiseMode;
};

// Cache for mesh evaluators
struct EvaluationData
{
	IECoreScene::ConstMeshPrimitivePtr triMesh;
	IECoreScene::ConstMeshPrimitiveEvaluatorPtr evaluator;
};

IECore::LRUCache<IECoreScene::ConstMeshPrimitivePtr, EvaluationData> g_evaluatorCache(
	[] ( IECoreScene::ConstMeshPrimitivePtr mesh, size_t &cost ) -> EvaluationData
	{
		cost = 1;
		EvaluationData data;
		data.triMesh = mesh->copy();
		data.triMesh = IECoreScene::MeshAlgo::triangulate( data.triMesh.get() );
		data.evaluator = new IECoreScene::MeshPrimitiveEvaluator( data.triMesh );
		return data;
	},
	1000
);

/*M44f signOnlyScaling( const M44f &m )
{
	V3f scale( 1 );
	V3f shear( 0 );
	V3f rotate( 0 );
	V3f translate( 0 );

	extractSHRT( m, scale, shear, rotate, translate );

	M44f result;

	result.translate( translate );
	result.rotate( rotate );
	result.shear( shear );
	result.scale( V3f( Imath::sign( scale.x ), Imath::sign( scale.y ), Imath::sign( scale.z ) ) );

	return result;
}*/

// Similar to `plug->source()`, but able to traverse through
// Spreadsheet outputs to find the appropriate input row.
/*V3fPlug *spreadsheetAwareSource( V3fPlug *plug, std::string &failureReason )
{
	plug = plug->source<V3fPlug>();
	if( !plug )
	{
		// Source is not a V3fPlug. Not much we can do about that.
		failureReason = "Plug input is not a V3fPlug";
		return nullptr;
	}

	auto *spreadsheet = runTimeCast<Spreadsheet>( plug->node() );
	if( !spreadsheet )
	{
		return plug;
	}

	if( !spreadsheet->outPlug()->isAncestorOf( plug ) )
	{
		return plug;
	}

	plug = static_cast<V3fPlug *>( spreadsheet->activeInPlug( plug ) );
	if( plug->ancestor<Spreadsheet::RowPlug>() == spreadsheet->rowsPlug()->defaultRow() )
	{
		// Default spreadsheet row. Editing this could affect any number
		// of unrelated objects, so don't allow that.
		failureReason = "Cannot edit default spreadsheet row";
		return nullptr;
	}

	return plug->source<V3fPlug>();
}*/

/*GraphComponent *editTargetOrNull( const PaintTool::Selection &selection )
{
	return selection.editable() ? selection.editTarget() : nullptr;
}*/

class HandlesGadget : public Gadget
{

	public :

		HandlesGadget( const std::string &name="HandlesGadget" )
			:	Gadget( name )
		{
		}

	protected :

		Imath::Box3f renderBound() const override
		{
			// We need `renderLayer()` to be called any time it will
			// be called for one of our children. Our children claim
			// infinite bounds to account for their raster scale, so
			// we must too.
			Box3f b;
			b.makeInfinite();
			return b;
		}

		void renderLayer( Layer layer, const Style *style, Gadget::RenderReason reason ) const override
		{
			if( layer != Layer::MidFront )
			{
				return;
			}

			// Clear the depth buffer so that the handles render
			// over the top of the SceneGadget. Otherwise they are
			// unusable when the object is larger than the handles.
			/// \todo Can we really justify this approach? Does it
			/// play well with new Gadgets we'll add over time? If
			/// so, then we should probably move the depth clearing
			/// to `Gadget::render()`, in between each layer. If
			/// not we'll need to come up with something else, perhaps
			/// going back to punching a hole in the depth buffer using
			/// `glDepthFunc( GL_GREATER )`. Or maybe an option to
			/// render gadgets in an offscreen buffer before compositing
			/// them over the current framebuffer?
			glClearDepth( 1.0f );
			glClear( GL_DEPTH_BUFFER_BIT );
			glEnable( GL_DEPTH_TEST );

		}

		unsigned layerMask() const override
		{
			return (unsigned)Layer::MidFront;
		}

};

template< class T >
bool applyPaint( std::vector< T > &outValue, std::vector<float> &outOpacity, const T &value, float opacity, int mode, float hardness, const IECoreScene::PrimitiveVariable::IndexedView<V3f> &pVar, const M44f& localPaintMatrix, int resolution, const float *depthMap, const UglyArray &iterators, const V2f *baseKDPoint )
{
	outValue.resize( pVar.size(), T( 0.0f ) );
	outOpacity.resize( pVar.size(), 0.0f );

	//M44f localPaintMatrixInverse = localPaintMatrix.inverse();

	//auto &paintIndices = paintIndicesData->writable();
	bool modified = false;

	//for( size_t i = 0; i < pVar.size(); i++ )
	for( auto it : iterators )
	{
		int i = &(*it) - baseKDPoint;
		//V3f offset = (*pVar)[i] - localOrigin;
		//float curOpac = opacity * ( toolSize - ( offset - localDir * offset.dot( dir ) ).length() ) / toolSize;
		V3f brushPos;
		localPaintMatrix.multVecMatrix( pVar[i], brushPos );

		float l2 = brushPos.x * brushPos.x + brushPos.y * brushPos.y;
		if( l2 > 1.0f )
		{
			continue;
		}

		float brushShape;
		if( hardness == 1.0f )
		{
			brushShape = l2 <= 1.0f ? 1.0f : 0.0f;
		}
		else
		{
			brushShape = std::min( ( 1 - sqrtf( l2 ) ) / ( 1.0f - hardness ), 1.0f );
		}

		float rasterX = ( brushPos.x * 0.5f + 0.5f ) * ( resolution - 1 );
		float rasterY = ( brushPos.y * 0.5f + 0.5f ) * ( resolution - 1 );

		float iX = std::min( floorf( rasterX ), resolution - 2.0f );
		float iY = std::min( floorf( rasterY ), resolution - 2.0f );

		float lerpX = rasterX - iX;
		float lerpY = rasterY - iY;

		int iiX = iX;
		int iiY = iY;

		//std::cerr << "HIT : " << rasterX << ", " << rasterY << "\n";
		float depth =
			( depthMap[ iiY * resolution + iiX ] * ( 1.0f - lerpX ) + ( depthMap[ iiY * resolution + iiX + 1 ] * lerpX ) ) * ( 1.0f - lerpY ) +
			( depthMap[ ( iiY + 1 ) * resolution + iiX ] * ( 1.0f - lerpX ) + ( depthMap[ ( iiY + 1 ) * resolution + iiX + 1 ] * lerpX ) ) * lerpY;


		//V3f localNearPlanePoint;

		//localPaintMatrixInverse.multVecMatrix( V3f( brushPos.x, brushPos.y, -1.0f ), localNearPlanePoint );

		const float lookupDepth = brushPos.z;

		const float depthTolerance = brushPos.z * 0.00001;


		//std::cerr << "COMPARE " << ( pVar[i] - localNearPlanePoint ).length() << " <= " << depth << "\n";
		float depthMask = std::max( 0.0f, std::min( 1.0f, ( 2.0f * depth - 1.0f + depthTolerance - lookupDepth ) / depthTolerance ) );

		float curOpac = opacity * brushShape * depthMask;
		if( curOpac > 0 )
		{
			modified = true;
			if( mode == 0 )
			{
				outValue[i] = ( 1.0f - curOpac ) * outValue[i];

				// TODO - this would be simpler if we stored ( 1 - opacity ), but maybe that's a harder
				// thing to name?
				outOpacity[i] = ( 1.0f - curOpac ) * outOpacity[i];
			}
			else
			{
				outValue[i] = ( 1.0f - curOpac ) * outValue[i] + curOpac * value;

				// TODO - this would be simpler if we stored ( 1 - opacity ), but maybe that's a harder
				// thing to name?
				outOpacity[i] = 1.0f - ( 1.0f - curOpac ) * ( 1.0f - outOpacity[i] );
			}
		}
	}
	return modified;
}


} // namespace

class PaintTool::BrushOutline : public GafferUI::Gadget
{

    public :

        /*enum BrushOutlineChangedReason
        {
            Invalid,
            SetBound,
            DragBegin,
            DragMove,
            DragEnd
        };*/

        BrushOutline()
            :   Gadget(), m_pos( 0.0f ), m_radius( 100.0f ), m_hardness( 0.5f )
        {
            /*mouseMoveSignal().connect( boost::bind( &BrushOutline::mouseMove, this, ::_2 ) );
            buttonPressSignal().connect( boost::bind( &BrushOutline::buttonPress, this, ::_2 ) );
            dragBeginSignal().connect( boost::bind( &BrushOutline::dragBegin, this, ::_1, ::_2 ) );
            dragEnterSignal().connect( boost::bind( &BrushOutline::dragEnter, this, ::_1, ::_2 ) );
            dragMoveSignal().connect( boost::bind( &BrushOutline::dragMove, this, ::_2 ) );
            dragEndSignal().connect( boost::bind( &BrushOutline::dragEnd, this, ::_2 ) );
            leaveSignal().connect( boost::bind( &BrushOutline::leave, this ) );*/


			/*const int numDivisions = 100;
			IntVectorDataPtr vertsPerCurveData = new IntVectorData( { numDivisions } );
			V3fVectorDataPtr pData = new V3fVectorData;
			std::vector<V3f> &p = pData->writable();
			for( int i = 0; i < numDivisions; ++i )
			{
				const float angle = 2 * M_PI * (float)i/(float)(numDivisions-1);
				p.push_back( 100.0f * V3f( cos( angle ), sin( angle ), 0 ) );
			}

			m_curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurveData );
			m_curves->addPrimitiveVariable( "P", IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, pData ) );*/
        }

        Imath::Box3f bound() const override
        {
			// We draw in raster space so don't have a sensible bound
			return Box3f();
        }

        void setPos( const Imath::V2f &pos )
        {
			m_pos = pos;
            dirty( DirtyType::Render );
        }

        void setBrush( float radius, float hardness )
        {
			m_radius = radius;
			m_hardness = hardness;
            dirty( DirtyType::Render );
        }

        /*const Imath::Box2f &getBrushOutline() const
        {
            return m_rectangle;
        }*/

        /*using UnarySignal = Signals::Signal<void ( BrushOutline *, BrushOutlineChangedReason )>;
        UnarySignal &rectangleChangedSignal()
        {
            return m_rectangleChangedSignal;
        }*/

    protected :

        void renderLayer( Layer layer, const Style *style, RenderReason reason ) const override
        {
            if( layer != Layer::Front )
            {
                return;
            }

            /// \todo Would it make sense for the ViewportGadget to have a way
            /// of adding a child as an overlay, so we didn't have to do the
            /// raster scope bit manually? Maybe that would let us write more reusable
            /// gadgets, which could be used in any space, and we wouldn't need
            /// eventPosition().
            std::optional<ViewportGadget::RasterScope> rasterScope;
			rasterScope.emplace( ancestor<ViewportGadget>() );

            glPushAttrib( GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT );

                if( !isSelectionRender( reason ) )
                {
                    glEnable( GL_LINE_SMOOTH );
                    glLineWidth( 1.5f );

					glTranslatef( m_pos.x, m_pos.y, 0.0f );

					for( int j = 0; j < 2; j++ )
					{
						if( j == 1 )
						{
							if( m_hardness == 1.0f )
							{
								continue;
							}
							glColor4f( 0.0f, 0.0f, 0.0, 1.0f );
							float h = std::max( 0.01f, m_hardness );
							glScalef( h, h, h );
						}
						else
						{
							glColor4f( 0.8f, 0.8f, 0.8, 1.0f );
							glScalef( m_radius, m_radius, m_radius );
						}


						glBegin( GL_LINE_LOOP );

							const int numDivisions = 100;
							for( int i = 0; i < numDivisions; ++i )
							{
								const float angle = 2 * M_PI * (float)i/(float)(numDivisions-1);
								glVertex2f( cos( angle ), sin( angle ) );
							}

						glEnd();
					}
                }

            glPopAttrib();

        }

        unsigned layerMask() const override
        {
            return (unsigned)Layer::Front;
        }

        Imath::Box3f renderBound() const override
        {
			// We don't have a sensible bound when we're drawing
			// in raster space
			Box3f b;
			b.makeInfinite();
			return b;
        }

    private :

        /*bool mouseMove( const ButtonEvent &event )
        {
            int x, y;
            bool inside;

            if( !inside || event.modifiers == ButtonEvent::Modifiers::Shift )
            {
                Pointer::setCurrent( "crossHair" );
            }
            else if( x && y )
            {
                const bool isDown = x * y > 0;
                Pointer::setCurrent( isDown ? "moveDiagonallyDown" : "moveDiagonallyUp" );
            }
            else if( x )
            {
                Pointer::setCurrent( "moveHorizontally" );
            }
            else if( y )
            {
                Pointer::setCurrent( "moveVertically" );
            }
            else
            {
                Pointer::setCurrent( "move" );
            }

            return false;
        }

        bool buttonPress( const GafferUI::ButtonEvent &event )
        {
            if( event.buttons != ButtonEvent::Left )
            {
                return false;
            }

            return true;
        }

        IECore::RunTimeTypedPtr dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
        {
            m_dragStart = eventPosition( event );
            m_dragStartBrushOutline = m_rectangle;
            return IECore::NullObject::defaultNullObject();
        }


       bool dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
        {
            if( event.sourceGadget != this )
            {
                return false;
            }

            updateDragBrushOutline( event, DragBegin );
            return true;
        }

        bool dragMove( const GafferUI::DragDropEvent &event )
        {
            updateDragBrushOutline( event, DragMove );
            return true;
        }

        bool dragEnd( const GafferUI::DragDropEvent &event )
        {
            updateDragBrushOutline( event, DragEnd );
            return true;
        }

        void updateDragBrushOutline( const GafferUI::DragDropEvent &event, BrushOutlineChangedReason reason )
        {
            const V2f p = eventPosition( event );
            Box2f b = m_dragStartBrushOutline;

            if( !m_dragInside || event.modifiers == ButtonEvent::Modifiers::Shift )
            {
                b.min = m_dragStart;
                b.max = p;
            }
            else if( m_xDragEdge || m_yDragEdge )
            {
                if( m_xDragEdge == -1 )
                {
                    b.min.x = p.x;
                }
                else if( m_xDragEdge == 1 )
                {
                    b.max.x = p.x;
                }

                if( m_yDragEdge == -1 )
                {
                    b.min.y = p.y;
                }
                else if( m_yDragEdge == 1 )
                {
                    b.max.y = p.y;
                }
            }
            else
            {
                const V2f offset = p - m_dragStart;
                b.min += offset;
                b.max += offset;
            }

            // fix max < min issues
            Box2f c;
            c.extendBy( b.min );
            c.extendBy( b.max );

            setBrushOutlineInternal( c, reason );
        }

        void leave()
        {
            Pointer::setCurrent( "" );
        }

        V2f eventPosition( const ButtonEvent &event ) const
        {
            const ViewportGadget *viewportGadget = ancestor<ViewportGadget>();
			return viewportGadget->gadgetToRasterSpace( event.line.p1, this );
        }*/

        /*Imath::Box2f m_rectangle;
        UnarySignal m_rectangleChangedSignal;

        Imath::Box2f m_dragStartBrushOutline;
        Imath::V2f m_dragStart;
        bool m_dragInside;
        int m_xDragEdge;
        int m_yDragEdge;*/

		V2f m_pos;
		float m_radius;
		float m_hardness;
		IECoreGL::CurvesPrimitivePtr m_curves;

};


//////////////////////////////////////////////////////////////////////////
// PaintTool::Selection
//////////////////////////////////////////////////////////////////////////

/// \todo Work out how to avoid baking graph component names into message
/// strings. We decided to put up with the stale name bugs here as the benefit
/// of better informing user outweighs the (hopefully less frequent) times
/// that the message is out of date.

PaintTool::Selection::Selection()
	:	m_paintEdit( nullptr ), m_editable( false )
{
}

PaintTool::Selection::Selection(
	const GafferScene::ConstScenePlugPtr scene,
	const GafferScene::ScenePlug::ScenePath &path,
	const Gaffer::ConstContextPtr &context,
	const Gaffer::EditScopePtr &editScope
)
	:	m_paintEdit( nullptr ), m_scene( scene ), m_path( path ), m_context( context ), m_editable( false ), m_editScope( editScope )
{
	Context::Scope scopedContext( context.get() );
	if( path.empty() )
	{
		m_warning = "Cannot edit root transform";
		return;
	}
	else if( !scene->exists( path ) )
	{
		m_warning = "Location does not exist";
		return;
	}

	bool editScopeFound = false;
	while( m_path.size() )
	{
		SceneAlgo::History::Ptr history = SceneAlgo::history( scene->transformPlug(), m_path );
		initWalk( history.get(), editScopeFound );
		if( editable() )
		{
			break;
		}
		m_path.pop_back();
	}

	if( !editable() && m_path.size() != path.size() )
	{
		// Attempts to edit a parent path failed. Reset to
		// the original path so we don't confuse client code.
		m_path = path;
	}

	if( editScope && !editScopeFound )
	{
		m_warning = "The target EditScope \"" + displayName( editScope.get() ) + "\" is not in the scene history";
		m_editable = false;
		return;
	}

	if( m_path.size() != path.size() )
	{
		std::string pathString;
		GafferScene::ScenePlug::pathToString( m_path, pathString );
		m_warning = "Editing parent location \"" + pathString + "\"";
	}
}

void PaintTool::Selection::initFromHistory( const GafferScene::SceneAlgo::History *history )
{
	m_upstreamScene = history->scene;
	m_upstreamPath = history->context->get<ScenePlug::ScenePath>( ScenePlug::scenePathContextName );
	m_upstreamContext = history->context;
}

// TODO - would we really ever expect to see paint outside an EditScope?
void PaintTool::Selection::initFromSceneNode( const GafferScene::SceneAlgo::History *history )
{
	// Check for SceneNode and return if there isn't one or it is disabled.

	/*const SceneNode *node = runTimeCast<const SceneNode>( history->scene->node() );
	if( !node )
	{
		return;
	}

	Context::Scope scopedContext( history->context.get() );
	if( !node->enabledPlug()->getValue() )
	{
		return;
	}

	// Determine if node edits the transform at this location.

	Int64VectorDataPlugPtr hackPlug = nullptr;
	if( const GafferScene::PrimitiveVariableTweaks *primVarTweaks = runTimeCast<const GafferScene::PrimitiveVariableTweaks>( node ) )
	{
		if(
			history->scene == primVarTweaks->outPlug() &&
			( primVarTweaks->filterPlug()->match( primVarTweaks->inPlug() ) & PathMatcher::ExactMatch )
		)
		{
			hackPlug = const_cast<Int64VectorDataPlug *>( primVarTweaks->idListPlug() );
		}
	}

	if( !hackPlug )
	{
		return;
	}

	// We found the TransformPlug and the upstream scene which authors the transform.
	// Recording this will terminate the search in `initWalk()`.

	initFromHistory( history );

	const GraphComponent *readOnlyAncestor = MetadataAlgo::readOnlyReason( hackPlug.get() );

	// Unlike normal read-only nodes, the user won't be able to simply
	// unlock a reference node, so we disable editing.
	if( const Node *node = runTimeCast<const Node>( readOnlyAncestor ) )
	{
		if( Gaffer::MetadataAlgo::getChildNodesAreReadOnly( node ) )
		{
			m_warning = "Transform is locked as it is inside \"" + displayName( node ) + "\" which disallows edits to its children";
			return;
		}
	}

	if( readOnlyAncestor )
	{
		m_warning = "\"" + displayName( readOnlyAncestor ) + "\" is locked";
	}

	m_editable = true;
	m_paintEdit = hackPlug;*/
}

void PaintTool::Selection::initFromEditScope( const GafferScene::SceneAlgo::History *history )
{
	initFromHistory( history );

	Context::Scope scopedContext( m_upstreamContext.get() );

	if( !m_editScope->enabledPlug()->getValue() )
	{
		m_warning = "The target EditScope \"" + displayName( m_editScope.get() ) + "\" is disabled";
		return;
	}

	if( const GraphComponent *readOnlyComponent = EditScopeAlgo::transformEditReadOnlyReason( m_editScope.get(), m_upstreamPath ) )
	{
		m_warning = "\"" + displayName( readOnlyComponent ) + "\" is locked";
		return;
	}

	m_editable = true;

	m_paintEdit = EditScopeAlgo::acquirePaintEdit( m_editScope.get(), /* createIfNecessary = */ false );
	m_paintEditPath = ScenePlug::pathToString( m_upstreamPath );
}

void PaintTool::Selection::initWalk( const GafferScene::SceneAlgo::History *history, bool &editScopeFound, const GafferScene::SceneAlgo::History *editScopeOutHistory )
{
	// Walk the history looking for a suitable node to edit.
	// Transform tools only support editing the last node to author the targets
	// transform (otherwise manipulators may be incorrectly placed or the
	// transform overwritten).

	Node *node = history->scene->node();

	// TODO
	/*if( !m_upstreamScene )
	{
		// First, check for a supported node in this history entry
		initFromSceneNode( history );

		if( m_upstreamScene )
		{
			const auto upstreamEditScope = m_upstreamScene->ancestor<EditScope>();
			if( m_editScope )
			{
				// If we found a node to edit here and the user has requested a
				// specific scope, check if the edit is in it.
				editScopeFound = upstreamEditScope == m_editScope;
			}
			else if( upstreamEditScope )
			{
				// We don't allow editing if the user hasn't requested a specific scope
				// and the upstream edit is inside an EditScope.
				m_warning = "Source is in an EditScope. Change scope to " + displayName( upstreamEditScope ) + " to edit";
				m_editable = false;
			}
		}
	}*/

	// If the user has requested an edit scope, and we've not found it yet,
	// check this entry.  We do this regardless of whether we already have an
	// edit so we can differentiate between a missing edit scope and one that
	// is overridden downstream.
	if( m_editScope && !editScopeFound )
	{
		EditScope *editScope = runTimeCast<EditScope>( node );

		if( editScope && editScope == m_editScope )
		{
			if( m_upstreamScene )
			{
				// If we're here with an existing edit, then it means it is
				// downstream of the requested scope.
				editScopeFound = true;

				m_warning = "The target EditScope \"" + displayName( m_editScope.get() ) + "\" is overridden downstream by \"" + displayName( m_upstreamScene->node() ) + "\"";
				m_editable = false;
			}
			else if( history->scene == editScope->outPlug() )
			{
				// We only consider using an EditScope to author new edits on
				// the way in (from the Node Graph perspective). This allows nodes
				// inside to take precedence, however we need to use the
				// history from the scopes output, so keep track of it for the
				// rest of the walk.
				editScopeOutHistory = history;
			}
			else if( history->scene == editScope->inPlug() )
			{
				editScopeFound = true;

				if( editScopeOutHistory )
				{
					initFromEditScope( editScopeOutHistory );
				}
				else
				{
					// This can happen if the viewed node is inside the chosen EditScope
					m_warning = "The output of the target EditScope \"" + displayName( m_editScope.get() ) + "\" is not in the scene history";
					m_editable = false;
				}
			}
		}
	}

	if( initRequirementsSatisfied( editScopeFound ) )
	{
		return;
	}

	for( const auto &p : history->predecessors )
	{
		initWalk( p.get(), editScopeFound, editScopeOutHistory );

		if( initRequirementsSatisfied( editScopeFound ) )
		{
			return;
		}
	}

	// If we get to here, we've exhausted all upstream history, without finding
	// the input to the chosen scope. This can happen if the object is created
	// inside a nested edit scope - as we can't use the creation node as it is
	// in another scope. The requested scope is still valid though.
	if( editScopeOutHistory && history->scene == editScopeOutHistory->scene )
	{
		editScopeFound = true;
		initFromEditScope( editScopeOutHistory );
	}
}

bool PaintTool::Selection::initRequirementsSatisfied( bool editScopeFound )
{
	// If we don't have an EditScope specified, having a node to edit is enough.
	// If we do, we need to make sure we have found it.
	return ( m_upstreamScene && !m_editScope ) || ( m_editScope && editScopeFound );
}

const GafferScene::ScenePlug *PaintTool::Selection::scene() const
{
	return m_scene.get();
}

const GafferScene::ScenePlug::ScenePath &PaintTool::Selection::path() const
{
	return m_path;
}

const Gaffer::Context *PaintTool::Selection::context() const
{
	return m_context.get();
}

const GafferScene::ScenePlug *PaintTool::Selection::upstreamScene() const
{
	return m_upstreamScene.get();
}

const GafferScene::ScenePlug::ScenePath &PaintTool::Selection::upstreamPath() const
{
	return m_upstreamPath;
}

const Gaffer::Context *PaintTool::Selection::upstreamContext() const
{
	return m_upstreamContext.get();
}

bool PaintTool::Selection::editable() const
{
	return m_editable;
}

const std::string &PaintTool::Selection::warning() const
{
	return m_warning;
}

Gaffer::CachedDataNode* PaintTool::Selection::acquirePaintEdit( bool createIfNecessary ) const
{
	throwIfNotEditable();
	if( !m_paintEdit && createIfNecessary )
	{
		assert( m_editScope );

		m_paintEdit = EditScopeAlgo::acquirePaintEdit( m_editScope.get() );
		m_paintEditPath = ScenePlug::pathToString( upstreamPath() );
	}
	return m_paintEdit;
}

const EditScope *PaintTool::Selection::editScope() const
{
	return m_editScope.get();
}

Gaffer::GraphComponent *PaintTool::Selection::editTarget() const
{
	throwIfNotEditable();
	if( m_editScope && !m_paintEdit )
	{
		return m_editScope.get();
	}
	else
	{
		assert( m_paintEdit );
		return m_paintEdit;
	}
}

PaintTool::Selection &PaintTool::Selection::operator=( const Selection & other )
{
	throw IECore::Exception( "refused" );
}

void PaintTool::Selection::throwIfNotEditable() const
{
	if( !editable() )
	{
		throw IECore::Exception( "Selection is not editable" );
	}
}

std::string PaintTool::Selection::displayName( const GraphComponent *component )
{
	return component->relativeName( component->ancestor<ScriptNode>() );
}

//////////////////////////////////////////////////////////////////////////
// PaintTool
//////////////////////////////////////////////////////////////////////////

GAFFER_NODE_DEFINE_TYPE( PaintTool );

PaintTool::ToolDescription<PaintTool, SceneView> PaintTool::g_toolDescription;

size_t PaintTool::g_firstPlugIndex = 0;

PaintTool::PaintTool( SceneView *view, const std::string &name )
	:	SelectionTool( view, name ),
		m_gadget( new PaintGadget( *this ) ),
		//m_handlesDirty( true ),
		m_selectionDirty( true ),
		m_priorityPathsDirty( true ),
		m_dragging( false ),
		m_mergeGroupId( 0 ),
		m_depthRender( Imath::V2i( 32, 32 ) ),
		m_mouseIn( false )
{
	view->viewportGadget()->addChild( m_gadget );
	m_gadget->setVisible( false );

	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new ScenePlug( "__scene", Plug::In ) );
	addChild( new StringPlug( "variableName", Plug::In, "Cs" ) );
	addChild( new IntPlug( "variableType", Plug::In, Color3fVectorData::staticTypeId() ) );

	// TODO - enum
	addChild( new IntPlug( "mode", Plug::In, 1 ) );

	addChild( new FloatPlug( "size", Plug::In, 30.0f, 0.0f ) );
	addChild( new FloatPlug( "floatValue", Plug::In, 1.0f ) );
	addChild( new Color3fPlug( "colorValue", Plug::In, Imath::Color3f( 1.0f ) ) );
	addChild( new FloatPlug( "opacity", Plug::In, 1.0f, 0.0f, 1.0f ) );
	addChild( new FloatPlug( "hardness", Plug::In, 0.0f, 0.0f, 1.0f ) );
	addChild( new IntPlug( "visualiseMode", Plug::In, 0, 0, 1 ) );

	scenePlug()->setInput( view->inPlug<ScenePlug>() );

	view->viewportGadget()->keyPressSignal().connect( boost::bind( &PaintTool::keyPress, this, ::_2 ) );
	view->viewportGadget()->enterSignal().connectFront( boost::bind( &PaintTool::enter, this, ::_2 ) );
	view->viewportGadget()->leaveSignal().connectFront( boost::bind( &PaintTool::leave, this, ::_2 ) );
	view->viewportGadget()->mouseMoveSignal().connect( boost::bind( &PaintTool::mouseMove, this, ::_2 ) );
	view->viewportGadget()->buttonPressSignal().connectFront( boost::bind( &PaintTool::buttonPress, this, ::_2 ) );
	view->viewportGadget()->buttonReleaseSignal().connectFront( boost::bind( &PaintTool::buttonRelease, this, ::_2 ) );


	view->viewportGadget()->dragBeginSignal().connect( boost::bind( &PaintTool::dragBegin, this, ::_1, ::_2 ) );
	view->viewportGadget()->dragEnterSignal().connect( boost::bind( &PaintTool::dragEnter, this, ::_1, ::_2 ) );
	view->viewportGadget()->dragMoveSignal().connect( boost::bind( &PaintTool::dragMove, this, ::_2 ) );
	view->viewportGadget()->dragEndSignal().connect( boost::bind( &PaintTool::dragEnd, this, ::_2 ) );

	plugDirtiedSignal().connect( boost::bind( &PaintTool::plugDirtied, this, ::_1 ) );
	view->plugDirtiedSignal().connect( boost::bind( &PaintTool::plugDirtied, this, ::_1 ) );
	view->contextChangedSignal().connect( boost::bind( &PaintTool::contextChanged, this ) );

	ScriptNodeAlgo::selectedPathsChangedSignal( view->scriptNode() ).connect( boost::bind( &PaintTool::selectedPathsChanged, this ) );

	Metadata::plugValueChangedSignal().connect( boost::bind( &PaintTool::metadataChanged, this, ::_3 ) );
	Metadata::nodeValueChangedSignal().connect( boost::bind( &PaintTool::metadataChanged, this, ::_2 ) );

	m_brushOutline = new BrushOutline();
	m_brushOutline->setVisible( false );
	view->viewportGadget()->setChild( "__paintBrushOutline", m_brushOutline );

	// Init the brush gadget
	plugDirtied( sizePlug() );
}

PaintTool::~PaintTool()
{
    // NOTE : ensure that the gadget's reference to the tool is reset

    static_cast<PaintGadget *>( m_gadget.get() )->resetTool();
}

const std::vector<PaintTool::Selection> &PaintTool::selection() const
{
	updateSelection();
	return m_selection;
}

bool PaintTool::selectionEditable() const
{
	const auto &s = selection();
	if( s.empty() )
	{
		return false;
	}
	for( const auto &e : s )
	{
		if( !e.editable() )
		{
			return false;
		}
	}
	return true;
}

PaintTool::SelectionChangedSignal &PaintTool::selectionChangedSignal()
{
	return m_selectionChangedSignal;
}

GafferScene::ScenePlug *PaintTool::scenePlug()
{
	return getChild<ScenePlug>( g_firstPlugIndex );
}

const GafferScene::ScenePlug *PaintTool::scenePlug() const
{
	return getChild<ScenePlug>( g_firstPlugIndex );
}

Gaffer::StringPlug *PaintTool::variableNamePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::StringPlug *PaintTool::variableNamePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

Gaffer::IntPlug *PaintTool::variableTypePlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 2 );
}

const Gaffer::IntPlug *PaintTool::variableTypePlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 2 );
}

Gaffer::IntPlug *PaintTool::modePlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 3 );
}

const Gaffer::IntPlug *PaintTool::modePlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 3 );
}

Gaffer::FloatPlug *PaintTool::sizePlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 4 );
}

const Gaffer::FloatPlug *PaintTool::sizePlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 4 );
}

Gaffer::FloatPlug *PaintTool::floatValuePlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 5 );
}

const Gaffer::FloatPlug *PaintTool::floatValuePlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 5 );
}

Gaffer::Color3fPlug *PaintTool::colorValuePlug()
{
	return getChild<Color3fPlug>( g_firstPlugIndex + 6 );
}

const Gaffer::Color3fPlug *PaintTool::colorValuePlug() const
{
	return getChild<Color3fPlug>( g_firstPlugIndex + 6 );
}

Gaffer::FloatPlug *PaintTool::opacityPlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 7 );
}

const Gaffer::FloatPlug *PaintTool::opacityPlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 7 );
}

Gaffer::FloatPlug *PaintTool::hardnessPlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 8 );
}

const Gaffer::FloatPlug *PaintTool::hardnessPlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 8 );
}

Gaffer::IntPlug *PaintTool::visualiseModePlug()
{
	return getChild<IntPlug>( g_firstPlugIndex + 9 );
}

const Gaffer::IntPlug *PaintTool::visualiseModePlug() const
{
	return getChild<IntPlug>( g_firstPlugIndex + 9 );
}

/*GafferUI::Gadget *PaintTool::handles()
{
	return m_handles.get();
}

const GafferUI::Gadget *PaintTool::handles() const
{
	return m_handles.get();
}

bool PaintTool::affectsHandles( const Gaffer::Plug *input ) const
{
	return input == sizePlug();
}*/

void PaintTool::contextChanged()
{
	// Context changes may change the scene hierarchy or transform,
	// which impacts on the selection and handles.
	selectedPathsChanged();
}

void PaintTool::selectedPathsChanged()
{
	m_selectionDirty = true;
	selectionChangedSignal()( *this );
	m_gadgetDirty = true;
	m_priorityPathsDirty = true;
	updateCursor();
}

void PaintTool::plugDirtied( const Gaffer::Plug *plug )
{
	// Note : This method is called not only when plugs
	// belonging to the PaintTool are dirtied, but
	// _also_ when plugs belonging to the View are dirtied.

	if(
		plug == activePlug() ||
		plug == scenePlug()->childNamesPlug() ||
		plug == scenePlug()->transformPlug() ||
		// The `ancestor()` check protects us from accessing an
		// already-destructed View in the case that the View is
		// destroyed before the Tool.
		/// \todo It'd be good to have a more robust solution to
		/// this, perhaps with Tools being owned by Views so that
		/// the validity of `Tool::m_view` can be managed. Also
		/// see comments in `__toolPlugSet()` in Viewer.py.
		( plug->ancestor<View>() && plug == view()->editScopePlug() )
	)
	{
		m_selectionDirty = true;
		if( !m_dragging )
		{
			// See associated comment in `updateSelection()`, and
			// `dragEnd()` where we emit to complete the
			// deferral started here.
			selectionChangedSignal()( *this );
		}
		m_gadgetDirty = true;
		m_priorityPathsDirty = true;
	}
	else if( plug == variableNamePlug() || plug == visualiseModePlug() )
	{
		m_gadgetDirty = true;
		view()->viewportGadget()->renderRequestSignal()(
			view()->viewportGadget()
		);
	}
	else if( plug == sizePlug() || plug == hardnessPlug() )
	{
		m_brushOutline->setBrush( sizePlug()->getValue(), hardnessPlug()->getValue() );
	}

	/*if( affectsHandles( plug ) )
	{
		m_handlesDirty = true;
	}*/

	if( plug == activePlug() )
	{
		bool active = activePlug()->getValue();

		view()->getChild<Plug>( "drawingMode" )->getChild<BoolPlug>( "hideSelected" )->setValue( active );
		if( active )
		{
			m_preRenderConnection = view()->viewportGadget()->preRenderSignal().connect( boost::bind( &PaintTool::preRender, this ) );
		}
		else
		{
			m_preRenderConnection.disconnect();
			m_gadget->setVisible( false );
			m_brushOutline->setVisible( false );
			SceneGadget *sceneGadget = static_cast<SceneGadget *>( view()->viewportGadget()->getPrimaryChild() );
			sceneGadget->setPriorityPaths( IECore::PathMatcher() );
		}
	}
}

void PaintTool::metadataChanged( IECore::InternedString key )
{
	if( !MetadataAlgo::readOnlyAffectedByChange( key ) )
	{
		return;
	}

	// We could spend a little time here to figure out if the metadata
	// change definitely applies to `selection().transformPlug`, but that
	// would involve computing our selection, which might trigger a compute.
	// Our general rule is to delay all computes until `preRender()`
	// so that a hidden Viewer has no overhead, so we just assume the worst
	// for now and do a more accurate analysis in `updateHandles()`.

	if( !m_selectionDirty )
	{
		m_selectionDirty = true;
		selectionChangedSignal()( *this );
	}

	/*if( !m_handlesDirty )
	{
		m_handlesDirty = true;
		view()->viewportGadget()->renderRequestSignal()(
			view()->viewportGadget()
		);
	}*/
}

void PaintTool::updateSelection() const
{
	if( !m_selectionDirty )
	{
		return;
	}

	if( m_dragging )
	{
		// In theory, an expression or some such could change the effective
		// transform plug while we're dragging (for instance, by driving the
		// enabled status of a downstream transform using the translate value
		// we're editing). But we ignore that on the grounds that it's unlikely,
		// and also that it would be very confusing for the selection to be
		// changed mid-drag.
		return;
	}

	// Clear the selection.
	m_selection.clear();
	m_selectionDirty = false;

	// If we're not active, then there's
	// no need to do anything.
	if( !activePlug()->getValue() )
	{
		return;
	}

	// If there's no input scene, then there's no need to
	// do anything. Our `scenePlug()` receives its input
	// from the View's input, but that doesn't count.
	const ScenePlug *scene = scenePlug()->getInput<ScenePlug>();
	scene = scene ? scene->getInput<ScenePlug>() : scene;
	if( !scene )
	{
		return;
	}

	// Otherwise we need to populate our selection from
	// the scene selection.

	const PathMatcher selectedPaths = ScriptNodeAlgo::getSelectedPaths( view()->scriptNode() );
	if( selectedPaths.isEmpty() )
	{
		return;
	}

	ScenePlug::ScenePath lastSelectedPath = ScriptNodeAlgo::getLastSelectedPath( view()->scriptNode() );
	assert( selectedPaths.match( lastSelectedPath ) & IECore::PathMatcher::ExactMatch );

	for( PathMatcher::Iterator it = selectedPaths.begin(), eIt = selectedPaths.end(); it != eIt; ++it )
	{
		Selection selection( scene, *it, view()->context(), const_cast<EditScope *>( view()->editScope() ) );
		m_selection.push_back( selection );
		/*if( *it == lastSelectedPath )
		{
			lastSelectedPath = selection.path();
		}*/
	}

	// Multiple selected paths may have transforms originating from
	// the same node, in which case we need to remove duplicates from
	// `m_selection`. We also need to make sure that the selection for
	// `lastSelectedPath` appears last, and isn't removed by the
	// deduplication.

	// Sort by `editTarget()`, ensuring `lastSelectedPath` comes first
	// in its group (so it survives deduplication).

	/*std::sort(
		m_selection.begin(), m_selection.end(),
		[&lastSelectedPath]( const Selection &a, const Selection &b )
		{
			const auto ta = editTargetOrNull( a );
			const auto tb = editTargetOrNull( b );
			if( ta < tb )
			{
				return true;
			}
			else if( tb < ta )
			{
				return false;
			}
			return ( a.path() != lastSelectedPath ) < ( b.path() != lastSelectedPath );
		}
	);

	// Deduplicate by `editTarget()`, being careful to avoid removing
	// items in EditScopes where the plug hasn't been created yet.

	auto last = std::unique(
		m_selection.begin(), m_selection.end(),
		[]( const Selection &a, const Selection &b )
		{
			const auto ta = editTargetOrNull( a );
			const auto tb = editTargetOrNull( b );
			return
				ta && tb &&
				ta != a.editScope() &&
				tb != b.editScope() &&
				ta == tb
			;
		}
	);
	m_selection.erase( last, m_selection.end() );

	// Move `lastSelectedPath` to the end

	auto lastSelectedIt = std::find_if(
		m_selection.begin(), m_selection.end(),
		[&lastSelectedPath]( const Selection &x )
		{
			return x.path() == lastSelectedPath;
		}
	);

	if( lastSelectedIt != m_selection.end() )
	{
		std::swap( m_selection.back(), *lastSelectedIt );
	}
	else
	{
		// We shouldn't get here, because ScriptNodeAlgo guarantees that lastSelectedPath is
		// contained in selectedPaths, and we've preserved lastSelectedPath through our
		// uniquefication process. But we could conceivably get here if an extension has
		// edited "ui:scene:selectedPaths" directly instead of using ScriptNodeAlgo,
		// in which case we emit a warning instead of crashing.
		IECore::msg( IECore::Msg::Warning, "PaintTool::updateSelection", "Last selected path not included in selection" );
	}*/
}

void PaintTool::preRender()
{
	// TODO - what is m_dragging doing?
	/*if( !m_dragging )
	{
		updateSelection();
		if( m_priorityPathsDirty )
		{
			m_priorityPathsDirty = false;
			SceneGadget *sceneGadget = static_cast<SceneGadget *>( view()->viewportGadget()->getPrimaryChild() );
			if( selection().size() )
			{
				sceneGadget->setPriorityPaths( ScriptNodeAlgo::getSelectedPaths( view()->scriptNode() ) );
			}
			else
			{
				sceneGadget->setPriorityPaths( IECore::PathMatcher() );
			}
		}
	}*/

	// TODO - should have an accessor paintGadget()
    static_cast<PaintGadget *>( m_gadget.get() )->setVisualiseMode( visualiseModePlug()->getValue() );
	m_gadget->setVisible( true );

	if( m_gadgetDirty )
	{
		m_gadgetDirty = false;
	}
}

/*void PaintTool::dragBegin()
{
	m_dragging = true;
}*/

IECore::RunTimeTypedPtr PaintTool::dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
{
	// TODO - used to prevent clearing selection
	//m_dragging = true;
	/*if( !m_ourButtonPress )
	{
		return nullptr;
	}*/

	return gadget;

	/*
	// Derived classes may wish to override the handling of buttonPress. To
	// consume the event, they must return true from it. This also tells the
	// drag system that they may wish to start a drag later, and so it will
	// then call 'dragBegin'. If they have no interest in actually performing
	// a drag (as maybe they just wanted to do something on click) this is a
	// real pain as now they also have to implement dragBegin to prevent the
	// code below from doing its thing. To avoid this boilerplate overhead,
	// we only start our own drag if we know we were the one who returned
	// true from buttonPress. We also track whether we initiated a drag so
	// the other drag methods can early-out accordingly.
	m_initiatedDrag = false;
	if( !m_acceptedButtonPress )
	{
		return nullptr;
	}
	m_acceptedButtonPress = false;

	SceneGadget *sg = sceneGadget();
	ScenePlug::ScenePath objectUnderMouse;

	if( !sg->objectAt( event.line, objectUnderMouse ) )
	{
		// drag to select
		dragOverlay()->setStartPosition( event.line.p1 );
		dragOverlay()->setEndPosition( event.line.p1 );
		dragOverlay()->setVisible( true );
		m_initiatedDrag = true;
		return gadget;
	}
	else
	{
		const PathMatcher &selection = sg->getSelection();
		if( selection.match( objectUnderMouse ) & PathMatcher::ExactMatch )
		{
			// drag the selection somewhere
			IECore::StringVectorDataPtr dragData = new IECore::StringVectorData();
			selection.paths( dragData->writable() );
			Pointer::setCurrent( "objects" );
			m_initiatedDrag = true;
			return dragData;
		}
	}
	return nullptr;*/
}

bool PaintTool::dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
{
	//return event.sourceGadget == gadget; // && event.data == gadget;
	return event.data == gadget;
}

bool PaintTool::dragMove( const GafferUI::DragDropEvent &event )
{
    /*if( !m_dragging )
    {
        return false;
    }*/

	if( event.buttons != ButtonEvent::Left || event.modifiers )
	{
		return false;
	}

	if( !activePlug()->getValue() )
	{
		return false;
	}

	m_brushOutline->setPos( V2f( event.line.p1.x, event.line.p1.y ) );

	// TODO
	DirtyPropagationScope dirtyPropagationScope;

	//V2f dir( event.line.p0.x, event.line.p0.y );
	//paint( dir, dir ); //sceneGadget() );

	V2f end( event.line.p0.x, event.line.p0.y );
	V2f drag = end - m_dragPosition;


	const float toolSize = sizePlug()->getValue();
	const float targetSpacing = std::max( 1.0f, toolSize * 0.333f );

	float dragLength = drag.length();

	int numSamples = std::max( 1, int( ceilf( dragLength / targetSpacing ) ) );

	float opacity = opacityPlug()->getValue();
	float hardness = hardnessPlug()->getValue();
	int mode = modePlug()->getValue();

	IECore::InternedString variableName = variableNamePlug()->getValue();

	std::variant<float, Color3f> value;
	int variableType = variableTypePlug()->getValue();
	if( variableType == FloatVectorData::staticTypeId() )
	{
		value = floatValuePlug()->getValue();
	}
	else if( variableType == Color3fVectorData::staticTypeId() )
	{
		value = colorValuePlug()->getValue();
	}
	else
	{
		throw IECore::Exception( fmt::format( "PaintTool unsupported type {}", variableType ) );
	}

	for( int i = 0; i < numSamples; i++ )
	{
		V2f rasterPos = m_dragPosition + drag * float( i + 1 ) / numSamples;
		M44f paintMatrix = view()->viewportGadget()->projectionMatrix() * M44f(
			1.0f / toolSize, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f / toolSize, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-rasterPos.x / toolSize, -rasterPos.y / toolSize, 0.0f, 1.0f
		);

		paint( variableName, paintMatrix, value, opacity, hardness, mode );
	}

	m_dragPosition = end;

	int visualiseMode = visualiseModePlug()->getValue();
	if( visualiseMode == 1 )
	{
		applyCurrentStroke();
	}

	// TODO
	view()->viewportGadget()->renderRequestSignal()(
		view()->viewportGadget()
	);

	return true;
}

bool PaintTool::dragEnd( const GafferUI::DragDropEvent &event )
{
	applyCurrentStroke();
	m_dragging = false;
	return true;
}

/*void PaintTool::dragEnd()
{
	m_dragging = false;
	m_mergeGroupId++;
	selectionChangedSignal()( *this );
}*/

std::string PaintTool::undoMergeGroup() const
{
	return fmt::format( "PaintTool{}{}", fmt::ptr( this ), m_mergeGroupId );
}

void PaintTool::paint( const IECore::InternedString &variableName, const M44f &projectionMatrix, const std::variant<float, Color3f> &value, float opacity, float hardness, int mode )
{

	//std::cerr << projectionMatrix << "\n";
	//LineSegment3f foo = view()->viewportGadget()->rasterToGadgetSpace( start, view()->viewportGadget()->getPrimaryChild() );

	/*
	float opacity = opacityPlug()->getValue();
	float hardness = hardnessPlug()->getValue();
	float toolSize = sizePlug()->getValue();
	M44f paintMatrix = view()->viewportGadget()->projectionMatrix() * M44f(
		1.0f / toolSize, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / toolSize, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-start.x / toolSize, -start.y / toolSize, 0.0f, 1.0f
	);
	Imath::Color3f color = colorValuePlug()->getValue();
	*/

	int resolution = 32;
	const M44f projectionMatrixPadded = projectionMatrix * M44f(
		//resolution / (resolution + 1.0f ), 0.0f, 0.0f, 0.0f,
		//0.0f, resolution / (resolution + 1.0f ), 0.0f, 0.0f,
		( resolution - 1.0f ) / resolution, 0.0f, 0.0f, 0.0f,
		0.0f, ( resolution - 1.0f ) / resolution, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	/*std::vector<float> depthMap( resolution * resolution, std::numeric_limits<float>::infinity() );

	for( const auto &s : selection() )
	{
		IECoreScene::ConstMeshPrimitivePtr mesh = IECore::runTimeCast< const IECoreScene::MeshPrimitive>( s.scene()->object( s.path() ) );
		if( !mesh )
		{
			continue;
		}

		const EvaluationData evalData = g_evaluatorCache.get( mesh );
		IECoreScene::PrimitiveEvaluator::ResultPtr result = evalData.evaluator->createResult();

		M44f localPaintMatrix = s.scene()->transform( s.path() ) * projectionMatrix;
		M44f localPaintInverseMatrix = ( s.scene()->transform( s.path() ) * projectionMatrix ).inverse();

		//std::cerr << localPaintInverseMatrix << "\n";
		for( int iy = 0; iy < resolution; iy++ )
		{
			float y = ( 2.0f * iy ) / ( resolution - 1 ) - 1.0f;
			for( int ix = 0; ix < resolution; ix++ )
			{
				float x = ( 2.0f * ix ) / ( resolution - 1 ) - 1.0f;

				V3f near, far;
				localPaintInverseMatrix.multVecMatrix( V3f( x, y, -1 ), near );
				localPaintInverseMatrix.multVecMatrix( V3f( x, y, 1 ), far );
				//std::cerr << "N: " << near << " : " << far << "\n";

				if( evalData.evaluator->intersectionPoint( near, ( far - near ).normalize(), result.get(), ( far - near ).length() ) )
				{
					//float distance = ( result->point() - near ).length();
					V3f proj;
					localPaintMatrix.multVecMatrix( result->point(), proj );
					float distance = proj.z;
					depthMap[ resolution * iy + ix ] = std::min( depthMap[ resolution * iy + ix ], distance );
				}
			}
		}
	}

	std::cerr << "DEPTHS : ";
	for( int iy = 0; iy < resolution; iy++ )
	{
		for( int ix = 0; ix < resolution; ix++ )
		{
			std::cerr << " " << depthMap[ resolution * iy + ix ];
		}
		std::cerr << "\n";
	}

	const float* newDepthMap = m_depthRender.render( projectionMatrixPadded, view()->viewportGadget(), Gadget::Layer::Main );
	std::cerr << "NEW DEPTHS : ";
	for( int iy = 0; iy < resolution; iy++ )
	{
		for( int ix = 0; ix < resolution; ix++ )
		{
			std::cerr << " " << 2.0f * newDepthMap[ resolution * iy + ix ] - 1.0f;
		}
		std::cerr << "\n";
	}

	std::cerr << "OUTLINE : ";
	for( int iy = 0; iy < resolution; iy++ )
	{
		for( int ix = 0; ix < resolution; ix++ )
		{
			bool newIn = newDepthMap[ resolution * iy + ix ] != 1.0f;
			std::cerr << " " << ( std::isfinite( depthMap[ resolution * iy + ix ] ) ? ( newIn ? "1" : "Q" ) : ( newIn ? "X" : "0" ) );
		}
		std::cerr << "\n";
	}*/
	//const float* depthMap = m_depthRender.render( projectionMatrixPadded, view()->viewportGadget(), Gadget::Layer::Main );

	//std::cerr << "DEPTH RENDER\n";
	const float* depthMap = m_depthRender.render( projectionMatrixPadded, view()->viewportGadget(), Gadget::Layer( 0 ) );
	//std::cerr << "DEPTH RENDER DONE\n";
	//std::cerr << "depthMap : " << depthMap[0] << "\n";


	//int numDepths = 0;
	//float sumDepths = 0.0f;

	M44f kdTreeProjection = view()->viewportGadget()->projectionMatrix();
	int visualiseMode = visualiseModePlug()->getValue();

	for( const auto &s : selection() )
	{
		if( !s.editable() )
		{
			continue;
		}

		IECore::InternedString tempPath = ScenePlug::pathToString( s.upstreamPath() ); // TODO
		/*IECoreScene::ConstMeshPrimitivePtr mesh = IECore::runTimeCast< const IECoreScene::MeshPrimitive>( s.scene()->object( s.path() ) );*/
		IECoreScene::ConstMeshPrimitivePtr mesh;

		{
			ScenePlug::PathScope scope( s.context(), &s.path() );
			if( visualiseMode == 1 || !s.m_paintEdit )
			{
				// Check path exists
				if( !s.scene()->existsPlug()->getValue() )
				{
					continue;
				}

				// Extract mesh primitive
				mesh = runTimeCast<const IECoreScene::MeshPrimitive>( s.scene()->objectPlug()->getValue() );
			}
			else
			{
				//TODO - should the cache be inside the paint or something?
				const PrimitiveVariablePaint *primVarPaint = IECore::runTimeCast<PrimitiveVariablePaint>( (*s.m_paintEdit->dataPlug()->outputs().begin())->node() );
				if( !primVarPaint )
				{
					throw IECore::Exception( "TODO QQQQQQ" );
				}

				// Check path exists
				if( !primVarPaint->outPlug()->existsPlug()->getValue() )
				{
					continue;
				}
				mesh = IECore::runTimeCast< const IECoreScene::MeshPrimitive>( primVarPaint->outPlug()->object( s.upstreamPath() ) );
			}
		}

		if( !mesh )
		{
			continue;
		}

		if( !mesh->arePrimitiveVariablesValid() )
		{
			throw IECore::Exception( "Cannot paint mesh with invalid variables" );
		}

		auto pVar = mesh->variableIndexedView<V3fVectorData>( "P", IECoreScene::PrimitiveVariable::Vertex, true );
		if( !pVar )
		{
			throw IECore::Exception( "Vertex P prim var required for painting" );
		}


		if( s.m_kdTreeProjection != kdTreeProjection )
		{
			s.m_kdTreePoints.clear();
			s.m_kdTreePoints.reserve( pVar->size() );

			M44f localKDTreeMatrix = s.scene()->fullTransform( s.path() ) * kdTreeProjection;

			for( const V3f &p : *pVar )
			{
				Imath::V3f projected;
				localKDTreeMatrix.multVecMatrix( p, projected );
				s.m_kdTreePoints.push_back( Imath::V2f( projected.x, projected.y ) );
			}

			s.m_kdTree.init( s.m_kdTreePoints.begin(), s.m_kdTreePoints.end(), 4 );
			s.m_kdTreeProjection = kdTreeProjection;
		}

		M44f localPaintMatrix = s.scene()->fullTransform( s.path() ) * projectionMatrix;

		M44f localPaintToKDTree = localPaintMatrix.inverse() * kdTreeProjection;

		Box2f kdTreeLookupBound;
		V3f corner;
		localPaintToKDTree.multVecMatrix( V3f( 1, 1, 1 ), corner );
		kdTreeLookupBound.extendBy( V2f( corner.x, corner.y ) );
		localPaintToKDTree.multVecMatrix( V3f( -1, 1, 1 ), corner );
		kdTreeLookupBound.extendBy( V2f( corner.x, corner.y ) );
		localPaintToKDTree.multVecMatrix( V3f( 1, -1, 1 ), corner );
		kdTreeLookupBound.extendBy( V2f( corner.x, corner.y ) );
		localPaintToKDTree.multVecMatrix( V3f( -1, -1, 1 ), corner );
		kdTreeLookupBound.extendBy( V2f( corner.x, corner.y ) );

        UglyArray kdTreeIterators;

        s.m_kdTree.enclosedPoints( kdTreeLookupBound, std::back_insert_iterator<UglyArray>( kdTreeIterators ) );

		//M44f localPaintMatrix = projectionMatrix * s.scene()->transform( s.path() );


		/*CompoundData* varPaintData = locPaintData->member< CompoundData >( variableName );
		if( !varPaintData )
		{
			// TODO - order backwards, doesn't hold smart pointer?
			varPaintData = new CompoundData();
			locPaintData->writable()[variableName] = varPaintData;
		}*/

		if( !s.m_currentStroke )
		{
			throw IECore::Exception( fmt::format( "FOO FOO FOO : {} : {} : {}", ScenePlug::pathToString( s.upstreamPath() ), (size_t)&s, (size_t) s.m_currentStroke.get() ) );
		}

		FloatVectorData* opacityData = s.m_currentStroke->member< FloatVectorData >( "opacity" );
		/*if( !opacityData )
		{
			opacityData = new FloatVectorData();
			varPaintData->writable()["opacity"] = opacityData;
		}*/

		bool modified = false;
		if( std::holds_alternative<float>( value ) )
		{
			FloatVectorData* valueData = s.m_currentStroke->member< FloatVectorData >( "value" );
			/*if( !valueData )
			{
				valueData = new FloatVectorData();
				varPaintData->writable()["value"] = valueData;
			}*/

			modified = applyPaint<float>( valueData->writable(), opacityData->writable(), std::get<float>( value ), opacity, mode, hardness, *pVar, localPaintMatrix, resolution, depthMap, kdTreeIterators, &s.m_kdTreePoints[0] );
		}
		else
		{
			Color3fVectorData* valueData = s.m_currentStroke->member< Color3fVectorData >( "value" );
			/*if( !valueData )
			{
				valueData = new Color3fVectorData();
				varPaintData->writable()["value"] = valueData;
			}*/

			//applyPaint<Color3f>( s.m_colorValue, opacityData->writable(), std::get<Color3f>( value ), opacity, mode, hardness, *pVar, localPaintMatrix, resolution, depthMap );
			modified = applyPaint<Color3f>( valueData->writable(), opacityData->writable(), std::get<Color3f>( value ), opacity, mode, hardness, *pVar, localPaintMatrix, resolution, depthMap, kdTreeIterators, &s.m_kdTreePoints[0] );
		}
		if( modified )
		{
			s.m_currentStrokeDirty = true;
		}

		/*if( visualiseMode == 1 )
		{
			// TODO TODO TODO
			if( !s.m_initialEditValue )
			{
				s.m_initialEditValue = new IECore::CompoundData();
			}

			CompoundDataPtr newVal = newPrimVarComposite(
				std::holds_alternative<float>( value ) ? FloatVectorDataTypeId : Color3fVectorDataTypeId,
				opacityData->readable().size(), true
			);
			applyPrimVarComposite( s.m_initialEditValue.get(), s.m_currentStroke.get(), 1, opacity, newVal.get() );

			// TODO - lift out of loop
			CachedDataNode* paintEdit = s.acquirePaintEdit( true );

			if( !paintEdit )
			{
				throw IECore::Exception( "Why no paint edit? TODO" );
			}

			IECore::InternedString curPath = ScenePlug::pathToString( s.upstreamPath() );

			ConstCompoundDataPtr sourceData = IECore::runTimeCast<const CompoundData>( paintEdit->getEntry( curPath, false ) );

			CompoundDataPtr locPaintData;
			if( sourceData )
			{
				locPaintData = sourceData->copy();
			}
			else
			{
				locPaintData = new CompoundData();
			}

			locPaintData->writable()[variableName] = newVal;

			paintEdit->setEntry( curPath, locPaintData );
		}*/
	}

	//std::cerr << "AVG MAP : " << std::accumulate( depthMap.begin(), depthMap.end(), 0.0f ) / depthMap.size() << " AVG LOOKUP : " << sumDepths / numDepths << "\n";

	/*M44f cameraTrans = view()->viewportGadget()->getCameraTransform();
	V3f origin = cameraTrans.translation();
	V3f dir = foo * cameraTrans - origin;
	std::cerr << origin << " : " << dir << "\n";*/
}

CompoundDataPtr PaintTool::targetVariableTypes()
{
	CompoundDataPtr resultData = new CompoundData;
	auto &result = resultData->writable();

	result["Cs"] = new IntData( Color3fVectorData::staticTypeId() );
	for( const auto &s : selection() )
	{
		IECoreScene::ConstPrimitivePtr prim = IECore::runTimeCast< const IECoreScene::Primitive>( s.scene()->object( s.path() ) );
		if( !prim )
		{
			continue;
		}

		for( const auto &v : prim->variables )
		{
			IECore::TypeId typeId = v.second.data->typeId();

			if( !( typeId == FloatVectorData::staticTypeId() || typeId == Color3fVectorData::staticTypeId() ) )
			{
				// TODO - should we expand this support a bit?
				continue;
			}

			auto existingIt = result.find( v.first );
			if( existingIt == result.end() )
			{
				result[v.first] = new IntData( typeId );
			}
			else
			{
				int &oldTypeId = IECore::runTimeCast<IntData>( existingIt->second )->writable();
				if( oldTypeId != typeId )
				{
					oldTypeId = 0;
				}
			}
		}
	}
	return resultData;
}

bool PaintTool::enter( const ButtonEvent &event )
{
	m_mouseIn = true;
	updateCursor();
	return false;
}

bool PaintTool::leave( const ButtonEvent &event )
{
	m_mouseIn = false;
	updateCursor();
	return false;
}

bool PaintTool::mouseMove( const ButtonEvent &event )
{
	// TODO - shouldn't be necessary to update cursor this often if we can accurately track when it should be changed
	// ( Currently I've been noticing this going wrong after middle mouse panning )
	updateCursor();

	m_brushOutline->setPos( V2f( event.line.p1.x, event.line.p1.y ) );
	return false;
}

bool PaintTool::buttonPress( const GafferUI::ButtonEvent &event )
{
	if( event.buttons != ButtonEvent::Left || event.modifiers )
	{
		return false;
	}

	if( !activePlug()->getValue() )
	{
		return false;
	}

	if( !selectionEditable() )
	{
		// We're not editable, but we should still consume the event
		return true;
	}

	// TODO - now about button being down rather than dragging?
	m_dragging = true;

	IECore::InternedString variableName = variableNamePlug()->getValue();
	IECore::TypeId variableType = (IECore::TypeId)variableTypePlug()->getValue();

	for( const auto &s : selection() )
	{
		if( !s.m_paintEdit )
		{
			s.m_paintEdit = s.acquirePaintEdit( true );
		}

		IECore::ConstCompoundDataPtr paintEntry = IECore::runTimeCast<const CompoundData>(
			s.m_paintEdit->getEntry( ScenePlug::pathToString( s.upstreamPath() ), false )
		);
		s.m_initialEditValue = nullptr;
		if( paintEntry )
		{
			s.m_initialEditValue = paintEntry->member< const CompoundData >( variableName );
		}

		//TODO - should the cache be inside the paint or something?
		const PrimitiveVariablePaint *primVarPaint = IECore::runTimeCast<PrimitiveVariablePaint>( (*s.m_paintEdit->dataPlug()->outputs().begin())->node() );
		if( !primVarPaint )
		{
			throw IECore::Exception( "TODO QQQQQQ" );
		}
		IECoreScene::ConstMeshPrimitivePtr mesh = IECore::runTimeCast< const IECoreScene::MeshPrimitive>( primVarPaint->inPlug()->object( s.upstreamPath() ) );
		if( !mesh )
		{
			continue;
		}

		size_t numVerts = mesh->variableSize( IECoreScene::PrimitiveVariable::Interpolation::Vertex );

		s.m_currentStroke = newPrimVarComposite( variableType, numVerts );
		s.m_currentStrokeDirty = true;

		s.m_composedInputValue = new CompoundData();
		const Data *inputData = mesh->variableData<Data>( variableName );
		if( inputData )
		{
			// TODO expand
			// TODO think about COW
			s.m_composedInputValue->writable()["value"] = mesh->variableData<Data>( variableName )->copy();
		}
		else
		{
			s.m_composedInputValue = newPrimVarComposite( variableType, numVerts, false );
		}

		if( s.m_initialEditValue )
		{
			applyPrimVarComposite( s.m_composedInputValue.get(), s.m_initialEditValue.get(), 1, 1.0f, s.m_composedInputValue.get() );
		}

		// TODO - writable shouldn't be needed
		if( !s.m_composedValue || IECore::size( s.m_composedValue->writable()["value"].get() ) != numVerts ) // TODO type
		{
			s.m_composedValue = newPrimVarComposite( variableType, numVerts );
		}


		//std::cerr << "TEST SETUP : " << ScenePlug::pathToString( s.upstreamPath() ) << " : " << (size_t)&s << " : " << (size_t) s.m_currentStroke.get() << "\n";
		/*s.m_currentStroke = new CompoundData();

		FloatVectorDataPtr opacityData = new FloatVectorData();
		opacityData->writable().resize( numVerts, 0.0f );
		s.m_currentStroke->writable()["opacity"] = opacityData;
		if( variableType == FloatVectorData::staticTypeId() )
		{
			FloatVectorDataPtr valueData = new FloatVectorData();
			valueData->writable().resize( numVerts, 0.0f );
			s.m_currentStroke->writable()["value"] = valueData;
		}
		else
		{
			Color3fVectorDataPtr valueData = new Color3fVectorData();
			valueData->writable().resize( numVerts, 0.0f );
			s.m_currentStroke->writable()["value"] = valueData;
		}*/
	}

	//m_ourButtonPress = true;

	//Imath::V3f direction = event.line.p1 - event.line.p0

	float opacity = opacityPlug()->getValue();
	float hardness = hardnessPlug()->getValue();
	int mode = modePlug()->getValue();
	float toolSize = sizePlug()->getValue();

	V2f rasterPos( event.line.p0.x, event.line.p0.y );
	M44f paintMatrix = view()->viewportGadget()->projectionMatrix() * M44f(
		1.0f / toolSize, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / toolSize, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-rasterPos.x / toolSize, -rasterPos.y / toolSize, 0.0f, 1.0f
	);
	m_dragPosition = rasterPos;


	std::variant<float, Color3f> value;
	//int variableType = variableTypePlug()->getValue();
	if( variableType == FloatVectorData::staticTypeId() )
	{
		value = floatValuePlug()->getValue();
	}
	else if( variableType == Color3fVectorData::staticTypeId() )
	{
		value = colorValuePlug()->getValue();
	}
	else
	{
		throw IECore::Exception( fmt::format( "PaintTool unsupported type {}", variableType ) );
	}

	// TODO
	DirtyPropagationScope dirtyPropagationScope;
	paint( variableName, paintMatrix, value, opacity, hardness, mode );
	//V3f foo = view()->viewportGadget()->rasterToGadgetSpace( Imath::V2f( event.line.p0.x, event.line.p0,y ), sceneGadget() );

	//std::cerr << event.line.p0 << " : " << event.line.p1 << "\n";
	//std::cerr << foo << "\n";

	// TODO
	view()->viewportGadget()->renderRequestSignal()(
		view()->viewportGadget()
	);
	return true;
}

void PaintTool::applyCurrentStroke()
{
	IECore::InternedString variableName = variableNamePlug()->getValue();
	IECore::TypeId variableType = (IECore::TypeId)variableTypePlug()->getValue();
	float opacity = opacityPlug()->getValue();
	// TODO share code
	for( const auto &s : selection() )
	{
		if( !s.m_currentStroke )
		{
			continue;
		}
		// TODO TODO TODO
		/*if( !s.m_initialEditValue )
		{
			s.m_initialEditValue = new IECore::CompoundData();
		}*/

		CompoundDataPtr newVal;
		if( s.m_initialEditValue )
		{
			newVal = newPrimVarComposite( variableType, IECore::size( s.m_currentStroke->writable()["opacity"].get() ), true );
			applyPrimVarComposite( s.m_initialEditValue.get(), s.m_currentStroke.get(), 1, opacity, newVal.get() );
		}
		else
		{
			newVal = s.m_currentStroke->copy();
			s.m_initialEditValue = newVal;
		}

		// TODO - lift out of loop
		CachedDataNode* paintEdit = s.acquirePaintEdit( true );

		if( !paintEdit )
		{
			throw IECore::Exception( "Why no paint edit? TODO" );
		}

		IECore::InternedString curPath = ScenePlug::pathToString( s.upstreamPath() );

		ConstCompoundDataPtr sourceData = IECore::runTimeCast<const CompoundData>( paintEdit->getEntry( curPath, false ) );

		CompoundDataPtr locPaintData;
		if( sourceData )
		{
			locPaintData = sourceData->copy();
		}
		else
		{
			locPaintData = new CompoundData();
		}

		locPaintData->writable()[variableName] = newVal;

		paintEdit->setEntry( curPath, locPaintData );
	}
}

void PaintTool::updateCursor()
{
	bool useBrush = m_mouseIn && activePlug()->getValue();
	if( useBrush && !selectionEditable() )
	{
		useBrush = false;
	}

	if( useBrush )
	{
		m_brushOutline->setVisible( true );
		Pointer::setCurrent( "invisible" );
	}
	else
	{
		m_brushOutline->setVisible( false );
		Pointer::setCurrent( "" );
	}

	/*view()->viewportGadget()->renderRequestSignal()(
		view()->viewportGadget()
	);*/
}

bool PaintTool::buttonRelease( const GafferUI::ButtonEvent &event )
{
	m_dragging = false;
	//m_ourButtonPress = false;
	applyCurrentStroke();

	updateCursor();
	return false; // TODO
}

bool PaintTool::keyPress( const GafferUI::KeyEvent &event )
{
	/*if( !activePlug()->getValue() )
	{
		return false;
	}

	if( event.key == "S" && !event.modifiers )
	{
		if( !selectionEditable() )
		{
			return false;
		}

		UndoScope undoScope( view()->scriptNode() );
		for( const auto &s : selection() )
		{
			Context::Scope contextScope( s.context() );
			const auto edit = s.acquireTransformEdit();
			for( const auto &vectorPlug : { edit->translate, edit->rotate, edit->scale, edit->pivot } )
			{
				for( const auto &plug : FloatPlug::Range( *vectorPlug ) )
				{
					if( Animation::canAnimate( plug.get() ) )
					{
						const float value = plug->getValue();
						Animation::CurvePlug* const curve = Animation::acquire( plug.get() );
						curve->insertKey( s.context()->getTime(), value );
					}
				}
			}
		}
		return true;
	}
	else if( event.key == "Plus" || event.key == "Equal" )
	{
		sizePlug()->setValue( sizePlug()->getValue() + 0.2 );
	}
	else if( event.key == "Minus" || event.key == "Underscore" )
	{
		sizePlug()->setValue( max( sizePlug()->getValue() - 0.2, 0.2 ) );
	}
	*/

	return false;
}

bool PaintTool::canSetValueOrAddKey( const Gaffer::FloatPlug *plug )
{
	// Refuse to edit a plug that is driven by ganging. Our use of
	// `source()` below would mean that we'd end up editing the driver,
	// which would cause very unintuitive behaviour.
	if( auto parent = plug->parent<V3fPlug>() )
	{
		if( plug->getInput() && plug->getInput()->parent() == parent )
		{
			return false;
		}
	}

	if( Animation::isAnimated( plug ) )
	{
		// Check editability of the source animation.
		return !MetadataAlgo::readOnly( plug->source() );
	}

	// We expect `plug` to have been acquired via `spreadsheetAwareSource()`
	// courtesy of the Selection class. But that _doesn't_ mean that it can't have
	// an input, because `spreadsheetAwareSource()` operates on the parent V3fPlug,
	// and won't account for an input to an individual component. So we still
	// need to call `source()` to find the plug we really want to edit.
	const FloatPlug *source = plug->source<FloatPlug>();
	if( !source )
	{
		// Input isn't a FloatPlug.
		return false;
	}

	return source->settable() && !MetadataAlgo::readOnly( source );
}

void PaintTool::setValueOrAddKey( Gaffer::FloatPlug *plug, float time, float value )
{
	if( Animation::isAnimated( plug ) )
	{
		Animation::CurvePlug *curve = Animation::acquire( plug );
		curve->insertKey( time, value );
	}
	else
	{
		FloatPlug *source = plug->source<FloatPlug>();
		assert( source );
		source->setValue( value );
	}
}
