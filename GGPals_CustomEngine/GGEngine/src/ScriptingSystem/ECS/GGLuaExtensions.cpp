/*************************************************************************
  \file       GGLuaExtensions.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       2 February 2026
  \version    1.2
  \brief
    Implementation of extended ECS bindings for Lua.
    Implements EntityName, AABB, MeshRenderer, TextComponent components
    and query systems, plus audio and time utilities. Fixed to match
    actual component structures. TextComponent bindings enable dynamic
    text rendering and editing from Lua scripts.
  \dependencies
    GGLuaExtensions.hpp, GGECSWorld.hpp, GGECSComponents.hpp
  \see
    GGLuaExtensions.hpp
  \history
    - 1.0 (1 November 2025): Initial implementation.
    - 1.1 (31 January 2026): Added TextComponent bindings (add/get/set).
    - 1.2 (2 February 2026): Exposed fontAssetID and shaderAssetID in
      TextComponent bindings for dynamic font switching from Lua scripts.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGLuaExtensions.hpp"
#include "GGLuaAnimationBindings.hpp"
#include "GGLuaVM.hpp"
#include "GGLuaBindings.hpp"  // For helper functions
#include "GGECSWorld.hpp"
#include "GGECSComponents.hpp"
#include "GGEngine.hpp"
#include "GGAudioManager.hpp"
#include <GGECSAssetInterface.hpp>
#include <GGAudioAsset.hpp>
#include "GGTime.hpp"
#include <iostream>
#include <cstring>
#include <GGSystemManagers.hpp>
#include <unordered_map>
#include <chrono>
#include <GGJSONConfig.hpp>

// Global map for entity name lookups (optimization for findByName)
static std::unordered_map<std::string, Entity> g_entityNameMap;

void SetWorldBinding(GGLuaVM& luaVM, World& world)
{
    lua_State* L = luaVM.GetState();

    // Store world pointer in registry
    lua_pushlightuserdata(L, &world);
    lua_setfield(L, LUA_REGISTRYINDEX, "gg_world_ptr");
}

/*************************************************************************
  ENTITYNAME COMPONENT IMPLEMENTATION
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addEntityName(entity, name)
  \details
    Adds Name Component and updates global name map.
*************************************************************************/
int lua_ECS_AddEntityName(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);
    const char* name = luaL_checkstring(L, 2);

    // Check if name already exists
    if (g_entityNameMap.find(name) != g_entityNameMap.end())
    {
        luaL_error(L, "Entity name '%s' already exists", name);
        return 0;
    }

    // Add component
    NameComponent nameComp;
    strncpy(nameComp.name, name, sizeof(nameComp.name) - 1);
    nameComp.name[sizeof(nameComp.name) - 1] = '\0';

    world->AddComponent<NameComponent>(entity, nameComp);

    // Update global map
    g_entityNameMap[name] = entity;

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.getEntityName(entity) -> string
  \details
    Returns entity name or nil if not named.
*************************************************************************/
int lua_ECS_GetEntityName(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<NameComponent>(entity))
    {
        lua_pushnil(L);
        return 1;
    }

    NameComponent* nameComp = world->GetComponent<NameComponent>(entity);
    lua_pushstring(L, nameComp->name);

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setEntityName(entity, name)
  \details
    Changes existing entity name, updates global map.
*************************************************************************/
int lua_ECS_SetEntityName(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);
    const char* newName = luaL_checkstring(L, 2);

    if (!world->HasComponent<NameComponent>(entity))
    {
        luaL_error(L, "Entity does not have EntityName component");
        return 0;
    }

    NameComponent* nameComp = world->GetComponent<NameComponent>(entity);

    // Remove old name from map
    auto it = g_entityNameMap.find(nameComp->name);
    if (it != g_entityNameMap.end() && it->second.id == entity.id)
    {
        g_entityNameMap.erase(it);
    }

    // Check new name doesn't exist
    if (g_entityNameMap.find(newName) != g_entityNameMap.end())
    {
        luaL_error(L, "Entity name '%s' already exists", newName);
        return 0;
    }

    // Update component
    strncpy(nameComp->name, newName, sizeof(nameComp->name) - 1);
    nameComp->name[sizeof(nameComp->name) - 1] = '\0';

    // Update map
    g_entityNameMap[newName] = entity;

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.findEntityByName(name) -> entity or nil
  \details
    Fast lookup using global name map.
*************************************************************************/
int lua_ECS_FindEntityByName(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    const char* name = luaL_checkstring(L, 1);

    auto it = g_entityNameMap.find(name);
    if (it == g_entityNameMap.end())
    {
        lua_pushnil(L);
        return 1;
    }

    Entity entity = it->second;

    // Validate entity still exists
    if (!world->IsEntityValidHandle(entity))
    {
        // Clean up stale entry
        g_entityNameMap.erase(it);
        lua_pushnil(L);
        return 1;
    }

    // Pack and return entity
    uint64_t packed = PackEntity(entity);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));

    return 1;
}

/*************************************************************************
  AABB COMPONENT IMPLEMENTATION
  Uses top/bottom/left/right as per actual AABBFloat structure
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addAABB(entity, halfWidth, halfHeight)
  \details
    Adds AABBFloat component. Converts halfWidth/halfHeight to bounds.
    Assumes entity position is at (0,0) - use with Transform2DFloat.
*************************************************************************/
int lua_ECS_AddAABB(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    float halfWidth = static_cast<float>(luaL_checknumber(L, 2));
    float halfHeight = static_cast<float>(luaL_checknumber(L, 3));

    AABBFloat aabb;
    // Convert half-extents to bounds (assuming center at origin)
    aabb.left = -halfWidth;
    aabb.right = halfWidth;
    aabb.bottom = -halfHeight;
    aabb.top = halfHeight;

    world->AddComponent<AABBFloat>(entity, aabb);

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.getAABB(entity) -> {left, right, top, bottom}
  \details
    Returns AABB bounds as table.
*************************************************************************/
int lua_ECS_GetAABB(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<AABBFloat>(entity))
    {
        luaL_error(L, "Entity does not have AABBFloat component");
        return 0;
    }

    AABBFloat* aabb = world->GetComponent<AABBFloat>(entity);

    lua_newtable(L);

    lua_pushnumber(L, aabb->left);
    lua_setfield(L, -2, "left");

    lua_pushnumber(L, aabb->right);
    lua_setfield(L, -2, "right");

    lua_pushnumber(L, aabb->top);
    lua_setfield(L, -2, "top");

    lua_pushnumber(L, aabb->bottom);
    lua_setfield(L, -2, "bottom");

    // Also provide halfWidth/halfHeight for convenience
    float halfWidth = (aabb->right - aabb->left) * 0.5f;
    float halfHeight = (aabb->top - aabb->bottom) * 0.5f;

    lua_pushnumber(L, halfWidth);
    lua_setfield(L, -2, "halfWidth");

    lua_pushnumber(L, halfHeight);
    lua_setfield(L, -2, "halfHeight");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setAABB(entity, table)
  \details
    Updates AABB bounds from table. Can use either bounds or half-extents.
*************************************************************************/
int lua_ECS_SetAABB(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for AABB parameters");
        return 0;
    }

    if (!world->HasComponent<AABBFloat>(entity))
    {
        luaL_error(L, "Entity does not have AABBFloat component");
        return 0;
    }

    AABBFloat* aabb = world->GetComponent<AABBFloat>(entity);

    // Check for direct bounds
    lua_getfield(L, 2, "left");
    if (!lua_isnil(L, -1))
    {
        aabb->left = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "right");
    if (!lua_isnil(L, -1))
    {
        aabb->right = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "top");
    if (!lua_isnil(L, -1))
    {
        aabb->top = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "bottom");
    if (!lua_isnil(L, -1))
    {
        aabb->bottom = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // Alternative: use halfWidth/halfHeight if provided
    lua_getfield(L, 2, "halfWidth");
    lua_getfield(L, 2, "halfHeight");
    if (!lua_isnil(L, -2) && !lua_isnil(L, -1))
    {
        float hw = static_cast<float>(lua_tonumber(L, -2));
        float hh = static_cast<float>(lua_tonumber(L, -1));
        float cx = (aabb->left + aabb->right) * 0.5f;
        float cy = (aabb->bottom + aabb->top) * 0.5f;
        aabb->left = cx - hw;
        aabb->right = cx + hw;
        aabb->bottom = cy - hh;
        aabb->top = cy + hh;
    }
    lua_pop(L, 2);

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.checkAABBOverlap(entity1, entity2) -> boolean
  \details
    Tests AABB-AABB collision between two entities.
*************************************************************************/
int lua_ECS_CheckAABBOverlap(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity1 = GetValidatedEntityHandle(L, 1, *world);
    Entity entity2 = GetValidatedEntityHandle(L, 2, *world);

    // Both entities need Transform and AABB
    if (!world->HasComponent<WorldTransform>(entity1) ||
        !world->HasComponent<AABBFloat>(entity1))
    {
        luaL_error(L, "Entity 1 missing Transform2DFloat or AABBFloat");
        return 0;
    }

    if (!world->HasComponent<WorldTransform>(entity2) ||
        !world->HasComponent<AABBFloat>(entity2))
    {
        luaL_error(L, "Entity 2 missing Transform2DFloat or AABBFloat");
        return 0;
    }

    WorldTransform* t1 = world->GetComponent<WorldTransform>(entity1);
    WorldTransform* t2 = world->GetComponent<WorldTransform>(entity2);
    AABBFloat* aabb1 = world->GetComponent<AABBFloat>(entity1);
    AABBFloat* aabb2 = world->GetComponent<AABBFloat>(entity2);

    // Calculate world space bounds (add transform position to local AABB)
    float left1 = t1->matrix.getTranslation().x + aabb1->left;
    float right1 = t1->matrix.getTranslation().x + aabb1->right;
    float bottom1 = t1->matrix.getTranslation().y + aabb1->bottom;
    float top1 = t1->matrix.getTranslation().y + aabb1->top;

    float left2 = t2->matrix.getTranslation().x + aabb2->left;
    float right2 = t2->matrix.getTranslation().x + aabb2->right;
    float bottom2 = t2->matrix.getTranslation().y + aabb2->bottom;
    float top2 = t2->matrix.getTranslation().y + aabb2->top;

    // Check overlap
    bool overlap = !(left1 > right2 || right1 < left2 ||
        bottom1 > top2 || top1 < bottom2);

    lua_pushboolean(L, overlap);
    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.getWorldPosition(entity) -> {x, y} or nil
  \details
    Returns the world position of an entity as a table with x and y fields.
    Returns nil if the entity does not have a WorldTransform component.
*************************************************************************/
int lua_ECS_GetWorldPosition(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<WorldTransform>(entity))
    {
        lua_pushnil(L);
        return 1;
    }

    WorldTransform* worldTransform = world->GetComponent<WorldTransform>(entity);
    GGMath::Vector2 position = worldTransform->matrix.getTranslation();

    lua_newtable(L);
    lua_pushnumber(L, position.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, position.y);
    lua_setfield(L, -2, "y");

    return 1;
}

/*************************************************************************
  HIERARCHY FUNCTIONS IMPLEMENTATION
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.setParent(parent, child)
  \details
    Sets the parent of the child entity in the hierarchy.
*************************************************************************/
int lua_ECS_SetParent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity parent = GetValidatedEntityHandle(L, 1, *world);
    Entity child = GetValidatedEntityHandle(L, 2, *world);

    if (!world->HasComponent<LocalTransform>(parent))
    {
        luaL_error(L, "Parent entity missing LocalTransform");
        return 0;
    }

    if (!world->HasComponent<LocalTransform>(child))
    {
        luaL_error(L, "Child entity missing LocalTransform");
        return 0;
    }

    auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();
    hierarchySystem.SetParent(*world, child, parent);

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.getChildren(entity) -> table
  \details
    Returns a table of entity handles for all direct children.
*************************************************************************/
int lua_ECS_GetChildren(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity parent = GetValidatedEntityHandle(L, 1, *world);

    auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();
    std::vector<Entity> children = hierarchySystem.GetChildren(*world, parent);

    lua_newtable(L);
    for (size_t i = 0; i < children.size(); ++i)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
        uint64_t packed = PackEntity(children[i]);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
        lua_settable(L, -3);
    }

    return 1;
}

/*************************************************************************
  MESHRENDERER COMPONENT IMPLEMENTATION
  Without color field as per actual MeshRenderer structure
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addMeshRenderer(entity, shaderID)
  \details
    Adds MeshRenderer component for sprite rendering.
*************************************************************************/
int lua_ECS_AddMeshRenderer(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int shaderID = static_cast<int>(luaL_checkinteger(L, 2));

    MeshRenderer renderer;
    renderer.shaderAssetID = shaderID;
    // No color field in the structure

    world->AddComponent<MeshRenderer>(entity, renderer);

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.getMeshRenderer(entity) -> table
  \details
    Returns MeshRenderer properties as table.
*************************************************************************/
int lua_ECS_GetMeshRenderer(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<MeshRenderer>(entity))
    {
        luaL_error(L, "Entity does not have MeshRenderer component");
        return 0;
    }

    MeshRenderer* renderer = world->GetComponent<MeshRenderer>(entity);

    lua_newtable(L);

    lua_pushinteger(L, renderer->shaderAssetID);
    lua_setfield(L, -2, "shaderAssetID");

    lua_pushstring(L, "colorAdd");
    lua_newtable(L);
    lua_pushstring(L, "r");
    lua_pushnumber(L, renderer->colorAdd.r);
    lua_settable(L, -3);
    lua_pushstring(L, "g");
    lua_pushnumber(L, renderer->colorAdd.g);
    lua_settable(L, -3);
    lua_pushstring(L, "b");
    lua_pushnumber(L, renderer->colorAdd.b);
    lua_settable(L, -3);
    lua_pushstring(L, "a");
    lua_pushnumber(L, renderer->colorAdd.a);
    lua_settable(L, -3);
    lua_settable(L, -3);

    lua_pushstring(L, "colorMultiply");
    lua_newtable(L);
    lua_pushstring(L, "r");
    lua_pushnumber(L, renderer->colorMultiply.r);
    lua_settable(L, -3);
    lua_pushstring(L, "g");
    lua_pushnumber(L, renderer->colorMultiply.g);
    lua_settable(L, -3);
    lua_pushstring(L, "b");
    lua_pushnumber(L, renderer->colorMultiply.b);
    lua_settable(L, -3);
    lua_pushstring(L, "a");
    lua_pushnumber(L, renderer->colorMultiply.a);
    lua_settable(L, -3);
    lua_settable(L, -3);

    lua_pushinteger(L, renderer->layer);
    lua_setfield(L, -2, "layer");

    lua_pushboolean(L, renderer->pickableObject);
    lua_setfield(L, -2, "pickableObject");

    lua_pushboolean(L, renderer->pixelPerfectPicking);
    lua_setfield(L, -2, "pixelPerfectPicking");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setMeshRenderer(entity, table)
  \details
    Updates MeshRenderer properties from table.
*************************************************************************/
int lua_ECS_SetMeshRenderer(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for MeshRenderer parameters");
        return 0;
    }

    if (!world->HasComponent<MeshRenderer>(entity))
    {
        luaL_error(L, "Entity does not have MeshRenderer component");
        return 0;
    }

    MeshRenderer* renderer = world->GetComponent<MeshRenderer>(entity);

    // Get shaderAssetID
    lua_getfield(L, 2, "shaderAssetID");
    if (!lua_isnil(L, -1))
    {
        renderer->shaderAssetID = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "colorAdd");
    if (lua_istable(L, -1) && !lua_isnil(L, -1))
    {
        lua_getfield(L, -1, "r");
        renderer->colorAdd.r = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        renderer->colorAdd.g = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        renderer->colorAdd.b = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        renderer->colorAdd.a = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "colorMultiply");
    if (lua_istable(L, -1) && !lua_isnil(L, -1))
    {
        lua_getfield(L, -1, "r");
        renderer->colorMultiply.r = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        renderer->colorMultiply.g = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        renderer->colorMultiply.b = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        renderer->colorMultiply.a = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "layer");
    if (!lua_isnil(L, -1))
    {
        renderer->layer = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "perspectiveFactor");
    if (!lua_isnil(L, -1))
    {
        renderer->perspectiveFactor = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "pickableObject");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isboolean(L, -1))
        {
            luaL_error(L, "MeshRenderer.pickableObject must be a bool");
        }
        renderer->pickableObject = static_cast<bool>(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "pixelPerfectPicking");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isboolean(L, -1))
        {
            luaL_error(L, "MeshRenderer.pixelPerfectPicking must be a bool");
        }
        renderer->pixelPerfectPicking = static_cast<bool>(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    return 0;
}

/*************************************************************************
  MESHRENDERER COMPONENT IMPLEMENTATION
  Without color field as per actual MeshRenderer structure
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addMeshRenderer(entity, shaderID)
  \details
    Adds MeshRenderer component for sprite rendering.
*************************************************************************/
int lua_ECS_AddUIComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int shaderID = static_cast<int>(luaL_checkinteger(L, 2));

    UIComponent renderer;
    renderer.shaderAssetID = shaderID;

    world->AddComponent<UIComponent>(entity, renderer);

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.getMeshRenderer(entity) -> table
  \details
    Returns MeshRenderer properties as table.
*************************************************************************/
int lua_ECS_GetUIComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<UIComponent>(entity))
    {
        luaL_error(L, "Entity does not have UIComponent component");
        return 0;
    }

    UIComponent* renderer = world->GetComponent<UIComponent>(entity);

    lua_newtable(L);

    lua_pushinteger(L, renderer->shaderAssetID);
    lua_setfield(L, -2, "shaderAssetID");

    lua_pushinteger(L, renderer->layer);
    lua_setfield(L, -2, "layer");

    lua_pushstring(L, "colorAdd");
    lua_newtable(L);
    lua_pushstring(L, "r");
    lua_pushnumber(L, renderer->colorAdd.r);
    lua_settable(L, -3);
    lua_pushstring(L, "g");
    lua_pushnumber(L, renderer->colorAdd.g);
    lua_settable(L, -3);
    lua_pushstring(L, "b");
    lua_pushnumber(L, renderer->colorAdd.b);
    lua_settable(L, -3);
    lua_pushstring(L, "a");
    lua_pushnumber(L, renderer->colorAdd.a);
    lua_settable(L, -3);
    lua_settable(L, -3);

    lua_pushstring(L, "colorMultiply");
    lua_newtable(L);
    lua_pushstring(L, "r");
    lua_pushnumber(L, renderer->colorMultiply.r);
    lua_settable(L, -3);
    lua_pushstring(L, "g");
    lua_pushnumber(L, renderer->colorMultiply.g);
    lua_settable(L, -3);
    lua_pushstring(L, "b");
    lua_pushnumber(L, renderer->colorMultiply.b);
    lua_settable(L, -3);
    lua_pushstring(L, "a");
    lua_pushnumber(L, renderer->colorMultiply.a);
    lua_settable(L, -3);
    lua_settable(L, -3);

    
    lua_pushboolean(L, renderer->pickableObject);
    lua_setfield(L, -2, "pickableObject");

    lua_pushboolean(L, renderer->pixelPerfectPicking);
    lua_setfield(L, -2, "pixelPerfectPicking");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setMeshRenderer(entity, table)
  \details
    Updates MeshRenderer properties from table.
*************************************************************************/
int lua_ECS_SetUIComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for UIComponent parameters");
        return 0;
    }

    if (!world->HasComponent<UIComponent>(entity))
    {
        luaL_error(L, "Entity does not have UIComponent component");
        return 0;
    }

    UIComponent* renderer = world->GetComponent<UIComponent>(entity);

    // Get shaderAssetID
    lua_getfield(L, 2, "shaderAssetID");
    if (!lua_isnil(L, -1))
    {
        renderer->shaderAssetID = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "layer");
    if (!lua_isnil(L, -1))
    {
        renderer->layer = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "colorAdd");
    if (lua_istable(L, -1) && !lua_isnil(L, -1))
    {
        lua_getfield(L, -1, "r");
        renderer->colorAdd.r = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        renderer->colorAdd.g = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        renderer->colorAdd.b = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        renderer->colorAdd.a = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "colorMultiply");
    if (lua_istable(L, -1) && !lua_isnil(L, -1))
    {
        lua_getfield(L, -1, "r");
        renderer->colorMultiply.r = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        renderer->colorMultiply.g = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        renderer->colorMultiply.b = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        renderer->colorMultiply.a = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "pickableObject");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isboolean(L, -1))
        {
            luaL_error(L, "UIComponent.pickableObject must be a bool");
        }
        renderer->pickableObject = static_cast<bool>(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "pixelPerfectPicking");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isboolean(L, -1))
        {
            luaL_error(L, "UIComponent.pixelPerfectPicking must be a bool");
        }
        renderer->pixelPerfectPicking = static_cast<bool>(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);


    return 0;
}

int lua_ECS_AddSprite(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int textureID = static_cast<int>(luaL_checkinteger(L, 2));

    if (textureID < 0)
    {
        return luaL_error(L, "Invalid texture ID: %d (must be non-negative)", textureID);
    }

    Sprite renderer;
    renderer.textureAssetID = static_cast<uint32_t>(textureID);

    world->AddComponent<Sprite>(entity, renderer);

    return 0;
}

int lua_ECS_GetSprite(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<Sprite>(entity))
    {
        luaL_error(L, "Entity does not have Sprite component");
        return 0;
    }

    Sprite* renderer = world->GetComponent<Sprite>(entity);

    lua_newtable(L);

    lua_pushinteger(L, renderer->textureAssetID);
    lua_setfield(L, -2, "textureAssetID");

    return 1;
}

int lua_ECS_SetSprite(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for Sprite parameters");
        return 0;
    }

    if (!world->HasComponent<Sprite>(entity))
    {
        luaL_error(L, "Entity does not have Sprite component");
        return 0;
    }

    Sprite* renderer = world->GetComponent<Sprite>(entity);

    // Get shaderAssetID
    lua_getfield(L, 2, "textureAssetID");
    if (!lua_isnil(L, -1))
    {
        int id = static_cast<int>(lua_tointeger(L, -1));
        if (id < 0)
        {
            return luaL_error(L, "Invalid texture ID: %d (must be non-negative)", id);
        }

        renderer->textureAssetID = static_cast<uint32_t>(id);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "uvBounds");

    if (lua_istable(L, -1) && !lua_isnil(L, -1))
    {
        lua_getfield(L, -1, "sMin");
        renderer->textureUVBounds.x = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "tMin");
        renderer->textureUVBounds.y = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "sMax");
        renderer->textureUVBounds.z = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "tMax");
        renderer->textureUVBounds.w = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return 0;
}

/*************************************************************************
  GRID COMPONENT IMPLEMENTATION
  Without color field as per actual MeshRenderer structure
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addGridComponent(entity, gridSizeX, gridSizeY)
  \details
    Adds Grid Component to the entity.
*************************************************************************/
int lua_ECS_AddGridComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int gridSizeX = static_cast<int>(luaL_optinteger(L, 2, 0));
    int gridSizeY = static_cast<int>(luaL_optinteger(L, 3, 0));

    ECSGridComponent grid;
    grid.gridSizeX = gridSizeX;
    grid.gridSizeY = gridSizeY;

    world->AddComponent<ECSGridComponent>(entity, grid);

    ECSGridComponent* gridPtr = world->GetComponent<ECSGridComponent>(entity);

    gridPtr->gridID = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().GetGridManager()->CreateGrid(true);

    return 0;
}

/*************************************************************************
  \brief
    Lua: ECS.getGridComponent(entity) -> table
  \details
    Returns Grid Component properties as table.
*************************************************************************/
int lua_ECS_GetGridComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<ECSGridComponent>(entity))
    {
        luaL_error(L, "Entity does not have Grid component");
        return 0;
    }

    ECSGridComponent* grid = world->GetComponent<ECSGridComponent>(entity);

    lua_newtable(L);

    lua_pushinteger(L, grid->gridSizeX);
    lua_setfield(L, -2, "gridSizeX");

    lua_pushinteger(L, grid->gridSizeY);
    lua_setfield(L, -2, "gridSizeY");

    lua_pushinteger(L, grid->gridID);
    lua_setfield(L, -2, "gridID");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setGridComponent(entity, table)
  \details
    Updates Grid Component properties from table.
*************************************************************************/
int lua_ECS_SetGridComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for MeshRenderer parameters");
        return 0;
    }

    if (!world->HasComponent<ECSGridComponent>(entity))
    {
        luaL_error(L, "Entity does not have Grid component");
        return 0;
    }

    ECSGridComponent* grid = world->GetComponent<ECSGridComponent>(entity);

    // Get gridSizeX
    lua_getfield(L, 2, "gridSizeX");
    if (!lua_isnil(L, -1))
    {
        grid->gridSizeX = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Get gridSizeY
    lua_getfield(L, 2, "gridSizeY");
    if (!lua_isnil(L, -1))
    {
        grid->gridSizeY = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    return 0;
}

int lua_ECS_GetTilemapComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<TilemapComponent>(entity))
    {
        luaL_error(L, "Entity does not have Tilemap component");
        return 0;
    }

    TilemapComponent* tilemap = world->GetComponent<TilemapComponent>(entity);

    lua_newtable(L);

    lua_pushnumber(L, tilemap->perspectiveFactor);
    lua_setfield(L, -2, "perspectiveFactor");

    lua_pushinteger(L, tilemap->tilemapAssetID);
    lua_setfield(L, -2, "tilemapID");

    return 1;
}
/*************************************************************************
  TEXTCOMPONENT IMPLEMENTATION
  Text rendering with dynamic content and styling
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addTextComponent(entity, text, fontAssetID, fontSize)
  \param[in] entity
    Entity handle to attach TextComponent to.
  \param[in] text
    Text string to display (max 255 characters).
  \param[in] fontAssetID
    Font asset ID (integer) - required.
  \param[in] fontSize
    Font size in pixels (optional, default: 48).
  \details
    Adds TextComponent with specified text, font, and size. Initializes
    with white color, LEFT alignment, BASELINE vertical alignment, and
    visible=true. Use getTextComponent/setTextComponent to modify other
    properties after creation.
  \par Lua Usage
  \code
    -- Create text with specific font
    local entity = ECS.createEntity()
    ECS.addTextComponent(entity, "Hello World", 5, 48)

    -- With custom font size
    ECS.addTextComponent(entity, "Large Text", 7, 72)

    -- Using default font size (48)
    ECS.addTextComponent(entity, "Default Size", 5)
  \endcode
*************************************************************************/
int lua_ECS_AddTextComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    const char* text = luaL_checkstring(L, 2);
    uint32_t fontAssetID = static_cast<uint32_t>(luaL_checkinteger(L, 3));

    // Optional fontSize (default 48)
    unsigned int fontSize = 48;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4))
    {
        fontSize = static_cast<unsigned int>(luaL_checkinteger(L, 4));
    }

    TextComponent textComp;
    strncpy(textComp.text, text, sizeof(textComp.text) - 1);
    textComp.text[sizeof(textComp.text) - 1] = '\0';
    textComp.fontAssetID = fontAssetID;
    textComp.fontSize = fontSize;
    textComp.color = glm::vec4(1.0f, 1.0f, 1.0f,1.0f); // Default white
    textComp.layer = 100;
    textComp.ySort = false;
    textComp.visible = true;
    textComp.alignment = static_cast<uint32_t>(TextComponent::Alignment::LEFT);
    textComp.verticalAlignment = static_cast<uint32_t>(TextComponent::VerticalAlignment::BASELINE);
    textComp.maxWidth = 0.0f;

    world->AddComponent<TextComponent>(entity, textComp);

    return 0;
}


/*************************************************************************
  \brief
    Lua: ECS.getTextComponent(entity) -> table
  \param[in] entity
    Entity handle with TextComponent.
  \return
    Table with TextComponent properties.
  \details
    Returns TextComponent properties as a Lua table. Use this to read
    current text properties or to prepare modifications before calling
    setTextComponent.
  \par Return Table Fields
    - text (string): Current text content
    - fontAssetID (integer): Font asset ID
    - shaderAssetID (integer): Shader asset ID
    - fontSize (integer): Font size in pixels
    - color (table): Color {r, g, b} with values 0.0-1.0
    - layer (integer): Rendering layer
    - ySort (boolean): Y-axis sorting enabled
    - visible (boolean): Visibility flag
    - alignment (integer): Horizontal alignment (0=LEFT, 1=CENTER, 2=RIGHT)
    - verticalAlignment (integer): Vertical alignment (0=TOP, 1=MIDDLE, 2=BOTTOM, 3=BASELINE)
    - maxWidth (number): Maximum width for text wrapping (0=no wrap)
  \par Lua Usage
  \code
    -- Get text component
    local textData = ECS.getTextComponent(entity)
    print("Text: " .. textData.text)
    print("Font ID: " .. textData.fontAssetID)
    print("Font Size: " .. textData.fontSize)
    print("Color: " .. textData.color.r .. ", " .. textData.color.g .. ", " .. textData.color.b)

    -- Check current font
    if textData.fontAssetID == 5 then
        print("Using default font")
    end
  \endcode
*************************************************************************/
int lua_ECS_GetTextComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<TextComponent>(entity))
    {
        luaL_error(L, "Entity does not have TextComponent");
        return 0;
    }

    TextComponent* textComp = world->GetComponent<TextComponent>(entity);

    lua_newtable(L);

    // Text string
    lua_pushstring(L, textComp->text);
    lua_setfield(L, -2, "text");


    // Font asset ID
    lua_pushinteger(L, textComp->fontAssetID);
    lua_setfield(L, -2, "fontAssetID");

    // Shader asset ID
    lua_pushinteger(L, textComp->shaderAssetID);
    lua_setfield(L, -2, "shaderAssetID");
    // Font size
    lua_pushinteger(L, textComp->fontSize);
    lua_setfield(L, -2, "fontSize");

    // Color as nested table
    lua_newtable(L);
    lua_pushnumber(L, textComp->color.r);
    lua_setfield(L, -2, "r");
    lua_pushnumber(L, textComp->color.g);
    lua_setfield(L, -2, "g");
    lua_pushnumber(L, textComp->color.b);
    lua_setfield(L, -2, "b");
    lua_pushnumber(L, textComp->color.a);
    lua_setfield(L, -2, "a");
    lua_setfield(L, -2, "color");

    // Layer
    lua_pushinteger(L, textComp->layer);
    lua_setfield(L, -2, "layer");

    // Y-sort
    lua_pushboolean(L, textComp->ySort);
    lua_setfield(L, -2, "ySort");

    // Visible
    lua_pushboolean(L, textComp->visible);
    lua_setfield(L, -2, "visible");

    // Alignment
    lua_pushinteger(L, textComp->alignment);
    lua_setfield(L, -2, "alignment");

    // Vertical alignment
    lua_pushinteger(L, textComp->verticalAlignment);
    lua_setfield(L, -2, "verticalAlignment");

    // Max width
    lua_pushnumber(L, textComp->maxWidth);
    lua_setfield(L, -2, "maxWidth");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setTextComponent(entity, table)
  \param[in] entity
    Entity handle with TextComponent.
  \param[in] table
    Table with optional TextComponent properties to update.
  \details
    Updates TextComponent properties from a Lua table. Only fields present
    in the table are modified - omitted fields retain their current values.
    This enables both full updates and partial property changes.
  \par Supported Table Fields (all optional)
    - text (string): Text content
    - fontAssetID (integer): Font asset ID for dynamic font switching
    - shaderAssetID (integer): Shader asset ID
    - fontSize (integer): Font size in pixels
    - color (table): Color {r, g, b} with values 0.0-1.0
    - layer (integer): Rendering layer
    - ySort (boolean): Y-axis sorting enabled
    - visible (boolean): Visibility flag
    - alignment (integer): Horizontal alignment (0=LEFT, 1=CENTER, 2=RIGHT)
    - verticalAlignment (integer): Vertical alignment (0=TOP, 1=MIDDLE, 2=BOTTOM, 3=BASELINE)
    - maxWidth (number): Maximum width for text wrapping
  \par Lua Usage
  \code
    -- Full update: Get, modify, set
    local textData = ECS.getTextComponent(entity)
    textData.text = "Updated Text"
    textData.fontAssetID = 7
    textData.fontSize = 64
    textData.color = { r = 1.0, g = 0.0, b = 0.0 }
    ECS.setTextComponent(entity, textData)

    -- Partial update: Only change specific fields
    ECS.setTextComponent(entity, {
        fontAssetID = 10,
        fontSize = 48
    })

    -- Dynamic font switching
    if useSpecialFont then
        ECS.setTextComponent(entity, { fontAssetID = specialFontID })
    end

    -- Change color only
    ECS.setTextComponent(entity, {
        color = { r = 0.0, g = 1.0, b = 0.0 }
    })

    -- Hide text temporarily
    ECS.setTextComponent(entity, { visible = false })
  \endcode
*************************************************************************/
int lua_ECS_SetTextComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for TextComponent parameters");
        return 0;
    }

    if (!world->HasComponent<TextComponent>(entity))
    {
        luaL_error(L, "Entity does not have TextComponent");
        return 0;
    }

    TextComponent* textComp = world->GetComponent<TextComponent>(entity);

    // Update text string
    lua_getfield(L, 2, "text");
    if (!lua_isnil(L, -1))
    {
        const char* newText = lua_tostring(L, -1);
        if (newText)
        {
            strncpy(textComp->text, newText, sizeof(textComp->text) - 1);
            textComp->text[sizeof(textComp->text) - 1] = '\0';
        }
    }
    lua_pop(L, 1);

    // Update font asset ID
    lua_getfield(L, 2, "fontAssetID");
    if (!lua_isnil(L, -1))
    {
        textComp->fontAssetID = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update shader asset ID
    lua_getfield(L, 2, "shaderAssetID");
    if (!lua_isnil(L, -1))
    {
        textComp->shaderAssetID = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update font size
    lua_getfield(L, 2, "fontSize");
    if (!lua_isnil(L, -1))
    {
        textComp->fontSize = static_cast<unsigned int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update color
    lua_getfield(L, 2, "color");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "r");
        if (!lua_isnil(L, -1))
        {
            textComp->color.r = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        if (!lua_isnil(L, -1))
        {
            textComp->color.g = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        if (!lua_isnil(L, -1))
        {
            textComp->color.b = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        if (!lua_isnil(L, -1))
        {
            textComp->color.a = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    // Update layer
    lua_getfield(L, 2, "layer");
    if (!lua_isnil(L, -1))
    {
        textComp->layer = static_cast<int32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update ySort
    lua_getfield(L, 2, "ySort");
    if (!lua_isnil(L, -1))
    {
        textComp->ySort = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    // Update visible
    lua_getfield(L, 2, "visible");
    if (!lua_isnil(L, -1))
    {
        textComp->visible = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    // Update alignment
    lua_getfield(L, 2, "alignment");
    if (!lua_isnil(L, -1))
    {
        textComp->alignment = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update vertical alignment
    lua_getfield(L, 2, "verticalAlignment");
    if (!lua_isnil(L, -1))
    {
        textComp->verticalAlignment = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update max width
    lua_getfield(L, 2, "maxWidth");
    if (!lua_isnil(L, -1))
    {
        textComp->maxWidth = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    return 0;
}

/*************************************************************************
  WORLDTEXTCOMPONENT IMPLEMENTATION
  Text rendering with dynamic content and styling
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addWorldTextComponent(entity, text, fontAssetID, fontSize)
  \param[in] entity
    Entity handle to attach TextComponent to.
  \param[in] text
    Text string to display (max 255 characters).
  \param[in] fontAssetID
    Font asset ID (integer) - required.
  \param[in] fontSize
    Font size in pixels (optional, default: 48).
  \details
    Adds orldTextComponent with specified text, font, and size. Initializes
    with white color, LEFT alignment, BASELINE vertical alignment, and
    visible=true. Use getTextComponent/setTextComponent to modify other
    properties after creation.
  \par Lua Usage
  \code
    -- Create text with specific font
    local entity = ECS.createEntity()
    ECS.addTextComponent(entity, "Hello World", 5, 48)

    -- With custom font size
    ECS.addTextComponent(entity, "Large Text", 7, 72)

    -- Using default font size (48)
    ECS.addTextComponent(entity, "Default Size", 5)
  \endcode
*************************************************************************/
int lua_ECS_AddWorldTextComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    const char* text = luaL_checkstring(L, 2);
    uint32_t fontAssetID = static_cast<uint32_t>(luaL_checkinteger(L, 3));

    // Optional fontSize (default 48)
    unsigned int fontSize = 48;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4))
    {
        fontSize = static_cast<unsigned int>(luaL_checkinteger(L, 4));
    }

    WorldTextComponent textComp;
    strncpy(textComp.text, text, sizeof(textComp.text) - 1);
    textComp.text[sizeof(textComp.text) - 1] = '\0';
    textComp.fontAssetID = fontAssetID;
    textComp.fontSize = fontSize;
    textComp.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
    textComp.layer = 100;
    textComp.ySort = false;
    textComp.visible = true;
    textComp.alignment = static_cast<uint32_t>(WorldTextComponent::Alignment::LEFT);
    textComp.verticalAlignment = static_cast<uint32_t>(WorldTextComponent::VerticalAlignment::BASELINE);
    textComp.maxWidth = 0.0f;

    world->AddComponent< WorldTextComponent>(entity, textComp);

    return 0;
}


/*************************************************************************
  \brief
    Lua: ECS.getTextComponent(entity) -> table
  \param[in] entity
    Entity handle with TextComponent.
  \return
    Table with TextComponent properties.
  \details
    Returns TextComponent properties as a Lua table. Use this to read
    current text properties or to prepare modifications before calling
    setTextComponent.
  \par Return Table Fields
    - text (string): Current text content
    - fontAssetID (integer): Font asset ID
    - shaderAssetID (integer): Shader asset ID
    - fontSize (integer): Font size in pixels
    - color (table): Color {r, g, b} with values 0.0-1.0
    - layer (integer): Rendering layer
    - ySort (boolean): Y-axis sorting enabled
    - visible (boolean): Visibility flag
    - alignment (integer): Horizontal alignment (0=LEFT, 1=CENTER, 2=RIGHT)
    - verticalAlignment (integer): Vertical alignment (0=TOP, 1=MIDDLE, 2=BOTTOM, 3=BASELINE)
    - maxWidth (number): Maximum width for text wrapping (0=no wrap)
  \par Lua Usage
  \code
    -- Get text component
    local textData = ECS.getTextComponent(entity)
    print("Text: " .. textData.text)
    print("Font ID: " .. textData.fontAssetID)
    print("Font Size: " .. textData.fontSize)
    print("Color: " .. textData.color.r .. ", " .. textData.color.g .. ", " .. textData.color.b)

    -- Check current font
    if textData.fontAssetID == 5 then
        print("Using default font")
    end
  \endcode
*************************************************************************/
int lua_ECS_GetWorldTextComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent< WorldTextComponent>(entity))
    {
        luaL_error(L, "Entity does not have TextComponent");
        return 0;
    }

    WorldTextComponent* textComp = world->GetComponent<WorldTextComponent>(entity);

    lua_newtable(L);

    // Text string
    lua_pushstring(L, textComp->text);
    lua_setfield(L, -2, "text");


    // Font asset ID
    lua_pushinteger(L, textComp->fontAssetID);
    lua_setfield(L, -2, "fontAssetID");

    // Shader asset ID
    lua_pushinteger(L, textComp->shaderAssetID);
    lua_setfield(L, -2, "shaderAssetID");
    // Font size
    lua_pushinteger(L, textComp->fontSize);
    lua_setfield(L, -2, "fontSize");

    // Color as nested table
    lua_newtable(L);
    lua_pushnumber(L, textComp->color.r);
    lua_setfield(L, -2, "r");
    lua_pushnumber(L, textComp->color.g);
    lua_setfield(L, -2, "g");
    lua_pushnumber(L, textComp->color.b);
    lua_setfield(L, -2, "b");
    lua_pushnumber(L, textComp->color.a);
    lua_setfield(L, -2, "a");
    lua_setfield(L, -2, "color");

    // Layer
    lua_pushinteger(L, textComp->layer);
    lua_setfield(L, -2, "layer");

    // Y-sort
    lua_pushboolean(L, textComp->ySort);
    lua_setfield(L, -2, "ySort");

    // Visible
    lua_pushboolean(L, textComp->visible);
    lua_setfield(L, -2, "visible");

    // Alignment
    lua_pushinteger(L, textComp->alignment);
    lua_setfield(L, -2, "alignment");

    // Vertical alignment
    lua_pushinteger(L, textComp->verticalAlignment);
    lua_setfield(L, -2, "verticalAlignment");

    // Max width
    lua_pushnumber(L, textComp->maxWidth);
    lua_setfield(L, -2, "maxWidth");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setTextComponent(entity, table)
  \param[in] entity
    Entity handle with TextComponent.
  \param[in] table
    Table with optional TextComponent properties to update.
  \details
    Updates TextComponent properties from a Lua table. Only fields present
    in the table are modified - omitted fields retain their current values.
    This enables both full updates and partial property changes.
  \par Supported Table Fields (all optional)
    - text (string): Text content
    - fontAssetID (integer): Font asset ID for dynamic font switching
    - shaderAssetID (integer): Shader asset ID
    - fontSize (integer): Font size in pixels
    - color (table): Color {r, g, b} with values 0.0-1.0
    - layer (integer): Rendering layer
    - ySort (boolean): Y-axis sorting enabled
    - visible (boolean): Visibility flag
    - alignment (integer): Horizontal alignment (0=LEFT, 1=CENTER, 2=RIGHT)
    - verticalAlignment (integer): Vertical alignment (0=TOP, 1=MIDDLE, 2=BOTTOM, 3=BASELINE)
    - maxWidth (number): Maximum width for text wrapping
  \par Lua Usage
  \code
    -- Full update: Get, modify, set
    local textData = ECS.getTextComponent(entity)
    textData.text = "Updated Text"
    textData.fontAssetID = 7
    textData.fontSize = 64
    textData.color = { r = 1.0, g = 0.0, b = 0.0 }
    ECS.setTextComponent(entity, textData)

    -- Partial update: Only change specific fields
    ECS.setTextComponent(entity, {
        fontAssetID = 10,
        fontSize = 48
    })

    -- Dynamic font switching
    if useSpecialFont then
        ECS.setTextComponent(entity, { fontAssetID = specialFontID })
    end

    -- Change color only
    ECS.setTextComponent(entity, {
        color = { r = 0.0, g = 1.0, b = 0.0 }
    })

    -- Hide text temporarily
    ECS.setTextComponent(entity, { visible = false })
  \endcode
*************************************************************************/
int lua_ECS_SetWorldTextComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for TextComponent parameters");
        return 0;
    }

    if (!world->HasComponent<WorldTextComponent>(entity))
    {
        luaL_error(L, "Entity does not have TextComponent");
        return 0;
    }

    WorldTextComponent* textComp = world->GetComponent<WorldTextComponent>(entity);

    // Update text string
    lua_getfield(L, 2, "text");
    if (!lua_isnil(L, -1))
    {
        const char* newText = lua_tostring(L, -1);
        if (newText)
        {
            strncpy(textComp->text, newText, sizeof(textComp->text) - 1);
            textComp->text[sizeof(textComp->text) - 1] = '\0';
        }
    }
    lua_pop(L, 1);

    // Update font asset ID
    lua_getfield(L, 2, "fontAssetID");
    if (!lua_isnil(L, -1))
    {
        textComp->fontAssetID = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update shader asset ID
    lua_getfield(L, 2, "shaderAssetID");
    if (!lua_isnil(L, -1))
    {
        textComp->shaderAssetID = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update font size
    lua_getfield(L, 2, "fontSize");
    if (!lua_isnil(L, -1))
    {
        textComp->fontSize = static_cast<unsigned int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update color
    lua_getfield(L, 2, "color");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "r");
        if (!lua_isnil(L, -1))
        {
            textComp->color.r = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        if (!lua_isnil(L, -1))
        {
            textComp->color.g = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        if (!lua_isnil(L, -1))
        {
            textComp->color.b = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        if (!lua_isnil(L, -1))
        {
            textComp->color.a = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    // Update layer
    lua_getfield(L, 2, "layer");
    if (!lua_isnil(L, -1))
    {
        textComp->layer = static_cast<int32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update ySort
    lua_getfield(L, 2, "ySort");
    if (!lua_isnil(L, -1))
    {
        textComp->ySort = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    // Update visible
    lua_getfield(L, 2, "visible");
    if (!lua_isnil(L, -1))
    {
        textComp->visible = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    // Update alignment
    lua_getfield(L, 2, "alignment");
    if (!lua_isnil(L, -1))
    {
        textComp->alignment = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update vertical alignment
    lua_getfield(L, 2, "verticalAlignment");
    if (!lua_isnil(L, -1))
    {
        textComp->verticalAlignment = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);

    // Update max width
    lua_getfield(L, 2, "maxWidth");
    if (!lua_isnil(L, -1))
    {
        textComp->maxWidth = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    return 0;
}

/*************************************************************************
  AUDIOSOURCE IMPLEMENTATION
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.getAudioSource(entity)
    e.g.    local audioSrc = ECS.getAudioSource(entity)

            local volume = audioSrc.volume
            local pitch = audioSrc.pitch
  \details
    Returns AudioSource component current properties.
*************************************************************************/
int lua_ECS_GetAudioSource(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* audioSrc = world->GetComponent<AudioSource>(entity);

    lua_newtable(L);

    lua_pushboolean(L, audioSrc->isLooping);
    lua_setfield(L, -2, "isLooping");

    lua_pushboolean(L, audioSrc->isMuted);
    lua_setfield(L, -2, "isMuted");

    lua_pushnumber(L, audioSrc->volume);
    lua_setfield(L, -2, "volume");

    lua_pushnumber(L, audioSrc->pitch);
    lua_setfield(L, -2, "pitch");

    lua_pushnumber(L, audioSrc->stereoPan);
    lua_setfield(L, -2, "stereoPan");

    lua_pushboolean(L, audioSrc->isPlaying);
    lua_setfield(L, -2, "isPlaying");

    lua_pushboolean(L, audioSrc->isPaused);
    lua_setfield(L, -2, "isPaused");

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.setAudioSource(entity, table) 
  \details
    Sets AudioSource component properties.
*************************************************************************/
int lua_ECS_SetAudioSource(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for AudioSource parameters");
        return 0;
    }

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* audioSrc = world->GetComponent<AudioSource>(entity);

    lua_getfield(L, 2, "isLooping");
    if (!lua_isnil(L, -1))
    {   
        if (!lua_isboolean(L, -1)) 
        {
            luaL_error(L, "AudioSource.isLooping must be a bool");
        }

        audioSrc->isLooping = static_cast<bool>(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "isMuted");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isboolean(L, -1))
        {
            luaL_error(L, "AudioSource.isMuted must be a bool");
        }

        audioSrc->isMuted = static_cast<bool>(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "volume");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isnumber(L, -1))
        {
            luaL_error(L, "AudioSource.volume must be a number");
        }

        audioSrc->volume = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "pitch");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isnumber(L, -1))
        {
            luaL_error(L, "AudioSource.pitch must be a number");
        }

        audioSrc->pitch = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "stereoPan");
    if (!lua_isnil(L, -1))
    {
        if (!lua_isnumber(L, -1))
        {
            luaL_error(L, "AudioSource.stereoPan must be a number");
        }

        audioSrc->stereoPan = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    return 0;
}

/*************************************************************************
  QUERY SYSTEM IMPLEMENTATION
  Note: World may not have GetAllEntities() - using workaround
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.getEntitiesWithComponent(componentName) -> {entities}
  \details
    Returns all entities with specified component type.
*************************************************************************/
int lua_ECS_GetEntitiesWithComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    const char* componentName = luaL_checkstring(L, 1);

    lua_newtable(L);
    int index = 1;

    (void)world;  // TODO: Will be used when implemented
    (void)componentName;  // TODO: Will be used when implemented  
    (void)index;  // TODO: Will be used when implemented

    // For now, return empty table with a warning
    std::cout << "WARNING: getEntitiesWithComponent not fully implemented" << std::endl;
    std::cout << "         World class needs GetAllEntities() method" << std::endl;

    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.getEntityCount() -> number
  \details
    Returns total number of active entities.
*************************************************************************/
int lua_ECS_GetEntityCount(lua_State* L)
{
    // World may not have GetEntityCount() 
    // Return placeholder value
    lua_pushinteger(L, 0);
    std::cout << "WARNING: getEntityCount not implemented - World needs GetEntityCount()" << std::endl;
    return 1;
}

/*************************************************************************
  \brief
    Lua: ECS.getAllEntities() -> {entities}
  \details
    Returns array of all active entities.
*************************************************************************/
int lua_ECS_GetAllEntities(lua_State* L)
{
    // Return empty table as World may not have GetAllEntities()
    lua_newtable(L);
    std::cout << "WARNING: getAllEntities not implemented - World needs GetAllEntities()" << std::endl;
    return 1;
}

/*************************************************************************
  AUDIO SYSTEM IMPLEMENTATION
*************************************************************************/

// Track audio handles for cleanup
static uint32_t g_nextAudioHandle = 1;
static std::unordered_map<uint32_t, void*> g_audioHandles;

/*************************************************************************
  NEW AUDIO SYSTEM IMPLEMENTATION (USING AUDIOSOURCE COMPONENT)
*************************************************************************/

static AudioAssetConfig ApplyAudioAssetConfig(World* world, Entity entity, int audioAssetID)
{
    AudioAssetConfig defaultCfg{};

    if (!world->HasComponent<ScriptComponent>(entity)) { return defaultCfg; }

    ScriptComponent* script = world->GetComponent<ScriptComponent>(entity);
    if (!script || script->scriptAssetID == 0) { return defaultCfg; }

    auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
    EntityInstance* instance = scriptSystem.GetScriptInstance(script->scriptInstance);
    if (!instance || !instance->type) { return defaultCfg; }

    GGScriptMetadata* meta = instance->type;

    for (const auto& fieldName : meta->audioAssetFields)
    {
        auto fieldIt = meta->fieldNameToIndex.find(fieldName);
        if (fieldIt == meta->fieldNameToIndex.end()) { continue; }

        const FieldDescriptor& fd = meta->fieldDescriptors[fieldIt->second];
        char* dataPtr = static_cast<char*>(instance->data);
        int storedAssetID = *static_cast<int*>(static_cast<void*>(dataPtr + fd.offset));

        if (storedAssetID != audioAssetID) { continue; }

        auto cfgIt = meta->audioAssetConfigs.find(fieldName);
        if (cfgIt == meta->audioAssetConfigs.end()) { continue; }

        return cfgIt->second;
    }

    return defaultCfg;
}

int lua_Audio_TriggerSound(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int audioAssetID = static_cast<int>(luaL_checkinteger(L, 2));

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* source = world->GetComponent<AudioSource>(entity);

    const AudioAssetConfig cfg = ApplyAudioAssetConfig(world, entity, audioAssetID);

    source->configVolume = cfg.volume;
    source->isLooping = cfg.loop;
    source->pitch = cfg.pitchVariance;

    // play sound without stopping the existing channel
    if (cfg.oneShot)
    {
        source->audioAssetID = audioAssetID;
        source->justTriggered = true;
    }

    // stop existing channel before triggering new sound
    if (source->runtimeChannel)
    {
        bool isPlaying = false;
        source->runtimeChannel->isPlaying(&isPlaying);
        if (isPlaying)
        {
            source->runtimeChannel->stop();
        }
        source->runtimeChannel = nullptr;
    }

    source->audioAssetID = audioAssetID;
    source->justTriggered = true; // play the sound

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.triggerSoundOneShot(entity, audioAssetID)
  \details
    Does not cancel out the previous sound being played
*************************************************************************/
int lua_Audio_TriggerSoundOneShot(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int audioAssetID = static_cast<int>(luaL_checkinteger(L, 2));

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* source = world->GetComponent<AudioSource>(entity);

    const AudioAssetConfig cfg = ApplyAudioAssetConfig(world, entity, audioAssetID);

    // --- Play immediately via FMOD, bypassing the single-slot AudioSource ---
    GGFMODWrapper* audioMgr = GGEngine::GetEngine().GetAudioSystem();
    if (!audioMgr)
    {
        luaL_error(L, "Audio system not initialized");
        return 0;
    }

    if (!GGECSStandardAssetManager<GGAudioAsset>::HasAsset(audioAssetID))
    {
        return 0;
    }

    const GGAudioAsset& asset = GGECSStandardAssetManager<GGAudioAsset>::GetAsset(audioAssetID);
    FMOD::Sound* sound = asset.GetFMODSound();

    if (!sound)
    {
        const std::string& path = asset.GetFilePath();
        if (path.empty()) { return 0; }
        sound = audioMgr->CreateSound(path);
        if (!sound) { return 0; }
    }

    FMOD::Channel* channel = nullptr;
    AudioResult result = audioMgr->PlayAudio(sound, &channel, false);
    if (result != AudioResult::SUCCESS || !channel)
    {
        return 0;
    }

    // Apply volume: source volume * per-asset config volume
    float finalVolume = source->volume * cfg.volume;

    // Route to the correct audio bus
    FMOD::ChannelGroup* busGroup = audioMgr->GetChannelGroupFromBus(static_cast<AudioBusID>(source->outputBus));
    if (busGroup)
    {
        channel->setChannelGroup(busGroup);
    }

    audioMgr->SetChannelVolume(channel, finalVolume);
    audioMgr->SetChannelPitch(channel, cfg.pitchVariance > 0.f ? cfg.pitchVariance : source->pitch);
    audioMgr->SetChannelPan(channel, source->stereoPan);
    audioMgr->SetChannelLoop(channel, false);

    // Do NOT touch source->audioAssetID, justTriggered, or runtimeChannel.
    // The channel is fire-and-forget: FMOD will clean it up when it finishes.

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.triggerMusic(entity, audioAssetID, isLooping)
*************************************************************************/
int lua_Audio_TriggerMusic(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    int audioAssetID = static_cast<int>(luaL_checkinteger(L, 2));

    if (!lua_isboolean(L, 3))
    {
        luaL_error(L, "Expected boolean for looping");
    }

    bool looping = lua_toboolean(L, 3);

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have an AudioSource component");
        return 0;
    }

    AudioSource* source = world->GetComponent<AudioSource>(entity);

    const AudioAssetConfig cfg = ApplyAudioAssetConfig(world, entity, audioAssetID);

    source->configVolume = cfg.volume;
    source->isLooping = cfg.loop;
    source->pitch = cfg.pitchVariance;

    // play sound without stopping the existing channel
    if (cfg.oneShot)
    {
        source->audioAssetID = audioAssetID;
        source->justTriggered = true;
    }

    // stop existing channel before triggering new sound
    if (source->runtimeChannel)
    {
        bool isPlaying = false;
        source->runtimeChannel->isPlaying(&isPlaying);
        if (isPlaying)
        {
            source->runtimeChannel->stop();
        }
        source->runtimeChannel = nullptr;
    }

    source->audioAssetID = audioAssetID;
    source->justTriggered = true; // play the sound

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.setAudioSourceVolume(entity)
*************************************************************************/
int lua_Audio_GetAudioSourceVolume(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
    }

    AudioSource* audio = world->GetComponent<AudioSource>(entity);
    float volume = audio->volume;

    lua_pushnumber(L, volume);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Audio.setAudioSourceVolume(entity, volumeValue)
*************************************************************************/
int lua_Audio_SetAudioSourceVolume(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    float newVolume = static_cast<float>(luaL_checknumber(L, 2));

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* audio = world->GetComponent<AudioSource>(entity);
    
    audio->volume = newVolume;

    std::cout << "[Audio] AudioSource volume set to: " << audio->volume << std::endl;
    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.getAudioSourceStereoPan(entity)
*************************************************************************/
int lua_Audio_GetAudioSourceStereoPan(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
    }

    AudioSource* audio = world->GetComponent<AudioSource>(entity);
    float stereoPan = audio->stereoPan;

    lua_pushnumber(L, stereoPan);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Audio.setAudioSourceStereoPan(entity, stereoPanValue)
*************************************************************************/
int lua_Audio_SetAudioSourceStereoPan(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    float newStereoPan = static_cast<float>(luaL_checknumber(L, 2));

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* audio = world->GetComponent<AudioSource>(entity);

    audio->stereoPan = newStereoPan;

    std::cout << "[Audio] AudioSource stereoPan value set to: " << audio->stereoPan << std::endl;
    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.getAudioSourcePitch(entity)
*************************************************************************/
int lua_Audio_GetAudioSourcePitch(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
    }

    AudioSource* audio = world->GetComponent<AudioSource>(entity);
    float pitch = audio->pitch;

    lua_pushnumber(L, pitch);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Audio.setAudioSourcePitch(entity, pitchValue)
*************************************************************************/
int lua_Audio_SetAudioSourcePitch(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    float newPitch = static_cast<float>(luaL_checknumber(L, 2));

    if (!world->HasComponent<AudioSource>(entity))
    {
        luaL_error(L, "Entity does not have AudioSource component");
        return 0;
    }

    AudioSource* audio = world->GetComponent<AudioSource>(entity);

    audio->pitch = newPitch;

    std::cout << "[Audio] AudioSource pitch set to: " << audio->pitch << std::endl;
    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.getAudioConfig()
  \details
    For session persistent audio settings
*************************************************************************/
int lua_Audio_GetAudioConfig(lua_State* L)
{
    lua_newtable(L);

    using namespace GGEngineConfig::GGEngineAudio;

    lua_pushnumber(L, MasterVolume());
    lua_setfield(L, -2, "masterVolume");

    lua_pushnumber(L, BGMVolume());
    lua_setfield(L, -2, "bgmVolume");

    lua_pushnumber(L, SFXVolume());
    lua_setfield(L, -2, "sfxVolume");

    return 1;
}

/*************************************************************************
  \brief
    Lua: Audio.setAudioConfig()
  \details
    For session persistent audio settings
*************************************************************************/
int lua_Audio_SetAudioConfig(lua_State* L)
{
    if (!lua_istable(L, 1))
    {
        luaL_error(L, "Expected table for AudioConfig parameters");
    }

    using namespace GGEngineConfig::GGEngineAudio;

    lua_getfield(L, 1, "masterVolume");
    if (!lua_isnil(L, -1))
    {
        SetMasterVolume(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "bgmVolume");
    if (!lua_isnil(L, -1))
    {
        SetBGMVolume(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "sfxVolume");
    if (!lua_isnil(L, -1))
    {
        SetSFXVolume(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, -1);

    SaveAudioSettings(); // serialize to config file

    return 0;
}

/*************************************************************************
  Anything below this point is the:
  OLD AUDIO SYSTEM IMPLEMENTATION (USING AUDIOSOURCE COMPONENT)
  -> can still be used but may not mesh well with the 
     AUDIOSOURCE implementation
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Audio.playSound(assetPath, volume) -> handle
*************************************************************************/
int lua_Audio_PlaySound(lua_State* L)
{
    const char* assetPath = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_checknumber(L, 2));

    (void)assetPath; // TODO: Will be used when implemented
    (void)volume; // TODO: Will be used when implemented

    // Get audio manager from engine
    GGFMODWrapper* audio = GGEngine::GetEngine().GetAudioSystem();
    if (!audio)
    {
        luaL_error(L, "Audio system not initialized");
        return 0;
    }

    // Play sound and get handle
    // TODO: Implement actual sound playing
    audio->PlayEditorAudioPreview(assetPath);

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.stopSound(handle)
*************************************************************************/
int lua_Audio_StopSound(lua_State* L)
{
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    auto it = g_audioHandles.find(handle);
    if (it != g_audioHandles.end())
    {
        // TODO: Stop the sound
        // audio->StopSound(it->second);
        g_audioHandles.erase(it);
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.setVolume(handle, volume)
*************************************************************************/
int lua_Audio_SetVolume(lua_State* L)
{
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float volume = static_cast<float>(luaL_checknumber(L, 2));

    (void)volume; // TODO: Will be used when implemented

    auto it = g_audioHandles.find(handle);
    if (it != g_audioHandles.end())
    {
        // TODO: Set volume
        // audio->SetVolume(it->second, volume);
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.playMusic(assetPath, volume, loop)
*************************************************************************/
int lua_Audio_PlayMusic(lua_State* L)
{
    const char* assetPath = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    bool loop = lua_toboolean(L, 3);

    // TODO: Implement music playback
    std::cout << "Playing music: " << assetPath << " (volume: " << volume
        << ", loop: " << loop << ")" << std::endl;

    AudioResult result = GetEngineInstance().GetAudioSystem()->PlayAudioByPath(assetPath);
    if (result == AudioResult::SUCCESS)
    {
        std::cout << "Playing Audio " << assetPath << std::endl;
    }
    else
    {
        std::cerr << "Failed to play audio: " << assetPath << " - " << GetEngineInstance().GetAudioSystem()->GetResultString(result) << std::endl;
    }

    lua_pushinteger(L, g_nextAudioHandle++);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Audio.getMasterVolume()
*************************************************************************/
int lua_Audio_GetMasterVolume(lua_State* L)
{
    float volume = GetEngineInstance().GetAudioSystem()->GetMasterVolume();
    lua_pushnumber(L, volume);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Audio.setMasterVolume(volume)
    Note: volume range is between 0.0 and 1.0
*************************************************************************/
int lua_Audio_SetMasterVolume(lua_State* L)
{
    if (!lua_isnumber(L, 1))
    {
        luaL_error(L, "Audio.setMasterVolume expects a number (0.0 - 1.0)");
        return 0;
    }

    float volume = static_cast<float>(lua_tonumber(L, 1));

    AudioResult result = GetEngineInstance().GetAudioSystem()->SetMasterVolume(volume);

    if (result != AudioResult::SUCCESS)
    {
        luaL_error(L, "SetMasterVolume failed");
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua: Audio.stopMusic()
*************************************************************************/
int lua_Audio_StopMusic(lua_State* L)
{
    (void)L;
    GetEngineInstance().GetAudioSystem()->StopAllSounds();
    std::cout << "Stopping all music" << std::endl;
    return 0;
}

/*************************************************************************
  TIME SYSTEM IMPLEMENTATION
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Time.getDeltaTime() -> number
*************************************************************************/
int lua_Time_GetDeltaTime(lua_State* L)
{
    double dt = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().GetDeltaTime();
    lua_pushnumber(L, dt);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Time.getTotalTime() -> number
*************************************************************************/
int lua_Time_GetTotalTime(lua_State* L)
{
    // TODO: Get from actual time system
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - startTime;

    lua_pushnumber(L, elapsed.count());
    return 1;
}

/*************************************************************************
  \brief
    Lua: Time.getFPS() -> number
*************************************************************************/
int lua_Time_GetFPS(lua_State* L)
{
    double dt = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().GetDeltaTime();
    double fps = dt > 0 ? 1.0f / dt : 0.0f;

    lua_pushnumber(L, fps);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Time.getFrameCount() -> number
*************************************************************************/
int lua_Time_GetFrameCount(lua_State* L)
{
    // TODO: Track actual frame count
    static uint64_t frameCount = 0;
    frameCount++;

    lua_pushinteger(L, frameCount);
    return 1;
}

/*************************************************************************
  OBJECTEFFECTCOMPONENT IMPLEMENTATION
*************************************************************************/

int lua_ECS_AddObjectEffectComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    ObjectEffectComponent effect;
    // Optional: accept effectType as second arg
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
    {
        effect.effectType = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }

    world->AddComponent<ObjectEffectComponent>(entity, effect);
    return 0;
}

int lua_ECS_GetObjectEffectComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!world->HasComponent<ObjectEffectComponent>(entity))
    {
        luaL_error(L, "Entity does not have ObjectEffectComponent");
        return 0;
    }

    ObjectEffectComponent* effect = world->GetComponent<ObjectEffectComponent>(entity);

    lua_newtable(L);

    lua_pushnumber(L, effect->intensity);
    lua_setfield(L, -2, "intensity");

    lua_pushnumber(L, effect->effectDuration);
    lua_setfield(L, -2, "effectDuration");

    lua_pushboolean(L, effect->loop);
    lua_setfield(L, -2, "loop");

    lua_pushinteger(L, effect->effectType);
    lua_setfield(L, -2, "effectType");

    // Color as nested table
    lua_newtable(L);
    lua_pushnumber(L, effect->color.r);
    lua_setfield(L, -2, "r");
    lua_pushnumber(L, effect->color.g);
    lua_setfield(L, -2, "g");
    lua_pushnumber(L, effect->color.b);
    lua_setfield(L, -2, "b");
    lua_pushnumber(L, effect->color.a);
    lua_setfield(L, -2, "a");
    lua_setfield(L, -2, "color");

    return 1;
}

int lua_ECS_SetObjectEffectComponent(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    if (!lua_istable(L, 2))
    {
        luaL_error(L, "Expected table for ObjectEffectComponent parameters");
        return 0;
    }

    if (!world->HasComponent<ObjectEffectComponent>(entity))
    {
        luaL_error(L, "Entity does not have ObjectEffectComponent");
        return 0;
    }

    ObjectEffectComponent* effect = world->GetComponent<ObjectEffectComponent>(entity);

    lua_getfield(L, 2, "intensity");
    if (!lua_isnil(L, -1))
        effect->intensity = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 2, "effectDuration");
    if (!lua_isnil(L, -1))
        effect->effectDuration = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 2, "loop");
    if (!lua_isnil(L, -1))
        effect->loop = static_cast<bool>(lua_toboolean(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 2, "effectType");
    if (!lua_isnil(L, -1))
        effect->effectType = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 2, "color");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "r");
        if (!lua_isnil(L, -1)) effect->color.r = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "g");
        if (!lua_isnil(L, -1)) effect->color.g = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "b");
        if (!lua_isnil(L, -1)) effect->color.b = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "a");
        if (!lua_isnil(L, -1)) effect->color.a = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return 0;
}


/*************************************************************************
  REGISTRATION FUNCTIONS
*************************************************************************/

/*************************************************************************
  \brief
    Registers extended ECS bindings to existing ECS table.
*************************************************************************/
/*************************************************************************
  SCRIPT INSTANCE IMPLEMENTATION
  Returns the script "self" userdata for an entity's ScriptComponent
*************************************************************************/

/*************************************************************************
  \brief
    Lua C function: ECS.getScriptInstance(entity)
  \param[in] L
    Lua state. Stack position 1 must contain entity handle (lightuserdata).
  \return
    Number of Lua return values (1 - userdata or nil).
  \details
    Retrieves the ScriptComponent from the entity, looks up the active
    EntityInstance via the script system's instance pool, and pushes a
    new InstanceUserdata with the script type's metatable. Returns nil
    if the entity has no ScriptComponent or the instance is uninitialized.
*************************************************************************/
int lua_ECS_GetScriptInstance(lua_State* L)
{
    World* world = GetWorldFromLua(L);
    Entity entity = GetValidatedEntityHandle(L, 1, *world);

    // Entity must have a ScriptComponent
    if (!world->HasComponent<ScriptComponent>(entity))
    {
        lua_pushnil(L);
        return 1;
    }

    ScriptComponent* script = world->GetComponent<ScriptComponent>(entity);

    // Get the script system to access the instance pool
    auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
    EntityInstance* instance = scriptSystem.GetScriptInstance(script->scriptInstance);

    if (!instance || !instance->type)
    {
        lua_pushnil(L);
        return 1;
    }

    // Create the InstanceUserdata (same pattern used in Init/Start/Update calls)
    InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));
    userData->instance = instance;

    // Set the script type's metatable so __index/__newindex work
    if (instance->type->metatableRef != LUA_NOREF && instance->type->metatableRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, instance->type->metatableRef);
        lua_setmetatable(L, -2);
    }

    return 1; // Return the userdata
}

/*************************************************************************
  \brief
    Lua C function: ECS.isEntityAlive(entity)
  \param[in] L
    Lua state. Stack position 1 must contain entity handle (lightuserdata).
  \return
    Number of Lua return values (1 - boolean).
  \details
    Returns true if the entity handle is still valid (not destroyed).
    Unlike GetValidatedEntityHandle, this does NOT raise a Lua error
    for invalid handles -- it simply returns false. Useful for guarding
    event callbacks that may fire after the entity has been destroyed.
*************************************************************************/
int lua_ECS_IsEntityAlive(lua_State* L)
{
    if (!lua_islightuserdata(L, 1))
    {
        lua_pushboolean(L, false);
        return 1;
    }

    World* world = GetWorldFromLua(L);
    void* ptr = lua_touserdata(L, 1);
    uint64_t packed = reinterpret_cast<uint64_t>(ptr);
    Entity entity = UnpackEntity(packed);

    lua_pushboolean(L, world->IsEntityValidHandle(entity));
    return 1;
}

void RegisterExtendedECSBindings(GGLuaVM& luaVM, World& world)
{
    lua_State* L = luaVM.GetState();

    // Store world pointer if not already stored
    lua_pushlightuserdata(L, &world);
    lua_setfield(L, LUA_REGISTRYINDEX, "gg_world_ptr");

    // Get existing ECS table
    lua_getglobal(L, "ECS");
    if (!lua_istable(L, -1))
    {
        // Create if doesn't exist
        lua_pop(L, 1);
        lua_newtable(L);
    }

    // EntityName functions
    lua_pushcfunction(L, lua_ECS_AddEntityName);
    lua_setfield(L, -2, "addEntityName");

    lua_pushcfunction(L, lua_ECS_GetEntityName);
    lua_setfield(L, -2, "getEntityName");

    lua_pushcfunction(L, lua_ECS_SetEntityName);
    lua_setfield(L, -2, "setEntityName");

    lua_pushcfunction(L, lua_ECS_FindEntityByName);
    lua_setfield(L, -2, "findEntityByName");

    // AABB functions
    lua_pushcfunction(L, lua_ECS_AddAABB);
    lua_setfield(L, -2, "addAABB");

    lua_pushcfunction(L, lua_ECS_GetAABB);
    lua_setfield(L, -2, "getAABB");

    lua_pushcfunction(L, lua_ECS_SetAABB);
    lua_setfield(L, -2, "setAABB");

    lua_pushcfunction(L, lua_ECS_CheckAABBOverlap);
    lua_setfield(L, -2, "checkAABBOverlap");

    // World position function
    lua_pushcfunction(L, lua_ECS_GetWorldPosition);
    lua_setfield(L, -2, "getWorldPosition");

    // Hierarchy functions
    lua_pushcfunction(L, lua_ECS_SetParent);
    lua_setfield(L, -2, "setParent");

    lua_pushcfunction(L, lua_ECS_GetChildren);
    lua_setfield(L, -2, "getChildren");

    lua_pushcfunction(L, lua_ECS_DestroyEntity);
    lua_setfield(L, -2, "destroyEntity");

    // MeshRenderer functions
    lua_pushcfunction(L, lua_ECS_AddMeshRenderer);
    lua_setfield(L, -2, "addMeshRenderer");

    lua_pushcfunction(L, lua_ECS_GetMeshRenderer);
    lua_setfield(L, -2, "getMeshRenderer");

    lua_pushcfunction(L, lua_ECS_SetMeshRenderer);
    lua_setfield(L, -2, "setMeshRenderer");

    // UIComponent functions
    lua_pushcfunction(L, lua_ECS_AddUIComponent);
    lua_setfield(L, -2, "addUIComponent");

    lua_pushcfunction(L, lua_ECS_GetUIComponent);
    lua_setfield(L, -2, "getUIComponent");

    lua_pushcfunction(L, lua_ECS_SetUIComponent);
    lua_setfield(L, -2, "setUIComponent");

    // Sprite functions
    lua_pushcfunction(L, lua_ECS_AddSprite);
    lua_setfield(L, -2, "addSprite");

    lua_pushcfunction(L, lua_ECS_GetSprite);
    lua_setfield(L, -2, "getSprite");

    lua_pushcfunction(L, lua_ECS_SetSprite);
    lua_setfield(L, -2, "setSprite");

    // Grid functions
    lua_pushcfunction(L, lua_ECS_AddGridComponent);
    lua_setfield(L, -2, "addGridComponent");

    lua_pushcfunction(L, lua_ECS_GetGridComponent);
    lua_setfield(L, -2, "getGridComponent");

    lua_pushcfunction(L, lua_ECS_SetGridComponent);
    lua_setfield(L, -2, "setGridComponent");

    lua_pushcfunction(L, lua_ECS_GetTilemapComponent);
    lua_setfield(L, -2, "getTilemapComponent");


    // TextComponent functions
    lua_pushcfunction(L, lua_ECS_AddTextComponent);
    lua_setfield(L, -2, "addTextComponent");

    lua_pushcfunction(L, lua_ECS_GetTextComponent);
    lua_setfield(L, -2, "getTextComponent");

    lua_pushcfunction(L, lua_ECS_SetTextComponent);
    lua_setfield(L, -2, "setTextComponent");

    // TextComponent functions
    lua_pushcfunction(L, lua_ECS_AddWorldTextComponent);
    lua_setfield(L, -2, "addWorldTextComponent");

    lua_pushcfunction(L, lua_ECS_GetWorldTextComponent);
    lua_setfield(L, -2, "getWorldTextComponent");

    lua_pushcfunction(L, lua_ECS_SetWorldTextComponent);
    lua_setfield(L, -2, "setWorldTextComponent");

    // AudioSource functions
    lua_pushcfunction(L, lua_ECS_GetAudioSource);
    lua_setfield(L, -2, "getAudioSource");

    lua_pushcfunction(L, lua_ECS_SetAudioSource);
    lua_setfield(L, -2, "setAudioSource");

    // Animation functions
    lua_pushcfunction(L, lua_ECS_GetAnimation);
    lua_setfield(L, -2, "getAnimation");

    lua_pushcfunction(L, lua_ECS_SetAnimation);
    lua_setfield(L, -2, "setAnimation");

    // Query functions
    lua_pushcfunction(L, lua_ECS_GetEntitiesWithComponent);
    lua_setfield(L, -2, "getEntitiesWithComponent");

    lua_pushcfunction(L, lua_ECS_GetEntityCount);
    lua_setfield(L, -2, "getEntityCount");

    lua_pushcfunction(L, lua_ECS_GetAllEntities);
    lua_setfield(L, -2, "getAllEntities");

    // Script instance access
    lua_pushcfunction(L, lua_ECS_GetScriptInstance);
    lua_setfield(L, -2, "getScriptInstance");

    // Entity validity check (safe, no Lua error on invalid handles)
    lua_pushcfunction(L, lua_ECS_IsEntityAlive);
    lua_setfield(L, -2, "isEntityAlive");

    // ObjectEffectComponent functions
    lua_pushcfunction(L, lua_ECS_AddObjectEffectComponent);
    lua_setfield(L, -2, "addObjectEffectComponent");

    lua_pushcfunction(L, lua_ECS_GetObjectEffectComponent);
    lua_setfield(L, -2, "getObjectEffectComponent");

    lua_pushcfunction(L, lua_ECS_SetObjectEffectComponent);
    lua_setfield(L, -2, "setObjectEffectComponent");

    // Set back as global
    lua_setglobal(L, "ECS");

    std::cout << "[LuaExtensions] Extended ECS bindings registered" << std::endl;
}

/*************************************************************************
  \brief
    Registers Audio API in global namespace.
*************************************************************************/
void RegisterAudioBindings(GGLuaVM& luaVM)
{
    lua_State* L = luaVM.GetState();

    // Create Audio table
    lua_newtable(L);

    // old audio functions
    lua_pushcfunction(L, lua_Audio_PlaySound);
    lua_setfield(L, -2, "playSound");

    lua_pushcfunction(L, lua_Audio_StopSound);
    lua_setfield(L, -2, "stopSound");

    lua_pushcfunction(L, lua_Audio_SetVolume);
    lua_setfield(L, -2, "setVolume");

    lua_pushcfunction(L, lua_Audio_PlayMusic);
    lua_setfield(L, -2, "playMusic");

    lua_pushcfunction(L, lua_Audio_StopMusic);
    lua_setfield(L, -2, "stopMusic");

    // master volume controls
    lua_pushcfunction(L, lua_Audio_GetMasterVolume);
    lua_setfield(L, -2, "getMasterVolume");

    lua_pushcfunction(L, lua_Audio_SetMasterVolume);
    lua_setfield(L, -2, "setMasterVolume");

    // new audio functions using AudioSource Component instead of raw fmod channels
    lua_pushcfunction(L, lua_Audio_TriggerSound);
    lua_setfield(L, -2, "triggerSound");

    lua_pushcfunction(L, lua_Audio_TriggerSoundOneShot);
    lua_setfield(L, -2, "triggerSoundOneShot");

    lua_pushcfunction(L, lua_Audio_TriggerMusic);
    lua_setfield(L, -2, "triggerMusic");

    lua_pushcfunction(L, lua_Audio_GetAudioSourceVolume);
    lua_setfield(L, -2, "getAudioSourceVolume");

    lua_pushcfunction(L, lua_Audio_SetAudioSourceVolume);
    lua_setfield(L, -2, "setAudioSourceVolume");

    lua_pushcfunction(L, lua_Audio_GetAudioSourceStereoPan);
    lua_setfield(L, -2, "getAudioSourceStereoPan");

    lua_pushcfunction(L, lua_Audio_SetAudioSourceStereoPan);
    lua_setfield(L, -2, "setAudioSourceStereoPan");

    lua_pushcfunction(L, lua_Audio_GetAudioSourcePitch);
    lua_setfield(L, -2, "getAudioSourcePitch");

    lua_pushcfunction(L, lua_Audio_SetAudioSourcePitch);
    lua_setfield(L, -2, "setAudioSourcePitch");

    lua_pushcfunction(L, lua_Audio_GetAudioConfig);
    lua_setfield(L, -2, "getAudioConfig");

    lua_pushcfunction(L, lua_Audio_SetAudioConfig);
    lua_setfield(L, -2, "setAudioConfig");

    // Set as global
    lua_setglobal(L, "Audio");

    std::cout << "[LuaExtensions] Audio bindings registered" << std::endl;
}

/*************************************************************************
  \brief
    Registers Time API in global namespace.
*************************************************************************/
void RegisterTimeBindings(GGLuaVM& luaVM)
{
    lua_State* L = luaVM.GetState();

    // Create Time table
    lua_newtable(L);

    lua_pushcfunction(L, lua_Time_GetDeltaTime);
    lua_setfield(L, -2, "getDeltaTime");

    lua_pushcfunction(L, lua_Time_GetTotalTime);
    lua_setfield(L, -2, "getTotalTime");

    lua_pushcfunction(L, lua_Time_GetFPS);
    lua_setfield(L, -2, "getFPS");

    lua_pushcfunction(L, lua_Time_GetFrameCount);
    lua_setfield(L, -2, "getFrameCount");

    // Set as global
    lua_setglobal(L, "Time");

    std::cout << "[LuaExtensions] Time bindings registered" << std::endl;
}

void RegisterObjectEffectsBindings(GGLuaVM& luaVM) {
    lua_State* L = luaVM.GetState();

    // Grab the existing ECS table that RegisterExtendedECSBindings already created
    lua_getglobal(L, "ECS");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        std::cerr << "[LuaExtensions] ERROR: ECS table not found - call RegisterExtendedECSBindings first" << std::endl;
        return;
    }

    // Build EffectType constants table and set it onto ECS
    lua_newtable(L);

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::NONE));
    lua_setfield(L, -2, "NONE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::FOG));
    lua_setfield(L, -2, "FOG");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::FADE));
    lua_setfield(L, -2, "FADE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::BLOOM));
    lua_setfield(L, -2, "BLOOM");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::OUTLINE));
    lua_setfield(L, -2, "OUTLINE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::SQUARE_DISSOLVE));
    lua_setfield(L, -2, "SQUARE_DISSOLVE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::SHINE));
    lua_setfield(L, -2, "SHINE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::SHINERTL));
    lua_setfield(L, -2, "SHINERTL");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::BURN_DISSOLVE));
    lua_setfield(L, -2, "BURN_DISSOLVE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::PIXEL_DISSOLVE));
    lua_setfield(L, -2, "PIXEL_DISSOLVE");

    lua_pushinteger(L, static_cast<int>(ObjectEffectComponent::EffectType::PULSE));
    lua_setfield(L, -2, "PULSE");
    

    // Stack: [-2] = ECS table, [-1] = EffectType table
    // This sets ECS.EffectType = the constants table
    lua_setfield(L, -2, "EffectType");

    // Pop the ECS table
    lua_pop(L, 1);

    std::cout << "[LuaExtensions] Object Effect bindings registered" << std::endl;
}

