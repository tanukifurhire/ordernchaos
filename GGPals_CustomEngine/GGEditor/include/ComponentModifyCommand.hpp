/*************************************************************************
  \file       ComponentModifyCommand.hpp
  \author     Tok Duan Jie Nelson
  \par        Email: t.duanjienelson@digipen.edu
  \date       19 November 2025
  \version    1.0
  \brief
    Defines command class for undoable component field modifications.
  \details
    This file implements the Command pattern for tracking and reverting
    changes to component data fields. It enables undo/redo functionality
    for inspector modifications by storing component state before and
    after editing operations.
    
    Key Features:
    - Generic template works with any component type
    - Stores complete before/after snapshots (lossless undo)
    - Memory comparison avoids creating unnecessary undo entries
    - Integrates with EditorUndoStack for command history
  \dependencies
    IEditorCommand.hpp, GGECSInterface.hpp, GGECSWorld.hpp,
    GGECSComponents.hpp, EditorUndoStack.hpp, <cstring>
  \see
    IEditorCommand, EditorUndoStack, ComponentCommand
  \history
    - 1.0 ([Date]): Initial implementation with memcmp optimization
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <IEditorCommand.hpp>
#include <GGECSInterface.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>
#include <EditorUndoStack.hpp>

/*************************************************************************
  \class ModifyComponentCommand
  \brief
    Command for tracking component field modifications with undo support.
  \tparam T
    Component type being modified (must satisfy IsComponent concept).
  \details
    Encapsulates a change to a component's fields, storing both the
    before and after states. This enables bidirectional navigation
    through edit history:
    - Execute() applies the "after" state (redo)
    - Undo() applies the "before" state (revert changes)
    Use Cases:
    - Dragging transform position slider
    - Changing color values in color picker
    - Modifying physics properties (mass, velocity, etc.)
    - Editing any component field in the inspector
  \note
    Components must be POD-like (trivially copyable) for memcmp to work
    correctly. Complex components with pointers or non-trivial copy
    semantics may require custom comparison operators.
  \see
    AddComponentCommand, RemoveComponentCommand, TransformCommand
*************************************************************************/
template<typename T>
class ModifyComponentCommand : public IEditorCommand
{
public:

    /*************************************************************************
      \brief
        Constructs a modify component command.
      \param[in] entity
        Target entity whose component is being modified.
      \param[in] before
        Component state before the edit operation.
      \param[in] after
        Component state after the edit operation.
      \details
        Captures the complete before/after snapshots of the component.
        Both states are deep-copied to ensure independence from the
        original component storage in the World's ComponentArray.

        Typical Usage Pattern:
        1. User starts editing (mouse down on slider)
           -> Capture "before" state
        2. User adjusts value (dragging)
           -> Update component in real-time
        3. User finishes editing (mouse up)
           -> Capture "after" state
           -> Create ModifyComponentCommand
           -> Push to undo stack
      \note
        The constructor does not validate that before != after.
        Use PushModifyComponentCommand() helper for automatic change detection.
    *************************************************************************/
	ModifyComponentCommand(Entity entity, const T& before, const T& after)
		: targetEntity(entity), beforeState(before), afterState(after) 
	{

	}

    /*************************************************************************
      \brief
        Executes the modification (applies "after" state for redo).
      \param[in,out] world
        ECS world containing the target entity and component.
      \details
        Overwrites the component's current data with the stored "after"
        state, effectively reapplying the edit operation. This is used
        when redoing a previously undone modification.

        The operation is a simple memory copy (operator=) of the entire
        component structure, ensuring all fields are updated atomically.
      \note
        O(1) operation via World::GetComponent<T>() pointer access.
        If component doesn't exist, silently does nothing (safe no-op).
      \warning
        Does not trigger any component change notifications or events.
        Systems that cache component data may need manual refresh.
    *************************************************************************/
	void Execute(World& world) override
	{
        if (!world.IsEntityAlive(targetEntity)) return;

		if (auto* component = world.GetComponent<T>(targetEntity))
		{
			*component = afterState;
		}
	}

    /*************************************************************************
      \brief
        Undoes the modification (applies "before" state to revert).
      \param[in,out] world
        ECS world containing the target entity and component.
      \details
        Overwrites the component's current data with the stored "before"
        state, effectively reverting the edit operation. This restores
        the component to its exact state prior to the modification.

        The restoration is lossless because we store complete component
        snapshots (not just changed fields or deltas).
      \note
        O(1) operation via World::GetComponent<T>() pointer access.
        Safe to call even if component was removed (no-op in that case).
    *************************************************************************/
	void Undo(World& world) override
	{
        if (!world.IsEntityAlive(targetEntity)) return;

		if (auto* component = world.GetComponent<T>(targetEntity))
		{
			*component = beforeState;
		}
	}

    /*************************************************************************
      \brief
        Updates entity reference after entity recreation.
      \param[in] original
        Original entity handle before destruction/recreation.
      \param[in] newEntity
        New entity handle after recreation with same logical identity.
      \details
        Called by EditorUndoStack when an entity is deleted and then
        recreated (e.g., via undo of entity deletion). Updates this
        command's entity reference to track the new handle.

        This handles complex undo/redo chains where entity lifecycle
        commands (create/delete) are mixed with component modification
        commands. Without this, component modifications would target
        invalid (destroyed) entity handles.
      \note
        Entity handles use generational indices to detect use-after-free.
        This method ensures commands always reference the current generation.
      \see
        EditorUndoStack::UpdateEntityReference(), CreateEntityCommand
    *************************************************************************/
	void UpdateEntityParentReference(Entity original, Entity newEntity) override
	{
		if (targetEntity == original)
		{
			targetEntity = newEntity;
		}
	}

private:
	Entity targetEntity;			// entity owning the modified component
	T beforeState;					// component state before modification (Undo)
	T afterState;					// conponent state after modification (Redo)
};

/*************************************************************************
  \brief
    Helper function to create and push a modification command with
    automatic change detection.
  \tparam T
    Component type being modified.
  \param[in] entity
    Target entity whose component was modified.
  \param[in] before
    Component state before the edit sequence.
  \param[in] after
    Component state after the edit sequence.
  \details
    Convenience wrapper that:
    1. Compares before/after states via memcmp (byte-level comparison)
    2. Only creates a command if there was an actual change
    3. Marks command as already executed (SetExecuted(true))
    4. Pushes command onto the global undo stack

    The memcmp optimization prevents cluttering the undo stack with
    no-op commands when the user clicks on a slider but doesn't move it.

    Typical Integration Pattern:
    \code
    // In component inspector code:
    static AABBFloat beforeState{};
    static bool isEditing = false;

    AABBFloat beforeFrame = *aabb;
    auto result = InspectComponentAABBFloat(*aabb);

    if (result == ComponentInspectorResult::Modified) {
        if (!isEditing) {
            beforeState = beforeFrame;  // Capture on first edit
            isEditing = true;
        }
    }

    if (isEditing && !ImGui::IsAnyItemActive()) {
        // User finished editing (released mouse/keyboard)
        PushModifyComponentCommand(entity, beforeState, *aabb);
        isEditing = false;
    }
    \endcode
  \note
    memcmp performs byte-level comparison, which works for POD types.
    Components with padding bytes or floating-point fields may trigger
    false positives if padding is uninitialized. Zero-initialize
    components to avoid this.
  \par Performance
    sizeof(T) bytes compared via memcmp (typically 16-128 bytes).
    Negligible overhead compared to ImGui interaction latency (16ms frame).
  \see
    ModifyComponentCommand, TransformCommand (custom snapshot example)
*************************************************************************/
template<typename T>
inline void PushModifyComponentCommand(Entity entity, const T& before, const T& after)
{
	// only push if there was an actual change
	if (memcmp(&before, &after, sizeof(T)) != 0)
	{
		auto cmd = std::make_unique<ModifyComponentCommand<T>>(entity, before, after);
		cmd->SetExecuted(true);
		GetUndoStack()->Push(std::move(cmd));

        std::cout << "Pushing modify component command for component: " << (ComponentTypeInfo<T>::GetTypeName()) << "\n" << "for entity: " << entity.id << "\n";
	}
}