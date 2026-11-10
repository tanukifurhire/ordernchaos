/*************************************************************************
  \file       GGLuaBindings.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       25 October 2025
  \version    1.2
  \brief
    C API bindings for ECS access from Lua scripts.
  \details
    Exposes entity creation, component access, and queries to Lua via
    the Lua C API. Uses table-based component representation (copying
    small structs) to avoid FFI pointer instability issues.

    Design decisions:
    - C API (not FFI) for stability and safety
    - Table copying for components (8-20 byte structs are cheap to copy)
    - EntityHandle validation on every Lua->C++ crossing
    - No virtual functions (performance-first mindset)

    Lua API exposed:
    ```lua
    local entity = ECS.createEntity()
    ECS.addTransform(entity, 0, 0, 1, 1, 0) -- x, y, sx, sy, rotation

    local transform = ECS.getTransform(entity)
    transform.x = transform.x + 5
    ECS.setTransform(entity, transform)

    if ECS.hasComponent(entity, "Transform2DFloat") then
        -- Component exists
    end

    ECS.destroyEntity(entity)
    ```
  \dependencies
    lua.h, GGLuaVM.hpp, GGECSWorld.hpp, GGECSEntity.hpp, GGECSComponents.hpp
  \see
    GGLuaVM, World, EntityHandle, GGEventBus, GGLuaDebug, GGLuaInput, GGLuaExtensions
  \history
    - 1.0 (25 October 2025): Initial version with table-based components.
    - 1.1 (1 November 2025): Added event system and debug bindings.
    - 1.2 (1 November 2025): Added input and extended ECS bindings.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <cstdint>
#include "GGECSInterface.hpp"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Forward declarations
class World;
class GGLuaVM;
class GGEventBus;  // Added for event system

/*************************************************************************
  \brief
    Registers all Lua bindings for the engine.
  \param[in] luaVM
    Lua VM to register functions in.
  \param[in] world
    ECS World instance to bind.
  \param[in] eventBus
    Event bus instance for event routing.
  \details
    Master registration function that calls all individual binding
    registration functions. This is the only function that needs to
    be called from GGEngine.
  \par Usage
  \code
    GGLuaVM luaVM;
    World world;
    GGEventBus eventBus;
    RegisterAllLuaBindings(luaVM, world, eventBus);
  \endcode
  \note
    Must be called after all subsystems are initialized.
*************************************************************************/
void RegisterAllLuaBindings(GGLuaVM& luaVM, World& world, GGEventBus& eventBus);

/*************************************************************************
  \brief
    Registers ECS API in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \param[in] world
    ECS World instance to bind (stored in Lua registry for access).
  \details
    Creates global "ECS" table with functions:
    - ECS.createEntity() -> entity
    - ECS.destroyEntity(entity)
    - ECS.addTransform(entity, x, y, sx, sy, rotation)
    - ECS.getTransform(entity) -> table
    - ECS.setTransform(entity, table)
    - ECS.addRigidbody(entity, vx, vy)
    - ECS.getRigidbody(entity) -> table
    - ECS.setRigidbody(entity, table)
    - ECS.hasComponent(entity, componentName) -> bool

    World pointer is stored in Lua registry under key "gg_world_ptr"
    for retrieval in C function implementations.
  \par Usage
  \code
    GGLuaVM luaVM;
    World world;
    RegisterECSBindings(luaVM, world);

    luaVM.ExecuteString("local e = ECS.createEntity()", "test");
  \endcode
  \note
    World pointer is not owned by Lua. Ensure World outlives GGLuaVM.
*************************************************************************/
void RegisterECSBindings(GGLuaVM& luaVM, World& world);

/*************************************************************************
  \brief
    Registers Event Bus API in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \param[in] eventBus
    Event bus instance for event routing and validation.
  \details
    Creates global "Event" table with functions:
    - Event.define(name, schema) - Define event with parameter types
    - Event.subscribe(name, callback) - Subscribe to event
    - Event.emitLocal(name, params) - Emit local-only event
    - Event.emitNetworked(name, params) - Emit networked event

    Event bus pointer is stored in registry for C callback access.
  \par Usage
  \code
    GGEventBus eventBus;
    RegisterEventBindings(luaVM, eventBus);

    luaVM.ExecuteString("Event.define('Test', {value = 'number'})", "test");
  \endcode
  \note
    EventBus pointer is not owned by Lua. Ensure it outlives GGLuaVM.
*************************************************************************/
void RegisterEventBindings(GGLuaVM& luaVM, GGEventBus& eventBus);

/*************************************************************************
  \brief
    Registers debug utilities in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \details
    Creates global tables with debug functions:
    - Log.info(message) - Log informational message
    - Log.warning(message) - Log warning message
    - Log.error(message) - Log error message
    - Debug.stackTrace() - Get current stack trace
    - Debug.getMemory() - Get Lua memory usage
    - Profile.start(name) - Start profiling timer
    - Profile.stop(name) - Stop timer and log elapsed time
  \par Usage
  \code
    RegisterDebugBindings(luaVM);

    luaVM.ExecuteString("Log.info('Script loaded')", "test");
  \endcode
*************************************************************************/
void RegisterDebugBindings(GGLuaVM& luaVM);

/*************************************************************************
  INTERNAL C FUNCTIONS (not exposed directly to Lua)

  These functions implement the Lua C API callbacks for ECS operations.
  They follow the lua_CFunction signature: int(*)(lua_State*)

  Each function:
  1. Retrieves World* from Lua registry
  2. Validates entity handles via GetValidatedEntityHandle()
  3. Calls ECS API with error checking
  4. Pushes results to Lua stack
  5. Returns number of return values

  Error handling:
  - Invalid entity handles: luaL_error() with message
  - Missing components: luaL_error() with message
  - Type errors: luaL_error() with message

  All errors use luaL_error(), which longjmps to lua_pcall in GGLuaVM,
  so script execution is sandboxed (no engine crash).
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.createEntity() -> entity
  \details
    Creates new entity with generation counter.
    Returns Entity packed as lightuserdata (uint64_t).
  \par Lua Usage
  \code
    local entity = ECS.createEntity()
  \endcode
*************************************************************************/
int lua_ECS_CreateEntity(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.destroyEntity(entity)
  \param[in] 1 (Lua stack)
    Entity handle (lightuserdata).
  \details
    Validates entity handle, destroys entity, increments generation.
  \par Lua Usage
  \code
    ECS.destroyEntity(entity)
  \endcode
*************************************************************************/
int lua_ECS_DestroyEntity(lua_State* L);

int lua_ECS_DestroyEntityDeferred(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.addTransform(entity, x, y, sx, sy, rotation)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2-6
    Transform values (numbers).
  \details
    Creates Transform2DFloat component with provided values.
  \par Lua Usage
  \code
    ECS.addTransform(entity, 100, 200, 1, 1, 0)
  \endcode
*************************************************************************/
int lua_ECS_AddTransform(lua_State* L);

/*************************************************************************
  \brief
    Lua: local t = ECS.getTransform(entity)
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with keys: x, y, sx, sy, rotation
  \details
    Copies Transform2DFloat data to Lua table. Modifications to table
    do not affect ECS - must call setTransform to apply changes.
  \par Lua Usage
  \code
    local t = ECS.getTransform(entity)
    print(t.x, t.y) -- Read values
  \endcode
*************************************************************************/
int lua_ECS_GetTransform(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setTransform(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with keys: x, y, sx, sy, rotation (all optional).
  \details
    Copies table values back to Transform2DFloat component.
    Missing keys preserve existing values (partial update support).
  \par Lua Usage
  \code
    local t = ECS.getTransform(entity)
    t.x = t.x + 5 -- Modify
    ECS.setTransform(entity, t) -- Apply
  \endcode
*************************************************************************/
int lua_ECS_SetTransform(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.addRigidbody(entity, vx, vy)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2-3
    Velocity values (numbers).
  \details
    Creates Rigidbody2DFloat component with provided velocity.
  \par Lua Usage
  \code
    ECS.addRigidbody(entity, 10, 0) -- Move right at 10 units/sec
  \endcode
*************************************************************************/
int lua_ECS_AddRigidbody(lua_State* L);

/*************************************************************************
  \brief
    Lua: local rb = ECS.getRigidbody(entity)
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with keys: vx, vy
  \details
    Copies Rigidbody2DFloat to Lua table.
  \par Lua Usage
  \code
    local rb = ECS.getRigidbody(entity)
    print(rb.vx, rb.vy)
  \endcode
*************************************************************************/
int lua_ECS_GetRigidbody(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setRigidbody(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with keys: vx, vy (optional).
  \details
    Copies table values back to Rigidbody2DFloat.
  \par Lua Usage
  \code
    local rb = ECS.getRigidbody(entity)
    rb.vx = 0 -- Stop horizontal movement
    ECS.setRigidbody(entity, rb)
  \endcode
*************************************************************************/
int lua_ECS_SetRigidbody(lua_State* L);

/*************************************************************************
  \brief
    Lua: local has = ECS.hasComponent(entity, "Transform2DFloat")
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Component name (string).
  \return (Lua stack)
    Boolean (true if component exists).
  \details
    Checks if entity has specified component type.
  \par Lua Usage
  \code
    if ECS.hasComponent(entity, "Transform2DFloat") then
        -- Entity has transform
    end
  \endcode
*************************************************************************/
int lua_ECS_HasComponent(lua_State* L);

/*************************************************************************
  HELPER FUNCTIONS

  Internal utilities for Lua<->C++ data marshalling.
*************************************************************************/

/*************************************************************************
  \brief
    Retrieves World pointer from Lua registry.
  \param[in] L
    Lua state.
  \return
    Pointer to World (never null if RegisterECSBindings called).
  \details
    Looks up "gg_world_ptr" in Lua registry. Assumes RegisterECSBindings()
    stored the pointer. If missing, triggers luaL_error().
*************************************************************************/
World* GetWorldFromLua(lua_State* L);

/*************************************************************************
  \brief
    Packs Entity into uint64_t for Lua lightuserdata storage.
  \param[in] entity
    Entity to pack.
  \return
    Packed uint64_t representation.
  \details
    Combines entity ID and generation into single 64-bit value for storage
    as Lua lightuserdata. Lua lightuserdata can only store pointers/integers,
    not arbitrary structs, so we pack the Entity fields.
  \par Bit Layout
    - Bits 0-23:  Entity ID
    - Bits 24-31: Generation counter
  \see
    UnpackEntity
*************************************************************************/
uint64_t PackEntity(const Entity& entity);

/*************************************************************************
  \brief
    Unpacks Entity from packed uint64_t representation.
  \param[in] packed
    Entity packed as uint64_t (ID in lower 24 bits, generation in next 8 bits).
  \return
    Unpacked Entity struct.
  \details
    Reverses PackEntity operation. Used to extract Entity from Lua lightuserdata.
  \par Bit Layout
    - Bits 0-23:  Entity ID
    - Bits 24-31: Generation counter
  \see
    PackEntity
*************************************************************************/
Entity UnpackEntity(uint64_t packed);

/*************************************************************************
  \brief
    Validates entity handle from Lua stack and unpacks it.
  \param[in] L
    Lua state.
  \param[in] stackIndex
    Stack position of entity handle (lightuserdata).
  \param[in] world
    World instance for validation.
  \return
    Validated Entity.
  \details
    Retrieves lightuserdata from stack, unpacks Entity, validates via
    World::IsEntityValidHandle(). If invalid, triggers luaL_error().
  \par Error Messages
    - "Invalid entity handle (entity destroyed)"
    - "Expected entity handle at argument N"
  \note
    This function does not return on error - it longjmps via luaL_error.
*************************************************************************/
Entity GetValidatedEntityHandle(lua_State* L, int stackIndex, World& world);