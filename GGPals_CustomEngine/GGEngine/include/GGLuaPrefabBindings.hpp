/*************************************************************************
  \file       GGLuaPrefabBindings.hpp
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
#pragma once

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class GGLuaVM;

/*************************************************************************
  \brief
    Register prefab management API in Lua global namespace.

  \param[in] luaVM
    Lua VM instance.

  \details
    Creates global "Prefab" table with prefab management functions:
    - Prefab.spawn(prefabName) -> entity
    - Prefab.spawn(prefabName, x, y) -> entity
    - Prefab.spawn(prefabName, x, y, parent) -> entity
*************************************************************************/
void RegisterPrefabBindings(GGLuaVM& luaVM);

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

    -- Spawn with parent entity
    local weapon = Prefab.spawn("Sword", 0, 0, player)

    -- Check if spawn succeeded
    if enemy then
        Log.info("Enemy spawned successfully")
    else
        Log.error("Failed to spawn enemy - prefab not found")
    end
*************************************************************************/
int lua_Prefab_Spawn(lua_State* L);