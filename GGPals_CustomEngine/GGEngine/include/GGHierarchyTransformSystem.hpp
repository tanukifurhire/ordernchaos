/*************************************************************************
  \file       GGHierarchyTransformSystem.hpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
    ECS hierarchy transform propagation system interface.

  \details
    Defines the GGHierarchyTransformSystem class, which manages
    parent�child entity relationships and hierarchical transform
    propagation within the ECS framework. Handles:
      - Linking/unlinking of parent�child entities.
      - Recursive hierarchy depth assignment.
      - Dirty flagging and transform recomputation.
      - Matrix composition from LocalTransform to WorldTransform.
      - Depth-sorted single-pass transform updates.

  \dependencies
    GGECSWorld.hpp, GGSystemInterface.hpp, GGECSInterface.hpp,
    GGECSComponents.hpp, GGMath/Matrix3x3.hpp

  \see
    GGHierarchyTransformSystem.cpp, GGECSComponents.hpp

  \history

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGECSWorld.hpp>
#include <GGSystemInterface.hpp>
#include <GGECSInterface.hpp>
#include <GGECSComponents.hpp>
#include <GGMath/Matrix3x3.hpp>
#include <vector>

class GGHierarchyTransformSystem : public SystemInterface
{
public:
    /*************************************************************************
      \brief
        Initializes the hierarchy transform system.
      \return
        Always returns true.
      \details
        Called once during system startup. Performs no initialization logic
        for now but exists to match the standard system interface.
    *************************************************************************/
    bool Init() override;
 
    /*************************************************************************
      \brief
        Updates hierarchical transforms for all entities in the world.
      \param[in,out] world
        Reference to the ECS world containing entities and components.
      \details
        Executes a single-pass update (Every entity (or object) is processed 
        once, in one continuous pass through the data) over all entities with
        LocalTransform and WorldTransform components.  Collects
        entities whose transforms are dirty or invalid, marks their
        descendants dirty, sorts them by hierarchy depth, and
        propagates parent-to-child world transforms in depth order.
    *************************************************************************/
    void Update(World& world) override;

    // --- Hierarchy API (functions to manipulate or query those parent�child 
    // relationship) ---
    /*************************************************************************
      \brief
        Sets an entity's parent in the hierarchy.
      \param[in,out] world
        Reference to the ECS world.
      \param[in] child
        Child entity to be attached.
      \param[in] parent
        Parent entity under which to attach the child.
      \details
        Performs safety checks against null or self-parenting,
        prevents cyclic relationships, detaches the child from any
        previous parent, and links it to the new parent.  Also updates
        hierarchy depths and marks the child transform dirty.
    *************************************************************************/
    void SetParent(World& world, Entity child, Entity parent);

    /*************************************************************************
      \brief
        Removes an entity from its parent, making it a root.
      \param[in,out] world
        Reference to the ECS world.
      \param[in] child
        Child entity to unparent.
      \details
        Unlinks the specified child from its parent's child list,
        clears its parent reference, resets hierarchy depth to zero,
        and marks its LocalTransform dirty for recomputation.
    *************************************************************************/
    void RemoveParent(World& world, Entity child);

    /*************************************************************************
      \brief
        Retrieves the parent of a given entity.
      \param[in] world
        Reference to the ECS world.
      \param[in] child
        Entity whose parent is requested.
      \return
        Parent entity handle, or NULL_ENTITY if none.
      \details
        Reads the HierarchyComponent of the specified entity and
        returns its stored parent value.
    *************************************************************************/
    Entity GetParent(World& world, Entity child) const;

    /*************************************************************************
      \brief
        Returns the direct children of a parent entity.
      \param[in] world
        Reference to the ECS world.
      \param[in] parent
        Parent entity whose immediate children will be listed.
      \return
        Vector of child entity handles (may be empty).
      \details
        Traverses the linked-list of firstChild / nextSibling stored in
        the parent�s HierarchyComponent and collects all direct children.
    *************************************************************************/
    std::vector<Entity> GetChildren(World& world, Entity parent) const;

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
    Entity CreateNewEntity(World& world, Entity parent);

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

private:
    // --- Helpers ---
    /*************************************************************************
    \brief
        Builds a local TRS matrix for an entity.

    \details
        Uses GGMath::Matrix3x3::TRS(local.position, local.rotation, local.scale).

    \param[in] local
        LocalTransform component for the entity.

    \return
        3x3 transform matrix in column-major layout.
    *************************************************************************/
    GGMath::Matrix3x3 ComputeLocalMatrix(const LocalTransform& local) const;

    /*************************************************************************
      \brief
        Sorts a list of entities by hierarchy depth.
      \param[in,out] entities
        Vector of entity handles to be sorted.
      \param[in] world
        Reference to the ECS world.
      \details
        Orders entities in ascending hierarchyDepth so that
        parent transforms are updated before their children.
    *************************************************************************/
    void SortEntitiesByDepth(std::vector<Entity>& entities, World& world);

    /*************************************************************************
      \brief
        Links a child entity under a specified parent.
      \param[in,out] world
        Reference to the ECS world.
      \param[in] child
        Entity to link.
      \param[in] parent
        Parent entity to attach under.
      \details
        Updates both parent and child HierarchyComponents:
        sets the child's parent field, assigns its nextSibling
        pointer to the parent�s current firstChild, and
        updates the parent�s firstChild to the new child.
    *************************************************************************/
    void LinkChildToParent(World& world, Entity child, Entity parent);

    /*************************************************************************
      \brief
        Unlinks a child from its parent�s child list.
      \param[in,out] world
        Reference to the ECS world.
      \param[in] child
        Entity to detach from its parent.
      \details
        Walks the parent�s singly linked child list and removes
        the specified child, reconnecting remaining siblings.
    *************************************************************************/
    void UnlinkChildFromParent(World& world, Entity child);

    /*************************************************************************
      \brief
        Updates hierarchy depth for an entity and its descendants.
      \param[in,out] world
        Reference to the ECS world.
      \param[in] root
        Root entity whose subtree will be updated.
      \param[in] startDepth
        Depth value to assign to the root entity.
      \details
        Recursively assigns hierarchyDepth fields starting at
        \p startDepth and increments by one for each level.
        Marks all processed entities� LocalTransforms dirty.
    *************************************************************************/
    void UpdateHierarchyDepths(World& world, Entity root, uint8_t startDepth);

    /*************************************************************************
      \brief
        Marks all descendants of a root entity as dirty.
      \param[in,out] world
        Reference to the ECS world.
      \param[in] root
        Root entity whose descendants will be marked.
      \param[in,out] dirty
        Vector collecting entities that must be updated.
      \details
        Performs a breadth-first traversal (BFS) (it processes all nodes
        at the current depth before moving to the next level) through all
        descendants of \p root, marking their LocalTransforms
        dirty and appending them to the \p dirty list for later
        transform recomputation.
    *************************************************************************/
    void MarkDescendantsDirty(World& world, Entity root, std::vector<Entity>& dirty);

    Callback<World&, Entity> onEntityDestroyedCallback;
};
