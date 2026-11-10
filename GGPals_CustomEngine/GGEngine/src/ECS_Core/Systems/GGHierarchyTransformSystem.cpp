/*************************************************************************
  \file       GGHierarchyTransformSystem.cpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
    Implements the GGHierarchyTransformSystem class for hierarchical
    transform propagation in the ECS architecture.

  \details
    Provides functionality for maintaining parent–child spatial relationships
    and propagating local transform data through the hierarchy to compute
    final world-space matrices. Supports:
      - Depth-sorted single-pass transform propagation.
      - Parent–child linking and unlinking with cycle prevention.
      - Dynamic hierarchy depth updates on reparenting.
      - Marking dirty transforms for efficient partial updates.
      - Recursive child transform updates through breadth-first traversal.
      - Integration with GGEngine’s ECS world and component systems.

  \dependencies
    GGHierarchyTransformSystem.hpp, GGLogger.hpp, GGMath/Matrix3x3.hpp

  \see
    GGHierarchyTransformSystem.hpp, GGECSComponents.hpp, GGMath::Matrix3x3

  \history

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGHierarchyTransformSystem.hpp>
#include <GGSystemManagers.hpp>
#include <GGLogger.hpp>
#include <algorithm>

/*************************************************************************
  \brief
    Initializes the hierarchy transform system.
  \return
    Always returns true.
  \details
    Called once during system startup. Performs no initialization logic
    for now but exists to match the standard system interface.
*************************************************************************/
bool GGHierarchyTransformSystem::Init() {
    GlobalEventBus::onPlayModeStart += onStartSimulate;
    GlobalEventBus::onStopPlayMode += onStopSimulate;

    onEntityDestroyedCallback = [&](World& world, Entity entity)
        {
		    GGLogger::Info("Entity destroyed: " + std::to_string(entity.id) + " gen: " + std::to_string(entity.generation));
            if (!world.HasComponent<HierarchyComponent>(entity))
            {
    		    GGLogger::Info("No hierarchy component!");
                world.DestroyEntityHandle(entity);
                return;
            }

            DeleteEntity(world, entity);
        };

    onProcessEntityDestroyed += onEntityDestroyedCallback;

    return true;
}

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
void GGHierarchyTransformSystem::Update(World& world) {
    #ifdef GG_EDITOR_BUILD
    static int frameCounter = 0;
    frameCounter++;
    #endif
    
    // Store using temporary array of entities that needing recomputation
    std::vector<Entity> dirtyEntities;

    // Lambda function: collects entities whose Local or World transforms are dirty/invalid
    std::function<void(Entity, LocalTransform&, WorldTransform&)> collectDirty =
        [&](Entity entity, LocalTransform& local, WorldTransform& worldTransform) {
            // If this entity's local transform is dirty or its world transform is invalid queue it for update
            if (local.isDirty || !worldTransform.isValid) {
                dirtyEntities.push_back(entity);
            }
        };
    // Iterate
    world.ForEach<LocalTransform, WorldTransform>(collectDirty);

    {
        // For each initially dirty entity, also mark all descendants dirty (seeds is the start)
        // This ensures children get updated if their parent has changed
        const auto seeds = dirtyEntities; // ACTUAL copy of initial dirty entities (not reference!)
        for (Entity root : seeds) {
            GGHierarchyTransformSystem::MarkDescendantsDirty(world, root, dirtyEntities);
        }
    }

    //  Sort entities by hierarchy depth parents first, then children
    GGHierarchyTransformSystem::SortEntitiesByDepth(dirtyEntities, world);

    // Single-pass propagation in sorted order (updating all entity world transforms once from parents to children after sorting by hierarchy depth)
    for (Entity entity : dirtyEntities) {
        auto& localTransfrom = *world.GetComponent<LocalTransform>(entity);   // Local transform (position/rotation/scale)
        auto& worldTransform = *world.GetComponent<WorldTransform>(entity);  // Output world matrix + validity flag

        // Compute the local TRS(position/rotation/scale) matrix for this entity
        GGMath::Matrix3x3 localMatrix = GGHierarchyTransformSystem::ComputeLocalMatrix(localTransfrom);

        // Check if this entity participates in hierarchy (has parent)
        if (world.HasComponent<HierarchyComponent>(entity)) {
            // Get hierarchy (Parent, firstChild and nextSibling)
            const auto& hierarchyComponent = *world.GetComponent<HierarchyComponent>(entity);

            // If the entity has a valid parent, compose its world matrix
            if (hierarchyComponent.parent != NULL_ENTITY && world.HasComponent<WorldTransform>(hierarchyComponent.parent)) {
                // Retrieve parent world transform and compose:
                // child world = parentWorld * child local
                const auto& parentWorld = *world.GetComponent<WorldTransform>(hierarchyComponent.parent);
                worldTransform.matrix = parentWorld.matrix * localMatrix;
            }
            // No valid parent = treat as root (parent itself)
            else worldTransform.matrix = localMatrix;
        }
        // Entity not part of hierarchy its world transform = local transform (parent itself)
        else worldTransform.matrix = localMatrix;

        // Mark transform as updated (clean)
        localTransfrom.isDirty = false;
        worldTransform.isValid = true;
        
        //#ifdef GG_EDITOR_BUILD
        //// Debug logging for editor builds only - log when UI entities get revalidated
        //if (world.HasComponent<NameComponent>(entity)) {
        //    auto* nameComp = world.GetComponent<NameComponent>(entity);
        //}
        //#endif
    }
}

Entity GGHierarchyTransformSystem::CreateNewEntity(World& world, Entity parent)
{
    // Create Local Transform for child
    Entity newEntity = world.CreateEntityHandle();

    LocalTransform localTransform = {};

    // Initialise
    localTransform.position = { 0.0f, 0.0f };
    localTransform.scale = { 1.0f,1.0f };
    localTransform.rotation = 0.0f;
    localTransform.isDirty = true;

    // Add child to scene
    world.AddComponent<LocalTransform>(newEntity, localTransform);
    world.AddComponent<WorldTransform>(newEntity, {});

    NameComponent nameComponent;
    std::strcpy(nameComponent.name, "New Entity");
    world.AddComponent<NameComponent>(newEntity, nameComponent);

    // If there is a parent, set parent
    if (parent != NULL_ENTITY) {
        auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();
        hierarchySystem.SetParent(world, newEntity, parent);	// Set child to parent
    }

    return newEntity;
}

void GGHierarchyTransformSystem::DeleteEntity(World& world, Entity entity)
{
    auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

	GGLogger::Warning("Deleting entity: " + std::to_string(entity.id));

    // Remove all children
    std::vector<Entity> children = hierarchySystem.GetChildren(world, entity);
    for (Entity child : children) {
        GGLogger::Info("Recursively deleting child entity: " + std::to_string(child.id));
        DeleteEntity(world, child);	// recursively deletes descendants first
    }

    // Remove from parent and destroy self
    hierarchySystem.RemoveParent(world, entity);

    // Destroy the entity itself
    world.DestroyEntityHandle(entity);
}


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
void GGHierarchyTransformSystem::SetParent(World& world, Entity child, Entity parent) {
    // Safety checks: prevent null or self-parenting
    if (child == parent || child == NULL_ENTITY || parent == NULL_ENTITY) {
        return;
    }

    // Prevent circular parenting an entity ends up being its own ancestor
    Entity current = parent;
    while (current != NULL_ENTITY) {
        if (current == child) {
            GGLogger::Warning("Attempted to create hierarchy cycle.");
            return;
        }
        if (world.HasComponent<HierarchyComponent>(current)) {
            current = world.GetComponent<HierarchyComponent>(current)->parent;
        }
        else {
            break;
        }
    }

    // Remove child from any previous parent before reattaching
    RemoveParent(world, child);

    // Ensure both parent and child have hierarchy components
    if (!world.HasComponent<HierarchyComponent>(child)) {
        world.AddComponent<HierarchyComponent>(child, {});
    }
    if (!world.HasComponent<HierarchyComponent>(parent)) {
        world.AddComponent<HierarchyComponent>(parent, {});
    }

    // Link child to parent's child list
    GGHierarchyTransformSystem::LinkChildToParent(world, child, parent);

    // Update hierarchy depth for the child and all descendants
    auto& parentLocal = *world.GetComponent<LocalTransform>(parent);
    GGHierarchyTransformSystem::UpdateHierarchyDepths(world, child, parentLocal.hierarchyDepth + 1);

    // Mark child as dirty so its world transform will be recomputed
    world.GetComponent<LocalTransform>(child)->isDirty = true;
}

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
void GGHierarchyTransformSystem::RemoveParent(World& world, Entity child) {
    // Skip if no hierarchy info
    if (!world.HasComponent<HierarchyComponent>(child)) {
        return;
    }

    // Already root, no action
    auto& childHierarchy = *world.GetComponent<HierarchyComponent>(child);
    if (childHierarchy.parent == NULL_ENTITY) {
        return;
    }

    // Detach child from its parents linked child list
    GGHierarchyTransformSystem::UnlinkChildFromParent(world, child);

    // Clear parent reference
    childHierarchy.parent = NULL_ENTITY;

    // Reset hierarchy depth and mark dirty to recompute
    if (world.HasComponent<LocalTransform>(child)) {
        auto& localTransform = *world.GetComponent<LocalTransform>(child);
        localTransform.hierarchyDepth = 0;
        localTransform.isDirty = true;
    }
}

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
Entity GGHierarchyTransformSystem::GetParent(World& world, Entity child) const {
    // If no hierarchy = no parent
    if (!world.HasComponent<HierarchyComponent>(child)) {
        return NULL_ENTITY;
    }

    // Return stored parent
    return world.GetComponent<HierarchyComponent>(child)->parent;
}


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
    the parent’s HierarchyComponent and collects all direct children.
*************************************************************************/
std::vector<Entity> GGHierarchyTransformSystem::GetChildren(World& world, Entity parent) const {
    std::vector<Entity> result;

    // Skip if parent has no hierarchy component
    if (!world.HasComponent<HierarchyComponent>(parent)) {
        return result;
    }

    // Get first child
    Entity currentChild = world.GetComponent<HierarchyComponent>(parent)->firstChild;

    // Loop until no more children
    while (currentChild != NULL_ENTITY) {
        if (!world.HasComponent<HierarchyComponent>(currentChild)) {
            break;
        }
        // Add this child to list
        result.push_back(currentChild);
        // Move to next
        currentChild = world.GetComponent<HierarchyComponent>(currentChild)->nextSibling;
    }

    // Return collected children
    return result;
}

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
    pointer to the parent’s current firstChild, and
    updates the parent’s firstChild to the new child.
*************************************************************************/
void GGHierarchyTransformSystem::LinkChildToParent(World& world, Entity child, Entity parent) {
    auto parentHierarchy = world.GetComponent<HierarchyComponent>(parent);
    auto childHierarchy = world.GetComponent<HierarchyComponent>(child);

    // Record parent reference in child
    childHierarchy->parent = parent;

    // Point child’s next sibling to parent’s first child
    childHierarchy->nextSibling = parentHierarchy->firstChild;

    // Insert child at beginning of child list
    parentHierarchy->firstChild = child;
}

/*************************************************************************
  \brief
    Unlinks a child from its parent’s child list.
  \param[in,out] world
    Reference to the ECS world.
  \param[in] child
    Entity to detach from its parent.
  \details
    Walks the parent’s singly linked child list and removes
    the specified child, reconnecting remaining siblings.
*************************************************************************/
void GGHierarchyTransformSystem::UnlinkChildFromParent(World& world, Entity child) {
    Entity parent = world.GetComponent<HierarchyComponent>(child)->parent;
    if (parent == NULL_ENTITY || !world.HasComponent<HierarchyComponent>(parent)) {
        return;
    }

    auto& parentComponent = *world.GetComponent<HierarchyComponent>(parent);
    // Pointer to first child link
    Entity* iterator = &parentComponent.firstChild;

    // Loop through all siblings
    while (*iterator != NULL_ENTITY) {
        // If found the child to remove
        if (*iterator == child) {
            // Skip it and stop the search
            *iterator = world.GetComponent<HierarchyComponent>(*iterator)->nextSibling;
            break;
        }

        // Move to next sibling
        iterator = &world.GetComponent<HierarchyComponent>(*iterator)->nextSibling;
    }
}

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
    Marks all processed entities’ LocalTransforms dirty.
*************************************************************************/
void GGHierarchyTransformSystem::UpdateHierarchyDepths(World& world, Entity root, uint8_t startDepth) {
    // Limit max depth to 16 (Can be changed)
    if (startDepth > 16)
    {
        GGLogger::Warning("Hierarchy depth exceeded 16 levels for entity " + std::to_string(root.id) + ". Depth clamped to 16.");
        startDepth = 16;
    }

    if (!world.HasComponent<LocalTransform>(root)) {
        return;
    }

    auto& localTransform = *world.GetComponent<LocalTransform>(root);
    localTransform.hierarchyDepth = startDepth;
    localTransform.isDirty = true;

    // Stop if no children
    if (!world.HasComponent<HierarchyComponent>(root)) {
        return;
    }

    // Get first child
    Entity child = world.GetComponent<HierarchyComponent>(root)->firstChild;

    // Loop through all children
    while (child != NULL_ENTITY) {
        // Recursive call with +1 depth for next child
        GGHierarchyTransformSystem::UpdateHierarchyDepths(world, child, startDepth + 1);

        // Move to next child
        child = world.GetComponent<HierarchyComponent>(child)->nextSibling;
    }
}

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
void GGHierarchyTransformSystem::SortEntitiesByDepth(std::vector<Entity>& entities, World& world) {
    // Sort the vector in place
    std::sort(entities.begin(), entities.end(), 
        // Lambda comparison function [&] to catch world
        [&](Entity a, Entity b) {
            uint8_t depthA = 0, depthB = 0;
            if (world.HasComponent<LocalTransform>(a)) {
                depthA = world.GetComponent<LocalTransform>(a)->hierarchyDepth;
            }
            if (world.HasComponent<LocalTransform>(b)) {
                depthB = world.GetComponent<LocalTransform>(b)->hierarchyDepth;
            }

            // Sort ascending (Returns true if a depth is smaller than b meaning a comes before b)               
            return depthA < depthB;
        });
}

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
GGMath::Matrix3x3 GGHierarchyTransformSystem::ComputeLocalMatrix(const LocalTransform& local) const {
    return GGMath::Matrix3x3::TRS(local.position * Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().WORLD_TO_SCREEN_SCALE, local.rotation, local.scale);
}


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
void GGHierarchyTransformSystem::MarkDescendantsDirty(World& world, Entity root, std::vector<Entity>& dirty) {
    if (!world.HasComponent<HierarchyComponent>(root)) {
        return;
    }

    // Create queue for breadth-first traversal
    // (it processes all nodes at the current depth before moving to the next level)
    std::queue<Entity> queue;

    // Get first child
    Entity child = world.GetComponent<HierarchyComponent>(root)->firstChild;

    // If there is a child
    while (child != NULL_ENTITY) {
        // add child to queue
        queue.push(child);

        // nextSibling
        if (world.HasComponent<HierarchyComponent>(child)) {
            child = world.GetComponent<HierarchyComponent>(child)->nextSibling;
        }
        else {
            break;
        }
    }

    // While there are entities to process
    while (!queue.empty()) {
        // Get entity at front of queue
        Entity entity = queue.front(); 
        
        // Remove it from queue
        queue.pop();

        // If it has a LocalTransform
        if (world.HasComponent<LocalTransform>(entity)) {
            auto& localTransform = *world.GetComponent<LocalTransform>(entity);

            // If it is not already dirty
            if (!localTransform.isDirty) {

                // Mark as dirty and add back to the list
                localTransform.isDirty = true;
                dirty.push_back(entity);
            }
        }

        // If it has children push all of its direct children into the queue
        if (world.HasComponent<HierarchyComponent>(entity)) {
            // Get first child
            Entity children = world.GetComponent<HierarchyComponent>(entity)->firstChild;
            while (children != NULL_ENTITY) {
                queue.push(children);
                children = world.GetComponent<HierarchyComponent>(children)->nextSibling;
            }
        }
    }
}
