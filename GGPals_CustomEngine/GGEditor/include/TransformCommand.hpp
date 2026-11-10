/*************************************************************************
  \file       TransformCommand.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       25 November 2025
  \version    1.0
  \brief
    Declares the TransformCommand class for undo/redo of entity transforms.
  \details
    Implements the IEditorCommand interface for storing and applying
    changes to an entity's transform component (position, scale, rotation).
    Supports Execute and Undo operations for integration with the editor's
    undo/redo system.
*************************************************************************/
#pragma once
// Editor command interface
#include <IEditorCommand.hpp>

// ECS includes
#include <GGECSInterface.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>

// GGMath includes
#include <GGMath/Vector2.hpp>
#include <GGMath/Types.hpp>

/*************************************************************************
  \struct TransformSnapshot
  \brief 
    Stores a snapshot of an entity's transform state.
  \details 
    Holds position, scale, and rotation for undo/redo operations.
*************************************************************************/
struct TransformSnapshot {
    GGMath::Vector2 position{ 0.f, 0.f };
    GGMath::Vector2 scale{ 1.f, 1.f };
    GGMath::float32 rotation{ 0.f };
};

/*************************************************************************
  \brief 
    Equality comparison operator for TransformSnapshot
  \details 
    Returns true if any of position, scale, or rotation differ.
*************************************************************************/
inline bool operator!=(TransformSnapshot const& lhs, TransformSnapshot const& rhs)
{
    constexpr float EPSILON = 0.0001f;

    bool posChanged = std::abs(lhs.position.x - rhs.position.x) > EPSILON ||
        std::abs(lhs.position.y - rhs.position.y) > EPSILON;

    bool scaleChanged = std::abs(lhs.scale.x - rhs.scale.x) > EPSILON ||
        std::abs(lhs.scale.y - rhs.scale.y) > EPSILON;

    bool rotChanged = std::abs(lhs.rotation - rhs.rotation) > EPSILON;

    return posChanged || scaleChanged || rotChanged;
}

/*************************************************************************
  \class TransformCommand
  \brief
    Command for applying and undoing transform changes to an entity.
  \details
    Implements IEditorCommand to integrate with the editor's
    undo/redo system. Stores a before and after snapshot of the
    entity's transform and applies it in Execute or Undo.
*************************************************************************/
class TransformCommand : public IEditorCommand {
private:
    Entity targetEntity;
    TransformSnapshot beforeState;
    TransformSnapshot afterState;

public:
    /*************************************************************************
      \brief 
        Constructor for TransformCommand
      \param 
        entity The entity to modify
      \param 
        before Snapshot of the entity's transform before changes
      \param 
        after Snapshot of the entity's transform after changes
    *************************************************************************/
    TransformCommand(Entity entity, const TransformSnapshot& before, const TransformSnapshot& after)
        : targetEntity(entity), beforeState(before), afterState(after) {
    }

    /*************************************************************************
      \brief 
        Executes the transform change
      \param
        world The ECS world containing the entity
      \details 
        Sets the entity's transform to the afterState values.
    *************************************************************************/
    void Execute(World& world) override {
        if (auto* transform = world.GetComponent<LocalTransform>(targetEntity)) {
            transform->position = afterState.position;
            transform->scale = afterState.scale;
            transform->rotation = afterState.rotation;
            transform->isDirty = true;
        }
    }

    /*************************************************************************
      \brief 
        Undoes the transform change
      \param 
        world The ECS world containing the entity
      \details 
        Restores the entity's transform to the beforeState values.
    *************************************************************************/
    void Undo(World& world) override {
        if (auto* transform = world.GetComponent<LocalTransform>(targetEntity)) {
            transform->position = beforeState.position;
            transform->scale = beforeState.scale;
            transform->rotation = beforeState.rotation;
            transform->isDirty = true;
        }
    }

    /*************************************************************************
      \brief 
        Updates entity references if the parent entity changes
      \param 
        old The old parent entity
      \param 
        newEntity The new parent entity
    *************************************************************************/
    void UpdateEntityParentReference([[maybe_unused]] Entity old, [[maybe_unused]] Entity newEntity) override
    {
        if (targetEntity == old)
            targetEntity = newEntity;
    }
};