/*************************************************************************
  \file       GGECSWorld.hpp
  \author     Tai Xiao Xuan, Zafir Haqqym
  \par        Email: xiaoxuan.tai@digipen.edu, zafir.h@digipen.edu
  \date       25 October 2025
  \version    1.1
  \brief
    Defines the World container for entity lifecycle and component management.
  \details
    The World class is the central coordinator of the ECS architecture,
    managing entity creation/destruction with generational indices, type-safe
    component attachment/detachment, and efficient query primitives for system
    iteration. Implements a free-list allocator for entity slots with O(1)
    creation/recycling, and lazy component array instantiation to minimize
    memory footprint for unused component types. The design prioritizes
    cache-friendly iteration patterns via contiguous component storage while
    maintaining O(1) random access via entity handles.
    Core Responsibilities:
    - Entity lifecycle: creation, destruction, generation management
    - Component attachment: type-safe add/remove/query operations
    - System iteration: ForEach/ForEachNested with compile-time signature validation
    - Component storage: lazy allocation of ComponentArray<T> instances
  \dependencies
    GGECSInterface.hpp, GGEvent.hpp, <queue>, <unordered_set>, <memory>
  \see
    Entity, ComponentArray, IsComponent, SystemInterface
  \history
    - 1.0 (3 October 2025): Initial implementation with free-list allocator.
    - 1.1 (25 October 2025): Added Lua entity handle validation system with
                              generation tracking for safe script entity access.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGECSInterface.hpp>
#include <GGEvent.hpp>
#include <queue>
#include <unordered_set>
#include <memory>

class World;


inline GGEvent<World&, Entity> onProcessEntityDestroyed;

inline GGEvent<World&, Entity> onEntityDestroyed;

/*************************************************************************
  \class World
  \brief
    ECS world managing entity lifecycle and component storage with generational indices.
  \details
    Coordinates all ECS operations through a centralized interface. Uses generational
    entity handles to detect use-after-free scenarios without pointer validation.
    Component arrays are lazily instantiated on first use per component type,
    reducing memory overhead for sparse component types. The free-list pattern
    enables O(1) entity creation/destruction while maintaining index stability
    for cache-friendly iteration.

    Memory Architecture:
    - m_freeEntities: FIFO queue of recycled entity slots
    - m_livingEntities: Hash set for O(1) liveness checks
    - m_componentArrays: Type-erased map of ComponentArray<T> instances
    - m_entityTypes: Optional type metadata for prefab system integration

    Thread Safety: Not thread-safe. External synchronization required for
    concurrent access. Consider migration to ECS archetypes (EnTT-style) if
    multithreaded system execution becomes critical.
  \note
    The design intentionally avoids virtual dispatch in hot paths. Component
    queries use direct template instantiation for zero-cost abstraction.
  \warning
    MAX_ENTITIES (4096) is a hard limit. Exceeding this triggers assertion
    failure rather than dynamic growth. For larger worlds, increase the constant
    or implement chunked entity storage.
  \see
    Entity, ComponentArray, ForEach
*************************************************************************/
class World
{
public:
    static constexpr uint32_t MAX_ENTITIES = 4096;

    /*************************************************************************
      \brief
        Entity handle type alias for Lua bindings.
      \details
        EntityHandle is simply an alias for Entity since Entity already includes
        generation tracking. This alias improves code clarity when interfacing
        with scripting systems that need to distinguish between raw entities
        and validated handles.
    *************************************************************************/
    using EntityHandle = Entity;

    /*************************************************************************
      \brief
        Constructor initializes free entity pool.
      \details
        Pre-populates the free entity queue with all available entity slots
        (IDs 1 through MAX_ENTITIES-1). ID 0 is reserved for NULL_ENTITY.
        All entities start with generation 0.
    *************************************************************************/
    World()
    {
        for (uint32_t i = 1; i < MAX_ENTITIES; ++i)
        {
            m_freeEntities.push(Entity{ i, 0 });
        }
    }

    // Forward declarations of EntityHandle methods
    // (Implementations follow after private member declarations)
    inline EntityHandle CreateEntityHandle();
    inline void DestroyEntityHandle(EntityHandle handle);
    inline bool IsEntityValidHandle(EntityHandle handle) const;
    inline uint32_t GetEntityGeneration(Entity entity) const;

    /*************************************************************************
      \brief
        Creates new entity from free-list with generational index.
      \return
        Valid Entity handle if slots available, NULL_ENTITY on exhaustion.
      \details
        Pops entity from free-list FIFO queue and inserts into living set.
        The returned entity retains its generation counter from previous
        destruction (if recycled) to invalidate stale handles. No components
        are attached; signature is empty. Logs creation to stdout for debugging.
      \note
        O(1) average case for queue pop and hash set insert. Generation counter
        is NOT incremented on creation, only on destruction. This means the first
        use of slot N has generation 0, second use has generation 1, etc.
      \warning
        Asserts if m_freeEntities is empty. Production code should handle gracefully
        (return NULL_ENTITY and log error). Consider implementing entity pooling
        or defragmentation if frequent creation/destruction causes fragmentation.
      \par Usage
      \code
        World world;
        Entity player = world.CreateEntity();
        if (player.IsValid()) {
            world.AddComponent<Transform2DFloat>(player, {0.f, 0.f, 1.f, 1.f, 0.f});
        }
      \endcode
    *************************************************************************/
    inline Entity CreateEntity()
    {
        if (m_freeEntities.empty())
        {
            assert(false && "Entity limit reached!");
            return NULL_ENTITY;
        }

        //std::cout << "Creating Entity!" << std::endl;

        Entity entity = m_freeEntities.front();
        m_freeEntities.pop();
        m_livingEntities.insert(entity);

        return entity;
    }

    /*************************************************************************
      \brief
        Destroys entity, removes all components, and recycles slot with incremented generation.
      \param[in] entity
        Entity handle to destroy (no-op if not alive).
      \details
        Execution flow:
        1. Verify entity is in m_livingEntities (early-out if not)
        2. Iterate all component arrays and call RemoveEntity (swap-and-pop)
        3. Increment generation counter (wraps at 255)
        4. Clear component signature
        5. Push recycled entity to m_freeEntities for reuse
        6. Remove from m_livingEntities

        The generation increment invalidates all outstanding Entity handles
        referencing this ID, preventing use-after-free. After 256 destructions,
        generation wraps to 0, creating theoretical ABA problem.
      \note
        O(C) where C = number of component types in m_componentArrays. Iteration
        over component arrays is unavoidable without per-entity component tracking.
        For sparse component distributions, consider maintaining per-entity
        component lists to skip empty arrays.
      \warning
        Does not notify systems of entity destruction. Implement observer pattern
        via GGEvent if systems require destruction callbacks (e.g., release GPU
        resources, unregister from spatial partition).
      \par Usage
      \code
        world.DestroyEntity(enemy);
        // All Entity handles with matching ID+gen are now invalid
        // Systems using old handles will fail IsValid() checks
      \endcode
    *************************************************************************/
    inline void DestroyEntity(Entity entity)
    {
        if (m_livingEntities.find(entity) == m_livingEntities.end())
        {
            return;
        }

        onEntityDestroyed.Emit(*this, Entity(entity));

        for (auto& [typeID, componentArray] : m_componentArrays)
        {
            componentArray->RemoveEntity(entity);
        }

        Entity recycled = { entity.id, static_cast<uint8_t>(entity.generation + 1) };
        recycled.signature.reset();
        m_freeEntities.push(recycled);
        m_livingEntities.erase(entity);

        std::cout << "Destroyed Entity " << entity.id << std::endl;
    }

    /*************************************************************************
      \brief
        Attaches component to entity with signature update.
      \tparam T
        Component type satisfying IsComponent concept.
      \param[in] entity
        Entity to attach component to.
      \param[in] component
        Component data to copy into storage.
      \details
        Verifies entity is alive, updates its signature bitset, then delegates
        to ComponentArray<T>::AddComponent. The const_cast on signature is
        necessary because m_livingEntities stores const Entity references to
        prevent accidental modification of the hash key.
      \note
        O(1) hash lookup plus O(1) component insertion. Signature update is
        performed via bit-set operation on ComponentSignature bitset.
      \warning
        Adding a component twice to the same entity is undefined behavior.
        Current implementation will overwrite the existing component without
        warning. Consider adding HasComponent check for debug builds.
      \par Usage
      \code
        Entity bullet = world.CreateEntity();
        world.AddComponent<Transform2DFloat>(bullet, {100.f, 200.f, 1.f, 1.f, 0.f});
        world.AddComponent<Rigidbody2DFloat>(bullet, {{300.f, 0.f}, 1.f, true});
      \endcode
    *************************************************************************/
    template<IsComponent T>
    inline void AddComponent(Entity entity, const T& component)
    {
        auto it = m_livingEntities.find(entity);
        if (it == m_livingEntities.end())
        {
            return;
        }

        const_cast<ComponentSignature&>(it->signature).set(T::COMPONENT_ID);
        GetComponentArray<T>().AddComponent(entity, component);
    }

    /*************************************************************************
      \brief
        Detaches component from entity and updates signature.
      \tparam T
        Component type to remove.
      \param[in] entity
        Entity to remove component from.
      \details
        Clears the component's bit in the entity's signature, then delegates
        to ComponentArray<T>::RemoveComponent for swap-and-pop removal.
        No-op if entity is not alive or doesn't have the component.
      \note
        O(1) signature update plus O(1) swap-and-pop. The removed component
        slot is filled by moving the last component in the array, maintaining
        cache locality but not preserving iteration order.
      \warning
        Removing a component during ForEach iteration over that component type
        causes undefined behavior. Use deferred removal queue or iterate in
        reverse if removal is necessary.
      \par Usage
      \code
        world.RemoveComponent<CollisionTag>(bullet);
        // Component data is invalidated; any pointers to it are now dangling
      \endcode
    *************************************************************************/
    template<IsComponent T>
    inline void RemoveComponent(Entity entity)
    {
        std::cout << "Remove Component!" << std::endl;
        auto it = m_livingEntities.find(entity);
        if (it == m_livingEntities.end())
        {
            return;
        }

        const_cast<ComponentSignature&>(it->signature).reset(T::COMPONENT_ID);
        GetComponentArray<T>().RemoveComponent(entity);
    }

    /*************************************************************************
      \brief
        Retrieves mutable component reference (crashes if absent).
      \tparam T
        Component type to retrieve.
      \param[in] entity
        Entity owning the component.
      \return
        Mutable reference to component data.
      \details
        Direct pointer access via ComponentArray<T>::GetComponent. No validation
        is performed; caller must ensure entity is alive and has the component.
        Use HasComponent<T>(entity) for safe checks.
      \note
        O(1) hash lookup in m_entityToIndex. Pointer remains valid until component
        is removed or array is reallocated (rare after initialization).
      \warning
        Dereferencing returned pointer after component removal or entity destruction
        causes undefined behavior. Do not cache pointers across frame boundaries.
      \par Usage
      \code
        Transform2DFloat* t = world.GetComponent<Transform2DFloat>(player);
        t->position.x += velocity.x * deltaTime;
      \endcode
    *************************************************************************/
    template<IsComponent T>
    inline T* GetComponent(Entity entity)
    {
        return GetComponentArray<T>().GetComponent(entity);
    }

    /*************************************************************************
      \brief
        Retrieves immutable component reference (const version).
      \tparam T
        Component type to retrieve.
      \param[in] entity
        Entity owning the component.
      \return
        Const reference to component data.
      \details
        Const-qualified version of GetComponent for read-only access. Same
        performance characteristics and validation caveats apply.
    *************************************************************************/
    template<IsComponent T>
    inline const T* GetComponent(Entity entity) const
    {
        return GetComponentArray<T>().GetComponent(entity);
    }

    /*************************************************************************
      \brief
        Checks if entity possesses a specific component.
      \tparam T
        Component type to check.
      \param[in] entity
        Entity to query.
      \return
        true if entity has component T, false otherwise.
      \details
        Tests the component's bit in the entity's signature bitset. O(1) operation
        via bit-test instruction on most architectures.
      \note
        This does not verify the entity is alive. A stale Entity handle may
        still have its signature bit set. Use IsEntityAlive first for comprehensive
        validation.
      \par Usage
      \code
        if (world.HasComponent<Rigidbody2DFloat>(projectile)) {
            ApplyPhysics(projectile);
        }
      \endcode
    *************************************************************************/
    template<IsComponent T>
    inline bool HasComponent(Entity entity) const
    {
        auto it = m_livingEntities.find(entity);
        if (it == m_livingEntities.end())
        {
            return false;
        }

        return it->signature.test(T::COMPONENT_ID);
    }

    /*************************************************************************
      \brief
        Verifies entity is alive and valid.
      \param[in] entity
        Entity handle to validate.
      \return
        true if entity exists in m_livingEntities, false otherwise.
      \details
        Performs O(1) hash lookup in living entities set. This is the authoritative
        liveness check; entity.IsValid() only checks ID != 0 and does not validate
        generation or existence.
      \note
        An entity may have a valid-looking Entity struct (non-zero ID) but still
        be destroyed. Always use this method for runtime validation of cached handles.
      \par Usage
      \code
        Entity target = cachedTargetHandle;
        if (world.IsEntityAlive(target)) {
            Attack(target);
        }
      \endcode
    *************************************************************************/
    inline bool IsEntityAlive(Entity entity) const
    {
        return m_livingEntities.find(entity) != m_livingEntities.end();
    }

    /*************************************************************************
      \brief
        Iterates entities possessing required component set.
      \tparam ComponentTypes
        Variadic list of component types (at least 1 required).
      \param[in] func
        Callback with signature void(Entity, ComponentTypes&...).
      \details
        Constructs a ComponentSignature bitmask from the template parameters,
        then iterates all living entities testing their signature via bitwise AND.
        Only entities with all required components invoke the callback.

        The signature test is O(1) per entity. For large worlds with sparse
        component distributions, this is more efficient than iterating component
        arrays and intersecting results.
      \note
        The lambda captures entity by value; mutations to the Entity struct itself
        (e.g., changing generation) do not affect the underlying storage. To modify
        components, use the passed references.
      \warning
        Do not create/destroy entities within callback. Queue operations for
        post-iteration processing. Modifying unrelated components is safe.
      \par Usage
      \code
        world.ForEach<Transform2DFloat, Velocity>(
            [](Entity e, Transform2DFloat& t, Velocity& v) {
                t.position += v.velocity * deltaTime;
            }
        );
      \endcode
      \par Performance
        For large worlds (10k+ entities), consider archetype-based ECS (EnTT,
        Flecs) to reduce signature testing overhead via contiguous archetype
        iteration.
    *************************************************************************/
    template<IsComponent... ComponentTypes>
    inline void ForEach(std::function<void(Entity, ComponentTypes&...)> const& func)
    {
        static_assert(sizeof...(ComponentTypes) > 0, "Must specify at least one component type");

        static const ComponentSignature requiredSig = []()
            {
                ComponentSignature sig;
                (sig.set(ComponentTypes::COMPONENT_ID), ...);
                return sig;
            }();

        for (Entity entity : m_livingEntities)
        {
            if ((entity.signature & requiredSig) != requiredSig) continue;

            func(entity, *GetComponent<ComponentTypes>(entity)...);
        }
    }

    /*************************************************************************
      \brief
        Iterates entities with World reference passed to callback.
      \tparam ComponentTypes
        Variadic component types.
      \param[in] func
        Callback with signature void(World&, Entity, ComponentTypes&...).
      \details
        Identical to ForEach but passes `*this` as first parameter to callback.
        Enables nested entity/component manipulation within iteration:
        - Spawning child entities
        - Triggering component removal via deferred queue
        - Querying unrelated components on other entities

        The "Nested" suffix warns that the callback has mutation capabilities
        requiring careful handling to avoid iterator invalidation.
      \warning
        Direct entity creation/destruction within callback causes undefined behavior.
        Queue operations for post-iteration processing or use iterators that support
        concurrent modification (e.g., stable_vector-based storage).
      \par Usage
      \code
        world.ForEachNested<ProjectileTag>([](World& w, Entity e, ProjectileTag& tag) {
            if (tag.shouldSpawnExplosion) {
                Entity explosion = w.CreateEntity(); // Deferred spawn
                // ... configure explosion entity ...
            }
        });
      \endcode
    *************************************************************************/
    template<IsComponent... ComponentTypes>
    inline void ForEachNested(std::function<void(World&, Entity, ComponentTypes&...)> const& func)
    {
        static const ComponentSignature requiredSig = []()
            {
                ComponentSignature sig;
                (sig.set(ComponentTypes::COMPONENT_ID), ...);
                return sig;
            }();

        for (Entity entity : m_livingEntities)
        {
            if ((entity.signature & requiredSig) != requiredSig) continue;

            func(*this, entity, *GetComponent<ComponentTypes>(entity)...);
        }
    }

    inline void ResetWorld()
    {
        for (Entity entity : m_livingEntities)
        {
            onEntityDestroyed.Emit(*this, Entity(entity));

            for (auto& [typeID, componentArray] : m_componentArrays)
            {
                componentArray->RemoveEntity(entity);
            }

            entity.signature.reset();
            m_freeEntities.push(entity);
        }

        m_livingEntities.clear();
        generationMap.clear();
    }

    /*************************************************************************
      \brief
        Returns immutable reference to living entities set.
      \return
        const std::unordered_set<Entity>& for iteration or queries.
      \details
        Exposes internal living entity set for advanced queries not covered by
        ForEach (e.g., counting, set operations, serialization). Returned reference
        is invalidated by CreateEntity/DestroyEntity.
      \warning
        Do not modify returned set. Const reference is for read-only access.
        Adding/removing entities bypasses World's lifecycle management and
        corrupts internal state.
      \par Usage
      \code
        size_t entityCount = world.GetLivingEntities().size();
        for (Entity e : world.GetLivingEntities()) {
            // Manual iteration without component filtering
        }
      \endcode
    *************************************************************************/
    inline std::unordered_set<Entity> const& GetLivingEntities() const
    {
        return m_livingEntities;
    }

    size_t const GetLivingEntitiesCount() const {
        return m_livingEntities.size();
    }

    void SetSelectedEntity(Entity e) { m_selectedEntity = e; }

    Entity GetSelectedEntity() const { return m_selectedEntity; }

    /*************************************************************************
      \brief
        Returns immutable reference to component array registry.
      \return
        const map of component type ID to ComponentArrayBase pointers.
      \details
        Exposes internal component storage for serialization, reflection, and
        debugging tools. Keys are ComponentTypeInfo<T>::GetTypeID() values.
        Use dynamic_cast or static_cast to recover concrete ComponentArray<T>
        types for direct access.
      \warning
        Type-erased interface via ComponentArrayBase. Incorrect downcasting causes
        undefined behavior. Prefer using World's templated accessors for type-safe
        component access in production code.
      \par Usage
      \code
        // Serialization: iterate all component types
        for (auto& [typeID, array] : world.GetComponentArrays()) {
            array->SerializeToJSON(entity, writer);
        }
      \endcode
    *************************************************************************/
    inline std::unordered_map<uint16_t, std::unique_ptr<ComponentArrayBase>> const& GetComponentArrays() const
    {
        return m_componentArrays;
    }

    inline bool IsPlaying() const
    {
        return isPlaying;
    }

    inline void StartPlayMode()
    {
        isPlaying = true;
    }

    inline void StopPlayMode()
    {
        isPlaying = false;
    }

    inline const Entity GetEntityFromID(uint32_t id) const
    {
        for (auto& entity : m_livingEntities)
        {
            if (entity.id == id) return entity;
        }

        return NULL_ENTITY;
    }

    inline void QueueEntityForDestruction(Entity entity)
    {
        if (m_livingEntities.find(entity) != m_livingEntities.end())
        {
            m_entitiesToDestroy.insert(entity);
        }
    }

    inline void QueueEntityHandleForDestruction(EntityHandle handle)
    {
        if (IsEntityValidHandle(handle))
        {
			std::cout << "Queueing Entity Handle for Destruction!" << std::endl;
            m_entitiesToDestroy.insert(handle);
        }
    }

    // Runtime only
    inline void ProcessDeferredDestruction()
    {
        for (Entity entity : m_entitiesToDestroy)
        {
			std::cout << "Processing Deferred Destruction for Entity " << entity.id << std::endl;
			onProcessEntityDestroyed.Emit(*this, Entity(entity));
        }
        m_entitiesToDestroy.clear();
    }

    inline bool IsQueuedForDestruction(Entity entity) const
    {
        return m_entitiesToDestroy.find(entity) != m_entitiesToDestroy.end();
    }

private:
    bool isPlaying{ false };

    std::queue<Entity> m_freeEntities;
    std::unordered_set<Entity> m_livingEntities;
    std::unordered_map<uint16_t, std::unique_ptr<ComponentArrayBase>> m_componentArrays;

    // for storing the current selected entity
    Entity m_selectedEntity = NULL_ENTITY;

    // Entity types by entity ID for prefab system integration
    std::unordered_map<uint32_t, uint32_t> m_entityTypes; // entity.id -> type

    // Lua entity handle generation tracking
    // Maps Entity to its current generation for validation
    std::unordered_map<Entity, uint32_t> generationMap;

    // Deferred deletion queue for safe entity destruction during iteration/scripts
    std::unordered_set<Entity> m_entitiesToDestroy;

    /*************************************************************************
      \brief
        Retrieves or creates ComponentArray<T> for given component type.
      \tparam T
        Component type satisfying IsComponent concept.
      \return
        Mutable reference to ComponentArray<T> (guaranteed non-null).
      \details
        Implements lazy instantiation pattern:
        1. Check if array exists via ComponentTypeInfo<T>::GetTypeID() lookup
        2. If found, downcast ComponentArrayBase* to ComponentArray<T>&
        3. If not found, construct std::make_unique<ComponentArray<T>>()
        4. Insert into m_componentArrays and return reference

        The returned reference is stable (std::unique_ptr never reallocates).
        Multiple calls with same T return same instance.
      \note
        First call per component type allocates ~16KB (ComponentArray reserves
        4096 slots). For memory-constrained platforms, reduce MAX_COMPONENTS or
        implement on-demand resizing instead of upfront reservation.
      \warning
        Not thread-safe. Concurrent GetComponentArray calls for same T cause
        data race during insertion. Protect with mutex or ensure single-threaded
        access during initialization phase.
    *************************************************************************/
    template<IsComponent T>
    inline ComponentArray<T>& GetComponentArray()
    {
        auto it = m_componentArrays.find(ComponentTypeInfo<T>::GetTypeID());
        if (it == m_componentArrays.end())
        {
            auto newArray = std::make_unique<ComponentArray<T>>();
            ComponentArray<T>* ptr = newArray.get();
            m_componentArrays[ComponentTypeInfo<T>::GetTypeID()] = std::move(newArray);
            return *ptr;
        }

        return static_cast<ComponentArray<T>&>(*it->second);
    }

    /*************************************************************************
      \brief
        Retrieves existing ComponentArray<T> (const version, asserts if not found).
      \tparam T
        Component type satisfying IsComponent concept.
      \return
        Const reference to ComponentArray<T>.
      \details
        Read-only accessor for const World methods. Assumes array already exists
        (no lazy instantiation). Asserts if component type has never been used.
      \warning
        Assertion failure if T has no registered components. Use non-const
        GetComponentArray() to trigger lazy instantiation, or check existence
        before calling const version.
    *************************************************************************/
    template<IsComponent T>
    inline const ComponentArray<T>& GetComponentArray() const
    {
        auto it = m_componentArrays.find(ComponentTypeInfo<T>::GetTypeID());
        assert(it != m_componentArrays.end());
        return static_cast<const ComponentArray<T>&>(*it->second);
    }
};

// Inline EntityHandle method implementations
// (Defined here after World class is complete and generationMap is declared)

/*************************************************************************
  \brief
    Creates a new entity with handle validation tracking.
  \return
    EntityHandle for the newly created entity, or NULL_ENTITY if exhausted.
  \details
    Creates an entity through the standard CreateEntity path, then registers
    its generation in the validation map. This enables IsEntityValidHandle
    to detect stale handles even after the entity is destroyed and recycled.
  \note
    This is primarily for Lua bindings where we need additional validation
    beyond the standard Entity generation counter.
*************************************************************************/
inline World::EntityHandle World::CreateEntityHandle()
{
    Entity entity = CreateEntity();

    if (generationMap.find(entity) == generationMap.end())
    {
        generationMap[entity] = entity.generation;
    }

    return entity;
}

/*************************************************************************
  \brief
    Destroys an entity handle with generation tracking.
  \param[in] handle
    EntityHandle to destroy (validated before destruction).
  \details
    Verifies the handle is valid through IsEntityValidHandle, increments
    its tracked generation, then destroys the underlying entity. The
    generation increment ensures any cached handles become invalid.
*************************************************************************/
inline void World::DestroyEntityHandle(EntityHandle handle)
{
    if (IsEntityValidHandle(handle))
    {
        generationMap[handle]++;
        DestroyEntity(handle);
    }
}

/*************************************************************************
  \brief
    Validates an entity handle against tracked generation.
  \param[in] handle
    EntityHandle to validate.
  \return
    true if handle exists in generationMap and generation matches.
  \details
    Checks if the entity's generation in our tracking map matches the
    generation in the handle itself. This catches use-after-free scenarios
    where a handle is used after the entity has been destroyed and recycled.
*************************************************************************/
inline bool World::IsEntityValidHandle(EntityHandle handle) const
{
    auto it = generationMap.find(handle);
    return it != generationMap.end() && it->second == handle.generation;
}

/*************************************************************************
  \brief
    Retrieves the current generation number for an entity.
  \param[in] entity
    Entity to query generation for.
  \return
    Tracked generation from generationMap if found, otherwise entity's
    internal generation value.
  \details
    Used by Lua bindings to check if an entity reference is still valid
    by comparing against the original generation at creation time.
*************************************************************************/
inline uint32_t World::GetEntityGeneration(Entity entity) const
{
    auto it = generationMap.find(entity);
    return (it != generationMap.end()) ? it->second : entity.generation;
}

namespace WorldHelpers {
    // helper function to check if an entity has a specific component by ID
    /*bool HasComponentByID(Entity entity, uint16_t compID) {
        switch (static_cast<Components>(compID)) {
        case Components::AABB_FLOAT:        return world.HasComponent<AABBFloat>(entity);
        case Components::MESH_RENDERER:     return world.HasComponent<MeshRenderer>(entity);
        case Components::TRANSFORM2D_FLOAT: return world.HasComponent<Transform2DFloat>(entity);
        case Components::MOVEMENT_INPUT:    return world.HasComponent<MovementInput>(entity);
        case Components::RIGIDBODY2D_FLOAT: return world.HasComponent<Rigidbody2DFloat>(entity);
        case Components::ENTITY_NAME:       return world.HasComponent<EntityName>(entity);
        case Components::CLICK_TO_MOVE:     return world.HasComponent<ClickToMove>(entity);
        case Components::PREFAB_INSTANCE:   return world.HasComponent<PrefabInstance>(entity);
        case Components::SPRITE:            return world.HasComponent<Sprite>(entity);
        case Components::SPRITE_ANIMATOR:   return world.HasComponent<SpriteAnimator>(entity);
        case Components::SCREEN_BOUNDS:     return world.HasComponent<ScreenBounds>(entity);
        case Components::SELECTABLE:        return world.HasComponent<Selectable>(entity);

        case Components::NO_COMPONENT:
        case Components::MAX_COMPONENT:
        default:
            std::cerr << "[World] Unknown componentID: " << compID << "\n";
            return false;
        }
    }*/

    // helper function to get component type name by ID
    inline std::string GetComponentTypeName(uint16_t compID) {
        switch (static_cast<Components>(compID)) {
        case Components::AABB_FLOAT:               return "AABBFloat";
        case Components::MESH_RENDERER:            return "MeshRenderer";
        case Components::TRANSFORM2D_FLOAT:        return "Transform2DFloat";
        case Components::MOVEMENT_INPUT:           return "MovementInput";
        case Components::RIGIDBODY2D_FLOAT:        return "Rigidbody2DFloat";
        case Components::ENTITY_NAME:              return "EntityName";
        case Components::CLICK_TO_MOVE:            return "ClickToMove";
        case Components::PREFAB_INSTANCE:          return "PrefabInstance";
        case Components::SPRITE:                   return "Sprite";
        case Components::SPRITE_ANIMATOR:          return "SpriteAnimator";
        case Components::SCREEN_BOUNDS:            return "ScreenBounds";
        case Components::SELECTABLE:               return "Selectable";
        case Components::SCRIPT_COMPONENT:         return "Script";
        case Components::NAME_COMPONENT:           return "Name";
        case Components::HIERARCHY_COMPONENT:      return "Hierarchy";
        case Components::LOCAL_TRANSFORM:          return "LocalTransform";
        case Components::WORLD_TRANSFORM:          return "WorldTransform";
        case Components::GRID_COMPONENT:           return "GridComponent";
        case Components::TILEMAP_COMPONENT:        return "TilemapComponent";
        case Components::UI_COMPONENT:             return "UIComponent";
        case Components::TEXT_COMPONENT:           return "TextComponent";
        case Components::WORLD_TEXT_COMPONENT:     return "WorldTextComponent";
        case Components::OBJECT_EFFECT_COMPONENT:  return "ObjectEffectComponent";

        case Components::NO_COMPONENT:
        case Components::MAX_COMPONENT:
        default:
            std::cerr << "[World] Unknown componentID: " << compID << "\n";
            return "UnknownComponent";
        }
    }
}

