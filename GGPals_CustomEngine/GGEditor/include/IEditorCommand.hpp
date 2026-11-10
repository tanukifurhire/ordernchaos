/*************************************************************************
  \file       IEditorCommand.hpp
  \author	  Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief      Interface for editor undo/redo command objects.

  \details
	Defines the base interface for all editor command types used within
	the GGEngine editor's undo/redo system. Each command represents a
	reversible action, such as creating, deleting, or modifying entities
	within the ECS world.

	Derived command classes must implement Execute() to apply the action
	and Undo() to reverse it. The interface also provides support for
	updating internal entity references when entity IDs change during
	restoration, which is essential for maintaining hierarchy integrity
	after undo/redo operations.

	The interface is used by EditorUndoStack to manage command histories
	and ensure consistent state transitions across editing operations.

  \dependencies
	- GGECSInterface.hpp (Entity type, ECS definitions)
	- Forward declaration of World
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGECSInterface.hpp>

class World;

class IEditorCommand
{
public:
	/*************************************************************************
	  \brief Virtual destructor.
	  \details Ensures proper cleanup of derived command objects.
	*************************************************************************/
	virtual ~IEditorCommand() = default;

	/*************************************************************************
	  \brief Executes the command on a given ECS world.
	  \param world Reference to the World where the command should be applied.
	*************************************************************************/
	virtual void Execute(World& world) = 0;

	/*************************************************************************
	  \brief Undoes the command on a given ECS world.
	  \param world Reference to the World where the command should be undone.
	*************************************************************************/
	virtual void Undo(World& world) = 0;

	/*************************************************************************
	  \brief Updates entity parent references if an entity has been replaced.
	  \param original Original entity ID.
	  \param newEntity New entity ID to replace the original.
	  \details Useful for maintaining hierarchy consistency after undo/redo.
	*************************************************************************/
	virtual void UpdateEntityParentReference(Entity original, Entity newEntity) = 0;

	/*************************************************************************
	  \brief Checks if the command has already been executed.
	  \return True if executed, false otherwise.
	*************************************************************************/
	bool IsExecuted() const { return isExecuted; }

	/*************************************************************************
	  \brief Sets the executed state of the command.
	  \param executed True if executed, false otherwise.
	*************************************************************************/
	void SetExecuted(bool executed) { isExecuted = executed; }

protected:
	// NOTE: isExecuted is set by push helpers to indicate the action was already
	// performed before being pushed onto the undo stack. It is currently not read
	// by EditorUndoStack. If guarding against double-execution is needed in future,
	// wire this flag into Execute() on derived commands.
	bool isExecuted = false;
};