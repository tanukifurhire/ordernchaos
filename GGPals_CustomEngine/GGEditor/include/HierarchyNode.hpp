/*************************************************************************
  \file       HierarchyNode.hpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
	Editor panel class for ECS hierarchy visualization and management.

  \details
	Declares the HierarchyNode class, which provides an ImGui-based
	interface for viewing, selecting, and editing hierarchical entity
	relationships in the ECS world. Supports:
	  - Dynamic tree-based visualization of entities.
	  - Entity creation, deletion, and duplication.
	  - Drag-and-drop reparenting functionality.
	  - Real-time synchronization with the Inspector panel.

  \dependencies
	EditorSceneBase.hpp, GGECSInterface.hpp, GGEngine.hpp

  \see
	HierarchyNode.cpp, GGHierarchyTransformSystem.hpp

  \history

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <memory>

#include <GGECSInterface.hpp>
#include <GGEngine.hpp>

class HierarchyNode : public EditorSceneBase
{
public:
	HierarchyNode();

	/*************************************************************************
	  \brief
		Initializes the HierarchyNode panel.
	  \details
		Called once at editor startup.  Currently performs no initialization
		logic but exists to conform to the standard EditorSceneBase interface.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief
		Builds the dockable ImGui window for the Hierarchy panel.
	  \details
		Registers the Hierarchy window with the editors DockBuilder,
		positioning it according to the default dock layout managed by
		EditorManager.
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief
		Updates and renders the Hierarchy panel each frame.
	  \details
		Handles toolbar actions (Create, Delete, Show IDs), gathers all
		root entities from the ECS world, and renders the full hierarchical
		tree recursively using ImGui tree nodes.  Also manages drag-and-drop
		parenting, selection logic, and deselection when clicking empty space.
	*************************************************************************/
	virtual void Update() override;

	Entity GetSelectedEntity() {
		return selectedEntity;
	}

private:
	// Browser
	/*************************************************************************
	  \brief
		Renders a single entity and its children as ImGui tree nodes.
	  \param[in,out] world
		Reference to the ECS world providing hierarchy and name data.
	  \param[in] entity
		Entity to render in the tree.
	  \param[in] hierarchyDepth
		Current depth in the hierarchy tree (used for indentation and recursion).
	  \details
		Draws one entity entry in the Hierarchy window with expand/collapse
		arrows, selection highlighting, context menu, and drag-and-drop
		parenting.  Recursively calls itself to render all child entities.
	*************************************************************************/
	void RenderEntityHierarchyNode(World& world, Entity entitiy, int hierarchyDepth = 0);

	/*************************************************************************
	  \brief
		Builds a display string for an entitys label in the hierarchy.
	  \param[in] world
		Reference to the ECS world to query the entitys NameComponent.
	  \param[in] entity
		Entity whose display name should be retrieved.
	  \return
		Formatted string containing the entity name and, optionally,
		its internal ID when Show IDs is enabled.
	  \details
		Retrieves the NameComponent from the entity if present and appends
		its numeric ID when the showEntityIDs flag is true.
	*************************************************************************/
	std::string GetEntityDisplayName(World& world, Entity entity) const;

	/*************************************************************************
	  \brief
		Handles drag-and-drop parenting between entities.
	  \param[in,out] world
		Reference to the ECS world.
	  \param[in] sourceEntity
		Entity being dragged.
	  \param[in] targetEntity
		Entity onto which the source is dropped.
	  \details
		Invoked when a valid drag-and-drop payload is accepted.  Safely
		reparents \p sourceEntity under \p targetEntity using the hierarchy
		system, while preventing self-parenting and cyclic relationships.
	*************************************************************************/
	void HandleHierarchyDragDrop(World& world, Entity sourceEntity, Entity targetEntity);

	// Operations
	/*************************************************************************
	  \brief
		Creates a new entity, optionally under a specified parent.
	  \param[in,out] world
		Reference to the ECS world where the entity is created.
	  \param[in] parent
		Parent entity handle.  If NULL_ENTITY, the new entity is created
		at the root level.
	  \details
		Allocates a new entity, adds default LocalTransform, WorldTransform,
		and NameComponent components, and attaches it as a child of \p parent
		when provided.  The new entity becomes the currently selected one.
	*************************************************************************/
	void CreateNewEntity(World& world, Entity parent);

	/*************************************************************************
	  \brief
		Recursively deletes an entity and all its descendants.
	  \param[in,out] world
		Reference to the ECS world containing the entities.
	  \param[in] entity
		Entity to delete.
	  \details
		Traverses the hierarchy depth-first, removing all child entities,
		detaching them from their parent, and finally destroying the
		specified entity from the ECS world.
	*************************************************************************/
	void DeleteEntity(World& world, Entity entity);

	/*************************************************************************
	  \brief
		Displays the right-click context menu for a hierarchy entity.
	  \param[in,out] world
		Reference to the ECS world.
	  \param[in] entity
		Entity on which the context menu is invoked.
	  \details
		Provides editor options such as Create Child, Duplicate,
		and Delete for the selected entity.  Each option triggers
		the corresponding HierarchyNode operation.
	*************************************************************************/
	void ShowEntityContextMenu(World& world, Entity entity);

	/*************************************************************************
	  \brief
		Duplicates an entity, optionally attaching it under a new parent.
	  \param[in,out] world
		Reference to the ECS world.
	  \param[in] entity
		Source entity to duplicate.
	  \param[in] newParent
		Parent under which to place the duplicate.  If NULL_ENTITY,
		retains the original parent or creates a new root entity.
	  \details
		Copies all supported components from \p entity to a newly created
		entity, offsets its transform slightly to avoid overlap, and appends
		(Copy) to its name.  Recursively duplicates all children and
		re-creates their hierarchy relationships.  The top-level duplicate
		becomes the new selection in the editor.
	*************************************************************************/
	void DuplicateEntity(World& world, Entity entity, Entity newParent = NULL_ENTITY);

private:
	Entity selectedEntity = NULL_ENTITY;
	bool showEntityIDs = false;
	Entity draggedEntity = NULL_ENTITY;

	Callback<Entity> entitySelectedCallback;
	Callback<World&, Entity> onEntityDestroyedCallback;
	Callback<World&, World&> onSwitchScenesCallback;

	/*************************************************************************
	\brief
		Gets or sets the currently selected entity in the editor.
	\details
		- GetSelectedEntity(): Returns the entity currently selected in the editor’s
		  hierarchy or viewport.
		- SetSelectedEntity(): Updates the editor’s selected entity reference.

		These functions are used by the Hierarchy and Inspector panels to synchronize
		selection state, allowing both panels to display and modify data for the same entity.
	\return
		\GetSelectedEntity: Returns the Entity handle currently selected.
	\input
		\SetSelectedEntity:
			\param entity	The Entity to set as selected in the editor context.
	*************************************************************************/
	Entity GetSelectedEntity() const { return selectedEntity; }
	void SetSelectedEntity(Entity entity) { selectedEntity = entity; }
	int selectedSpawnPrefabIndex = -1;
};

/*************************************************************************
\brief
	Returns a global instance of the HierarchyNode editor panel.

\details
	- Provides a static, lazily-initialized singleton-like instance of the
	  HierarchyNode used throughout the editor.
	- Ensures only one HierarchyNode exists at any given time.
	- Used by other editor systems (e.g., Inspector, Scene Management)
	  to access and update the hierarchy view.

\return
	Reference to the active HierarchyNode instance.
*************************************************************************/
inline auto& GetHierarchyNode()
{
	static std::unique_ptr<HierarchyNode> hierarchy = std::make_unique<HierarchyNode>();
	return hierarchy;
}