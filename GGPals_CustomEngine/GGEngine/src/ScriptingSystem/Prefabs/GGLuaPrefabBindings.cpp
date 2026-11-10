/*************************************************************************
  \file       GGLuaPrefabBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Exposes Prefab management functionality to Lua scripts for runtime
    spawning and hierarchy control of entities.
  \details
    This header defines all Lua bindings for prefab management, including:
    - Spawning prefabs at runtime
    - Optional positioning and parenting
    - Returning spawned entity handles to Lua scripts

  \dependencies
    - GGLuaVM.hpp
    - GGEngine.hpp
    - GGECSWorld.hpp
    - GGECSInterface.hpp
    - GGECSComponents.hpp
    - Lua 5.x headers (lua.h, lauxlib.h, lualib.h)
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaPrefabBindings.hpp>

#include <GGLuaVM.hpp>

#include <GGEngine.hpp>

#include <GGECSWorld.hpp>
#include <GGECSInterface.hpp>
#include <GGECSComponents.hpp>

#include <GGSystemManagers.hpp>

/*************************************************************************
  \brief
    Register prefab management API in Lua global namespace.

  \param[in] luaVM
    Lua VM instance.

  \details
    Creates global "Prefab" table with prefab management functions:
    - Prefab.spawn(prefabName) -> entity
    - Prefab.spawn(prefabName, parent) -> entity
    - Prefab.spawn(prefabName, x, y) -> entity
    - Prefab.spawn(prefabName, x, y, parent) -> entity
*************************************************************************/
void RegisterPrefabBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	lua_pushcfunction(L, lua_Prefab_Spawn);
	lua_setfield(L, -2, "spawn");

	lua_setglobal(L, "Prefab");

	std::cout << "[LuaBindings] Prefab management registered" << std::endl;
}

/*************************************************************************
  \brief
    Lua C function: Prefab.spawn(prefabName, [x, y, parent])

  \param[in] L
    Lua state. Stack:
    1: prefabName (string) - Name of prefab to spawn
    2: x (number, optional) - X position for spawned entity
    3: y (number, optional) - Y position for spawned entity
    4: parent (entity, optional) - Parent entity handle

  \return
    Number of Lua return values (1 - entity handle or nil on failure).
  
  \details
    Spawns an entity from a prefab. If x,y coordinates are provided and
    the entity has a Transform component, sets the position. If parent
    entity is provided, the spawned entity will be parented to it (this
    is independent of the prefab's internal hierarchy). Returns the
    spawned entity handle, or nil if prefab doesn't exist.
    The function spawns the prefab hierarchy (parent + all children) but
    only returns the root entity handle.

  \par Lua API
    -- Spawn without position or parent
    local enemy = Prefab.spawn("EnemyGoblin")

    -- Spawn at specific position
    local player = Prefab.spawn("PlayerCharacter", 100, 200)

    -- Spawn as child (no position override)
    local weapon = Prefab.spawn("Sword", player)

    -- Spawn as child at specific position
    local weapon2 = Prefab.spawn("Sword", 0, 0, player)

    -- Check if spawn succeeded
    if enemy then
        Log.info("Enemy spawned successfully")
    else
        Log.error("Failed to spawn enemy - prefab not found")
    end
*************************************************************************/
int lua_Prefab_Spawn(lua_State* L)
{
    auto& world = GetEngineInstance().GetSceneManager().GetActiveScene();

	const char* prefabName = luaL_checkstring(L, 1);

    // Optional x, y position arguments
    bool hasPosition = false;
    float posX = 0.0f;
    float posY = 0.0f;

    // Optional parent entity argument
    Entity parentEntity = NULL_ENTITY;

    int nargs = lua_gettop(L);

    // Overload: Prefab.spawn(prefabName, parent)
    if (nargs == 2 && lua_islightuserdata(L, 2))
    {
        parentEntity = GetValidatedEntityHandle(L, 2, world);
    }
    else
    {
        // Overload: Prefab.spawn(prefabName, x, y [, parent])
        if (nargs >= 3 && lua_isnumber(L, 2) && lua_isnumber(L, 3))
        {
            posX = static_cast<float>(luaL_checknumber(L, 2));
            posY = static_cast<float>(luaL_checknumber(L, 3));
            hasPosition = true;
        }

        if (nargs >= 4 && lua_islightuserdata(L, 4))
        {
            parentEntity = GetValidatedEntityHandle(L, 4, world);
        }
    }

	std::vector<Entity> spawnedEntities;

	Entity entity = GetEngineInstance().GetPrefabManager()
		.SpawnFromPrefab(prefabName, 
		world,
		spawnedEntities,
		parentEntity);

    if (entity == NULL_ENTITY)
    {
        lua_pushnil(L);
        return 1;
    }

    if (spawnedEntities.size() == 0)
    {
        lua_pushnil(L);
        return 1;
    }

    if (hasPosition && world.HasComponent<LocalTransform>(entity))
    {
        LocalTransform* transform = world.GetComponent<LocalTransform>(entity);
        
        if (transform)
        {
            transform->position.x = posX;
            transform->position.y = posY;
        }
    }

    for (Entity e : spawnedEntities)
    {
        if (world.HasComponent<ScriptComponent>(e))
        {
            Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().InvokeVoidFunction(
                e,
                world,
                "Init"
            );
        }
    }

    for (Entity e : spawnedEntities)
    {
        if (world.HasComponent<ScriptComponent>(e))
        {
            Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().InvokeVoidFunction(
                e,
                world,
                "Start"
            );
        }
    }

    uint64_t packed = PackEntity(entity);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));

	return 1;
}