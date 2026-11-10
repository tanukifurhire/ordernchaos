/*************************************************************************
  \file       ComponentCommand.hpp
  \author     Tok Duan Jie Nelson
  \par        Email: t.duanjienelson@digipen.edu
  \date       19 November 2025
  \version    1.0
  \brief
	Defines command classes for undoable component addition and removal.
  \details
	This file implements the Command pattern for ECS component operations,
	enabling undo/redo functionality in the editor. It provides:
	- AddComponentCommand: Records component addition for undo/redo
	- RemoveComponentCommand: Stores component state before removal
	- Helper functions for pushing commands to the undo stack

	The commands work with any component type satisfying the ECS
	component requirements (POD-like structures with COMPONENT_ID).
  \dependencies
	IEditorCommand.hpp, GGECSInterface.hpp, GGECSWorld.hpp,
	GGECSComponents.hpp, EditorUndoStack.hpp
  \see
	IEditorCommand, EditorUndoStack, ComponentModifyCommand
  \history
	- 1.0 (19 November 2025): Initial implementation with generic component support
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
  \class AddComponentCommand
  \brief
	Command for adding a component to an entity with undo support.
  \tparam T
	Component type to add (must satisfy IsComponent concept).
  \details
	Encapsulates the addition of a component to an entity, storing the
	component's initial data for redo operations. When executed, adds
	the component to the target entity. When undone, removes the component.
  \note
	Execute() checks if component already exists before adding to prevent
	duplicate component errors. This makes the command idempotent.
  \see
	RemoveComponentCommand, ModifyComponentCommand
*************************************************************************/
template <IsComponent T>
class AddComponentCommand : public IEditorCommand
{
public:
	/*************************************************************************
	  \brief
		Constructs an add component command.
	  \param[in] entity
		Target entity that will receive the component.
	  \param[in] data
		Initial component data to add (copied by value).
	  \details
		Captures the entity handle and component state at command creation.
		The component data is deep-copied to ensure independence from
		the original component instance.
	*************************************************************************/
	AddComponentCommand(Entity entity, const T& data) : targetEntity(entity), componentData(data)
	{

	}

	/*************************************************************************
	  \brief
		Executes the add component operation (for redo).
	  \param[in,out] world
		ECS world containing the target entity.
	  \details
		Adds the stored component data to the target entity if not already
		present. This makes the operation safe to call multiple times.
		The component is added via World::AddComponent<T>(), which updates
		the entity's component signature automatically.
	  \note
		O(1) component addition via hash-based lookup in ComponentArray.
		No-op if component already exists (idempotent behavior).
	*************************************************************************/
	void Execute(World& world) override
	{
		std::cout << "[AddComponentCommand] Adding component to Entity(id="
			<< targetEntity.id << ", gen=" << (int)targetEntity.generation << ")" << std::endl;

		if (!world.IsEntityAlive(targetEntity)) {
			std::cerr << "[AddComponentCommand] ERROR: Entity is not alive!" << std::endl;
			return;
		}

		if (!world.HasComponent<T>(targetEntity))
		{
			world.AddComponent<T>(targetEntity, componentData);
			std::cout << "[AddCOmponentCommand] Componenet added successful!" << std::endl;
		}
	}

	/*************************************************************************
	  \brief
		Undoes the add component operation by removing it.
	  \param[in,out] world
		ECS world containing the target entity.
	  \details
		Removes the component from the target entity if present. This
		reverses the Execute() operation, restoring the entity to its
		state before the component was added.

		The component data is preserved in this command object for
		potential redo operations.
	  \note
		O(1) component removal via swap-and-pop in ComponentArray.
		Safe to call even if component is not present.
	*************************************************************************/
	void Undo(World& world) override
	{
		if (!world.IsEntityAlive(targetEntity)) return;

		if (world.HasComponent<T>(targetEntity))
		{
			world.RemoveComponent<T>(targetEntity);
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
		recreated (e.g., via undo). Updates this command's entity
		reference to point to the new handle, maintaining command validity
		across entity lifecycle changes.

		This handles the edge case where a parent entity is deleted after
		a child, then recreated through undo operations.
	  \note
		Entity handles use generational indices; this method ensures
		commands track the correct generation after entity recycling.
	*************************************************************************/
	void UpdateEntityParentReference(Entity original, Entity newEntity) override
	{
		if (targetEntity == original)
		{
			std::cout << "[AddComponentCommand] Updating entity reference from "
				<< original.id << " to " << newEntity.id << std::endl;
			targetEntity = newEntity;
		}
	}

private:
	Entity targetEntity;		// entity to receive the component
	T componentData;			// component data to add (stored for redo)
};

/*************************************************************************
  \class RemoveComponentCommand
  \brief
	Command for removing a component from an entity with undo support.
  \tparam T
	Component type to remove (must satisfy IsComponent concept).
  \details
	Encapsulates the removal of a component from an entity, storing the
	component's data before removal for undo operations. When executed,
	removes the component. When undone, restores the component with its
	original data.
  \note
	The constructor captures component state, so it must be called
	BEFORE the component is actually removed from the world.
  \see
	AddComponentCommand, ModifyComponentCommand
*************************************************************************/
template<typename T>
class RemoveComponentCommand : public IEditorCommand
{
public:

	/*************************************************************************
	  \brief
		Constructs a remove component command.
	  \param[in] entity
		Target entity that will lose the component.
	  \param[in] data
		Current component data (captured before removal).
	  \details
		Captures the entity handle and complete component state at command
		creation. This snapshot enables perfect restoration during undo.

		The component data is deep-copied to preserve state independently
		of the component array storage.
	  \note
		CRITICAL: This constructor must be called BEFORE the component
		is removed from the world, otherwise data will be invalid/garbage.
	*************************************************************************/
	RemoveComponentCommand(Entity entity, const T& data) : targetEntity(entity), componentData(data)
	{

	}

	/*************************************************************************
	  \brief
		Executes the remove component operation (for redo).
	  \param[in,out] world
		ECS world containing the target entity.
	  \details
		Removes the component from the target entity if present. The
		component data remains stored in this command object for potential
		undo operations.

		This operation updates the entity's component signature and removes
		the component slot from the ComponentArray via swap-and-pop.
	  \note
		O(1) removal via ComponentArray::RemoveComponent().
		Safe to call even if component is not present (idempotent).
	*************************************************************************/
	void Execute(World& world) override
	{
		if (!world.IsEntityAlive(targetEntity)) return;

		if (world.HasComponent<T>(targetEntity))
		{
			world.RemoveComponent<T>(targetEntity);
		}
	}

	/*************************************************************************
	  \brief
		Undoes the remove operation by restoring the component.
	  \param[in,out] world
		ECS world containing the target entity.
	  \details
		Adds the component back to the entity with its original data
		captured at command creation. This provides perfect state
		restoration, preserving all field values exactly as they were.

		The restoration is lossless because we stored a complete copy
		of the component data (not just references or IDs).
	  \note
		O(1) component addition via World::AddComponent<T>().
		Safe to call multiple times (checks if component already exists).
	*************************************************************************/
	void Undo(World& world) override
	{
		if (!world.IsEntityAlive(targetEntity)) return;

		if (!world.HasComponent<T>(targetEntity))
		{
			// restore the compoent with saved data
			world.AddComponent<T>(targetEntity, componentData);
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
		recreated (e.g., via undo). Updates this command's entity
		reference to point to the new handle.

		This is essential for maintaining command validity when entities
		are destroyed and recreated through undo/redo chains involving
		entity deletion commands.
	  \see
		EditorUndoStack::UpdateEntityReference()
	*************************************************************************/
	void UpdateEntityParentReference(Entity original, Entity newEntity) override
	{
		if (targetEntity == original)
		{
			targetEntity = newEntity;
		}
	}

private:
	Entity targetEntity;			// entity to remove component from
	T componentData;				// component state before removal (for undo)
};

/*************************************************************************
  \brief
	Helper function to create and push an add component command.
  \tparam T
	Component type to add.
  \param[in] entity
	Target entity that will receive the component.
  \param[in] componentData
	Initial component data (copied by value).
  \details
	Convenience wrapper that:
	1. Creates an AddComponentCommand<T> with the provided data
	2. Marks it as already executed (SetExecuted(true))
	3. Pushes it onto the global undo stack

	This should be called AFTER the component has been added to the
	entity via World::AddComponent<T>(). The "already executed" flag
	tells the undo stack that calling Execute() again would be a redo,
	not the initial execution.
  \note
	This function assumes the component was just added successfully.
	It does not perform the addition itself.
  \par Usage
  \code
	AABBFloat newComponent{};
	world.AddComponent<AABBFloat>(entity, newComponent);
	PushAddComponentCommand(entity, newComponent);  // Enable undo
  \endcode
  \see
	AddComponentCommand, GetUndoStack()
*************************************************************************/
template<typename T>
inline void PushAddComponentCommand(Entity entity, const T& componentData)
{
	auto cmd = std::make_unique<AddComponentCommand<T>>(entity, componentData);
	cmd->SetExecuted(true);
	GetUndoStack()->Push(std::move(cmd));
}

/*************************************************************************
  \brief
	Helper function to create and push a remove component command.
  \tparam T
	Component type to remove.
  \param[in] entity
	Target entity that will lose the component.
  \param[in,out] world
	ECS world to retrieve current component data from.
  \details
	Convenience wrapper that:
	1. Retrieves current component data from the entity
	2. Creates a RemoveComponentCommand<T> with that data
	3. Marks it as already executed (SetExecuted(true))
	4. Pushes it onto the global undo stack

	CRITICAL: This must be called BEFORE World::RemoveComponent<T>()
	is executed, so the component data can be captured for undo.
  \note
	If the component doesn't exist on the entity, stores a default-
	initialized component (zero-filled memory). This is safe because
	the undo operation checks HasComponent before adding.
  \par Usage
  \code
	// Capture state BEFORE removal
	PushRemoveComponentCommand<AABBFloat>(entity, world);

	// Now safe to remove
	world.RemoveComponent<AABBFloat>(entity);
  \endcode
  \warning
	Calling this AFTER removal will result in garbage/invalid data
	being stored for undo, causing potential crashes or corruption.
  \see
	RemoveComponentCommand, GetUndoStack()
*************************************************************************/
template<typename T>
inline void PushRemoveComponentCommand(Entity entity, World& world)
{
	// get current componenet data before removing
	T componentData{};
	if (auto* comp = world.GetComponent<T>(entity))
	{
		componentData = *comp;
	}

	auto cmd = std::make_unique<RemoveComponentCommand<T>>(entity, componentData);
	cmd->SetExecuted(true);
	GetUndoStack()->Push(std::move(cmd));
}