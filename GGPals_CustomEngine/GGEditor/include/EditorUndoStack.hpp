/*************************************************************************
  \file       EditorUndoStack.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       20 November 2025
  \version    1.0
  \brief      Declares the EditorUndoStack class for managing editor undo/redo actions.

  \details
    The EditorUndoStack provides a command-based undo/redo management system
    for the GGEngine editor. It stores editor commands derived from
    IEditorCommand and allows pushing new actions, undoing previous actions,
    redoing reverted actions, and updating entity references when ECS entities
    are remapped. The system also ensures that editor changes trigger
    scene-modified events via the GlobalEventBus.

    The stack uses a fixed maximum history depth of 20 steps and automatically
    evicts the oldest commands when the limit is reached. Both undo and redo
    stacks are fully cleared when new actions are pushed.
  \dependencies
    - IEditorCommand.hpp
    - GGECSInterface.hpp (World, Entity)
    - GGEngineEventBus.hpp (GlobalEventBus)
    - <deque>, <memory>
  \history
    - 1.0 (20 November 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

// Editor command interface
#include <IEditorCommand.hpp>
#include <GGECSInterface.hpp>
#include <GGEngineEventBus.hpp>

#include <deque>
#include <memory>

class EditorUndoStack {
private:
    std::deque<std::unique_ptr<IEditorCommand>> undoStack;
    std::deque<std::unique_ptr<IEditorCommand>> redoStack;
    static constexpr size_t MAX_UNDO_STEPS = 30;

public:
    /*************************************************************************
      \brief
        Pushes a new editor command onto the undo stack and clears the redo stack.
        Emits a scene modified event.
      \param cmd
        Unique pointer to the editor command to be pushed.
    *************************************************************************/
    void Push(std::unique_ptr<IEditorCommand> cmd)
    {
        if (undoStack.size() >= MAX_UNDO_STEPS)
        {
            undoStack.pop_back();
        }

        undoStack.push_front(std::move(cmd));
        redoStack.clear();

        // mark scene dirty (*)
        GlobalEventBus::onSceneModified.Emit();
    }

    /*************************************************************************
      \brief
        Undoes the most recent command in the undo stack and moves it to the redo stack.
        Emits a scene modified event.
      \param world
        Reference to the ECS world for command execution.
    *************************************************************************/
    void Undo(World& world)
    {
        if (undoStack.empty()) return;

        // Get raw pointer first, execute, THEN move
        IEditorCommand* cmd = undoStack.front().get();
        cmd->Undo(world);

        redoStack.push_front(std::move(undoStack.front()));
        undoStack.pop_front();

        // mark scene dirty (*)
        GlobalEventBus::onSceneModified.Emit();
    }

    /*************************************************************************
      \brief
        Redoes the most recent command in the redo stack and moves it back to the undo stack.
        Emits a scene modified event.
      \param world
        Reference to the ECS world for command execution.
    *************************************************************************/
    void Redo(World& world)
    {
        if (redoStack.empty()) return;

        // Get raw pointer first, execute, THEN move
        IEditorCommand* cmd = redoStack.front().get();
        cmd->Execute(world);

        if (undoStack.size() >= MAX_UNDO_STEPS)
        {
            undoStack.pop_back();
        }
        undoStack.push_front(std::move(redoStack.front()));
        redoStack.pop_front();

        // mark scene dirty (*)
        GlobalEventBus::onSceneModified.Emit();
    }
    
    /*************************************************************************
      \brief
        Clears both the undo and redo stacks.
    *************************************************************************/
    void Clear()
    {
        undoStack.clear();
        redoStack.clear();
    }

    /*************************************************************************
      \brief
        Checks if there are commands available to undo.
      \return
        True if undo stack is not empty, false otherwise.
    *************************************************************************/
    bool CanUndo() const { return !undoStack.empty(); }

    /*************************************************************************
      \brief
        Checks if there are commands available to redo.
      \return
        True if redo stack is not empty, false otherwise.
    *************************************************************************/
    bool CanRedo() const { return !redoStack.empty(); }

    /*************************************************************************
      \brief
        Updates entity references in all stored commands in case
        a parent entity was deleted and recreated.
      \param original
        The original entity ID to replace.
      \param newEntity
        The new entity ID to use.
    *************************************************************************/
    void UpdateEntityReference(Entity original, Entity newEntity)
    {
        for (auto& ptr : undoStack)
        {
            ptr->UpdateEntityParentReference(original, newEntity);
        }

        for (auto& ptr : redoStack)
        {
            ptr->UpdateEntityParentReference(original, newEntity);
        }
    }
};

/*************************************************************************
  \brief
    Provides access to a singleton instance of EditorUndoStack.
  \return
    Reference to the unique_ptr managing the EditorUndoStack instance.
*************************************************************************/
inline std::unique_ptr<EditorUndoStack>& GetUndoStack()
{
    static std::unique_ptr<EditorUndoStack> undoStack = std::make_unique<EditorUndoStack>();
    return undoStack;
}