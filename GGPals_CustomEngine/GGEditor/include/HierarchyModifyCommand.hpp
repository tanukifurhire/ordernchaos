/*************************************************************************
  \file       HierarchyModifyCommand.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       12 March 2026
  \version    1.0
  \brief
    Defines an undoable command for entity hierarchy modifications.
  \details
    This file implements the Command pattern to support undo and redo
    functionality for hierarchy changes in the editor.

    The command records the hierarchy state of affected entities before
    and after an operation (such as re-parenting, detaching, or moving
    entities within the hierarchy). During undo or redo, the stored
    snapshots are restored to revert or reapply the hierarchy change.

    Key Features:
    - Stores before and after hierarchy snapshots for each affected entity
    - Supports undo/redo of drag-and-drop hierarchy operations
    - Skips unchanged snapshots to avoid unnecessary undo entries
    - Updates entity references if entities are remapped during editor operations
    - Integrates with the EditorUndoStack command history system

    Typical Usage:
    - Dragging an entity under another entity
    - Moving an entity between parents
    - Detaching an entity to the root level

  \dependencies
    IEditorCommand.hpp, GGECSInterface.hpp, GGECSWorld.hpp,
    GGECSComponents.hpp, EditorUndoStack.hpp, <vector>, <cstring>

  \see
    IEditorCommand, EditorUndoStack, HierarchyComponent

  \history
    - 1.0 (12 March 2026): Initial implementation for hierarchy undo/redo support

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <IEditorCommand.hpp>
#include <GGECSInterface.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>
#include <EditorUndoStack.hpp>
#include <vector>
#include <cstring>

/*************************************************************************
  \struct     HierarchySnapshot
  \brief
    Records the hierarchy state of a single entity before and after
    a hierarchy modification.
  \details
    Used by ModifyHierarchyCommand to store the before and after states
    of each affected entity so that undo and redo can restore either
    state precisely.
*************************************************************************/
struct HierarchySnapshot
{
    Entity entity;
    HierarchyComponent before;
    HierarchyComponent after;
};

/*************************************************************************
  \class      ModifyHierarchyCommand
  \brief
    Undoable command that applies and reverts entity hierarchy changes
    in the ECS world.
  \details
    Implements the IEditorCommand interface to provide undo and redo
    support for hierarchy modifications such as re-parenting, detaching,
    or reordering entities. Each command instance holds a list of
    HierarchySnapshots that record the before and after HierarchyComponent
    state for every entity affected by the operation.

    On Execute, the "after" states are applied. On Undo, the "before"
    states are restored. Entity handles stored in snapshots can be updated
    via UpdateEntityParentReference if entities are remapped during editor
    operations such as scene reloads or prefab instantiation.

    This command is not constructed directly — use PushModifyHierarchyCommand
    to create, filter, and push it onto the undo stack in one step.
  \see
    IEditorCommand, HierarchySnapshot, PushModifyHierarchyCommand,
    EditorUndoStack
*************************************************************************/
class ModifyHierarchyCommand : public IEditorCommand
{
public:
    /*************************************************************************
      \brief
        Constructs a ModifyHierarchyCommand with a list of hierarchy snapshots.
      \param[in] snapshots
        A vector of HierarchySnapshot entries, each recording the before and
        after HierarchyComponent state for an affected entity.
    *************************************************************************/
    ModifyHierarchyCommand(std::vector<HierarchySnapshot> snapshots)
        : snapshots(std::move(snapshots))
    {
    }

    /*************************************************************************
      \brief
        Applies the hierarchy change by restoring the "after" state for all
        affected entities.
      \param[in] world
        Reference to the ECS world containing the entities to modify.
      \details
        Iterates through all stored snapshots and writes the "after"
        HierarchyComponent back onto each entity. Skips entities that are
        no longer alive in the world.
    *************************************************************************/
    void Execute(World& world) override
    {
        for (auto& s : snapshots)
        {
            if (!world.IsEntityAlive(s.entity))
            {
                continue;
            }
            auto* hierarchy = world.GetComponent<HierarchyComponent>(s.entity);
            if (hierarchy)
            {
                *hierarchy = s.after;
            }
        }
    }

    /*************************************************************************
      \brief
        Reverts the hierarchy change by restoring the "before" state for all
        affected entities.
      \param[in] world
        Reference to the ECS world containing the entities to modify.
      \details
        Iterates through all stored snapshots and writes the "before"
        HierarchyComponent back onto each entity. Skips entities that are
        no longer alive in the world.
    *************************************************************************/
    void Undo(World& world) override
    {
        for (auto& s : snapshots)
        {
            if (!world.IsEntityAlive(s.entity))
            {
                continue;
            }
            auto* hierarchy = world.GetComponent<HierarchyComponent>(s.entity);
            if (hierarchy)
            {
                *hierarchy = s.before;
            }
        }
    }

    /*************************************************************************
      \brief
        Updates any snapshot whose entity matches the original entity handle
        to use the new remapped entity handle instead.
      \param[in] original
        The entity handle that has been remapped.
      \param[in] newEntity
        The new entity handle to replace the original with.
      \details
        Called when entities are remapped during editor operations (such as
        scene reload or prefab instantiation) to keep stored snapshots valid.
    *************************************************************************/
    void UpdateEntityParentReference(Entity original, Entity newEntity) override
    {
        for (auto& s : snapshots)
        {
            if (s.entity == original)
            {
                s.entity = newEntity;
            }
        }
    }
    
private:
    std::vector<HierarchySnapshot> snapshots;
};

/*************************************************************************
  \brief
    Constructs and pushes a ModifyHierarchyCommand onto the undo stack,
    filtering out any snapshots where no change occurred.
  \param[in] snapshots
    A vector of HierarchySnapshot entries representing all entities
    affected by the hierarchy operation.
  \details
    Before pushing the command, snapshots where the before and after
    HierarchyComponent states are identical are removed using a memcmp
    comparison. If no snapshots remain after filtering, no command is
    pushed. Otherwise, the command is marked as already executed and
    pushed onto the global undo stack.
*************************************************************************/
inline void PushModifyHierarchyCommand(std::vector<HierarchySnapshot> snapshots)
{
    // remove snapshots where nothing changed
    snapshots.erase(
        std::remove_if(snapshots.begin(), snapshots.end(), [](const HierarchySnapshot& s)
        {
            return memcmp(&s.before, &s.after, sizeof(HierarchyComponent)) == 0;
        }),
        snapshots.end()
    );

    // no changes, return
    if (snapshots.empty()) { return; }

    auto cmd = std::make_unique<ModifyHierarchyCommand>(std::move(snapshots));
    cmd->SetExecuted(true);
    GetUndoStack()->Push(std::move(cmd));
    std::cout << "Pushing modify hierarchy command\n";
}