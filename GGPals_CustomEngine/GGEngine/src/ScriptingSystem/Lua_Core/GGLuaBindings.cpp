/*************************************************************************
  \file       GGLuaBindings.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       04 November 2025
  \version    1.2
  \brief
    Implementation of Lua C API bindings for ECS and Event System.
  \details
    Implements entity creation, component access, validation, and event
    system bindings for Lua gameplay scripts. Uses table copying for small
    components to avoid FFI pointer stability issues. Includes type-safe
    event system with schema validation and legacy component support.
  \dependencies
    GGLuaBindings.hpp, GGECSWorld.hpp, GGECSEntity.hpp, GGECSComponents.hpp,
    GGEventBus.hpp, GGLuaDebug.hpp
  \see
    GGLuaBindings.hpp
  \history
    - 1.0 (25 October 2025): Initial implementation.
    - 1.1 (01 November 2025): Added event system and debug bindings.
    - 1.2 (04 November 2025): Added Transform2DFloat legacy support in
      hasComponent() for backwards compatibility with old lua scripts.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGLuaBindings.hpp"
#include "GGLuaVM.hpp"
#include "GGECSWorld.hpp"
#include "GGECSComponents.hpp"
#include "GGECSInterface.hpp"
#include "GGEventBus.hpp"
#include "GGLuaDebug.hpp"
#include "GGLuaInput.hpp"        // For RegisterInputBindings
#include "GGLuaExtensions.hpp"   // For RegisterExtendedECSBindings, RegisterAudioBindings, RegisterTimeBindings
#include "GGLuaVars.hpp"
#include "GGLuaGridBindings.hpp"
#include "GGLuaDefineType.hpp"
#include "GGLuaRPCBindings.hpp"
#include "GGLuaSteamBindings.hpp"
#include "GGLuaSceneBindings.hpp"
#include "GGLuaPrefabBindings.hpp"
#include "GGLuaCameraBindings.hpp"
#include "LuaSystemBindings.hpp"

#include <GGSystemManagers.hpp>

#include <iostream>
#include <cstring>

/*************************************************************************
  \brief
    Retrieves World pointer from Lua registry.
  \param[in] L
    Lua state.
  \return
    Pointer to World instance (never null if properly initialized).
  \details
    Looks up "gg_world_ptr" key in Lua registry. This pointer is stored
    during RegisterECSBindings(). If not found, triggers luaL_error()
    which longjmps to protected call boundary.
*************************************************************************/
World* GetWorldFromLua(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "gg_world_ptr");

    World* world = static_cast<World*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (!world)
    {
        luaL_error(L, "World pointer not found in registry");
    }

    return world;
}

/*************************************************************************
  \brief
    Packs Entity struct into uint64_t for Lua storage.
  \param[in] entity
    Entity to pack.
  \return
    Packed uint64_t representation (ID in lower 24 bits, gen in next 8).
  \details
    Entity uses bit fields: 24-bit ID + 8-bit generation. We combine these
    into a single uint64_t for storage as Lua lightuserdata (which only
    accepts pointer-sized integers, not arbitrary structs).
  \note
    Inverse of UnpackEntity().
*************************************************************************/
uint64_t PackEntity(const Entity& entity)
{
    uint64_t packed = 0;
    packed |= static_cast<uint64_t>(entity.id) & 0xFFFFFF;
    packed |= (static_cast<uint64_t>(entity.generation) & 0xFF) << 24;
    return packed;
}

/*************************************************************************
  \brief
    Unpacks uint64_t back to Entity struct.
  \param[in] packed
    Packed entity from PackEntity().
  \return
    Reconstructed Entity struct.
  \details
    Extracts ID from lower 24 bits, generation from next 8 bits. The
    signature field is not stored in Lua (it's ECS-internal state that
    gets reconstructed when entity is validated against World).
  \note
    Inverse of PackEntity().
*************************************************************************/
Entity UnpackEntity(uint64_t packed)
{
    Entity entity;
    entity.id = static_cast<uint32_t>(packed & 0xFFFFFF);
    entity.generation = static_cast<uint32_t>((packed >> 24) & 0xFF);
    return entity;
}

/*************************************************************************
  \brief
    Unpacks and validates entity handle from Lua stack.
  \param[in] L
    Lua state.
  \param[in] stackIndex
    Stack position of entity handle (expected to be lightuserdata).
  \param[in] world
    World instance to validate entity against.
  \return
    Validated Entity struct.
  \details
    Execution flow:
    1. Check argument is lightuserdata (entity handle type)
    2. Unpack uint64_t to Entity struct
    3. Validate entity via World::IsEntityValidHandle()
    4. Return entity if valid, else trigger luaL_error()
  \note
    If validation fails, this function does NOT return - it longjmps
    via luaL_error() to the pcall boundary.
*************************************************************************/
Entity GetValidatedEntityHandle(lua_State* L, int stackIndex, World& world)
{
    if (!lua_islightuserdata(L, stackIndex))
    {
        luaL_error(L, "Expected entity handle at argument %d", stackIndex);
    }

    // Unpack entity from lightuserdata
    void* ptr = lua_touserdata(L, stackIndex);
    uint64_t packed = reinterpret_cast<uint64_t>(ptr);
    Entity entity = UnpackEntity(packed);

    // Validate entity
    if (!world.IsEntityValidHandle(entity))
    {
        luaL_error(L, "Invalid entity handle (entity destroyed)");
    }

    return entity;
}

/*************************************************************************
  \brief
    Lua C function: ECS.createEntity()
  \param[in] L
    Lua state (no arguments expected).
  \return
    Number of Lua return values (1 - the entity handle).
  \details
    Creates a new entity via World::CreateEntityHandle(), packs it as
    uint64_t, and pushes as lightuserdata for Lua scripts to reference.
  \par Lua API
    local entity = ECS.createEntity()
*************************************************************************/
int lua_ECS_CreateEntity(lua_State* L)
{
    World* world = GetWorldFromLua(L);

    Entity entity = world->CreateEntityHandle();

    // Pack entity as lightuserdata
    uint64_t packed = PackEntity(entity);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));

    return 1; // Return entity handle
}

/*************************************************************************
  \brief
    Lua C function: ECS.destroyEntity(entity)
  \param[in] L
    Lua state. Stack position 1 must contain entity handle (lightuserdata).
  \return
    Number of Lua return values (0).
  \details
    Validates entity handle, then destroys the entity via
    World::DestroyEntityHandle() which increments generation counter.
  \par Lua API
    ECS.destroyEntity(entity)
*************************************************************************/
int lua_ECS_DestroyEntity(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    world->DestroyEntityHandle(entity);

    return 0; // No return values
}

int lua_ECS_DestroyEntityDeferred(lua_State* L)
{
    World* world = GetWorldFromLua(L);
	Entity entity = GetValidatedEntityHandle(L, 1, *world);

	GGLogger::Warning("Deferring destruction of entity: " + std::to_string(entity.id));

    world->QueueEntityHandleForDestruction(entity);

    return 0;
}

/*************************************************************************
  \brief
    Lua C function: ECS.addTransform(entity, x, y, sx, sy, rotation)
  \param[in] L
    Lua state. Expected stack:
    1: entity (lightuserdata)
    2: x (number)
    3: y (number)
    4: sx (number)
    5: sy (number)
    6: rotation (number)
  \return
    Number of Lua return values (0).
  \details
    Creates Transform2DFloat component with provided position, scale, and
    rotation values. If entity already has transform, this overwrites it.
  \par Lua API
    ECS.addTransform(entity, 100, 200, 1, 1, 0)
*************************************************************************/
int lua_ECS_AddTransform(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float sx = static_cast<float>(luaL_checknumber(L, 4));
    float sy = static_cast<float>(luaL_checknumber(L, 5));
    float rotation = static_cast<float>(luaL_checknumber(L, 6));

    LocalTransform local{};
    local.position = { x, y };
    local.scale = { sx, sy };
    local.rotation = rotation;


    world->AddComponent<LocalTransform>(entity, local);
    world->AddComponent<WorldTransform>(entity, {});

    return 0;
}

/*************************************************************************
  \brief
    Lua C function: ECS.getTransform(entity)
  \param[in] L
    Lua state. Stack position 1 must contain entity handle.
  \return
    Number of Lua return values (1 - table with transform data).
  \details
    Retrieves Transform2DFloat component and copies values to Lua table
    with keys: x, y, sx, sy, rotation. Modifications to the returned
    table do NOT affect ECS state - use setTransform to write back.
  \par Lua API
    local t = ECS.getTransform(entity)
    print(t.x, t.y)
*************************************************************************/
int lua_ECS_GetTransform(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<LocalTransform>(entity))
    {
        luaL_error(L, "Entity does not have Transform2DFloat component");
    }

    LocalTransform* local = world->GetComponent<LocalTransform>(entity);

    // Create table
    lua_newtable(L);

    // Set table fields
    lua_pushnumber(L, local->position.x);
    lua_setfield(L, -2, "x");

    lua_pushnumber(L, local->position.y);
    lua_setfield(L, -2, "y");

    lua_pushnumber(L, local->scale.x);
    lua_setfield(L, -2, "sx");

    lua_pushnumber(L, local->scale.y);
    lua_setfield(L, -2, "sy");

    lua_pushnumber(L, local->rotation);
    lua_setfield(L, -2, "rotation");

    return 1; // Return table
}

/*************************************************************************
  \brief
    Lua C function: ECS.setTransform(entity, table)
  \param[in] L
    Lua state. Expected stack:
    1: entity (lightuserdata)
    2: table with optional keys: x, y, sx, sy, rotation
  \return
    Number of Lua return values (0).
  \details
    Copies table values back to Transform2DFloat component. Missing keys
    preserve existing component values (partial update support). This
    enables efficient "read-modify-write" pattern from Lua.
  \par Lua API
    local t = ECS.getTransform(entity)
    t.x = t.x + 5
    ECS.setTransform(entity, t)
*************************************************************************/
int lua_ECS_SetTransform(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for transform parameters");
    }

    if (!world->HasComponent<LocalTransform>(entity))
    {
        luaL_error(L, "Entity does not have LocalTransform component");
    }

    LocalTransform* local = world->GetComponent<LocalTransform>(entity);

    // Get x (optional)
    lua_getfield(L, 2, "x");
    if (!lua_isnil(L, -1))
    {
        local->position.x = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // Get y (optional)
    lua_getfield(L, 2, "y");
    if (!lua_isnil(L, -1))
    {
        local->position.y = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // Get sx (optional)
    lua_getfield(L, 2, "sx");
    if (!lua_isnil(L, -1))
    {
        local->scale.x = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // Get sy (optional)
    lua_getfield(L, 2, "sy");
    if (!lua_isnil(L, -1))
    {
        local->scale.y = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // Get rotation (optional)
    lua_getfield(L, 2, "rotation");
    if (!lua_isnil(L, -1))
    {
        local->rotation = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    local->isDirty = true;

    // Invalidate WorldTransform if it exists so hierarchy system will recompute it
    if (world->HasComponent<WorldTransform>(entity))
    {
        WorldTransform* worldTransform = world->GetComponent<WorldTransform>(entity);
        worldTransform->isValid = false;
        
        #ifdef GG_EDITOR_BUILD
        // Debug logging for editor builds only
        if (world->HasComponent<NameComponent>(entity)) {
            auto* nameComp = world->GetComponent<NameComponent>(entity);
            std::string entityName(nameComp->name);
            // Only log OrderUI/ChaosUI and their children
            if (entityName.find("OrderUI") != std::string::npos || entityName.find("ChaosUI") != std::string::npos) {
                std::cout << "[ECS.setTransform] " << entityName << " (ID:" << entity.id << ") - Set isDirty=true, isValid=false" << std::endl;
            }
        }
        #endif
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua C function: ECS.addRigidbody(entity, vx, vy)
  \param[in] L
    Lua state. Expected stack:
    1: entity (lightuserdata)
    2: vx (number) - velocity X
    3: vy (number) - velocity Y
  \return
    Number of Lua return values (0).
  \details
    Creates Rigidbody2DFloat component with provided velocity. Mass
    defaults to 1.0, flags to 0.
  \par Lua API
    ECS.addRigidbody(entity, 10, 0)
*************************************************************************/
int lua_ECS_AddRigidbody(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));

    Rigidbody2DFloat rigidbody;
    rigidbody.vx = vx;
    rigidbody.vy = vy;
    rigidbody.mass = 1.0f;  // Default mass
    rigidbody.flags = 0;    // Default flags

    world->AddComponent<Rigidbody2DFloat>(entity, rigidbody);

    return 0;
}

/*************************************************************************
  \brief
    Lua C function: ECS.getRigidbody(entity)
  \param[in] L
    Lua state. Stack position 1 must contain entity handle.
  \return
    Number of Lua return values (1 - table with rigidbody data).
  \details
    Retrieves Rigidbody2DFloat component and copies velocity to Lua table
    with keys: vx, vy. Mass and flags are not exposed (can add if needed).
  \par Lua API
    local rb = ECS.getRigidbody(entity)
    print(rb.vx, rb.vy)
*************************************************************************/
int lua_ECS_GetRigidbody(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<Rigidbody2DFloat>(entity))
    {
        luaL_error(L, "Entity does not have Rigidbody2DFloat component");
    }

    Rigidbody2DFloat* rigidbody = world->GetComponent<Rigidbody2DFloat>(entity);

    // Create table
    lua_newtable(L);

    // Set velocity fields
    lua_pushnumber(L, rigidbody->vx);
    lua_setfield(L, -2, "vx");

    lua_pushnumber(L, rigidbody->vy);
    lua_setfield(L, -2, "vy");

    // Could add mass and flags if needed
    // lua_pushnumber(L, rigidbody->mass);
    // lua_setfield(L, -2, "mass");

    return 1; // Return table
}

/*************************************************************************
  \brief
    Lua C function: ECS.setRigidbody(entity, table)
  \param[in] L
    Lua state. Expected stack:
    1: entity (lightuserdata)
    2: table with optional keys: vx, vy
  \return
    Number of Lua return values (0).
  \details
    Copies table values back to Rigidbody2DFloat component. Only velocity
    is modifiable from Lua (mass/flags require separate functions).
  \par Lua API
    local rb = ECS.getRigidbody(entity)
    rb.vx = 0
    ECS.setRigidbody(entity, rb)
*************************************************************************/
int lua_ECS_SetRigidbody(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for rigidbody parameters");
    }

    if (!world->HasComponent<Rigidbody2DFloat>(entity))
    {
        luaL_error(L, "Entity does not have Rigidbody2DFloat component");
    }

    Rigidbody2DFloat* rigidbody = world->GetComponent<Rigidbody2DFloat>(entity);

    // Get vx (optional)
    lua_getfield(L, 2, "vx");
    if (!lua_isnil(L, -1))
    {
        rigidbody->vx = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // Get vy (optional)
    lua_getfield(L, 2, "vy");
    if (!lua_isnil(L, -1))
    {
        rigidbody->vy = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    return 0;
}

/*************************************************************************
  \brief
    Lua C function: ECS.hasComponent(entity, componentName)
  \param[in] L
    Lua state. Expected stack:
    1: entity (lightuserdata)
    2: component name (string)
  \return
    Number of Lua return values (1 - boolean).
  \details
    Checks if entity has specified component type. Currently supports:
    - "Transform2DFloat"
    - "Rigidbody2DFloat"
    Add more as needed.
  \par Lua API
    if ECS.hasComponent(entity, "Transform2DFloat") then
        -- Has transform
    end
*************************************************************************/
int lua_ECS_HasComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    const char* componentName = luaL_checkstring(L, 2);

    bool hasComponent = false;

    if (std::strcmp(componentName, "LocalTransform") == 0)
    {
        hasComponent = world->HasComponent<LocalTransform>(entity);
    }
    else if (std::strcmp(componentName, "WorldTransform") == 0)
    {
        hasComponent = world->HasComponent<WorldTransform>(entity);
    }
    else if (std::strcmp(componentName, "Transform2DFloat") == 0)
    {
        // Legacy support - check if entity has LocalTransform (new transform system)
        // ECS.addTransform() now creates LocalTransform + WorldTransform, not Transform2DFloat
        hasComponent = world->HasComponent<LocalTransform>(entity);
    }
    else if (strcmp(componentName, "Rigidbody2DFloat") == 0)
    {
        hasComponent = world->HasComponent<Rigidbody2DFloat>(entity);
    }
    // Add more component types as needed
    else
    {
        luaL_error(L, "Unknown component type: %s", componentName);
    }

    lua_pushboolean(L, hasComponent);
    return 1;
}

/*************************************************************************
  \brief
    Registers ECS bindings in Lua global namespace.
  \param[in] luaVM
    Lua VM instance.
  \param[in] world
    ECS World instance to bind.
  \details
    Creates global "ECS" table with all entity/component functions.
    Stores World pointer in Lua registry for C callback access.
*************************************************************************/
void RegisterECSBindings(GGLuaVM& luaVM, World& world)
{
    lua_State* L = luaVM.GetState();

    // Store world pointer in registry
    lua_pushlightuserdata(L, &world);
    lua_setfield(L, LUA_REGISTRYINDEX, "gg_world_ptr");

    // Create global ECS table
    lua_newtable(L);

    // Register functions
    lua_pushcfunction(L, lua_ECS_CreateEntity);
    lua_setfield(L, -2, "createEntity");

    lua_pushcfunction(L, lua_ECS_DestroyEntity);
    lua_setfield(L, -2, "destroyEntity");

	lua_pushcfunction(L, lua_ECS_DestroyEntityDeferred);
	lua_setfield(L, -2, "destroyEntityDeferred");

    lua_pushcfunction(L, lua_ECS_AddTransform);
    lua_setfield(L, -2, "addTransform");

    lua_pushcfunction(L, lua_ECS_GetTransform);
    lua_setfield(L, -2, "getTransform");

    lua_pushcfunction(L, lua_ECS_SetTransform);
    lua_setfield(L, -2, "setTransform");

    lua_pushcfunction(L, lua_ECS_AddRigidbody);
    lua_setfield(L, -2, "addRigidbody");

    lua_pushcfunction(L, lua_ECS_GetRigidbody);
    lua_setfield(L, -2, "getRigidbody");

    lua_pushcfunction(L, lua_ECS_SetRigidbody);
    lua_setfield(L, -2, "setRigidbody");

    lua_pushcfunction(L, lua_ECS_HasComponent);
    lua_setfield(L, -2, "hasComponent");

    // Set as global
    lua_setglobal(L, "ECS");
}

/*************************************************************************
  EVENT SYSTEM BINDINGS
  Added in version 1.1
*************************************************************************/

// Global pointer for event bus access (like World pointer)
static GGEventBus* g_eventBus = nullptr;

/*************************************************************************
  \brief
    Lua: Event.define(name, schema)
  \details
    Defines an event with parameter type validation schema.
*************************************************************************/
int lua_Event_Define(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    EventSchema schema;
    schema.isNetworked = false;  // Default local

    // Parse schema table
    lua_pushnil(L);
    while (lua_next(L, 2) != 0)
    {
        const char* paramName = lua_tostring(L, -2);
        const char* paramType = lua_tostring(L, -1);

        ParamDef def;
        def.name = paramName;
        def.required = true;

        if (strcmp(paramType, "number") == 0)
            def.type = ParamType::NUMBER;
        else if (strcmp(paramType, "string") == 0)
            def.type = ParamType::STRING;
        else if (strcmp(paramType, "boolean") == 0)
            def.type = ParamType::BOOLEAN;
        else if (strcmp(paramType, "entity") == 0)
            def.type = ParamType::ENTITY;
        else if (strcmp(paramType, "integer") == 0)
            def.type = ParamType::INTEGER;
        else
        {
            luaL_error(L, "Unknown parameter type: %s", paramType);
        }

        schema.params.push_back(def);
        lua_pop(L, 1);
    }

    g_eventBus->RegisterEvent(name, schema);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Event.defineNetworked(name, schema)
  \details
    Defines a networked event that will be synchronized.
*************************************************************************/
int lua_Event_DefineNetworked(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    EventSchema schema;
    schema.isNetworked = true;  // Networked event

    // Parse schema table (same as define)
    lua_pushnil(L);
    while (lua_next(L, 2) != 0)
    {
        const char* paramName = lua_tostring(L, -2);
        const char* paramType = lua_tostring(L, -1);

        ParamDef def;
        def.name = paramName;
        def.required = true;

        if (strcmp(paramType, "number") == 0)
            def.type = ParamType::NUMBER;
        else if (strcmp(paramType, "string") == 0)
            def.type = ParamType::STRING;
        else if (strcmp(paramType, "boolean") == 0)
            def.type = ParamType::BOOLEAN;
        else if (strcmp(paramType, "entity") == 0)
            def.type = ParamType::ENTITY;
        else if (strcmp(paramType, "integer") == 0)
            def.type = ParamType::INTEGER;
        else
        {
            luaL_error(L, "Unknown parameter type: %s", paramType);
        }

        schema.params.push_back(def);
        lua_pop(L, 1);
    }

    g_eventBus->RegisterEvent(name, schema);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Event.subscribe(event, callback)
  \details
    Subscribes to an event with a callback function.
*************************************************************************/
int lua_Event_Subscribe(lua_State* L)
{
    const char* event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    uint32_t id = g_eventBus->Subscribe(L, event, 2);
    lua_pushinteger(L, id);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Event.unsubscribe(subscriptionId)
  \details
    Removes an event subscription by ID.
*************************************************************************/
int lua_Event_Unsubscribe(lua_State* L)
{
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    g_eventBus->Unsubscribe(L, id);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Event.emitLocal(event, params)
  \details
    Emits a local-only event (not networked).
*************************************************************************/
int lua_Event_EmitLocal(lua_State* L)
{
    const char* event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    g_eventBus->EmitLocal(L, event, 2);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Event.emitNetworked(event, params)
  \details
    Emits a networked event (synchronized across clients).
*************************************************************************/
int lua_Event_EmitNetworked(lua_State* L)
{
    const char* event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    g_eventBus->EmitNetworked(L, event, 2);
    return 0;
}

/*************************************************************************
  \brief
    Register event system in Lua global namespace.
  \param[in] luaVM
    Lua VM instance.
  \param[in] eventBus
    Event bus instance for routing.
  \details
    Creates global "Event" table with event system functions.
    Stores EventBus pointer for C callback access.
*************************************************************************/
void RegisterEventBindings(GGLuaVM& luaVM, GGEventBus& eventBus)
{
    g_eventBus = &eventBus;
    lua_State* L = luaVM.GetState();

    // Create Event table
    lua_newtable(L);

    lua_pushcfunction(L, lua_Event_Define);
    lua_setfield(L, -2, "define");

    lua_pushcfunction(L, lua_Event_DefineNetworked);
    lua_setfield(L, -2, "defineNetworked");

    lua_pushcfunction(L, lua_Event_Subscribe);
    lua_setfield(L, -2, "subscribe");

    lua_pushcfunction(L, lua_Event_Unsubscribe);
    lua_setfield(L, -2, "unsubscribe");

    lua_pushcfunction(L, lua_Event_EmitLocal);
    lua_setfield(L, -2, "emitLocal");

    lua_pushcfunction(L, lua_Event_EmitNetworked);
    lua_setfield(L, -2, "emitNetworked");

    lua_setglobal(L, "Event");

    std::cout << "[LuaBindings] Event system registered" << std::endl;
}

/*************************************************************************
  \brief
    Register debug bindings wrapper.
  \param[in] luaVM
    Lua VM instance.
  \details
    Delegates to GGLuaDebug to register Log, Debug, and Profile tables.
*************************************************************************/
void RegisterDebugBindings(GGLuaVM& luaVM)
{
    GGLuaDebug::RegisterDebugBindings(luaVM.GetState());
    std::cout << "[LuaBindings] Debug bindings registered" << std::endl;
}

/*************************************************************************
  \brief
    Master registration function that registers all Lua bindings.
  \param[in] luaVM
    Lua VM instance.
  \param[in] world
    ECS World for component operations.
  \param[in] eventBus
    Event bus for event system.
  \details
    Single entry point for all Lua subsystem registration.
    Call this instead of individual registration functions.
*************************************************************************/
void RegisterAllLuaBindings(GGLuaVM& luaVM, World& world, GGEventBus& eventBus)
{
    // Register core ECS bindings
    RegisterECSBindings(luaVM, world);

    // Register extended ECS bindings
    RegisterExtendedECSBindings(luaVM, world);

    // Register prefab bindings
    RegisterPrefabBindings(luaVM);

    // Register scene bindings
    RegisterSceneBindings(luaVM);

    // Register event system
    RegisterEventBindings(luaVM, eventBus);

    // Register vars system
    RegisterVarBindings(luaVM);

    // Register grid system
    RegisterGridBindings(luaVM);

    // Register input system
    RegisterInputBindings(luaVM);

    // Register Camera systems
    RegisterCameraBindings(luaVM);

    // Register Define Type
    RegisterDefineTypeBindings(luaVM);

    // Register debug utilities
    RegisterDebugBindings(luaVM);

    // Register input system
    RegisterInputBindings(luaVM);

    // Register audio system
    RegisterAudioBindings(luaVM);

    // Register time utilities
    RegisterTimeBindings(luaVM);

   //Register Object effect type
    RegisterObjectEffectsBindings(luaVM);

    // Register RPC bindings
    RegisterRPCBindings(luaVM);

    // Register Steam platform + matchmaking bindings. Safe in builds without
    // the Steamworks SDK: the table still exists and reports unavailable, so
    // scripts branch on Steam.isAvailable() instead of being written twice.
    RegisterSteamBindings(luaVM);

    // Register System bindings
    RegisterSystemBindings(luaVM);


    std::cout << "[LuaBindings] All bindings registered successfully" << std::endl;
}