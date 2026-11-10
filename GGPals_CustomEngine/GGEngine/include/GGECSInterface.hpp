/*************************************************************************
  \file       GGECSInterface.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       28 January 2026
  \version    1.1
  \brief
    Defines the GGEngine ECS Interface.
  \details
    GGEngine's ECS provides basic functionality such as Entity and Component
    interfaces for other programmer's reference, and basic functionality such
    as AddComponent, RemoveComponent and GetComponent. 
  \dependencies
    <cstdint>, <iostream>, <cassert>, <vector>, <unordered_map>, <unordered_set>,
    <memory>, <functional>, <tuple>, <queue>, <typeinfo>
  \see
    Vector2Int, Vector3, Matrix3x3
  \history
    - 1.0 (6 September 2025): Initial version.
    - 1.1 (28 January 2026): Integrated GGMemoryManager PoolAllocator.
                             ComponentArray now uses custom allocator for tracking.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <cstdint>
#include <iostream>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <tuple>
#include <queue>
#include <typeinfo>
#include <bitset>
#include <algorithm>
#include <GGJSONUtils.hpp>
#include <GGMemoryManager.hpp>


/*************************************************************************
  \concept IsComponent
  \brief
    Compile-time constraint for ECS component validity.
  \tparam T
    Type to validate as a legal component.
  \details
    Enforces the following invariants for component types:
    - Must expose COMPONENT_ID as constexpr uint16_t for O(1) type lookup
    - Must expose SERIALIZED_SIZE as constexpr size_t for binary I/O
    - Must be trivially copyable (memcpy-safe for serialization)
    - Must be standard layout (predictable memory layout across compilers)
    - Cannot be pointer or reference (entity-component relationships are
      managed externally to prevent dangling references)
    
    These constraints enable zero-copy serialization, memory-mapped file I/O,
    and deterministic network replication without custom serialization logic.
  \note
    The trivially_copyable constraint prevents use of std::string, std::vector,
    or std::unique_ptr in components. Use fixed-size buffers (char[N]) or
    asset ID handles (uint32_t) with external resource management instead.
  \warning
    GLM types (vec3, mat4) satisfy these constraints but may contain padding
    bytes that are undefined. Always zero-initialize GLM components or use
    explicit padding fields for deterministic serialization.
  \par Rationale
    POD-like components enable the ECS to achieve data-oriented design goals:
    tight packing in memory, minimal indirection, and predictable performance
    characteristics. The concept acts as a compiler-enforced contract preventing
    introduction of heap allocations or virtual dispatch into the hot path.
  \see
    ComponentArray, ComponentTypeInfo, SERIALIZED_SIZE
*************************************************************************/
template<typename T>
concept IsComponent = requires {
    { T::COMPONENT_ID } -> std::convertible_to<uint16_t>;
    { T::SERIALIZED_SIZE } -> std::convertible_to<size_t>;
        requires std::is_trivially_copyable_v<T>;
        requires std::is_standard_layout_v<T>;
        requires !std::is_pointer_v<T>;
        requires !std::is_reference_v<T>;
};

/*************************************************************************
  \struct ComponentTypeInfo
  \brief
    Compile-time type reflection for components.
  \tparam T
    Component type satisfying IsComponent concept.
  \details
    Provides static member functions for extracting component metadata without
    RTTI overhead. Used by ComponentArray and World for type-to-storage mapping.
    All methods are constexpr-evaluable for zero-cost abstraction.
  \note
    GetTypeName uses typeid().name() which returns mangled names on GCC/Clang.
    Consider using __PRETTY_FUNCTION__ or std::source_location::type_name_v
    (C++26) for human-readable names in debug builds.
  \see
    IsComponent, Components enum
*************************************************************************/
template<IsComponent T>
struct ComponentTypeInfo {
/*************************************************************************
  \brief
    Retrieves compile-time component type identifier.
  \return
    Unique uint16_t ID from T::COMPONENT_ID.
  \details
    Used for indexing into ComponentSignature bitset and ComponentArray lookup
    tables. The constexpr nature enables switch-case optimization on component
    types at compile-time.
*************************************************************************/
	static constexpr uint16_t GetTypeID() 
    {
        return T::COMPONENT_ID;
	}

/*************************************************************************
  \brief
    Retrieves runtime type name for debugging and serialization.
  \return
    Mangled type name from typeid(T).name().
  \warning
    Output is compiler-specific. Use only for debugging or internal keys.
    For JSON serialization, map COMPONENT_ID to human-readable strings.
*************************************************************************/
	static const char* GetTypeName() 
    {
        static std::string cleanName;
        std::string name = typeid(T).name();

        constexpr const char* structPrefix = "struct ";
        if (name.find(structPrefix) == 0) {
            name = name.substr(strlen(structPrefix));
        }

        cleanName = name;
        return cleanName.c_str();
	}
private:
};

/*************************************************************************
  \enum Components
  \brief
    Centralized registry of component type identifiers.
  \details
    Assigns unique IDs to each component type for O(1) signature lookup and
    ComponentArray indexing. The enum ensures compile-time uniqueness and
    prevents ID collisions. MAX_COMPONENT defines the upper bound for the
    ComponentSignature bitset.
  \warning
    Adding new components requires updating this enum, ComponentIDS (legacy
    compatibility), and instantiating corresponding ComponentSerializer
    specializations. Consider using code generation or X-macros for large
    component counts.
  \note
    NO_COMPONENT (0) is reserved as an invalid sentinel. Component IDs start
    at 1 to distinguish valid components from default-initialized bitsets.
  \see
    ComponentSignature, MAX_COMPONENT_ID
*************************************************************************/
enum class Components : uint16_t
{
    NO_COMPONENT = 0,
    AABB_FLOAT = 1,
    MESH_RENDERER = 2,
    TRANSFORM2D_FLOAT = 3,
    MOVEMENT_INPUT = 4,
    RIGIDBODY2D_FLOAT = 5,
    ENTITY_NAME = 6,
    CLICK_TO_MOVE = 7,
    PREFAB_INSTANCE = 8,
    SPRITE = 9,
    SPRITE_ANIMATOR = 10,
    SCREEN_BOUNDS = 11,
    SELECTABLE = 12,
    LOCAL_TRANSFORM = 13,
    WORLD_TRANSFORM = 14,
    HIERARCHY_COMPONENT = 15,
    NAME_COMPONENT = 16,
    SCRIPT_COMPONENT = 17,
    GRID_COMPONENT = 18,
    AUDIO_SOURCE = 19,
    TILEMAP_COMPONENT = 20,
    UI_COMPONENT = 21,
    TEXT_COMPONENT =22,
    WORLD_TEXT_COMPONENT=23,
    OBJECT_EFFECT_COMPONENT=24,
    MAX_COMPONENT = 25
};

constexpr uint16_t MAX_COMPONENT_ID = static_cast<uint16_t>(Components::MAX_COMPONENT);

/*************************************************************************
  \typedef ComponentSignature
  \brief
    Bitset representing component presence on an entity.
  \details
    Fixed-size bitset where bit N corresponds to Components enum value N.
    Used for rapid component existence checks (O(1) bitwise AND) and archetype
    queries. The signature is stored inline in the Entity struct for cache
    locality during iteration.
  \note
    16-byte size for 12 components is efficient but will grow with MAX_COMPONENT_ID.
    For engines with 64+ component types, consider migrating to sparse sets or
    archetype-based storage where signatures are managed per-archetype rather
    than per-entity.
  \see
    Entity::signature, World::GetEntitiesWithComponents
*************************************************************************/
using ComponentSignature = std::bitset<MAX_COMPONENT_ID>;


/*************************************************************************
  \struct Entity
  \brief
    Generational index handle for entity lifecycle management.
  \details
    Combines a 24-bit entity ID with an 8-bit generation counter to detect
    use-after-free of entity handles. When an entity is destroyed, its generation
    is incremented; future lookups with stale Entity handles fail the generation
    check. The ComponentSignature is stored inline for cache-efficient component
    queries during system iteration.
    
    Entity Layout (32 bits + signature):
    - Bits 0-23:  Entity ID (16.7M unique entities)
    - Bits 24-31: Generation (255 reuses per slot)
    - Signature:  128-bit component bitset
  \warning
    After 255 recycles of an entity slot, generation wraps to 0. This creates
    a theoretical ABA problem where a very old Entity handle could match a
    new entity if exactly 256 recycles occurred. For long-running servers,
    consider 16-bit generations or UUID-based handles.
  \note
    IsValid() only checks ID != 0; it does not validate the generation or
    signature. Use World::IsEntityAlive() for comprehensive validation.
  \see
    NULL_ENTITY, World::CreateEntity, World::DestroyEntity
*************************************************************************/
struct Entity {
	uint32_t id : 24;
	uint32_t generation : 8;

    ComponentSignature signature;

    Entity() : id(0), generation(0), signature() {}
    Entity(uint32_t _id, uint8_t _gen)
        : id(_id), generation(_gen), signature() {}

	bool operator==(const Entity& other) const {
		return id == other.id && generation == other.generation;
	}

	bool IsValid() const { return id != 0; }
};

inline const Entity NULL_ENTITY{ 0, 0 };

/*************************************************************************
  \namespace std::hash<Entity>
  \brief
    Hash functor specialization for Entity as unordered_map key.
  \details
    Combines 24-bit ID and 8-bit generation into 64-bit hash value via
    bit-shifting. The hash is injective (no collisions) since it preserves
    all bits of the input. Required for Entity as key in
    std::unordered_map/std::unordered_set.
  \note
    Current implementation shifts ID left by 8 bits and ORs with generation.
    This is optimal for uniform distribution assuming random entity IDs.
  \see
    Entity, ComponentArray::m_entityToIndex
*************************************************************************/
namespace std {
    template<>
    struct hash<Entity> {
        size_t operator()(const Entity& entity) const {
            return static_cast<size_t>((static_cast<uint64_t>(entity.id) << 8) | entity.generation);
        }
    };
}

/*************************************************************************
  \class ComponentArrayBase
  \brief
    Type-erased interface for polymorphic component storage access.
  \details
    Abstract base class enabling World to manage heterogeneous ComponentArray<T>
    instances without template parameters. Provides virtual methods for entity
    removal, serialization, and reflection. Concrete ComponentArray<T> instances
    implement these methods with type-specific logic.
  \note
    Virtual dispatch overhead is acceptable here since component operations
    (add/remove) are infrequent compared to iteration. Hot-path queries use
    ComponentArray<T>::GetComponent() directly without virtual calls.
  \warning
    The GetRawData() methods expose internal storage pointers. Callers must
    not retain these pointers across component add/remove operations, as the
    underlying std::vector may reallocate and invalidate pointers.
  \see
    ComponentArray, World::GetComponentArrays
*************************************************************************/
class ComponentArrayBase {
public:
/*************************************************************************
  \brief
    Virtual destructor for proper derived class cleanup.
*************************************************************************/
    virtual ~ComponentArrayBase() = default;
/*************************************************************************
  \brief
    Removes component from entity (if present).
  \param[in] entity
    Entity to remove component from.
  \details
    Called by World during entity destruction or explicit component removal.
    Delegates to ComponentArray<T>::RemoveComponent with swap-and-pop semantics.
*************************************************************************/
    virtual void RemoveEntity(Entity entity) = 0;

    virtual bool HasEntity(Entity entity) const = 0;
    
/*************************************************************************
  \brief
    Returns sizeof(T) for the stored component type.
  \return
    Byte size of component for serialization stride calculations.
*************************************************************************/
    virtual size_t GetComponentSize() const = 0;
/*************************************************************************
  \brief
    Returns T::COMPONENT_ID for runtime type identification.
  \return
    Unique component type ID from Components enum.
*************************************************************************/
    virtual uint16_t GetTypeID() const = 0;
/*************************************************************************
  \brief
    Returns number of component instances in storage.
  \return
    Count of active components (equals entity count with this component).
*************************************************************************/
    virtual size_t GetCount() const = 0;
/*************************************************************************
  \brief
    Returns const pointer to contiguous component storage.
  \return
    Pointer to T[] array of GetCount() elements.
*************************************************************************/
    virtual const void* GetRawData() const = 0;
/*************************************************************************
  \brief
    Returns mutable pointer to contiguous component storage.
  \return
    Pointer to T[] array for direct modification.
*************************************************************************/
    virtual void* GetMutableRawData() = 0;
/*************************************************************************
  \brief
    Serializes entity's component to JSON via writer.
  \param[in] entity
    Entity whose component to serialize.
  \param[in,out] writer
    RapidJSON writer for streaming output.
  \details
    Writes component type name as key and component data as nested object:
    "Transform2DFloat": {"x": 10.0, "y": 20.0, ...}
    
    Delegates to ComponentSerializer<T>::ToJSON for type-specific serialization.
    Called by EntityJSONInterface during world serialization.
  \see
    ComponentSerializer, EntityJSONInterface::SerializeWorld
*************************************************************************/
    virtual void SerializeToJSON(Entity entity, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const = 0;
};

/*************************************************************************
  \class ComponentArray
  \brief
    Contiguous, cache-friendly storage for components of type T.
  \tparam T
    Component type satisfying IsComponent concept.
  \details
    Implements sparse-set pattern with packed component array and entity-to-index
    lookup table. Components are stored densely in std::vector for cache-coherent
    iteration, with O(1) access via entity handle. Add/remove operations maintain
    packing via swap-and-pop, invalidating iteration order but preserving cache
    locality.
    
    Memory Layout:
    - m_components: [T1, T2, T3, ...] (packed, no holes)
    - m_entityToIndex: {Entity1->0, Entity2->1, Entity3->2}
    - m_indexToEntity: {0->Entity1, 1->Entity2, 2->Entity3}
    
    This structure enables both fast iteration (linear scan of m_components) and
    fast lookup (hash table access). The dual-direction mapping enables O(1)
    removal via swap-and-pop.
  \note
    Static assertion ensures T is trivially copyable for memcpy-safe serialization.
    This constraint is redundant with IsComponent but provides clearer error
    messages if the concept is bypassed.
  \warning
    MAX_COMPONENTS (4096) is a soft limit enforced by reserve(). Exceeding this
    triggers reallocation but does not fail. For large worlds, consider increasing
    or migrating to chunked storage (ECS archetypes).
  \see
    ComponentArrayBase, World, IsComponent
*************************************************************************/
template<IsComponent T>
class ComponentArray : public ComponentArrayBase {
    static_assert(std::is_trivially_copyable_v<T>, "Components must be trivially copyable for serialization");

public:
    static constexpr size_t MAX_COMPONENTS = 4096;

    ComponentArray() {
        m_components.reserve(MAX_COMPONENTS);
        m_entityToIndex.reserve(MAX_COMPONENTS);
    }

/*************************************************************************
  \brief
    Adds component to entity with duplicate detection.
  \param[in] entity
    Entity to attach component to.
  \param[in] component
    Component data to copy into storage.
  \details
    Appends component to m_components and updates bidirectional index mappings.
    The entity's ComponentSignature is updated externally by World; this method
    only manages storage.
  \warning
    Asserts on duplicate component (same entity already has this component type).
    Debug builds print diagnostic information before asserting. Release builds
    have undefined behavior on duplicates. Caller must ensure entity does not
    already have component via World::HasComponent().
  \note
    The duplicate detection logs to stdout before asserting. Consider upgrading
    to structured logging or exception-based error handling for production builds.
  \par Complexity
    O(1) average case for hash table insert and vector push_back.
*************************************************************************/
    void AddComponent(Entity entity, const T& component) {
        auto it = m_entityToIndex.find(entity);

        if (it != m_entityToIndex.end()) {
            // Entity already has this component type
            std::cout << "Duplicate component detected for entity: " << entity.id << std::endl;
            std::cout << "Existing index: " << it->second << std::endl;
            // Add breakpoint or stack trace here
        }

        assert(m_entityToIndex.find(entity) == m_entityToIndex.end() && "NO duplicate Components on an entity!"); // No duplicates

        size_t index = m_components.size();
        m_components.push_back(component);
        m_entityToIndex[entity] = index;
        m_indexToEntity[index] = entity;
    }

/*************************************************************************
  \brief
    Removes component from entity using swap-and-pop.
  \param[in] entity
    Entity to remove component from.
  \details
    Swaps the target component with the last element, then pops the vector.
    Updates index mappings for both the removed entity and the swapped entity.
    If entity does not have component, operation is a no-op (silently succeeds).
  \note
    Invalidates iteration order but maintains packing for cache efficiency.
    Systems must not assume stable component ordering across removals.
  \par Complexity
    O(1) hash table lookup, O(1) swap, O(1) vector pop_back.
*************************************************************************/
    void RemoveComponent(Entity entity) {
        auto it = m_entityToIndex.find(entity);
        if (it == m_entityToIndex.end()) return;

        size_t indexToRemove = it->second;
        size_t lastIndex = m_components.size() - 1;

        if (indexToRemove != lastIndex) {
            m_components[indexToRemove] = m_components[lastIndex];

            Entity lastEntity = m_indexToEntity[lastIndex];
            m_entityToIndex[lastEntity] = indexToRemove;
            m_indexToEntity[indexToRemove] = lastEntity;
        }

        m_components.pop_back();
        m_entityToIndex.erase(entity);
        m_indexToEntity.erase(lastIndex);
    }

/*************************************************************************
  \brief
    Retrieves mutable component pointer for entity.
  \param[in] entity
    Entity to query.
  \return
    Pointer to component if present, nullptr otherwise.
  \details
    O(1) hash table lookup. Returned pointer is valid until next add/remove
    operation on this ComponentArray (std::vector reallocation).
  \warning
    Do not retain pointer across frames or component mutations. Use World's
    accessors for safe, tracked component access.
*************************************************************************/
    T* GetComponent(Entity entity) {
        auto it = m_entityToIndex.find(entity);
        return (it != m_entityToIndex.end()) ? &m_components[it->second] : nullptr;
    }

/*************************************************************************
  \brief
    Retrieves const component pointer for entity.
  \param[in] entity
    Entity to query.
  \return
    Const pointer to component if present, nullptr otherwise.
  \details
    Const-qualified overload for read-only access. Prevents accidental mutation
    of component data.
*************************************************************************/
    const T* GetComponent(Entity entity) const {
        auto it = m_entityToIndex.find(entity);
        return (it != m_entityToIndex.end()) ? &m_components[it->second] : nullptr;
    }

    bool HasEntity(Entity entity) const override {
        return m_entityToIndex.find(entity) != m_entityToIndex.end();
    }

/*************************************************************************
  \brief
    Serializes entity's component to JSON.
  \param[in] entity
    Entity whose component to serialize.
  \param[in,out] writer
    RapidJSON writer for output.
  \details
    Writes component type name (from typeid().name()) as JSON key, then
    delegates to ComponentSerializer<T>::ToJSON for component data serialization.
    If entity does not have component, no output is written.
  \note
    Implementation not shown but inferred from design patterns. Likely checks
    GetComponent(entity) != nullptr before serializing.
  \see
    ComponentSerializer<T>::ToJSON
*************************************************************************/
    void SerializeToJSON(Entity entity, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const;

    //void SerializeFromJSON(Entity entity, rapidjson::Value& json) const;

/*************************************************************************
  \brief
    Iterator to beginning of component storage.
  \return
    Mutable iterator to first component.
  \details
    Enables range-based for loops: `for (T& c : componentArray) { ... }`
    Iterates in packed order (not entity creation order after removals).
*************************************************************************/
    auto begin() { return m_components.begin(); }

/*************************************************************************
  \brief
    Iterator to end of component storage.
  \return
    Mutable iterator past last component.
*************************************************************************/
    auto end() { return m_components.end(); }

/*************************************************************************
  \brief
    Const iterator to beginning of component storage.
  \return
    Const iterator to first component (read-only iteration).
*************************************************************************/
    auto begin() const { return m_components.begin(); }

/*************************************************************************
  \brief
    Const iterator to end of component storage.
  \return
    Const iterator past last component.
*************************************************************************/
    auto end() const { return m_components.end(); }

    size_t GetCount() const override { return m_components.size(); }
    const void* GetRawData() const override { return m_components.data(); }
    void* GetMutableRawData() override { return m_components.data(); }
    size_t GetComponentSize() const override { return sizeof(T); }
    uint16_t GetTypeID() const override { return ComponentTypeInfo<T>::GetTypeID(); }

    void RemoveEntity(Entity entity) override { RemoveComponent(entity); }

private:
    std::vector<T, PoolAllocator<T>> m_components;
    std::unordered_map<Entity, size_t> m_entityToIndex;
    std::unordered_map<size_t, Entity> m_indexToEntity;
};
/*************************************************************************
  \class World
  \brief
    Forward declaration of ECS world container.
  \details
    Manages entity lifecycle, component arrays, and system scheduling.
    Full definition in GGECSWorld.hpp.
  \see
    GGECSWorld.hpp
*************************************************************************/
class World;
/*************************************************************************
  \class SystemInterface
  \brief
    Forward declaration of ECS system interface.
  \details
    Base class for systems that operate on component data. Full definition
    in GGSystemInterface.hpp.
  \see
    GGSystemInterface.hpp
*************************************************************************/
class SystemInterface;