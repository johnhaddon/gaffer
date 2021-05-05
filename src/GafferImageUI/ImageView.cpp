//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
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

#include "GafferImageUI/ImageView.h"

#include "GafferImageUI/ImageGadget.h"

#include "GafferImage/Clamp.h"
#include "GafferImage/Format.h"
#include "GafferImage/DeepState.h"
#include "GafferImage/Grade.h"
#include "GafferImage/ImagePlug.h"
#include "GafferImage/ImageSampler.h"
#include "GafferImage/ImageStats.h"

#include "GafferUI/Gadget.h"
#include "GafferUI/Pointer.h"
#include "GafferUI/StandardStyle.h"
#include "GafferUI/Style.h"

#include "Gaffer/ArrayPlug.h"
#include "Gaffer/Context.h"
#include "Gaffer/DeleteContextVariables.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/BoxPlug.h"
#include "Gaffer/Metadata.h"

#include "IECoreGL/IECoreGL.h"
#include "IECoreGL/Shader.h"
#include "IECoreGL/ShaderLoader.h"
#include "IECoreGL/Texture.h"
#include "IECoreGL/TextureLoader.h"
#include "IECoreGL/ToGLTextureConverter.h"

#include "IECore/NullObject.h"
#include "IECore/BoxAlgo.h"
#include "IECore/BoxOps.h"
#include "IECore/FastFloat.h"

#include "OpenEXR/ImathColorAlgo.h"

#include "boost/bind.hpp"
#include "boost/bind/placeholders.hpp"
#include "boost/format.hpp"
#include "boost/lexical_cast.hpp"

#include <cmath>

using namespace boost;
using namespace IECoreGL;
using namespace IECore;
using namespace Imath;
using namespace Gaffer;
using namespace GafferUI;
using namespace GafferImage;
using namespace GafferImageUI;

//////////////////////////////////////////////////////////////////////////
/// Implementation of ImageView::ChannelChooser
//////////////////////////////////////////////////////////////////////////

class ImageView::ChannelChooser : public boost::signals::trackable
{

	public :

		ChannelChooser( ImageView *view )
			:	m_view( view )
		{
			StringVectorDataPtr channelsDefaultData = new StringVectorData;
			std::vector<std::string> &channelsDefault = channelsDefaultData->writable();
			channelsDefault.push_back( "R" );
			channelsDefault.push_back( "G" );
			channelsDefault.push_back( "B" );
			channelsDefault.push_back( "A" );

			view->addChild( new StringVectorDataPlug( "channels", Plug::In, channelsDefaultData ) );

			view->addChild(
				new IntPlug(
					"soloChannel",
					Plug::In,
					/* defaultValue = */ -1,
					/* minValue = */ -1,
					/* maxValue = */ 3
				)
			);

			m_view->plugSetSignal().connect( boost::bind( &ChannelChooser::plugSet, this, ::_1 ) );
			m_view->viewportGadget()->keyPressSignal().connect( boost::bind( &ChannelChooser::keyPress, this, ::_2 ) );

		}

	private :

		StringVectorDataPlug *channelsPlug()
		{
			return m_view->getChild<StringVectorDataPlug>( "channels" );
		}

		IntPlug *soloChannelPlug()
		{
			return m_view->getChild<IntPlug>( "soloChannel" );
		}

		void plugSet( const Gaffer::Plug *plug )
		{
			if( plug == soloChannelPlug() )
			{
				ImageGadget *imageGadget = static_cast<ImageGadget *>(
					m_view->viewportGadget()->getPrimaryChild()
				);
				imageGadget->setSoloChannel( soloChannelPlug()->getValue() );
			}
			else if( plug == channelsPlug() )
			{
				ConstStringVectorDataPtr channelsData = channelsPlug()->getValue();
				const std::vector<std::string> &channels = channelsData->readable();
				ImageGadget::Channels c;
				for( size_t i = 0; i < std::min( channels.size(), (size_t)4 ); ++i )
				{
					c[i] = channels[i];
				}

				ImageGadget *imageGadget = static_cast<ImageGadget *>(
					m_view->viewportGadget()->getPrimaryChild()
				);
				imageGadget->setChannels( c );
			}
		}

		bool keyPress( const GafferUI::KeyEvent &event )
		{
			if( event.modifiers )
			{
				return false;
			}

			const char *rgba[4] = { "R", "G", "B", "A" };
			for( int i = 0; i < 4; ++i )
			{
				if( event.key == rgba[i] )
				{
					soloChannelPlug()->setValue(
						soloChannelPlug()->getValue() == i ? -1 : i
					);
					return true;
				}
			}

			return false;
		}

		ImageView *m_view;

};

//////////////////////////////////////////////////////////////////////////
/// Implementation of ImageView::ColorInspector
//////////////////////////////////////////////////////////////////////////

namespace
{

IECore::InternedString g_hoveredKey( "__hovered" );

class V2fContextVariable : public Gaffer::ComputeNode
{

	public :

		V2fContextVariable( const std::string &name = "V2fContextVariable" )
			:	ComputeNode( name )
		{
			storeIndexOfNextChild( g_firstPlugIndex );
			addChild( new StringPlug( "name" ) );
			addChild( new V2fPlug( "out", Plug::Out ) );
		}

		GAFFER_NODE_DECLARE_TYPE( V2fContextVariable, V2fContextVariableTypeId, ComputeNode );

		StringPlug *namePlug()
		{
			return getChild<StringPlug>( g_firstPlugIndex );
		}

		const StringPlug *namePlug() const
		{
			return getChild<StringPlug>( g_firstPlugIndex );
		}

		V2fPlug *outPlug()
		{
			return getChild<V2fPlug>( g_firstPlugIndex + 1 );
		}

		const V2fPlug *outPlug() const
		{
			return getChild<V2fPlug>( g_firstPlugIndex + 1 );
		}

		void affects( const Plug *input, AffectedPlugsContainer &outputs ) const override
		{
			ComputeNode::affects( input, outputs );

			if( input == namePlug() )
			{
				outputs.push_back( outPlug()->getChild( 0 ) );
				outputs.push_back( outPlug()->getChild( 1 ) );
			}
		}

	protected :

		void hash( const ValuePlug *output, const Context *context, IECore::MurmurHash &h ) const override
		{
			ComputeNode::hash( output, context, h );
			if( output->parent() == outPlug() )
			{
				// TODO - replace with context->variableHash in Gaffer 0.60
				const std::string name = namePlug()->getValue();
				h.append( context->get<V2f>( name, V2f( 0 ) ) );
			}
		}

		void compute( ValuePlug *output, const Context *context ) const override
		{
			if( output->parent() == outPlug() )
			{
				const std::string name = namePlug()->getValue();
				const V2f value = context->get<V2f>( name, V2f( 0 ) );
				const size_t index = output == outPlug()->getChild( 0 ) ? 0 : 1;
				static_cast<FloatPlug *>( output )->setValue( value[index] );
			}
			else
			{
				ComputeNode::compute( output, context );
			}
		}

	private :

		static size_t g_firstPlugIndex;

};

size_t V2fContextVariable::g_firstPlugIndex = 0;
GAFFER_NODE_DEFINE_TYPE( V2fContextVariable )

IE_CORE_DECLAREPTR( V2fContextVariable )

class Box2iContextVariable : public Gaffer::ComputeNode
{

	public :

		Box2iContextVariable( const std::string &name = "Box2iContextVariable" )
			:	ComputeNode( name )
		{
			storeIndexOfNextChild( g_firstPlugIndex );
			addChild( new StringPlug( "name" ) );
			addChild( new Box2iPlug( "out", Plug::Out ) );
		}

		GAFFER_NODE_DECLARE_TYPE( Box2iContextVariable, Box2iContextVariableTypeId, ComputeNode );

		StringPlug *namePlug()
		{
			return getChild<StringPlug>( g_firstPlugIndex );
		}

		const StringPlug *namePlug() const
		{
			return getChild<StringPlug>( g_firstPlugIndex );
		}

		Box2iPlug *outPlug()
		{
			return getChild<Box2iPlug>( g_firstPlugIndex + 1 );
		}

		const Box2iPlug *outPlug() const
		{
			return getChild<Box2iPlug>( g_firstPlugIndex + 1 );
		}

		void affects( const Plug *input, AffectedPlugsContainer &outputs ) const override
		{
			ComputeNode::affects( input, outputs );

			if( input == namePlug() )
			{
				outputs.push_back( outPlug()->minPlug()->getChild( 0 ) );
				outputs.push_back( outPlug()->minPlug()->getChild( 1 ) );
				outputs.push_back( outPlug()->maxPlug()->getChild( 0 ) );
				outputs.push_back( outPlug()->maxPlug()->getChild( 1 ) );
			}
		}

	protected :

		void hash( const ValuePlug *output, const Context *context, IECore::MurmurHash &h ) const override
		{
			ComputeNode::hash( output, context, h );
			const GraphComponent *parent = output->parent();
			if( parent->parent() == outPlug() )
			{
				// TODO - replace with context->variableHash in Gaffer 0.60
				const std::string name = namePlug()->getValue();
				h.append( context->get<Box2i>( name, Box2i() ) );
			}
		}

		void compute( ValuePlug *output, const Context *context ) const override
		{
			const GraphComponent *parent = output->parent();
			if( parent->parent() == outPlug() )
			{
				const std::string name = namePlug()->getValue();
				const Box2i value = context->get<Box2i>( name, Box2i() );

				const size_t index = output == parent->getChild( 0 ) ? 0 : 1;
				float result;
				if( parent == outPlug()->minPlug() )
				{
					result = value.min[index];
				}
				else
				{
					result = value.max[index];
				}
				static_cast<IntPlug *>( output )->setValue( result );
			}
			else
			{
				ComputeNode::compute( output, context );
			}
		}

	private :

		static size_t g_firstPlugIndex;

};


size_t Box2iContextVariable::g_firstPlugIndex = 0;
GAFFER_NODE_DEFINE_TYPE( Box2iContextVariable )

IE_CORE_DECLAREPTR( Box2iContextVariable )

void renderLine2D( const Style *style, V2f a, V2f b, float width, const Color4f &col )
{
	style->renderLine( LineSegment3f( V3f( a.x, a.y, 0.0 ), V3f( b.x, b.y, 0 ) ), width, &col );
}

// TODO - these are some terrible ways of drawing circles, but I just wanted something quick that works.  Add
// something better somewhere central
void renderCircle2D( const Style *style, V2f center, float radius, float width, const Color4f &col )
{
	int segments = 16;
	V2f prevAngle( 1, 0 );
	for( int i = 0; i < segments; i++ )
	{
		V2f angle( cos( 2.0f * M_PI * ( i + 1.0f ) / segments ), sin( 2.0f * M_PI * ( i + 1.0f ) / segments ) );
		renderLine2D( style, center + prevAngle * radius, center + angle * radius, width, col );
		prevAngle = angle;
	}
}

void renderFilledCircle2D( const Style *style, V2f center, float radius, const Color4f &col )
{
	// TODO : Terrible hack, rendering a dummy rectangle which will put the style's shader in a state where
	// it will allow us to draw a polygon
	style->renderRectangle( Box2f( center, center ) );
	int segments = 16;
	glColor( col );
	glBegin( GL_POLYGON );

		for( int i = 0; i < segments; i++ )
		{
			V2f angle( cos( 2.0f * M_PI * ( i + 1.0f ) / segments ), sin( 2.0f * M_PI * ( i + 1.0f ) / segments ) );
			glVertex2f( center.x + angle.x * radius, center.y + angle.y * radius );
		}

	glEnd();
}

class Box2iGadget : public GafferUI::Gadget
{

	public :

		Box2iGadget( Box2iPlugPtr plug, std::string id )
			:   Gadget(), m_plug( plug ), m_id( id ), m_editable( true ), m_handleSize( 10 ), m_hover( 0 ), m_deleting( false )
		{
			mouseMoveSignal().connect( boost::bind( &Box2iGadget::mouseMove, this, ::_2 ) );
			buttonPressSignal().connect( boost::bind( &Box2iGadget::buttonPress, this, ::_2 ) );
			dragBeginSignal().connect( boost::bind( &Box2iGadget::dragBegin, this, ::_1, ::_2 ) );
			dragEnterSignal().connect( boost::bind( &Box2iGadget::dragEnter, this, ::_1, ::_2 ) );
			dragMoveSignal().connect( boost::bind( &Box2iGadget::dragMove, this, ::_2 ) );
			dragEndSignal().connect( boost::bind( &Box2iGadget::dragEnd, this, ::_2 ) );
			buttonReleaseSignal().connect( boost::bind( &Box2iGadget::buttonRelease, this, ::_2 ) );
			leaveSignal().connect( boost::bind( &Box2iGadget::leave, this ) );

			plug->node()->plugDirtiedSignal().connect( boost::bind( &Box2iGadget::plugDirtied, this, ::_1 ) );
		}

		GAFFER_GRAPHCOMPONENT_DECLARE_TYPE( Box2iGadget, Box2iGadgetTypeId, GafferUI::Gadget );

		Imath::Box3f bound() const override
		{
			Box2i rect = m_plug->getValue();
			return Box3f(
				V3f( rect.min.x, rect.min.y, 0 ),
				V3f( rect.max.x, rect.max.y, 0 )
			);
		}

		const Box2iPlug *getPlug() const
		{
			return m_plug.get();
		}

		void setEditable( bool editable )
		{
			m_editable = editable;
		}

		bool getEditable() const
		{
			return m_editable;
		}

		void setHandleSize( bool handleSize )
		{
			m_handleSize = handleSize;
		}

		bool getHandleSize() const
		{
			return m_handleSize;
		}

		typedef boost::signal<void ( Plug * )> DeleteClickedSignal;
		DeleteClickedSignal &deleteClickedSignal()
		{
			return m_deleteClickedSignal;
		}

	protected :

		void doRenderLayer( Layer layer, const Style *style ) const override
		{
			const V2f planarScale = viewportPlanarScale();
			const V2f threshold( planarScale * m_handleSize );
			if( layer != Layer::Main )
			{
				return;
			}

			Box2i rect = m_plug->getValue();
			if( rect.isEmpty() )
			{
				return;
			}

			V2f crossHairSize(
				std::min ( threshold.x, rect.size().x * 0.5f ),
				std::min ( threshold.y, rect.size().y * 0.5f )
			);

			V2f rectCenter( 0.5f * ( V2f( rect.min ) + V2f( rect.max ) ) );
			V2f deleteButtonCenter( rect.max.x + threshold.x, rect.max.y + threshold.y );
			V2f deleteButtonSize( threshold.x * 0.5, threshold.y * 0.5 );
			glPushAttrib( GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT );

				if( IECoreGL::Selector::currentSelector() )
				{
					if( m_editable )
					{
						V2f upperLeft( rect.min.x, rect.max.y );
						V2f lowerRight( rect.max.x, rect.min.y );
						// Center handle
						style->renderSolidRectangle( Box2f( rectCenter - threshold, rectCenter + threshold ) );
						// Vertical bars
						style->renderSolidRectangle( Box2f( rect.min - threshold, upperLeft + threshold ) );
						style->renderSolidRectangle( Box2f( lowerRight - threshold, rect.max + threshold ) );
						// Horizontal bars
						style->renderSolidRectangle( Box2f( rect.min - threshold, lowerRight + threshold ) );
						style->renderSolidRectangle( Box2f( upperLeft - threshold, rect.max + threshold ) );
						// Delete button
						style->renderSolidRectangle( Box2f( V2f( deleteButtonCenter.x, deleteButtonCenter.y ) - 0.5f * threshold, V2f( deleteButtonCenter.x, deleteButtonCenter.y ) + 0.5f * threshold ) );
					}
				}
				else
				{
					glEnable( GL_LINE_SMOOTH );
					glLineWidth( 2.0f );
					glColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
					style->renderRectangle( Box2f( V2f(rect.min) - 1.0f * planarScale, V2f(rect.max) + 1.0f * planarScale ) );
					glLineWidth( 1.0f );
					glColor4f( 0.8f, 0.8f, 0.8f, 1.0f );
					Color4f foreground( 0.8f, 0.8f, 0.8f, 1.0f );
					style->renderRectangle( Box2f( rect.min, rect.max ) );
					renderLine2D( style, rectCenter - crossHairSize * V2f( 1, 0 ), rectCenter + crossHairSize * V2f( 1, 0 ), 1 * planarScale.x, foreground );
					renderLine2D( style, rectCenter - crossHairSize * V2f( 0, 1 ), rectCenter + crossHairSize * V2f( 0, 1 ), 1 * planarScale.x, foreground );

					if( m_hover )
					{
						renderFilledCircle2D( style, deleteButtonCenter, deleteButtonSize.x * 1.4f, Color4f( 0.4, 0.4, 0.4, 1.0 ) );
						Color4f buttonCol( 0.0f, 0.0f, 0.0f, 1.0f );
						if( m_hover == 2 )
						{
							buttonCol = Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
						}
						renderLine2D( style, deleteButtonCenter - deleteButtonSize, deleteButtonCenter + deleteButtonSize, 4.0 * planarScale.x, buttonCol );
						renderLine2D( style, deleteButtonCenter + deleteButtonSize * V2f( 1, -1 ), deleteButtonCenter + deleteButtonSize * V2f( -1, 1 ), 4.0 * planarScale.x, buttonCol );
					}

					float textScale = 10;
					float textLength = style->textBound( Style::LabelText, m_id ).size().x;
					glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
					glPushMatrix();
						glTranslatef( rect.min.x - ( textScale * textLength + 5 ) * planarScale.x, rect.max.y + 5 * planarScale.y, 0.0f );
						glScalef( textScale * planarScale.x, textScale * planarScale.y, 1 );
						style->renderText( Style::LabelText, m_id );
					glPopMatrix();
				}

			glPopAttrib();


		}

	private :

		void plugDirtied( Plug *plug )
		{
			if( plug == m_plug )
			{
				dirty( DirtyType::Bound );
			}
		}

		bool mouseMove( const ButtonEvent &event )
		{
			const V2f p = eventPosition( event );

			Metadata::registerValue( m_plug.get(), g_hoveredKey, new IECore::BoolData( true ), false );
			if( onDeleteButton( p ) )
			{
				Pointer::setCurrent( "" );
				m_hover = 2;
				return false;
			}

			m_hover = 1;

			V2f dir = dragDirection( p );
			if( dir.x && dir.y )
			{
				Pointer::setCurrent( ( dir.x * dir.y < 0 ) ? "moveDiagonallyDown" : "moveDiagonallyUp" );
			}
			else if( dir.x )
			{
				Pointer::setCurrent( "moveHorizontally" );
			}
			else if( dir.y )
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

			// Anything within the bound is draggable except the delete button
			const V2f p = eventPosition( event );
			if( onDeleteButton( p ) )
			{
				m_deleting = true;
				return true;
			}

			return true;
		}

		IECore::RunTimeTypedPtr dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
		{
			m_dragStart = eventPosition( event );
			m_dragDirection = dragDirection( m_dragStart );
			m_dragStartRectangle = m_plug->getValue();
			return IECore::NullObject::defaultNullObject();
		}

		bool dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
		{
			if( event.sourceGadget != this )
			{
				return false;
			}

			updateDragRectangle( event );
			return true;
		}

		bool dragMove( const GafferUI::DragDropEvent &event )
		{
			updateDragRectangle( event );
			return true;
		}

		bool dragEnd( const GafferUI::DragDropEvent &event )
		{
			updateDragRectangle( event );
			m_deleting = false;
			return true;
		}

		void updateDragRectangle( const GafferUI::DragDropEvent &event )
		{
			if( m_deleting )
			{
				return;
			}
			const V2f p = eventPosition( event );
			Box2i b = m_dragStartRectangle;

			if( m_dragDirection == V2i( 0, 0 ) )
			{
				const V2f offset = p - m_dragStart;
				const V2i intOffset = V2i( round( offset.x ), round( offset.y ) );
				b.min += intOffset;
				b.max += intOffset;
			}
			else
			{
				if( m_dragDirection.x == -1 )
				{
					b.min.x = p.x;
				}
				else if( m_dragDirection.x == 1 )
				{
					b.max.x = p.x;
				}

				if( m_dragDirection.y == -1 )
				{
					b.min.y = p.y;
				}
				else if( m_dragDirection.y == 1 )
				{
					b.max.y = p.y;
				}
			}

			// fix max < min issues
			Box2i c;
			c.extendBy( b.min );
			c.extendBy( b.max );

			m_plug->setValue( c );
		}

		bool buttonRelease( const GafferUI::ButtonEvent &event )
		{
			const V2f p = eventPosition( event );
			if( m_deleting && onDeleteButton( p ) )
			{
				m_deleteClickedSignal( m_plug.get() );
			}
			m_deleting = false;

			return true;
		}

		void leave()
		{
			Pointer::setCurrent( "" );
			m_hover = 0;
			m_deleting = false;

			Metadata::registerValue( m_plug.get(), g_hoveredKey, new IECore::BoolData( false ), false );
		}

		V2f viewportPlanarScale() const
		{
			// It's kinda silly to have to reverse engineer viewportGadget->m_planarScale because it isn't public
			const ViewportGadget *viewportGadget = ancestor<ViewportGadget>();
			return V2f(
				viewportGadget->getCamera()->getAperture()[0] / viewportGadget->getViewport()[0],
				viewportGadget->getCamera()->getAperture()[1] / viewportGadget->getViewport()[1]
			);
		}

		bool onDeleteButton( const V2f &p ) const
		{
			// Any positions that are part of the gadget, but not part of an extended bound, are
			// on the delete button
			Box2i rect = m_plug->getValue();
			const V2f planarScale = viewportPlanarScale();
			const V2f threshold( planarScale * m_handleSize );
			return
				p.x > rect.max.x + 0.5 * threshold.x - planarScale.x &&
				p.y > rect.max.y + 0.5 * threshold.y - planarScale.y;
		}

		V2i dragDirection( const V2f &p ) const
		{
			Box2i rect = m_plug->getValue();
			V2f rectCenter( 0.5f * ( V2f( rect.min ) + V2f( rect.max ) ) );
			V2f centerDisp = p - rectCenter;

			const V2f planarScale = viewportPlanarScale();
			const V2f threshold( viewportPlanarScale() * m_handleSize );

			if( rect.intersects( p ) && fabs( centerDisp.x ) < threshold.x && fabs( centerDisp.y ) < threshold.y )
			{
				// Center handle
				return V2i( 0, 0 );
			}

			Box2f rectInner( V2f(rect.min) + threshold, V2f(rect.max) - threshold );

			// We're not in the center, so we must be over an edge.  Return which edge
			// Not that there is an extra pixel of tolerance here, since the selection rect snaps
			// to the nearest half-pixel, and we need to include the whole selection rect
			return V2i(
				( p.x > rectInner.max.x - planarScale.x ) - ( p.x < rectInner.min.x + planarScale.x ),
				( p.y > rectInner.max.y - planarScale.y ) - ( p.y < rectInner.min.y + planarScale.y )
			);
		}

		V2f eventPosition( const ButtonEvent &event ) const
		{
			const ViewportGadget *viewportGadget = ancestor<ViewportGadget>();
			const ImageGadget *imageGadget = static_cast<const ImageGadget *>( viewportGadget->getPrimaryChild() );
			V2f pixel = imageGadget->pixelAt( event.line );
			Context::Scope contextScope( imageGadget->getContext() );
			pixel.x *= imageGadget->getImage()->format().getPixelAspect();
			return pixel;
		}

		Box2iPlugPtr m_plug;
		const std::string m_id;
		bool m_editable;
		float m_handleSize;
		int m_hover;
		bool m_deleting;

		DeleteClickedSignal m_deleteClickedSignal;


		Imath::Box2i m_dragStartRectangle;
		Imath::V2f m_dragStart;
		Imath::V2i m_dragDirection;
};
GAFFER_GRAPHCOMPONENT_DEFINE_TYPE( Box2iGadget )

class V2iGadget : public GafferUI::Gadget
{

	public :

		V2iGadget( V2iPlugPtr plug, std::string id )
			:   Gadget(), m_plug( plug ), m_id( id ), m_editable( true ), m_handleSize( 10 ), m_hover( 0 ), m_deleting( false )
		{
			mouseMoveSignal().connect( boost::bind( &V2iGadget::mouseMove, this, ::_2 ) );
			buttonPressSignal().connect( boost::bind( &V2iGadget::buttonPress, this, ::_2 ) );
			dragBeginSignal().connect( boost::bind( &V2iGadget::dragBegin, this, ::_1, ::_2 ) );
			dragEnterSignal().connect( boost::bind( &V2iGadget::dragEnter, this, ::_1, ::_2 ) );
			dragMoveSignal().connect( boost::bind( &V2iGadget::dragMove, this, ::_2 ) );
			dragEndSignal().connect( boost::bind( &V2iGadget::dragEnd, this, ::_2 ) );
			buttonReleaseSignal().connect( boost::bind( &V2iGadget::buttonRelease, this, ::_2 ) );
			leaveSignal().connect( boost::bind( &V2iGadget::leave, this ) );

			plug->node()->plugDirtiedSignal().connect( boost::bind( &V2iGadget::plugDirtied, this, ::_1 ) );
		}

		GAFFER_GRAPHCOMPONENT_DECLARE_TYPE( V2iGadget, V2iGadgetTypeId, GafferUI::Gadget );

		Imath::Box3f bound() const override
		{
			V2i p = m_plug->getValue();
			return Box3f(
				V3f( p.x, p.y, 0 ),
				V3f( p.x, p.y, 0 )
			);
		}

		const V2iPlug *getPlug() const
		{
			return m_plug.get();
		}

		void setEditable( bool editable )
		{
			m_editable = editable;
		}

		bool getEditable() const
		{
			return m_editable;
		}

		void setHandleSize( bool handleSize )
		{
			m_handleSize = handleSize;
		}

		bool getHandleSize() const
		{
			return m_handleSize;
		}

		typedef boost::signal<void ( Plug * )> DeleteClickedSignal;
		DeleteClickedSignal &deleteClickedSignal()
		{
			return m_deleteClickedSignal;
		}

	protected :

		void doRenderLayer( Layer layer, const Style *style ) const override
		{
			const V2f planarScale = viewportPlanarScale();
			const V2f threshold( planarScale * m_handleSize );
			if( layer != Layer::Main )
			{
				return;
			}

			V2f point = V2f(m_plug->getValue()) + V2f( 0.5 );
			V2f deleteButtonCenter( point.x + threshold.x, point.y + threshold.y );
			V2f deleteButtonSize( threshold.x * 0.5, threshold.y * 0.5 );
			glPushAttrib( GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT );

				if( IECoreGL::Selector::currentSelector() )
				{
					if( m_editable )
					{
						// Center handle
						style->renderSolidRectangle( Box2f( point - threshold, point + threshold ) );
						// Delete button
						style->renderSolidRectangle( Box2f( V2f( deleteButtonCenter.x, deleteButtonCenter.y ) - 0.5f * threshold, V2f( deleteButtonCenter.x, deleteButtonCenter.y ) + 0.5f * threshold ) );
					}
				}
				else
				{
					glEnable( GL_LINE_SMOOTH );
					Color4f black( 0.0f, 0.0f, 0.0f, 1.0f );
					renderLine2D( style, point - V2f( threshold.x, 0 ), point - V2f( 2.5 * planarScale.x, 0 ), planarScale.y * 2.0f, black );
					renderLine2D( style, point + V2f( threshold.x, 0 ), point + V2f( 2.5 * planarScale.x, 0 ), planarScale.y * 2.0f, black );
					renderLine2D( style, point - V2f( 0, threshold.y ), point - V2f( 0, 2.5 * planarScale.y ), planarScale.x * 2.0f, black );
					renderLine2D( style, point + V2f( 0, threshold.y ), point + V2f( 0, 2.5 * planarScale.y ), planarScale.x * 2.0f, black );
					renderCircle2D( style, point, 2.5 * planarScale.x, planarScale.x * 2.0f, Color4f( 0.8, 0.8, 0.8, 1.0 ) );

					if( m_hover )
					{
						renderFilledCircle2D( style, deleteButtonCenter, deleteButtonSize.x * 1.4f, Color4f( 0.4, 0.4, 0.4, 1.0 ) );
						Color4f buttonCol( 0.0f, 0.0f, 0.0f, 1.0f );
						if( m_hover == 2 )
						{
							buttonCol = Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
						}
						renderLine2D( style, deleteButtonCenter - deleteButtonSize, deleteButtonCenter + deleteButtonSize, 4.0 * planarScale.x, buttonCol );
						renderLine2D( style, deleteButtonCenter + deleteButtonSize * V2f( 1, -1 ), deleteButtonCenter + deleteButtonSize * V2f( -1, 1 ), 4.0 * planarScale.x, buttonCol );
					}

					float textScale = 10;
					float textLength = style->textBound( Style::LabelText, m_id ).size().x;
					glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
					glPushMatrix();
						glTranslatef( point.x - ( textScale * textLength + 5 ) * planarScale.x, point.y + 5 * planarScale.y, 0.0f );
						glScalef( textScale * planarScale.x, textScale * planarScale.y, 1 );
						style->renderText( Style::LabelText, m_id );
					glPopMatrix();
				}

			glPopAttrib();
		}

	private :

		void plugDirtied( Plug *plug )
		{
			if( plug == m_plug )
			{
				dirty( DirtyType::Bound );
			}
		}

		bool mouseMove( const ButtonEvent &event )
		{
			const V2f p = eventPosition( event );

			Metadata::registerValue( m_plug.get(), g_hoveredKey, new IECore::BoolData( true ), false );

			if( onDeleteButton( p ) )
			{
				Pointer::setCurrent( "" );
				m_hover = 2;
				return false;
			}

			m_hover = 1;

			Pointer::setCurrent( "move" );

			return false;
		}

		bool buttonPress( const GafferUI::ButtonEvent &event )
		{
			if( event.buttons != ButtonEvent::Left )
			{
				return false;
			}

			// Anything within the bound is draggable except the delete button
			const V2f p = eventPosition( event );
			if( onDeleteButton( p ) )
			{
				m_deleting = true;
				return true;
			}

			return true;
		}

		IECore::RunTimeTypedPtr dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
		{
			m_dragStart = eventPosition( event );
			m_dragStartPlugValue = m_plug->getValue();
			return IECore::NullObject::defaultNullObject();
		}

		bool dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
		{
			if( event.sourceGadget != this )
			{
				return false;
			}

			updateDragPoint( event );
			return true;
		}

		bool dragMove( const GafferUI::DragDropEvent &event )
		{
			updateDragPoint( event );
			return true;
		}

		bool dragEnd( const GafferUI::DragDropEvent &event )
		{
			updateDragPoint( event );
			m_deleting = false;
			return true;
		}

		void updateDragPoint( const GafferUI::DragDropEvent &event )
		{
			if( m_deleting )
			{
				return;
			}
			const V2f p = eventPosition( event );
			V2i point = m_dragStartPlugValue;

			const V2f offset = p - m_dragStart;
			point += V2i( round( offset.x ), round( offset.y ) );

			m_plug->setValue( point );
		}

		bool buttonRelease( const GafferUI::ButtonEvent &event )
		{
			const V2f p = eventPosition( event );
			if( m_deleting && onDeleteButton( p ) )
			{
				m_deleteClickedSignal( m_plug.get() );
			}
			m_deleting = false;

			return true;
		}

		void leave()
		{
			Pointer::setCurrent( "" );
			m_hover = 0;
			m_deleting = false;

			Metadata::registerValue( m_plug.get(), g_hoveredKey, new IECore::BoolData( false ), false );
		}

		V2f viewportPlanarScale() const
		{
			// It's kinda silly to have to reverse engineer viewportGadget->m_planarScale because it isn't public
			const ViewportGadget *viewportGadget = ancestor<ViewportGadget>();
			return V2f(
				viewportGadget->getCamera()->getAperture()[0] / viewportGadget->getViewport()[0],
				viewportGadget->getCamera()->getAperture()[1] / viewportGadget->getViewport()[1]
			);
		}

		bool onDeleteButton( const V2f &p ) const
		{
			// Any positions that are part of the gadget, but not part of an extended bound, are
			// on the delete button
			V2f point = V2f(m_plug->getValue()) + V2f( 0.5 );
			const V2f planarScale = viewportPlanarScale();
			const V2f threshold( planarScale * m_handleSize );
			return
				p.x > point.x + 0.5 * threshold.x - planarScale.x &&
				p.y > point.y + 0.5 * threshold.y - planarScale.y;
		}

		V2f eventPosition( const ButtonEvent &event ) const
		{
			const ViewportGadget *viewportGadget = ancestor<ViewportGadget>();
			const ImageGadget *imageGadget = static_cast<const ImageGadget *>( viewportGadget->getPrimaryChild() );
			V2f pixel = imageGadget->pixelAt( event.line );
			Context::Scope contextScope( imageGadget->getContext() );
			pixel.x *= imageGadget->getImage()->format().getPixelAspect();
			return pixel;
		}

		V2iPlugPtr m_plug;
		const std::string m_id;
		bool m_editable;
		float m_handleSize;
		int m_hover;
		bool m_deleting;

		DeleteClickedSignal m_deleteClickedSignal;

		Imath::V2i m_dragStartPlugValue;
		Imath::V2f m_dragStart;
};
GAFFER_GRAPHCOMPONENT_DEFINE_TYPE( V2iGadget )

} // namespace

ImageView::ColorInspectorPlug::ColorInspectorPlug( const std::string &name )
	: ValuePlug( name, ValuePlug::Direction::In, Default )
{
	addChild( new IntPlug( "mode" ) );
	addChild( new V2iPlug( "pixel" ) );
	addChild( new Box2iPlug( "region" ) );
}

Gaffer::IntPlug *ImageView::ColorInspectorPlug::modePlug()
{
	return getChild<IntPlug>( 0 );
}

const Gaffer::IntPlug *ImageView::ColorInspectorPlug::modePlug() const
{
	return getChild<IntPlug>( 0 );
}

Gaffer::V2iPlug *ImageView::ColorInspectorPlug::pixelPlug()
{
	return getChild<V2iPlug>( 1 );
}

const Gaffer::V2iPlug *ImageView::ColorInspectorPlug::pixelPlug() const
{
	return getChild<V2iPlug>( 1 );
}

Gaffer::Box2iPlug *ImageView::ColorInspectorPlug::regionPlug()
{
	return getChild<Box2iPlug>( 2 );
}

const Gaffer::Box2iPlug *ImageView::ColorInspectorPlug::regionPlug() const
{
	return getChild<Box2iPlug>( 2 );
}

bool ImageView::ColorInspectorPlug::acceptsChild( const Gaffer::GraphComponent *potentialChild ) const
{
	if( !Plug::acceptsChild( potentialChild ) )
	{
		return false;
	}
	return children().size() <= 3;
}

Gaffer::PlugPtr ImageView::ColorInspectorPlug::createCounterpart( const std::string &name, Direction direction ) const
{
	return new ColorInspectorPlug();
}

class ImageView::ColorInspector : public boost::signals::trackable
{

	public :

		ColorInspector( ImageView *view )
			:	m_view( view ),
				m_pixel( new V2fContextVariable ),
				m_region( new Box2iContextVariable ),
				m_deleteContextVariables( new DeleteContextVariables ),
				m_sampler( new ImageSampler ),
				m_regionSampler( new ImageStats )
		{
			// ---- Create a plug on ImageView which will be used for evaluating colorInspectors

			PlugPtr plug = new Plug( "colorInspector" );
			view->addChild( plug );

			PlugPtr evaluatorPlug = new Plug( "evaluator" );
			plug->addChild( evaluatorPlug );
			evaluatorPlug->addChild( new Color4fPlug( "pixelColor" ) );
			evaluatorPlug->addChild( new Color4fPlug( "regionColor" ) );

			// We use `m_pixel` to fetch a context variable to transfer
			// the mouse position into `m_sampler`. We could use `mouseMoveSignal()`
			// to instead call `m_sampler->pixelPlug()->setValue()`, but that
			// would cause cancellation of the ImageView background compute every
			// time the mouse was moved. The "colorInspector:source" variable is
			// created in ImageViewUI's `_ColorInspectorPlugValueWidget`.
			m_pixel->namePlug()->setValue( "colorInspector:source" );

			// The same thing, but when we need a region to evaluate regionColor
			// instead of a pixel to evaluate pixelColor
			m_region->namePlug()->setValue( "colorInspector:source" );

			// And we use a DeleteContextVariables node to make sure that our
			// private context variable doesn't become visible to the upstream
			// graph.
			m_deleteContextVariables->setup( view->inPlug<ImagePlug>() );
			m_deleteContextVariables->variablesPlug()->setValue( "colorInspector:source" );

			// We want to sample the image before the display transforms
			// are applied. We can't simply get this image from inPlug()
			// because derived classes may have called insertConverter(),
			// so we take it from the input to the display transform chain.

			ImagePlug *image = view->getPreprocessor()->getChild<ImagePlug>( "out" );
			m_deleteContextVariables->inPlug()->setInput( image );
			m_sampler->imagePlug()->setInput( m_deleteContextVariables->outPlug() );
			m_sampler->pixelPlug()->setInput( m_pixel->outPlug() );

			evaluatorPlug->getChild<Color4fPlug>( "pixelColor" )->setInput( m_sampler->colorPlug() );


			m_regionSampler->inPlug()->setInput( m_deleteContextVariables->outPlug() );
			m_regionSampler->areaPlug()->setInput( m_region->outPlug() );
			evaluatorPlug->getChild<Color4fPlug>( "regionColor" )->setInput( m_regionSampler->averagePlug() );

			ImageGadget *imageGadget = static_cast<ImageGadget *>( m_view->viewportGadget()->getPrimaryChild() );
			imageGadget->channelsChangedSignal().connect( boost::bind( &ColorInspector::channelsChanged, this ) );


			// ---- Create a plug on ImageView for storing colorInspectors
			plug->addChild( new ArrayPlug( "inspectors", Plug::In, new ColorInspectorPlug(), 1, 1024, Plug::Default & ~Plug::AcceptsInputs ) );
			colorInspectorsPlug()->childAddedSignal().connect( boost::bind( &ColorInspector::colorInspectorAdded, this, ::_2 ) );
			colorInspectorsPlug()->childRemovedSignal().connect( boost::bind( &ColorInspector::colorInspectorRemoved, this, ::_2 ) );

			colorInspectorsPlug()->getChild<ColorInspectorPlug>( 0 )->modePlug()->setValue( 2 );

			view->plugSetSignal().connect( boost::bind( &ColorInspector::plugSet, this, ::_1 ) );
		}

	private :

		Gaffer::ArrayPlug *colorInspectorsPlug()
		{
			return m_view->getChild<Plug>( "colorInspector" )->getChild<ArrayPlug>( "inspectors" );
		}

		const Gaffer::ArrayPlug *colorInspectorsPlug() const
		{
			return m_view->getChild<Plug>( "colorInspector" )->getChild<ArrayPlug>( "inspectors" );
		}

		void plugSet( Gaffer::Plug *plug )
		{
			if( plug->parent()->parent() == colorInspectorsPlug() )
			{
				ColorInspectorPlug *colorInspector = IECore::runTimeCast<ColorInspectorPlug>( plug->parent() );
				if( plug == colorInspector->modePlug() )
				{
					if( colorInspector->modePlug()->getValue() == 0 )
					{
						colorInspector->pixelPlug()->setValue( colorInspector->regionPlug()->getValue().center() );
					}
					else if( colorInspector->modePlug()->getValue() == 1 )
					{
						V2i pixel = colorInspector->pixelPlug()->getValue();
						colorInspector->regionPlug()->setValue( Box2i( pixel - V2i( 50 ), pixel + V2i( 50 ) ) );
					}
					colorInspectorRemoved( colorInspector );
					colorInspectorAdded( colorInspector );
				}
			}
		}

		void colorInspectorAdded( GraphComponent *colorInspector )
		{
			ColorInspectorPlug *colorInspectorTyped = IECore::runTimeCast<ColorInspectorPlug>( colorInspector );
			if( colorInspectorTyped->modePlug()->getValue() == 0 )
			{
				V2iGadget::Ptr r = new V2iGadget( colorInspectorTyped->pixelPlug(), colorInspector->getName().value().substr( 1 ) );
				r->deleteClickedSignal().connect( boost::bind( &ColorInspector::deleteClicked, this, ::_1 ) );
				m_view->viewportGadget()->addChild( r );
			}
			else
			{
				Box2iGadget::Ptr r = new Box2iGadget( IECore::runTimeCast<ColorInspectorPlug>( colorInspector )->regionPlug(), colorInspector->getName().value().substr( 1 ) );
				r->deleteClickedSignal().connect( boost::bind( &ColorInspector::deleteClicked, this, ::_1 ) );
				m_view->viewportGadget()->addChild( r );
			}
		}

		void colorInspectorRemoved( GraphComponent *colorInspector )
		{
			ColorInspectorPlug *colorInspectorTyped = IECore::runTimeCast<ColorInspectorPlug>( colorInspector );
			for( auto &i : m_view->viewportGadget()->children() )
			{
				if( (int)i->typeId() == (int)Box2iGadgetTypeId )
				{
					if( IECore::runTimeCast<Box2iGadget>( i.get() )->getPlug() == colorInspectorTyped->regionPlug() )
					{
						m_view->viewportGadget()->removeChild( i );
						return;
					}
				}
				else if( (int)i->typeId() == (int)V2iGadgetTypeId )
				{
					if( IECore::runTimeCast<V2iGadget>( i.get() )->getPlug() == colorInspectorTyped->pixelPlug() )
					{
						m_view->viewportGadget()->removeChild( i );
						return;
					}
				}
			}
		}

		void deleteClicked( Gaffer::Plug *plug )
		{
			colorInspectorsPlug()->removeChild( plug->parent() );
		}

		// Why is there a private function that's never used, should this just be deleted?
		/*Plug *plug()
		{
			return m_view->getChild<Plug>( "colorInspector" );
		}*/

		void channelsChanged()
		{
			ImageGadget *imageGadget = static_cast<ImageGadget *>( m_view->viewportGadget()->getPrimaryChild() );
			StringVectorDataPtr channels = new StringVectorData( std::vector<std::string>(
					imageGadget->getChannels().begin(),
					imageGadget->getChannels().end()
				) );
			m_sampler->channelsPlug()->setValue( channels );
			m_regionSampler->channelsPlug()->setValue( channels );
		}

		ImageView *m_view;
		V2fContextVariablePtr m_pixel;
		Box2iContextVariablePtr m_region;
		DeleteContextVariablesPtr m_deleteContextVariables;
		ImageSamplerPtr m_sampler;
		ImageStatsPtr m_regionSampler;

};

//////////////////////////////////////////////////////////////////////////
/// Implementation of ImageView
//////////////////////////////////////////////////////////////////////////

GAFFER_NODE_DEFINE_TYPE( ImageView );

ImageView::ViewDescription<ImageView> ImageView::g_viewDescription( GafferImage::ImagePlug::staticTypeId() );

ImageView::ImageView( const std::string &name )
	:	View( name, new GafferImage::ImagePlug() ),
		m_imageGadget( new ImageGadget() ),
		m_framed( false )
{

	// build the preprocessor we use for applying colour
	// transforms, and the stats node we use for displaying stats.

	NodePtr preprocessor = new Node;
	ImagePlugPtr preprocessorInput = new ImagePlug( "in" );
	preprocessor->addChild( preprocessorInput );

	BoolPlugPtr clippingPlug = new BoolPlug( "clipping", Plug::In, false, Plug::Default & ~Plug::AcceptsInputs );
	addChild( clippingPlug );

	FloatPlugPtr exposurePlug = new FloatPlug( "exposure", Plug::In, 0.0f,
		Imath::limits<float>::min(), Imath::limits<float>::max(), Plug::Default & ~Plug::AcceptsInputs
	);
	addChild( exposurePlug ); // dealt with in plugSet()

	PlugPtr gammaPlug = new FloatPlug( "gamma", Plug::In, 1.0f,
		Imath::limits<float>::min(), Imath::limits<float>::max(), Plug::Default & ~Plug::AcceptsInputs
	);
	addChild( gammaPlug );

	addChild( new StringPlug( "displayTransform", Plug::In, "Default", Plug::Default & ~Plug::AcceptsInputs ) );
	addChild( new BoolPlug( "lutGPU", Plug::In, true, Plug::Default & ~Plug::AcceptsInputs ) );


	ImagePlugPtr preprocessorOutput = new ImagePlug( "out", Plug::Out );
	preprocessor->addChild( preprocessorOutput );
	preprocessorOutput->setInput( preprocessorInput );

	// tell the base class about all the preprocessing we want to do

	setPreprocessor( preprocessor );

	// connect up to some signals

	plugSetSignal().connect( boost::bind( &ImageView::plugSet, this, ::_1 ) );
	viewportGadget()->keyPressSignal().connect( boost::bind( &ImageView::keyPress, this, ::_2 ) );
	viewportGadget()->preRenderSignal().connect( boost::bind( &ImageView::preRender, this ) );

	// get our display transform right

	insertDisplayTransform();

	// Now we can connect up our ImageGadget, which will do the
	// hard work of actually displaying the image.

	m_imageGadget->setImage( preprocessedInPlug<ImagePlug>() );
	m_imageGadget->setContext( getContext() );
	viewportGadget()->setPrimaryChild( m_imageGadget );

	m_channelChooser.reset( new ChannelChooser( this ) );
	m_colorInspector.reset( new ColorInspector( this ) );
}

void ImageView::insertConverter( Gaffer::NodePtr converter )
{
	PlugPtr converterInput = converter->getChild<Plug>( "in" );
	if( !converterInput )
	{
		throw IECore::Exception( "Converter has no Plug named \"in\"" );
	}
	ImagePlugPtr converterOutput = converter->getChild<ImagePlug>( "out" );
	if( !converterOutput )
	{
		throw IECore::Exception( "Converter has no ImagePlug named \"out\"" );
	}

	PlugPtr newInput = converterInput->createCounterpart( "in", Plug::In );
	setChild( "in", newInput );

	NodePtr preprocessor = getPreprocessor();
	Plug::OutputContainer outputsToRestore = preprocessor->getChild<ImagePlug>( "in" )->outputs();

	PlugPtr newPreprocessorInput = converterInput->createCounterpart( "in", Plug::In );
	preprocessor->setChild( "in", newPreprocessorInput );
	newPreprocessorInput->setInput( newInput );

	preprocessor->setChild( "__converter", converter );
	converterInput->setInput( newPreprocessorInput );

	for( Plug::OutputContainer::const_iterator it = outputsToRestore.begin(), eIt = outputsToRestore.end(); it != eIt; ++it )
	{
		(*it)->setInput( converterOutput );
	}
}

ImageView::~ImageView()
{
}

Gaffer::BoolPlug *ImageView::clippingPlug()
{
	return getChild<BoolPlug>( "clipping" );
}

const Gaffer::BoolPlug *ImageView::clippingPlug() const
{
	return getChild<BoolPlug>( "clipping" );
}

Gaffer::FloatPlug *ImageView::exposurePlug()
{
	return getChild<FloatPlug>( "exposure" );
}

const Gaffer::FloatPlug *ImageView::exposurePlug() const
{
	return getChild<FloatPlug>( "exposure" );
}

Gaffer::FloatPlug *ImageView::gammaPlug()
{
	return getChild<FloatPlug>( "gamma" );
}

const Gaffer::FloatPlug *ImageView::gammaPlug() const
{
	return getChild<FloatPlug>( "gamma" );
}

Gaffer::StringPlug *ImageView::displayTransformPlug()
{
	return getChild<StringPlug>( "displayTransform" );
}

const Gaffer::StringPlug *ImageView::displayTransformPlug() const
{
	return getChild<StringPlug>( "displayTransform" );
}

Gaffer::BoolPlug *ImageView::lutGPUPlug()
{
	return getChild<BoolPlug>( "lutGPU" );
}

const Gaffer::BoolPlug *ImageView::lutGPUPlug() const
{
	return getChild<BoolPlug>( "lutGPU" );
}

ImageGadget *ImageView::imageGadget()
{
	return m_imageGadget.get();
}

const ImageGadget *ImageView::imageGadget() const
{
	return m_imageGadget.get();
}

void ImageView::setContext( Gaffer::ContextPtr context )
{
	View::setContext( context );
	m_imageGadget->setContext( context );
}

void ImageView::plugSet( Gaffer::Plug *plug )
{
	if( plug == clippingPlug() )
	{
		m_imageGadget->setClipping( clippingPlug()->getValue() );
	}
	else if( plug == exposurePlug() )
	{
		m_imageGadget->setExposure( exposurePlug()->getValue() );
	}
	else if( plug == gammaPlug() )
	{
		m_imageGadget->setGamma( gammaPlug()->getValue() );
	}
	else if( plug == displayTransformPlug() )
	{
		insertDisplayTransform();
	}
	else if( plug == lutGPUPlug() )
	{
		m_imageGadget->setUseGPU( lutGPUPlug()->getValue() );
	}
}

bool ImageView::keyPress( const GafferUI::KeyEvent &event )
{
	if( event.key == "F" && !event.modifiers )
	{
		const Box3f b = m_imageGadget->bound();
		if( !b.isEmpty() && viewportGadget()->getCameraEditable() )
		{
			viewportGadget()->frame( b );
			return true;
		}
	}
	else if( event.key == "Home" && !event.modifiers )
	{
		V2i viewport = viewportGadget()->getViewport();
		V3f halfViewportSize(viewport.x / 2, viewport.y / 2, 0);
		V3f imageCenter = m_imageGadget->bound().center();
		viewportGadget()->frame(
			Box3f(
				V3f(imageCenter.x - halfViewportSize.x, imageCenter.y - halfViewportSize.y, 0),
				V3f(imageCenter.x + halfViewportSize.x, imageCenter.y + halfViewportSize.y, 0)
			)
		);
		return true;
	}
	else if( event.key == "Escape" )
	{
		m_imageGadget->setPaused( true );
	}
	else if( event.key == "G" && event.modifiers == ModifiableEvent::Modifiers::Alt )
	{
		lutGPUPlug()->setValue( !lutGPUPlug()->getValue() );
	}

	return false;
}

void ImageView::preRender()
{
	if( m_framed )
	{
		return;
	}

	const Box3f b = m_imageGadget->bound();
	if( b.isEmpty() )
	{
		return;
	}

	viewportGadget()->frame( b );
	m_framed = true;
}

void ImageView::insertDisplayTransform()
{
	const std::string name = displayTransformPlug()->getValue();

	ImageProcessorPtr displayTransform;
	DisplayTransformMap::const_iterator it = m_displayTransforms.find( name );
	if( it != m_displayTransforms.end() )
	{
		displayTransform = it->second;
	}
	else
	{
		displayTransform = createDisplayTransform( name );
		if( displayTransform )
		{
			m_displayTransforms[name] = displayTransform;
			// Even though technically the ImageGadget will own `displayTransform`,
			// we must parent it into our preprocessor so that `BackgroundTask::cancelAffectedTasks()`
			// can find the relevant tasks to cancel if plugs on `displayTransform` are edited.
			getPreprocessor()->addChild( displayTransform );
		}
	}

	m_imageGadget->setDisplayTransform( displayTransform );
}

void ImageView::registerDisplayTransform( const std::string &name, DisplayTransformCreator creator )
{
	displayTransformCreators()[name] = creator;
}

void ImageView::registeredDisplayTransforms( std::vector<std::string> &names )
{
	const DisplayTransformCreatorMap &m = displayTransformCreators();
	names.clear();
	for( DisplayTransformCreatorMap::const_iterator it = m.begin(), eIt = m.end(); it != eIt; ++it )
	{
		names.push_back( it->first );
	}
}

GafferImage::ImageProcessorPtr ImageView::createDisplayTransform( const std::string &name )
{
	const auto &m = displayTransformCreators();
	auto it = m.find( name );
	if( it != m.end() )
	{
		return it->second();
	}
	return nullptr;
}

ImageView::DisplayTransformCreatorMap &ImageView::displayTransformCreators()
{
	static auto g_creators = new DisplayTransformCreatorMap;
	return *g_creators;
}
