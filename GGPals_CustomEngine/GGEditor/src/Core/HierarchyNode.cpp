/*************************************************************************
  \file       HierarchyNode.cpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
	Implements the HierarchyNode editor panel for entity management.

  \details
	Provides a dockable ImGui panel for browsing and manipulating
	ECS entities in a hierarchical structure. Features include:
	  - Recursive tree rendering of parent child relationships.
	  - Entity creation, deletion, duplication, and reparenting.
	  - Drag-and-drop hierarchy manipulation.
	  - Selection and deselection management.
	  - Integration with the Inspector panel for property editing.

  \dependencies
	HierarchyNode.hpp, GGSystemManagers.hpp, InspectorNode.hpp,
	imgui_internal.h, string, EditorEventBus.hpp

  \see
	HierarchyNode.hpp, GGHierarchyTransformSystem.hpp

  \history
	- 1.0 (1 November 2025): dockable ImGui panel for browsing and manipulating
	  ECS entities in a hierarchical structure

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <HierarchyNode.hpp>
#include <GGSystemManagers.hpp>
#include <InspectorNode.hpp>
#include <EditorEventBus.hpp>

#include <EditorUndoStack.hpp>
#include <EntityCreationCommand.hpp>
#include <HierarchyModifyCommand.hpp>
#include <EditorUtils.hpp>

#include <string>

#include <imgui_internal.h>

/*************************************************************************
  \brief
	Constructs the HierarchyNode editor panel.
  \details
	Initializes ImGui callback bindings for Init(), Update(), and DockBuild()
	so this panel can be managed through the editors docking system.
*************************************************************************/
HierarchyNode::HierarchyNode() {
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
	Initializes the HierarchyNode panel.
  \details
	Called once at editor startup.  Currently performs no initialization
	logic but exists to conform to the standard EditorSceneBase interface.
*************************************************************************/
void HierarchyNode::Init()
{
	// NOTE: Prefabs are now loaded during GGEngine::Init() (see GGEngine.cpp line ~364)
	// No need to load them here anymore - they're available in all build configurations

	entitySelectedCallback =
		[&](Entity entity)
		{
			SetSelectedEntity(entity);
		};

	onEntityDestroyedCallback =
		[&](World&, Entity entity)
		{
			if (selectedEntity == entity)
			{
				selectedEntity = NULL_ENTITY;
			}
		};

	onSwitchScenesCallback =
		[&](World&, World&)
		{
			selectedEntity = NULL_ENTITY;
		};

	EditorEventBus::onEntitySelected += entitySelectedCallback;
	GlobalEventBus::onSwitchScenes += onSwitchScenesCallback;
	onEntityDestroyed += onEntityDestroyedCallback;
}

/*************************************************************************
  \brief
	Builds the dockable ImGui window for the Hierarchy panel.
  \details
	Registers the Hierarchy window with the editors DockBuilder,
	positioning it according to the default dock layout managed by
	EditorManager.
*************************************************************************/
void HierarchyNode::DockBuild() {
	ImGui::DockBuilderDockWindow("Hierarchy", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockLeft);
}


/*************************************************************************
  \brief
	Updates and renders the Hierarchy panel each frame.
  \details
	Handles toolbar actions (Create, Delete, Show IDs), gathers all
	root entities from the ECS world, and renders the full hierarchical
	tree recursively using ImGui tree nodes.  Also manages drag-and-drop
	parenting, selection logic, and deselection when clicking empty space.
*************************************************************************/
void HierarchyNode::Update() {
	ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
	if (!ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoResize)) {
		ImGui::End();	// Properly close the window even if hidden/collapsed
		return;			// Skip drawing the rest of the UI for this window
	}
	else {
		if (!sceneManager.HasActiveScene()) { // only create new scene if there is no active scene
			sceneManager.SetActiveScene("UnknownScene");
		}
	}

	// ------------------------------------------------------------------------------
	// Display current scene name + "Save Changes" button
	// ------------------------------------------------------------------------------
	EditorSceneUtils::DrawSceneSaveHeader(sceneManager);
	EditorSceneUtils::DrawSceneSavePopup(sceneManager);
	// ------------------------------------------------------------------------------

	ImGui::Separator();

	World& world = GGEngine::GetEngine().GetSceneManager().GetActiveScene();

	if (ImGui::Button("Create Entity")) {

		// If something is selected, create new entity as its child
		Entity parent = (selectedEntity != NULL_ENTITY) ? selectedEntity : NULL_ENTITY;
		HierarchyNode::CreateNewEntity(world, parent);
	}
	ImGui::SameLine();
	// If there is a Entity Seleced on before clicking delete Selected
	if (ImGui::Button("Delete Selected") && selectedEntity != NULL_ENTITY) {
		// Delete Entity and all its descendants
		HierarchyNode::DeleteEntity(world, selectedEntity);
		selectedEntity = NULL_ENTITY;
	}
	ImGui::SameLine();
	// For Debugging: Show Entity ID
	ImGui::Checkbox("Show IDs", &showEntityIDs);

	ImGui::Separator();

	// Collect root entities (entities without HierarchyComponent or with no parent)
	std::vector<Entity> rootEntities;
	std::function<void(Entity, LocalTransform&)> collectRoots =
		[&](Entity entity, LocalTransform& transform)
		{
			(void)transform;
			// Root entities are those that either:
			// 1. Have no HierarchyComponent at all, OR
			// 2. Have a HierarchyComponent but no valid parent (parent == NULL_ENTITY)
			if (!world.HasComponent<HierarchyComponent>(entity) || world.GetComponent<HierarchyComponent>(entity)->parent == NULL_ENTITY) {
				rootEntities.push_back(entity);
			}
		};
	// Execute Iterate
	world.ForEach<LocalTransform>(collectRoots);

	// Render hierarchy tree
	for (Entity rootEntity : rootEntities) {
		HierarchyNode::RenderEntityHierarchyNode(world, rootEntity, 0);
	}

	// Create an invisible ImGui widget (10px tall, full width) at the bottom of the tree
	// Ensure Spacing at the bottom of window to drop else no item to recieve drag-drop
	ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 10));

	// Handle global drag and drop(create root entities)
	if (ImGui::BeginDragDropTarget()) {
		// Get data
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
			Entity* sourceEntity = (Entity*)payload->Data;
			auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

			std::vector<HierarchySnapshot> snapshots;
			
			auto addSnapshot = [&](Entity e)
			{
				if (e == NULL_ENTITY || !world.IsEntityAlive(e)) { return; }
				if (!world.HasComponent<HierarchyComponent>(e)) { return; }

				HierarchySnapshot s;
				s.entity = e;
				s.before = *world.GetComponent<HierarchyComponent>(e);
				s.after = {};
				snapshots.push_back(s);
			};

			addSnapshot(*sourceEntity);

			if (world.HasComponent<HierarchyComponent>(*sourceEntity))
			{
				auto* sh = world.GetComponent<HierarchyComponent>(*sourceEntity);
				addSnapshot(sh->parent);
				addSnapshot(sh->nextSibling);
			}

			// unparent entity
			hierarchySystem.RemoveParent(world, *sourceEntity);

			// capture after state
			for (auto& s : snapshots)
			{
				if (world.IsEntityAlive(s.entity) && world.HasComponent<HierarchyComponent>(s.entity))
				{
					s.after = *world.GetComponent<HierarchyComponent>(s.entity);
				}
			}

			PushModifyHierarchyCommand(std::move(snapshots));
		}
		ImGui::EndDragDropTarget();
	}

	// Deselect entity if user clicks on empty space in the Hierarchy window
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
		// Only deselect if not clicking on an entity item
		if (!ImGui::IsAnyItemHovered()) {
			selectedEntity = NULL_ENTITY;
			EditorEventBus::onEntitySelected.Emit(Entity(NULL_ENTITY));
		}
	}

	ImGui::End();
}

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
void HierarchyNode::CreateNewEntity(World& world, Entity parent) {
	if (!world.IsEntityAlive(parent) && parent != NULL_ENTITY)
	{
		parent = NULL_ENTITY;
	}
	EntitySnapshot snapshot;
	snapshot.parentEntity = parent;

	// Create Local Transform for child
	Entity newEntity =
		Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>()
		.CreateNewEntity(world, parent);

	snapshot.entity = newEntity;

	snapshot.serializedData = GetEngineInstance().GetPrefabManager().SaveEntityBackup(newEntity, world);

	auto command = std::make_unique<CreateEntityCommand>(snapshot);
	command->SetExecuted(true);
	GetUndoStack()->Push(std::move(command));

	// Select New Entity
	selectedEntity = newEntity;
	EditorEventBus::onEntitySelected.Emit(Entity(newEntity));
}

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
void HierarchyNode::DeleteEntity(World& world, Entity entity) {
	if (!world.IsEntityAlive(entity)) return;

	EntitySnapshot snapshot;
	snapshot.entity = entity;

	if (world.HasComponent<HierarchyComponent>(entity))
	{
		HierarchyComponent* hierarchyComponent = world.GetComponent<HierarchyComponent>(entity);

		if (hierarchyComponent)
		{
			snapshot.parentEntity = hierarchyComponent->parent;
		}
	}

	snapshot.serializedData = GetEngineInstance().GetPrefabManager().SaveEntityBackup(entity, world);

	auto command = std::make_unique<DeleteEntityCommand>(snapshot);
	command->SetExecuted(true);
	GetUndoStack()->Push(std::move(command));

	Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>()
		.DeleteEntity(world, entity);
}

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
void HierarchyNode::RenderEntityHierarchyNode(World& world, Entity entity, int hierarchyDepth) {
	auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

	// Display set up to configure ImGui tree node appearance and interaction:
	// - ImGuiTreeNodeFlags_OpenOnArrow: expand/collapse only when the small arrow is clicked
	//   (clicking the label instead selects the entity, like in Unity's Hierarchy) (Dropdowm Menu)
	// - ImGuiTreeNodeFlags_SpanAvailWidth: make the selectable area span the full row width
	//   for better UX and consistent highlight behavior
	// - ImGuiTreeNodeFlags_Selected: visually highlight the node if this entity is currently selected.
	//  |  (bitwise OR) merges bits from multiple flags into one variable
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	// If this entity is currently selected, mark it visually as selected.
	if (selectedEntity == entity) {
		//  |=  (bitwise OR assignment) preserves existing flags and adds a new one
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// GetEntityDisplayName 
	std::string entityName = GetEntityDisplayName(world, entity);

	// Check if entity has children, this entity is considered a "leaf" node in the tree
	std::vector<Entity> children = hierarchySystem.GetChildren(world, entity);
	if (children.empty()) {
		// Add two ImGui tree node flags:
		// ImGuiTreeNodeFlags_Leaf:
		//   - Marks this node as a leaf (it has no expandable arrow next to it).
		//   - ImGui will render it as a single item instead of a collapsible folder.
		// ImGuiTreeNodeFlags_NoTreePushOnOpen:
		//   - Tells ImGui not to expect a matching ImGui::TreePop() later.
		//   - Used because wont open or recurse into this node (since it has no children).
		// The bitwise OR (|) combines both flags into the 'flags' variable so both effects apply.
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	ImGui::Indent(7);

	ImGui::PushID(entity.id);

	// Render this entity as a tree node in the Hierarchy window.
	// - (void*)(intptr_t)entity.id: unique ImGui ID (cast from entity's numeric ID)
	// - intptr_t: pointer-sized integer (avoids size loss on 32/64-bit systems)
	// - void*: ImGui expects IDs as void pointers, not raw integers.
	// - flags: visual/behavior settings (e.g., selected, open-on-arrow)
	// - entityName.c_str(): label text shown for the node
	// Returns true if the node is expanded, false if collapsed.
	bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity.id, flags, entityName.c_str());

	// Selection
	if (ImGui::IsItemClicked()) {
		selectedEntity = entity;
		// Tell inspector that selected entity changed and set to the new entity
		EditorEventBus::onEntitySelected.Emit(Entity(entity));

		WorldTransform* worldTransform = world.GetComponent<WorldTransform>(entity);

		if (worldTransform)
		{
			GGLogger::Info("Selected Entity World Transform: " + std::to_string(worldTransform->matrix.getTranslation().y));
		}
	}

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift)
	{
		ImGui::OpenPopup("EntityContextMenu");
	}

	// Context Menu (right-click on this entity)
	if (ImGui::BeginPopup("EntityContextMenu")) {
		HierarchyNode::ShowEntityContextMenu(world, entity);
		ImGui::EndPopup();
	}

	// Drag source (Entity)
	if (ImGui::BeginDragDropSource()) {
		// Define the payload (data) for this drag operation:
		// - "ENTITY_HIERARCHY" is the payload type identifier it lets drop targets
		//   recognize that the thing being dragged is a hierarchy entity
		// - &entity: is the pointer to the actual data being sent (the entity ID)
		// - sizeof(Entity): tells ImGui how many bytes to copy for this payload
		ImGui::SetDragDropPayload("ENTITY_HIERARCHY", &entity, sizeof(Entity));

		// Display a small tooltip under the cursor while dragging.
		// This is purely visual: it shows which entity is being moved.
		ImGui::Text("Move %s", entityName.c_str());

		ImGui::EndDragDropSource();

		// Store which entity is currently being dragged so can check it later during the drop target phase
		draggedEntity = entity;
	}

	// Drop target (only if not dragging self)
	if (draggedEntity != entity && ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
			Entity* sourceEntity = (Entity*)payload->Data;
			HierarchyNode::HandleHierarchyDragDrop(world, *sourceEntity, entity);
		}
		ImGui::EndDragDropTarget();
	}

	// Render children recursively
	// If this entity's node is expanded (nodeOpen == true)
	// and it actually has child entities need to draw them
	if (nodeOpen && !children.empty()) {
		for (Entity childEntity : children) {
			// This means each child will render its own node, check its own children building the full tree structure
			// hierarchyDepth + 1: tracks how deep are in the tree max 16
			HierarchyNode::RenderEntityHierarchyNode(world, childEntity, hierarchyDepth + 1);
		}
		// Once all children have been drawn close the parent node requires a matching TreePop() for every TreeNodeEx() that was opened
		ImGui::TreePop();
	}

	ImGui::PopID();
	ImGui::Unindent(7);
}

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
std::string HierarchyNode::GetEntityDisplayName(World& world, Entity entity) const {
	std::string baseName = "Entity";

	if (world.HasComponent<NameComponent>(entity)) {
		baseName = world.GetComponent<NameComponent>(entity)->name;
	}

	// If showEntityIDs == true
	if (showEntityIDs) {
		baseName += "(" + std::to_string(entity.id) + ")";
	}

	return baseName;
}

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
void HierarchyNode::ShowEntityContextMenu(World& world, Entity entity) {
	if (ImGui::MenuItem("Create Child")) {
		HierarchyNode::CreateNewEntity(world, entity);
	}
	if (ImGui::MenuItem("Duplicate")) {
		HierarchyNode::DuplicateEntity(world, entity);
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Delete")) {
		if (entity == selectedEntity) {
			selectedEntity = NULL_ENTITY;
			EditorEventBus::onEntitySelected.Emit(Entity(NULL_ENTITY));
		}
		DeleteEntity(world, entity);
	}
}

/*************************************************************************
* // TODO: USE ENTITY JSON SERIALIZATION FOR THIS.
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
void HierarchyNode::DuplicateEntity(World& world, Entity entity, Entity newParent) {
	// Create empty and copy them later
	Entity newEntity = world.CreateEntityHandle();

	// Copy LocalTransform (and offset it slightly to avoid overlapping)
	if (world.HasComponent<LocalTransform>(entity)) {
		auto localTransform = world.GetComponent<LocalTransform>(entity);
		localTransform->isDirty = true;
		world.AddComponent<LocalTransform>(newEntity, *localTransform);
		world.AddComponent<WorldTransform>(newEntity, {});
	}

	// Copy NameComponent and append '(Copy)' 
	if (world.HasComponent<NameComponent>(entity)) {
		auto nameComponent = world.GetComponent<NameComponent>(entity);

		NameComponent nameCopy = *nameComponent;
		std::string newName = "(Copy) " + std::string(nameCopy.name);

		// Safety so don't hold more than 28 char last byte alw for the null terminator ('\0') because of (Copy)
		std::strncpy(nameCopy.name, newName.c_str(), sizeof(nameCopy.name) - 1);
		nameCopy.name[sizeof(nameCopy.name) - 1] = '\0';

		world.AddComponent<NameComponent>(newEntity, nameCopy);
	}
	else {
		// If original had no name, add a generic one
		NameComponent defaultName;
		std::strcpy(defaultName.name, "Entity (Copy)");
		world.AddComponent<NameComponent>(newEntity, defaultName);
	}

	// Copy Rigidbody2DFloat (if applicable)
	if (world.HasComponent<Rigidbody2DFloat>(entity)) {
		auto body = world.GetComponent<Rigidbody2DFloat>(entity);
		world.AddComponent<Rigidbody2DFloat>(newEntity, *body);
	}

	// Copy Sprite (if applicable)
	if (world.HasComponent<Sprite>(entity)) {
		auto sprite = world.GetComponent<Sprite>(entity);
		world.AddComponent<Sprite>(newEntity, *sprite);
	}

	// Copy SpriteAnimator (if applicable)
	if (world.HasComponent<SpriteAnimator>(entity)) {
		auto animator = world.GetComponent<SpriteAnimator>(entity);
		world.AddComponent<SpriteAnimator>(newEntity, *animator);
	}

	// Copy MeshRenderer (if applicable)
	if (world.HasComponent<MeshRenderer>(entity)) {
		auto meshRenderer = world.GetComponent<MeshRenderer>(entity);
		world.AddComponent<MeshRenderer>(newEntity, *meshRenderer);
	}

	// Copy AABBFloat (if applicable)
	if (world.HasComponent<AABBFloat>(entity)) {
		auto aabb = world.GetComponent<AABBFloat>(entity);
		world.AddComponent<AABBFloat>(newEntity, *aabb);
	}

	// Copy ScreenBounds (if applicable)
	if (world.HasComponent<ScreenBounds>(entity)) {
		auto bounds = world.GetComponent<ScreenBounds>(entity);
		world.AddComponent<ScreenBounds>(newEntity, *bounds);
	}

	// Copy UIComponent (if applicable)
	if (world.HasComponent<UIComponent>(entity)) {
		auto ui = world.GetComponent<UIComponent>(entity);
		world.AddComponent<UIComponent>(newEntity, *ui);
	}

	// Copy ECSGridComponent (if applicable)
	if (world.HasComponent<ECSGridComponent>(entity)) {
		auto grid = world.GetComponent<ECSGridComponent>(entity);
		world.AddComponent<ECSGridComponent>(newEntity, *grid);
	}

	// Copy AudioSource (if applicable)
	if (world.HasComponent<AudioSource>(entity)) {
		auto audio = world.GetComponent<AudioSource>(entity);
		AudioSource copy = *audio;
		copy.runtimeChannel = nullptr;
		copy.isPlaying = false;
		copy.isPaused = false;
		copy.justTriggered = false;
		copy.hasPlayedOnAwake = false;
		world.AddComponent<AudioSource>(newEntity, copy);
	}

	// Copy TilemapComponent (if applicable)
	if (world.HasComponent<TilemapComponent>(entity)) {
		auto tilemap = world.GetComponent<TilemapComponent>(entity);
		TilemapComponent copy = *tilemap;
		// Reset runtime generated tilemap instance so the tilemap system creates a fresh one
		copy.tilemapInstance = 0;
		world.AddComponent<TilemapComponent>(newEntity, copy);
	}

	// Copy ScriptComponent (if applicable)
	if (world.HasComponent<ScriptComponent>(entity)) {
		auto script = world.GetComponent<ScriptComponent>(entity);
		ScriptComponent copy = *script;

		// Reset script VM instance so the duplicated entity gets its own script object
		copy.scriptInstance = 0;

		// Invalidate cached script module lookup so it is rebound for the new instance
		copy.moduleID = -2;

		// Invalidate cached start function ID so Start() is resolved again correctly
		copy.startFunctionID = -2;

		world.AddComponent<ScriptComponent>(newEntity, copy);
	}

	// Copy TextComponent (if applicable)
	if (world.HasComponent<TextComponent>(entity)) {
		auto text = world.GetComponent<TextComponent>(entity);
		world.AddComponent<TextComponent>(newEntity, *text);
	}
	// Copy TextComponent (if applicable)
	if (world.HasComponent<WorldTextComponent>(entity)) {
		auto text = world.GetComponent<WorldTextComponent>(entity);
		world.AddComponent<WorldTextComponent>(newEntity, *text);
	}


	//effect comp
	if (world.HasComponent<ObjectEffectComponent>(entity)) {
		auto text = world.GetComponent<ObjectEffectComponent>(entity);
		world.AddComponent<ObjectEffectComponent>(newEntity, *text);
	}

	// Keep the same parent instead of making the duplicate a parent itself (if applicable)
	auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

	// If this is a child duplicate, attach it under its new parent
	if (newParent != NULL_ENTITY) {
		// Recursively call SetParent() to link the newly created duplicate(newEntity) under its new duplicated parent(newParent)
		hierarchySystem.SetParent(world, newEntity, newParent);
	}
	// Otherwise, if original had a parent, keep the same one
	else if (world.HasComponent<HierarchyComponent>(entity)) {
		const auto& originalHierarchy = *world.GetComponent<HierarchyComponent>(entity);
		if (originalHierarchy.parent != NULL_ENTITY) {
			hierarchySystem.SetParent(world, newEntity, originalHierarchy.parent);
		}
	}

	// Recursively duplicate its children
	if (world.HasComponent<HierarchyComponent>(entity)) {
		std::vector<Entity> children = hierarchySystem.GetChildren(world, entity);
		for (Entity child : children) {
			// Pass newEntity as the parent for each recursive child duplication
			HierarchyNode::DuplicateEntity(world, child, newEntity);
		}
	}
	
	// Select the one the user directly duplicated 
	if (newParent == NULL_ENTITY)
	{
		selectedEntity = newEntity;
		EditorEventBus::onEntitySelected.Emit(Entity(newEntity));

		// Record the duplication as a CreateEntityCommand so Ctrl+Z can undo it
		EntitySnapshot snapshot;
		snapshot.entity = newEntity;

		// Capture the parent from the hierarchy if one exists
		if (world.HasComponent<HierarchyComponent>(newEntity))
		{
			HierarchyComponent* hier = world.GetComponent<HierarchyComponent>(newEntity);
			if (hier)
			{
				snapshot.parentEntity = hier->parent;
			}
		}

		snapshot.serializedData = GetEngineInstance().GetPrefabManager().SaveEntityBackup(newEntity, world);

		auto command = std::make_unique<CreateEntityCommand>(snapshot);
		command->SetExecuted(true);
		GetUndoStack()->Push(std::move(command));
	}
}

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
void HierarchyNode::HandleHierarchyDragDrop(World& world, Entity sourceEntity, Entity targetEntity) {
	auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

	// Safety: prevent self parenting and cycles
	if (sourceEntity == targetEntity) {
		return;
	}

	std::vector<HierarchySnapshot> snapshots;
	
	auto addSnapshot = [&](Entity e)
	{
		if (e == NULL_ENTITY || !world.IsEntityAlive(e)) { return; }
		if (!world.HasComponent<HierarchyComponent>(e))
		{
			// add hierarchy component for tracking if the entity doesn't have one
			world.AddComponent<HierarchyComponent>(e, HierarchyComponent());
			HierarchyComponent* hierarchy = world.GetComponent<HierarchyComponent>(e);

			hierarchy->parent = NULL_ENTITY;
			hierarchy->firstChild = NULL_ENTITY;
			hierarchy->nextSibling = NULL_ENTITY;
		}

		HierarchySnapshot s;
		s.entity = e;
		s.before = *world.GetComponent<HierarchyComponent>(e);
		s.after = {};
		snapshots.push_back(s);
	};

	addSnapshot(sourceEntity);
	addSnapshot(targetEntity);

	if (world.HasComponent<HierarchyComponent>(sourceEntity)) 
	{
		auto* sh = world.GetComponent<HierarchyComponent>(sourceEntity);
		addSnapshot(sh->nextSibling);
		// find previous sibling
		if (sh->parent != NULL_ENTITY && world.HasComponent<HierarchyComponent>(sh->parent))
		{
			Entity sib = world.GetComponent<HierarchyComponent>(sh->parent)->firstChild;
			while (sib != NULL_ENTITY && sib != sourceEntity)
			{
				auto* sibH = world.GetComponent<HierarchyComponent>(sib);
				if (!sibH) { break; }
				if (sibH->nextSibling == sourceEntity) 
				{ 
					addSnapshot(sib);
					break;
				}
				sib = sibH->nextSibling;
			}
		}
	}

	if (world.HasComponent<HierarchyComponent>(targetEntity))
	{
		auto* th = world.GetComponent<HierarchyComponent>(targetEntity);
		if (th)
		{
			addSnapshot(th->firstChild);
			addSnapshot(th->parent);
		}
	}

	// Set new parent relationship
	hierarchySystem.SetParent(world, sourceEntity, targetEntity);

	for (auto& s : snapshots)
	{
		if (world.IsEntityAlive(s.entity) && world.HasComponent<HierarchyComponent>(s.entity))
		{
			s.after = *world.GetComponent<HierarchyComponent>(s.entity);
		}
	}

	PushModifyHierarchyCommand(std::move(snapshots));
}