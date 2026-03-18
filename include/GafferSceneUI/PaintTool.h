//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2014-2016, John Haddon. All rights reserved.
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

#include "GafferSceneUI/SelectionTool.h"
#include "GafferSceneUI/TypeIds.h"

#include "GafferScene/EditScopeAlgo.h"
#include "GafferScene/SceneAlgo.h"
#include "GafferScene/ScenePlug.h"

#include "GafferUI/KeyEvent.h"
#include "GafferUI/DepthRender.h"

#include "Gaffer/EditScope.h"

#include "IECoreScene/MeshPrimitive.h"

#include "IECoreGL/Buffer.h"

#include "IECore/KDTree.h"

namespace GafferSceneUI
{

IE_CORE_FORWARDDECLARE( SceneView )

class GAFFERSCENEUI_API PaintTool : public GafferSceneUI::SelectionTool
{

	public :

		PaintTool( SceneView *view, const std::string &name = defaultName<PaintTool>() );
		~PaintTool() override;

		GAFFER_NODE_DECLARE_TYPE( GafferSceneUI::PaintTool, PaintToolTypeId, SelectionTool );

		Gaffer::StringPlug *variableNamePlug();
		const Gaffer::StringPlug *variableNamePlug() const;

		Gaffer::IntPlug *variableTypePlug();
		const Gaffer::IntPlug *variableTypePlug() const;

		Gaffer::IntPlug *modePlug();
		const Gaffer::IntPlug *modePlug() const;

		Gaffer::FloatPlug *sizePlug();
		const Gaffer::FloatPlug *sizePlug() const;

		Gaffer::FloatPlug *floatValuePlug();
		const Gaffer::FloatPlug *floatValuePlug() const;

		Gaffer::Color3fPlug *colorValuePlug();
		const Gaffer::Color3fPlug *colorValuePlug() const;

		Gaffer::FloatPlug *opacityPlug();
		const Gaffer::FloatPlug *opacityPlug() const;

		Gaffer::FloatPlug *hardnessPlug();
		const Gaffer::FloatPlug *hardnessPlug() const;

		Gaffer::IntPlug *visualiseModePlug();
		const Gaffer::IntPlug *visualiseModePlug() const;

		struct GAFFERSCENEUI_API Selection
		{

			// Constructs an empty selection.
			Selection();

			// Constructs a selection for the specified
			// viewed scene.
			Selection(
				const GafferScene::ConstScenePlugPtr scene,
				const GafferScene::ScenePlug::ScenePath &path,
				const Gaffer::ConstContextPtr &context,
				const Gaffer::EditScopePtr &editScope
			);

			/// Viewed scene
			/// ============
			///
			/// The scene being viewed.
			const GafferScene::ScenePlug *scene() const;
			/// The location within the viewed scene that is being
			/// edited.
			const GafferScene::ScenePlug::ScenePath &path() const;
			/// The context the scene is being viewed in.
			const Gaffer::Context *context() const;

			/// Upstream scene
			/// ==============
			///
			/// Often, the scene being viewed isn't actually the
			/// scene where the transform originates. Instead, the
			/// transform originates with an upstream node, and the
			/// user is viewing a downstream node to see the transform
			/// in the context of later changes. The `upstreamScene`
			/// is the output from the node where the transform
			/// originates.
			const GafferScene::ScenePlug *upstreamScene() const;
			/// The hierarchies of the upstream and viewed scenes may
			/// differ. The upstreamPath is the equivalent of
			/// the viewed path but in the upstream scene.
			const GafferScene::ScenePlug::ScenePath &upstreamPath() const;
			/// The upstream context is the equivalent of the
			/// viewed context, but for the upstream scene.
			const Gaffer::Context *upstreamContext() const;

			/// Status and editing
			/// ==================

			/// Returns true if the selected transform may be edited
			/// using `acquirePaintEdit()`
			bool editable() const;
			/// Returns a warning message, or "" if there are no
			/// warnings.
			const std::string &warning() const;

			/// Returns the plugs to edit. Throws if `status() != Editable`.
			/// > Caution : When using EditScopes, this may edit the graph
			/// > to create the plug unless `createIfNecessary == false`.
			Gaffer::CachedDataNode* acquirePaintEdit( bool createIfNecessary = true ) const;
			/// The EditScope passed to the constructor.
			const Gaffer::EditScope *editScope() const;
			/// Returns the GraphComponent that will be edited.
			/// Unlike `acquirePaintEdit()`, this never edits the graph,
			/// instead returning an `EditScope *` if an EditScope
			/// is in use but no paint has been created yet.
			/// Throws if `status() != Editable`.
			Gaffer::GraphComponent *editTarget() const;

			mutable std::vector<float> m_floatValueExpanded;
			//mutable std::vector<Imath::Color3f> m_colorValue;
			mutable std::vector<Imath::Color3f> m_colorValueExpanded;
			mutable IECoreGL::BufferPtr m_valueBuffer;
			mutable IECoreScene::MeshPrimitivePtr m_triangulatedTemp;

			mutable std::vector<Imath::V2f> m_kdTreePoints;
			mutable IECore::V2fTree m_kdTree;
			mutable Imath::M44f m_kdTreeProjection;

			mutable IECore::CompoundDataPtr m_currentStroke;
			mutable bool m_currentStrokeDirty;

			// TODO - not really a fan of having a lot of map lookups happening while processing these.
			// Rather than using a CompoundData, maybe makes sense for PrimitiveVariableProcessor to
			// declare a custom Data class with value, opacity and indices

			mutable IECore::ConstCompoundDataPtr m_initialEditValue;
			mutable IECore::ConstCompoundDataPtr m_initialMeshValue;
			mutable IECore::CompoundDataPtr m_composedInputValue;
			mutable IECore::CompoundDataPtr m_composedEditValue;
			mutable IECore::CompoundDataPtr m_composedValue;
			mutable Gaffer::CachedDataNode* m_paintEdit;
			//mutable IECore::ConstColor3fVectorDataPtr m_baseInputValue;
			//mutable std::vector<Imath::Color3f> m_composedInputValue;
			//mutable std::vector<Imath::Color3f> m_composedValue;

			Selection& operator=( const Selection & other );

			private :

				void initFromHistory( const GafferScene::SceneAlgo::History *history );
				void initFromSceneNode( const GafferScene::SceneAlgo::History *history );
				void initFromEditScope( const GafferScene::SceneAlgo::History *history );
				void initWalk(
					const GafferScene::SceneAlgo::History *history,
					bool &editScopeFound,
					const GafferScene::SceneAlgo::History *editScopeOutHistory = nullptr
				);
				bool initRequirementsSatisfied( bool editScopeFound );

				void throwIfNotEditable() const;
				Imath::M44f transformToLocalSpace() const;

				GafferScene::ConstScenePlugPtr m_scene;
				GafferScene::ScenePlug::ScenePath m_path;
				Gaffer::ConstContextPtr m_context;

				GafferScene::ConstScenePlugPtr m_upstreamScene;
				GafferScene::ScenePlug::ScenePath m_upstreamPath;
				Gaffer::ConstContextPtr m_upstreamContext;

				bool m_editable;
				std::string m_warning;
				Gaffer::EditScopePtr m_editScope;
				mutable std::string m_paintEditPath;


				static std::string displayName( const GraphComponent *component );
		};

		/// Returns the current selection.
		const std::vector<Selection> &selection() const;
		/// Returns true only if the selection is non-empty
		/// and every item is editable.
		bool selectionEditable() const;

		using SelectionChangedSignal = Gaffer::Signals::Signal<void (PaintTool &)>;
		SelectionChangedSignal &selectionChangedSignal();

		//void paint( const Imath::V2f &start, const Imath::V2f &end );
		void paint( const IECore::InternedString &variableName, const Imath::M44f &projectionMatrix, const std::variant<float, Imath::Color3f> &value, float opacity, float hardness, int mode );
		//void paint( const Imath::M44f &projectionMatrix, const Imath::Color3f &value, float opacity, float hardness, bool eraser );

		IECore::CompoundDataPtr targetVariableTypes();

	protected :


		/// The scene being edited.
		GafferScene::ScenePlug *scenePlug();
		const GafferScene::ScenePlug *scenePlug() const;

		/// Gadget under which derived classes
		/// should parent their handles.
		GafferUI::Gadget *handles();
		const GafferUI::Gadget *handles() const;

		/// Must be implemented by derived classes to return true if
		/// the input plug is used in updateHandles(). Implementation
		/// must call the base class implementation first, returning true
		/// if it does.
		bool affectsHandles( const Gaffer::Plug *input ) const;
		/// Must be implemented by derived classes to update the
		/// handles appropriately. Typically this means setting their
		/// transform and matching their enabled state to the editability
		/// of the selection.
		void updateHandles( float rasterScale );

		/// Must be called by derived classes when they begin
		/// a drag.
		/*void dragBegin();
		/// Must be called by derived classes when they end
		/// a drag.
		void dragEnd();*/
		/// Should be used in UndoScopes created by
		/// derived classes.
		std::string undoMergeGroup() const;

		/// TODO
		/// Utilities to help derived classes update plug values.
		static bool canSetValueOrAddKey( const Gaffer::FloatPlug *plug );
		static void setValueOrAddKey( Gaffer::FloatPlug *plug, float time, float value );

	private :

		IE_CORE_FORWARDDECLARE( BrushOutline );

		void contextChanged();
		void selectedPathsChanged();
		void plugDirtied( const Gaffer::Plug *plug );
		void metadataChanged( IECore::InternedString key );
		void updateSelection() const;
		void preRender();
		bool enter( const GafferUI::ButtonEvent &event );
		bool leave( const GafferUI::ButtonEvent &event );
		bool mouseMove( const GafferUI::ButtonEvent &event );
		bool buttonPress( const GafferUI::ButtonEvent &event );
		bool buttonRelease( const GafferUI::ButtonEvent &event );
		bool keyPress( const GafferUI::KeyEvent &event );
		IECore::RunTimeTypedPtr dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event );
		bool dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event );
		bool dragMove( const GafferUI::DragDropEvent &event );
		bool dragEnd( const GafferUI::DragDropEvent &event );

		void applyCurrentStroke();

		void updateCursor();

		Gaffer::Signals::ScopedConnection m_preRenderConnection;

		GafferUI::GadgetPtr m_gadget;
		bool m_gadgetDirty;

		mutable std::vector<Selection> m_selection;
		mutable bool m_selectionDirty;
		bool m_priorityPathsDirty;
		SelectionChangedSignal m_selectionChangedSignal;

		Imath::V2f m_dragPosition;
		bool m_dragging;
		int m_mergeGroupId;

		GafferUI::DepthRender m_depthRender;

		BrushOutlinePtr m_brushOutline;

		bool m_mouseIn;

		static ToolDescription<PaintTool, SceneView> g_toolDescription;
		static size_t g_firstPlugIndex;

};

IE_CORE_DECLAREPTR( PaintTool )

} // namespace GafferSceneUI
