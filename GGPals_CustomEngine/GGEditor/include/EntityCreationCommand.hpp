/*************************************************************************
  \file       EditorEntityCommands.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief      Defines editor command classes for entity creation and deletion.

  \details
    This file implements the command classes used by the GGEngine editor to
    perform undoable and redoable operations on ECS entities. It includes:
      - EntitySnapshot, which stores all data needed to restore an entity.
      - CreateEntityCommand, which creates an entity and supports undoing
        the creation by deleting it.
      - DeleteEntityCommand, which deletes an entity and supports undoing
        the deletion by restoring it from serialized data.

    Each command implements the IEditorCommand interface and works together
    with EditorUndoStack to provide reversible editing operations. Commands
    handle entity serialization, deserialization, hierarchy updates, and
    entity-ID remapping after restoration.
  \dependencies
    - IEditorCommand.hpp (editor command interface)
    - GGECSInterface.hpp, GGECSWorld.hpp, GGECSComponents.hpp
    - GGSystemManagers.hpp (hierarchy system access)
    - GGEngine.hpp (prefab manager, engine instance access)
    - GGMath/Vector2.hpp, GGMath/Types.hpp
    - rapidjson (entity serialization)
    - Standard library: <deque>, <memory>
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

// Editor command interface
#include <IEditorCommand.hpp>

// ECS includes
#include <GGECSInterface.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>
#include <GGSystemManagers.hpp>

// GGMath includes
#include <GGMath/Vector2.hpp>
#include <GGMath/Types.hpp>

// Core engine includes
#include <GGEngine.hpp>

/*************************************************************************
  \struct EntitySnapshot
  \brief 
    Snapshot of an entity for undo/redo.
  \details
    Stores the entity ID, parent entity, and serialized data for restoring
    the entity state.
*************************************************************************/
struct EntitySnapshot {
    EntitySnapshot() = default;

    EntitySnapshot(EntitySnapshot const& snapshot) :
        parentEntity(snapshot.parentEntity),
        entity(snapshot.entity)
    { 
        serializedData.CopyFrom(snapshot.serializedData, serializedData.GetAllocator());
    }

    Entity parentEntity = NULL_ENTITY;
    Entity entity = NULL_ENTITY;
    rapidjson::Document serializedData;
};

/*************************************************************************
  \class CreateEntityCommand
  \brief 
    Editor command to create an entity.
  \details
    Implements IEditorCommand interface. Supports Execute() to create the
    entity and Undo() to delete it. Maintains an EntitySnapshot for undo.
*************************************************************************/
class CreateEntityCommand : public IEditorCommand
{
private:
    EntitySnapshot snapshot;

public:
    CreateEntityCommand(const EntitySnapshot& snap) : snapshot(snap) {}

    /*************************************************************************
      \brief 
        Executes the command to create the entity in the world.
      \param 
        world Reference to the ECS world.
      \details
        - Deserializes entity from JSON.
        - Updates entity references in the undo stack.
        - Sets parent entity in hierarchy.
    *************************************************************************/
    void Execute(World& world) override
    {
        auto& prefabManager = GetEngineInstance().GetPrefabManager();
        auto& hierarchy = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

        Entity newRoot = prefabManager.DeserializeEntityFromJSON(snapshot.serializedData, world, snapshot.entity);
        
        GetUndoStack()->UpdateEntityReference(snapshot.entity, newRoot);

        snapshot.entity = newRoot;
        std::cout << "[CreateEntityCommand] newRoot.id=" << newRoot.id << " gen=" << (int)newRoot.generation << "\n";
        std::cout << "[CreateEntityCommand] snapshot.entity.id=" << snapshot.entity.id << " gen=" << (int)snapshot.entity.generation << "\n";
        hierarchy.SetParent(world, snapshot.entity, snapshot.parentEntity);
    }

    /*************************************************************************
      \brief 
        Undoes the creation of the entity.
      \param 
        world Reference to the ECS world.
      \details
        - Saves entity backup.
        - Deletes entity from the world.
    *************************************************************************/
    void Undo(World& world) override
    {
        auto& prefabManager = GetEngineInstance().GetPrefabManager();
        auto& hierarchy = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

        snapshot.serializedData = prefabManager.SaveEntityBackup(snapshot.entity, world);

        hierarchy.DeleteEntity(world, snapshot.entity);
    }

    /*************************************************************************
      \brief 
        Updates parent entity references when entity IDs change.
      \param 
        original Original entity ID.
      \param 
        newEntity New entity ID.
    *************************************************************************/
    void UpdateEntityParentReference(Entity original, Entity newEntity) override
    {
        if (snapshot.parentEntity == original) snapshot.parentEntity = newEntity;
        if (snapshot.entity == original)       snapshot.entity = newEntity;
    }
};

/*************************************************************************
  \class DeleteEntityCommand
  \brief    
    Editor command to delete an entity.
  \details
    Implements IEditorCommand interface. Supports Execute() to delete the
    entity and Undo() to restore it. Maintains an EntitySnapshot for undo.
*************************************************************************/
class DeleteEntityCommand : public IEditorCommand
{
private:
    EntitySnapshot snapshot;

public:
    DeleteEntityCommand(const EntitySnapshot& snap) : snapshot(snap) {}

    /*************************************************************************
      \brief 
        Executes the command to delete the entity in the world.
      \param 
        world Reference to the ECS world.
      \details
        - Saves entity backup before deletion.
        - Deletes entity from the hierarchy.
    *************************************************************************/
    void Execute(World& world) override
    {
        auto& prefabManager = GetEngineInstance().GetPrefabManager();
        auto& hierarchy = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

        snapshot.serializedData = prefabManager.SaveEntityBackup(snapshot.entity, world);

        hierarchy.DeleteEntity(world, snapshot.entity);
    }

    /*************************************************************************
      \brief 
        Undoes the deletion by restoring the entity.
      \param 
        world Reference to the ECS world.
      \details
        - Deserializes entity from JSON.
        - Updates entity references in undo stack.
        - Restores parent entity relationship.
    *************************************************************************/
    void Undo(World& world) override
    {
        auto& prefabManager = GetEngineInstance().GetPrefabManager();
        auto& hierarchy = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

        Entity newRoot = prefabManager.DeserializeEntityFromJSON(snapshot.serializedData, world, snapshot.entity);
    
        GetUndoStack()->UpdateEntityReference(snapshot.entity, newRoot);

        snapshot.entity = newRoot;

        hierarchy.SetParent(world, snapshot.entity, snapshot.parentEntity);
    }

    /*************************************************************************
      \brief 
        Updates parent entity references when entity IDs change.
      \param 
        original Original entity ID.
      \param 
        newEntity New entity ID.
    *************************************************************************/
    void UpdateEntityParentReference(Entity original, Entity newEntity) override
    {
        if (snapshot.parentEntity == original) snapshot.parentEntity = newEntity;
        if (snapshot.entity == original)       snapshot.entity = newEntity;
    }
};