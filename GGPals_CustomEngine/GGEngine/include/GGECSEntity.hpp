/*************************************************************************
  \file       GGECSEntity.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       25 October 2025
  \version    1.0
  \brief
    Entity handle with generation counter for Lua script safety.
  \details
    Wraps the existing uint32_t Entity ID with a generation counter to
    detect stale entity references from Lua scripts. The generation counter
    increments when entities are destroyed, invalidating old handles without
    requiring complex lifetime tracking in Lua.

    Layout (64-bit):
    - Lower 32 bits: Entity ID (compatible with existing ECS)
    - Upper 32 bits: Generation counter

    This allows gradual migration - existing C++ code continues using
    uint32_t Entity, while new Lua-facing code uses EntityHandle.
  \dependencies
    <cstdint>
  \see
    GGECSWorld.hpp, GGLuaBindings.hpp
  \history
    - 1.0 (25 October 2025): Initial version with generation validation.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <cstdint>

// Forward declare existing Entity type (from your ECS)
using Entity = uint32_t;

/*************************************************************************
  \struct EntityHandle
  \brief
    Safe entity handle for Lua with generation-based validation.
  \details
    Provides a 64-bit handle that combines entity ID and generation counter.
    When passed to Lua as lightuserdata (void*), the 64-bit value encodes
    both pieces of information. On retrieval from Lua, C++ validates the
    generation matches the current entity state.

    Design tradeoffs:
    - No inheritance/virtual functions (performance-first mindset)
    - Implicit conversion to Entity for C++ compatibility
    - Static validation function for World integration
  \warning
    Do not store EntityHandle across frames in Lua without validation.
    Always call World::IsEntityValid() before component access.
  \see
    World::CreateEntity, World::DestroyEntity, World::IsEntityValid
*************************************************************************/
struct EntityHandle
{
    uint32_t id;         // Entity ID (matches existing ECS Entity type)
    uint32_t generation; // Increments on entity destruction

    /*************************************************************************
      \brief
        Default constructor creates invalid handle.
      \details
        Both id and generation set to 0, representing the null entity.
        Use World::CreateEntity() to obtain valid handles.
      \par Usage
      \code
        EntityHandle handle; // Invalid by default
        if (handle.id == 0) { /* Handle is null */
}
\endcode
************************************************************************* /
EntityHandle() : id(0), generation(0) {}

/*************************************************************************
  \brief
    Constructs handle from entity ID and generation.
  \param[in] entityID
    The sparse set entity identifier.
  \param[in] gen
    Current generation counter for this ID slot.
  \details
    Called by World::CreateEntity() to wrap newly allocated entities.
    The generation starts at 1 (0 reserved for null).
*************************************************************************/
EntityHandle(uint32_t entityID, uint32_t gen) : id(entityID), generation(gen) {}

/*************************************************************************
  \brief
    Packs handle into 64-bit value for Lua lightuserdata.
  \return
    uint64_t with generation in upper 32 bits, id in lower 32 bits.
  \details
    Lua lightuserdata accepts void*, which we cast from uint64_t.
    This avoids heap allocation for entity handles in Lua.
  \par Usage (C++ -> Lua)
  \code
    EntityHandle handle = world.CreateEntity();
    uint64_t packed = handle.ToLua();
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
  \endcode
  \note
    Lua 5.1/LuaJIT stores lightuserdata as pointer-sized values.
    On 64-bit platforms, this preserves full 64 bits. On 32-bit,
    you must use userdata with __gc metamethod instead (not implemented).
*************************************************************************/
uint64_t ToLua() const
{
    return (static_cast<uint64_t>(generation) << 32) | id;
}

/*************************************************************************
  \brief
    Unpacks 64-bit Lua value into EntityHandle.
  \param[in] luaValue
    Value retrieved from lua_touserdata() or lua_tolightuserdata().
  \return
    EntityHandle with separated id and generation fields.
  \details
    Inverse of ToLua(). Extracts generation from upper 32 bits and
    id from lower 32 bits using bitwise operations.
  \par Usage (Lua -> C++)
  \code
    void* ptr = lua_touserdata(L, 1);
    uint64_t packed = reinterpret_cast<uint64_t>(ptr);
    EntityHandle handle = EntityHandle::FromLua(packed);

    if (world.IsEntityValid(handle)) {
        // Safe to access components
    }
  \endcode
*************************************************************************/
static EntityHandle FromLua(uint64_t luaValue)
{
    uint32_t id = static_cast<uint32_t>(luaValue & 0xFFFFFFFF);
    uint32_t gen = static_cast<uint32_t>(luaValue >> 32);
    return EntityHandle(id, gen);
}

/*************************************************************************
  \brief
    Implicit conversion to Entity for existing ECS API compatibility.
  \return
    The raw entity ID without generation.
  \details
    Allows EntityHandle to be passed to existing functions expecting
    Entity (uint32_t). This enables gradual migration:

    Old code: world.GetComponent<Transform>(entityID)
    New code: world.GetComponent<Transform>(handle) // handle.id used
  \warning
    This conversion discards generation information. Only use when
    validation has already occurred via IsEntityValid().
*************************************************************************/
operator Entity() const { return id; }

/*************************************************************************
  \brief
    Equality comparison based on both id and generation.
  \param[in] other
    Handle to compare against.
  \return
    true if both id and generation match, false otherwise.
  \details
    Two handles are equal only if they reference the same entity
    at the same generation. If an entity is destroyed and recreated
    with the same ID, the generation differs, making old handles invalid.
*************************************************************************/
bool operator==(const EntityHandle& other) const
{
    return id == other.id && generation == other.generation;
}

bool operator!=(const EntityHandle& other) const
{
    return !(*this == other);
}
};