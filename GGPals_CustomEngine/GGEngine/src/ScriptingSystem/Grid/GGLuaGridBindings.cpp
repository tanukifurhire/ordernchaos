/*************************************************************************
  \file       GGLuaGridBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Implements Lua bindings for the Grid system in GGEngine.
    Exposes grid management, tile queries, tile manipulation, pathfinding,
    and coordinate conversions to Lua scripts.
  \details
    This file provides a set of functions to interact with the Grid system
    from Lua scripts. Functions allow scripts to:
      - Retrieve and set the active grid
      - Access individual tiles or collections of tiles
      - Highlight tiles and manipulate tile visuals
      - Validate movement using pathfinding algorithms
      - Convert between grid coordinates and world coordinates
    Additionally, helper functions push Tile and MovementValidationResult
    data structures onto the Lua stack as tables.
  \dependencies
    - GGLuaGridBindings.hpp
    - GGLuaVM.hpp
    - GGLuaBindings.hpp
    - GGSystemManagers.hpp
    - GridManager
    - Tile, GridComponent, GridPathfinding
    - Lua 5.1/5.3 C API
    - GGMath types (Vector2, Vector2Int, Vector4)
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaGridBindings.hpp>
#include <GGLuaVM.hpp>
#include <GGLuaBindings.hpp>
#include <GGSystemManagers.hpp>

#include <iostream>
#include <string>

using namespace Grid;

/*************************************************************************
  \brief
    Register all Grid system bindings in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \details
    Creates global "Grid" table with all grid operations:
    - Grid creation and management
    - Tile queries and manipulation
    - Pathfinding and movement validation
    - Area operations and visual effects
    - Win condition checking
  \par Usage
  \code
    GGLuaVM luaVM;
    RegisterGridBindings(luaVM);
    luaVM.ExecuteString("local tile = Grid.getTile(Grid.getOrderGrid(), 0, 0)", "test");
  \endcode
  \note
    Must be called after GridManager is initialized.
*************************************************************************/
void RegisterGridBindings(GGLuaVM& luaVM)
{
    lua_State* L = luaVM.GetState();

    // Initialize pathfinding system
    if (!g_pathfinding)
    {
        g_pathfinding = new GridPathfinding(PathfindingAlgorithm::SIMPLE_VALIDATION, true);
    }

    // Create Grid table
    lua_newtable(L);

    // Grid Management
    lua_pushcfunction(L, lua_Grid_GetActiveGrid);
    lua_setfield(L, -2, "getActiveGrid");

    // Tile Queries
    lua_pushcfunction(L, lua_Grid_GetTile);
    lua_setfield(L, -2, "getTile");

    lua_pushcfunction(L, lua_Grid_GetTilesInRange);
    lua_setfield(L, -2, "getTilesInRange");

    lua_pushcfunction(L, lua_Grid_GetTilesInArea);
    lua_setfield(L, -2, "getTilesInArea");

    // Tile manipulation
    lua_pushcfunction(L, lua_Grid_HighlightTiles);
    lua_setfield(L, -2, "highlightTiles");

    // Pathfinding
    lua_pushcfunction(L, lua_Grid_ValidateMovement);
    lua_setfield(L, -2, "validateMovement");

    lua_pushcfunction(L, lua_Grid_SetPathfindingAlgorithm);
    lua_setfield(L, -2, "setPathfindingAlgorithm");

    lua_pushcfunction(L, lua_Grid_SetTileTexture);
    lua_setfield(L, -2, "setTileTexture");

    lua_pushcfunction(L, lua_Grid_SetTileColorAdd);
    lua_setfield(L, -2, "setTileColorAdd");

    lua_pushcfunction(L, lua_Grid_SetTileColorMultiply);
    lua_setfield(L, -2, "setTileColorMultiply");

    // Coordinate Conversion
    lua_pushcfunction(L, lua_Grid_GetMouseWorldPerspective);
    lua_setfield(L, -2, "getMouseWorldPerspective");

    lua_pushcfunction(L, lua_Grid_WorldToGrid);
    lua_setfield(L, -2, "worldToGrid");

    lua_pushcfunction(L, lua_Grid_GridToWorld);
    lua_setfield(L, -2, "gridToWorld");

    // Set global Grid table
    lua_setglobal(L, "Grid");

    std::cout << "[LuaBindings] Grid system registered" << std::endl;
}

/*************************************************************************
  \brief
    Lua: Grid.getActiveGrid() -> gridID
  \details
    Gets currently active grid ID.
  \return
    Active grid ID as integer
*************************************************************************/
int lua_Grid_GetActiveGrid(lua_State* L)
{
    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* active = manager->GetActiveGrid();
    if (!active)
    {
        lua_pushnil(L);
        return 1;
    }

    // Return the grid ID - we need to find which ID maps to this grid
    uint32_t orderID = manager->GetOrderGridID();
    uint32_t chaosID = manager->GetChaosGridID();

    if (manager->GetGrid(orderID) == active)
        lua_pushinteger(L, orderID);
    else if (manager->GetGrid(chaosID) == active)
        lua_pushinteger(L, chaosID);
    else
        lua_pushnil(L);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.setActiveGrid(gridID)
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \details
    Sets the active grid for operations.
*************************************************************************/
int lua_Grid_SetActiveGrid(lua_State* L)
{
    if (lua_gettop(L) != 1)
    {
        luaL_error(L, "Expected 1 argument: gridID");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    manager->SetActiveGrid(gridID);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Grid.getTile(gridID, x, y) -> tile_table or nil
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    X coordinate (integer, 0-5)
  \param[in] 3 (Lua stack)
    Y coordinate (integer, 0-3)
  \details
    Gets tile data as Lua table:
    {
      position = {x, y},
      isPassable = boolean,
      isVisible = boolean,
      isDestroyed = boolean,
      isHighlighted = boolean,
      isOccupied = boolean
    }
  \return
    Tile table or nil if invalid coordinates
*************************************************************************/
int lua_Grid_GetTile(lua_State* L)
{
    if (lua_gettop(L) != 3)
    {
        luaL_error(L, "Expected 3 arguments: gridID, x, y");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 3));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
    {
        lua_pushnil(L);
        return 1;
    }

    Tile* tile = grid->GetTile(x, y);
    if (!tile)
    {
        lua_pushnil(L);
        return 1;
    }

    PushTileToLua(L, *tile);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.getTilesInRange(gridID, centerX, centerY, range) -> tile_array
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    Center X coordinate (integer)
  \param[in] 3 (Lua stack)
    Center Y coordinate (integer)
  \param[in] 4 (Lua stack)
    Manhattan distance range (integer)
  \details
    Gets array of tiles within range (Manhattan distance).
  \return
    Array of tile tables
*************************************************************************/
int lua_Grid_GetTilesInRange(lua_State* L)
{
    if (lua_gettop(L) != 4)
    {
        luaL_error(L, "Expected 4 arguments: gridID, centerX, centerY, range");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int32_t centerX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t centerY = static_cast<int32_t>(luaL_checkinteger(L, 3));
    int32_t range = static_cast<int32_t>(luaL_checkinteger(L, 4));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
    {
        lua_newtable(L);
        return 1;
    }

    GGMath::Vector2Int center(centerX, centerY);
    auto tiles = grid->GetTilesInRange(center, range);

    // Create array table
    lua_newtable(L);
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        lua_pushinteger(L, i + 1);  // Lua 1-based indexing
        PushTileToLua(L, *tiles[i]);
        lua_settable(L, -3);
    }

    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.getTilesInArea(gridID, centerX, centerY, width, height) -> tile_array
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    Center X coordinate (integer)
  \param[in] 3 (Lua stack)
    Center Y coordinate (integer)
  \param[in] 4 (Lua stack)
    Area width (integer)
  \param[in] 5 (Lua stack)
    Area height (integer)
  \details
    Gets array of tiles in rectangular area.
  \return
    Array of tile tables
*************************************************************************/
int lua_Grid_GetTilesInArea(lua_State* L)
{
    if (lua_gettop(L) != 5)
    {
        luaL_error(L, "Expected 5 arguments: gridID, centerX, centerY, width, height");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int32_t centerX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t centerY = static_cast<int32_t>(luaL_checkinteger(L, 3));
    int32_t width = static_cast<int32_t>(luaL_checkinteger(L, 4));
    int32_t height = static_cast<int32_t>(luaL_checkinteger(L, 5));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
    {
        lua_newtable(L);
        return 1;
    }

    GGMath::Vector2Int center(centerX, centerY);
    auto tiles = grid->GetTilesInArea(center, width, height);

    // Create array table
    lua_newtable(L);
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        lua_pushinteger(L, i + 1);  // Lua 1-based indexing
        PushTileToLua(L, *tiles[i]);
        lua_settable(L, -3);
    }

    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.highlightTiles(gridID, positions, clear=true)
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    Array of position tables: {{x,y}, {x,y}, ...}
  \param[in] 3 (Lua stack)
    Clear previous highlights (boolean, optional, default true)
  \details
    Highlights specified tiles for visual feedback.
*************************************************************************/
int lua_Grid_HighlightTiles(lua_State* L)
{
    int argc = lua_gettop(L);
    if (argc < 2 || argc > 3)
    {
        luaL_error(L, "Expected 2-3 arguments: gridID, positions, [clear=true]");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool clear = (argc == 3) ? lua_toboolean(L, 3) : true;

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
        return 0;

    // Extract positions from Lua table
    auto positions = GetPositionArrayFromLua(L, 2);

    // Convert positions to tile pointers
    std::vector<Tile*> tilesToHighlight;
    for (const auto& pos : positions)
    {
        Tile* tile = grid->GetTile(pos);
        if (tile)
            tilesToHighlight.push_back(tile);
    }

    grid->HighlightTiles(tilesToHighlight, clear);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Grid.setTileTexture(gridID, gridX, gridY, layer, texID)
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    grid X position of tile
  \param[in] 3 (Lua stack)
    grid Y position of tile
  \param[in] 4 (Lua stack)
    layer of tile
  \param[in] 5 (Lua stack)
    texture id to render
  \details
    Sets the texture of a tile on a specific layer
*************************************************************************/
int lua_Grid_SetTileTexture(lua_State* L)
{
    int argc = lua_gettop(L);
    if (argc != 5)
    {
        luaL_error(L, "Expected 5 arguments: gridID, gridX, gridY, layer, texID");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
        return 0;

    int32_t gridX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t gridY = static_cast<int32_t>(luaL_checkinteger(L, 3));
    uint32_t layer = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    uint32_t texID = static_cast<uint32_t>(luaL_checkinteger(L, 5));

    if (grid->GetTileMap().layers.size() <= layer)
    {
        luaL_error(L, "Invalid layer id!");
        return 0;
    }

    grid->GetTileMap().layers[layer]
        .GetTile(gridX, gridY).spriteID = texID;

    return 0;
}

/*************************************************************************
  \brief
    Lua: Grid.setTileTexture(gridID, gridX, gridY, layer, { r, g, b, a })
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    grid X position of tile
  \param[in] 3 (Lua stack)
    grid Y position of tile
  \param[in] 4 (Lua stack)
    layer of tile
  \param[in] 5 (Lua stack)
    color add value
  \details
    Sets the texture of a tile on a specific layer
*************************************************************************/
int lua_Grid_SetTileColorAdd(lua_State* L)
{
    int argc = lua_gettop(L);
    if (argc != 5)
    {
        luaL_error(L, "Expected 5 arguments: gridID, gridX, gridY, layer, { r, g, b, a }");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
        return 0;

    int32_t gridX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t gridY = static_cast<int32_t>(luaL_checkinteger(L, 3));
    uint32_t layer = static_cast<uint32_t>(luaL_checkinteger(L, 4));

    if (grid->GetTileMap().layers.size() <= layer)
    {
        luaL_error(L, "Invalid layer id!");
        return 0;
    }

    if (!lua_istable(L, 5))
    {
        luaL_error(L, "Expected table for Tile color parameters");
        return 0;
    }

    GGMath::Vector4 colorAdd{};

    lua_getfield(L, 5, "r");
    if (!lua_isnil(L, -1))
    {
        colorAdd.x = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 5, "g");
    if (!lua_isnil(L, -1))
    {
        colorAdd.y = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 5, "b");
    if (!lua_isnil(L, -1))
    {
        colorAdd.z = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 5, "a");
    if (!lua_isnil(L, -1))
    {
        colorAdd.w = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    grid->GetTileMap().layers[layer]
        .GetTile(gridX, gridY).SetColorAdd(colorAdd);

    return 0;
}

/*************************************************************************
  \brief
    Lua: Grid.setTileTexture(gridID, gridX, gridY, layer, { r, g, b, a })
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    grid X position of tile
  \param[in] 3 (Lua stack)
    grid Y position of tile
  \param[in] 4 (Lua stack)
    layer of tile
  \param[in] 5 (Lua stack)
    color multiply value
  \details
    Sets the texture of a tile on a specific layer
*************************************************************************/
int lua_Grid_SetTileColorMultiply(lua_State* L)
{
    int argc = lua_gettop(L);
    if (argc != 5)
    {
        luaL_error(L, "Expected 5 arguments: gridID, gridX, gridY, layer, { r, g, b, a }");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
        return 0;

    int32_t gridX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t gridY = static_cast<int32_t>(luaL_checkinteger(L, 3));
    uint32_t layer = static_cast<uint32_t>(luaL_checkinteger(L, 4));

    if (grid->GetTileMap().layers.size() <= layer)
    {
        luaL_error(L, "Invalid layer id!");
        return 0;
    }

    if (!lua_istable(L, 5))
    {
        luaL_error(L, "Expected table for Tile color parameters");
        return 0;
    }

    GGMath::Vector4 colorMul{};

    lua_getfield(L, 5, "r");
    if (!lua_isnil(L, -1))
    {
        colorMul.x = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 5, "g");
    if (!lua_isnil(L, -1))
    {
        colorMul.y = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 5, "b");
    if (!lua_isnil(L, -1))
    {
        colorMul.z = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 5, "a");
    if (!lua_isnil(L, -1))
    {
        colorMul.w = static_cast<GGMath::float32>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    grid->GetTileMap().layers[layer]
        .GetTile(gridX, gridY).SetColorMultiply(colorMul);
    return 0;
}

/*************************************************************************
  \brief
    Lua: Grid.validateMovement(gridID, fromX, fromY, toX, toY, maxRange, unitID=0) -> result_table
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    From X coordinate (integer)
  \param[in] 3 (Lua stack)
    From Y coordinate (integer)
  \param[in] 4 (Lua stack)
    To X coordinate (integer)
  \param[in] 5 (Lua stack)
    To Y coordinate (integer)
  \param[in] 6 (Lua stack)
    Maximum movement range (integer)
  \param[in] 7 (Lua stack)
    Unit ID (integer, optional, default 0)
  \details
    Validates movement from one position to another.
  \return
    Movement result table:
    {
      canMove = boolean,
      path = {{x,y}, {x,y}, ...},
      reason = string,
      validationTimeMs = number,
      tilesChecked = integer,
      algorithmUsed = string
    }
*************************************************************************/
int lua_Grid_ValidateMovement(lua_State* L)
{
    int argc = lua_gettop(L);
    if (argc < 6 || argc > 7)
    {
        luaL_error(L, "Expected 6-7 arguments: gridID, fromX, fromY, toX, toY, maxRange, [unitID=0]");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int32_t fromX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t fromY = static_cast<int32_t>(luaL_checkinteger(L, 3));
    int32_t toX = static_cast<int32_t>(luaL_checkinteger(L, 4));
    int32_t toY = static_cast<int32_t>(luaL_checkinteger(L, 5));
    int32_t maxRange = static_cast<int32_t>(luaL_checkinteger(L, 6));
    uint32_t unitID = (argc == 7) ? static_cast<uint32_t>(luaL_checkinteger(L, 7)) : 0;

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GridComponent* grid = manager->GetGrid(gridID);
    if (!grid)
    {
        luaL_error(L, "Invalid grid ID");
        return 0;
    }

    if (!g_pathfinding)
    {
        luaL_error(L, "Pathfinding system not initialized");
        return 0;
    }

    GGMath::Vector2Int from(fromX, fromY);
    GGMath::Vector2Int to(toX, toY);

    auto result = g_pathfinding->ValidateMovement(grid, from, to, maxRange, unitID);

    PushMovementResultToLua(L, result);
    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.setPathfindingAlgorithm(algorithm)
  \param[in] 1 (Lua stack)
    Algorithm name (string: "SIMPLE" or "BFS")
  \details
    Sets pathfinding algorithm for future validations.
*************************************************************************/
int lua_Grid_SetPathfindingAlgorithm(lua_State* L)
{
    if (lua_gettop(L) != 1)
    {
        luaL_error(L, "Expected 1 argument: algorithm");
        return 0;
    }

    const char* algorithmName = luaL_checkstring(L, 1);

    if (!g_pathfinding)
    {
        luaL_error(L, "Pathfinding system not initialized");
        return 0;
    }

    if (strcmp(algorithmName, "SIMPLE") == 0)
    {
        g_pathfinding->SetAlgorithm(PathfindingAlgorithm::SIMPLE_VALIDATION);
    }
    else if (strcmp(algorithmName, "BFS") == 0)
    {
        g_pathfinding->SetAlgorithm(PathfindingAlgorithm::BFS_PATHFINDING);
    }
    else
    {
        luaL_error(L, "Invalid algorithm name: expected 'SIMPLE' or 'BFS'");
        return 0;
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua: Grid.getMouseWorldPerspective(perspective) -> {x, y} or nil
  \param[in] 1 (Lua stack)
    perspective factor (number)
  \details
    Converts world coordinates to grid position.
  \return
    World position table {x, y}
*************************************************************************/
int lua_Grid_GetMouseWorldPerspective(lua_State* L)
{
    if (lua_gettop(L) != 1)
    {
        luaL_error(L, "Expected 1 arguments: perspective");
        return 0;
    }

    GGMath::float32 perspective = static_cast<GGMath::float32>(luaL_checknumber(L, 1));

    auto& input = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

    glm::vec2 pos = input.GetMouseWorldPositionWithPerspective(perspective);

    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, pos.x);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, pos.y);
    lua_settable(L, -3);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.worldToGrid(gridID, worldX, worldY) -> {x, y} or nil
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    World X coordinate (number)
  \param[in] 3 (Lua stack)
    World Y coordinate (number)
  \details
    Converts world coordinates to grid position.
  \return
    Grid position table {x, y} or nil if out of bounds
*************************************************************************/
int lua_Grid_WorldToGrid(lua_State* L)
{
    if (lua_gettop(L) != 3)
    {
        luaL_error(L, "Expected 3 arguments: gridID, worldX, worldY");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    GGMath::float32 worldX = static_cast<GGMath::float32>(luaL_checknumber(L, 2));
    GGMath::float32 worldY = static_cast<GGMath::float32>(luaL_checknumber(L, 3));
    
    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GGMath::Vector2 worldPos(worldX, worldY);
    GGMath::Vector2Int gridPos = manager->WorldToGrid(worldPos, gridID);

    if (gridPos.x < 0 || gridPos.y < 0)
    {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushinteger(L, gridPos.x);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushinteger(L, gridPos.y);
    lua_settable(L, -3);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Grid.gridToWorld(gridID, gridX, gridY) -> {x, y}
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \param[in] 2 (Lua stack)
    Grid X coordinate (integer)
  \param[in] 3 (Lua stack)
    Grid Y coordinate (integer)
  \details
    Converts grid position to world coordinates.
  \return
    World position table {x, y}
*************************************************************************/
int lua_Grid_GridToWorld(lua_State* L)
{
    if (lua_gettop(L) != 3)
    {
        luaL_error(L, "Expected 3 arguments: gridID, gridX, gridY");
        return 0;
    }

    uint32_t gridID = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int32_t gridX = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int32_t gridY = static_cast<int32_t>(luaL_checkinteger(L, 3));

    GridManager* manager = GridManager::GetInstance();
    if (!manager)
    {
        luaL_error(L, "GridManager not initialized");
        return 0;
    }

    GGMath::Vector2Int gridPos(gridX, gridY);
    GGMath::Vector2 worldPos = manager->GridToWorld(gridPos, gridID);

    if (worldPos == GGMath::Vector2(-1000.f, -1000.f))
    {
        lua_pushnil(L);
        return 1;
	}

    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, worldPos.x);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, worldPos.y);
    lua_settable(L, -3);

    return 1;
}

/*************************************************************************
  \brief
    Create Lua table from Tile struct
  \param[in] L
    Lua state
  \param[in] tile
    Tile to convert
  \details
    Pushes tile data as table to Lua stack.
*************************************************************************/
void PushTileToLua(lua_State* L, const Tile& tile)
{
    lua_newtable(L);
    
    // Position
    lua_pushstring(L, "position");
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushinteger(L, tile.position.x);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushinteger(L, tile.position.y);
    lua_settable(L, -3);
    lua_settable(L, -3);
    
    // Convenience booleans
    lua_pushstring(L, "isPassable");
    lua_pushboolean(L, tile.IsPassable());
    lua_settable(L, -3);
    
    lua_pushstring(L, "isVisible");
    lua_pushboolean(L, tile.IsVisible());
    lua_settable(L, -3);
    
    lua_pushstring(L, "isDestroyed");
    lua_pushboolean(L, tile.state & TILE_DESTROYED);
    lua_settable(L, -3);
    
    lua_pushstring(L, "isHighlighted");
    lua_pushboolean(L, tile.state & TILE_HIGHLIGHTED);
    lua_settable(L, -3);
    
    lua_pushstring(L, "isOccupied");
    lua_pushboolean(L, tile.state & TILE_OCCUPIED);
    lua_settable(L, -3);
}

/*************************************************************************
  \brief
    Create Lua table from MovementValidationResult
  \param[in] L
    Lua state
  \param[in] result
    Movement result to convert
  \details
    Pushes movement validation result as table to Lua stack.
*************************************************************************/
void PushMovementResultToLua(lua_State* L, const MovementValidationResult& result)
{
    lua_newtable(L);
    
    lua_pushstring(L, "canMove");
    lua_pushboolean(L, result.canMove);
    lua_settable(L, -3);
    
    lua_pushstring(L, "reason");
    lua_pushstring(L, result.reason.c_str());
    lua_settable(L, -3);
    
    lua_pushstring(L, "validationTimeMs");
    lua_pushnumber(L, result.validationTimeMs);
    lua_settable(L, -3);
    
    lua_pushstring(L, "tilesChecked");
    lua_pushinteger(L, result.tilesChecked);
    lua_settable(L, -3);
    
    lua_pushstring(L, "algorithmUsed");
    switch (result.algorithmUsed)
    {
    case PathfindingAlgorithm::SIMPLE_VALIDATION:
        lua_pushstring(L, "SIMPLE");
        break;
    case PathfindingAlgorithm::BFS_PATHFINDING:
        lua_pushstring(L, "BFS");
        break;
    }
    lua_settable(L, -3);
    
    // Path array
    lua_pushstring(L, "path");
    lua_newtable(L);
    for (size_t i = 0; i < result.path.size(); ++i)
    {
        lua_pushinteger(L, static_cast<int>(i + 1));  // Lua 1-based indexing
        lua_newtable(L);
        lua_pushstring(L, "x");
        lua_pushinteger(L, result.path[i].x);
        lua_settable(L, -3);
        lua_pushstring(L, "y");
        lua_pushinteger(L, result.path[i].y);
        lua_settable(L, -3);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);
}

/*************************************************************************
  \brief
    Extract position array from Lua table
  \param[in] L
    Lua state
  \param[in] index
    Stack index of position array
  \return
    Vector of grid positions
  \details
    Converts Lua array of {x,y} tables to Vector2Int vector.
*************************************************************************/
std::vector<GGMath::Vector2Int> GetPositionArrayFromLua(lua_State* L, int index)
{
    std::vector<GGMath::Vector2Int> positions;
    
    if (!lua_istable(L, index))
    {
        luaL_error(L, "Expected table for positions");
        return positions;
    }
    
    // Get table length using Lua 5.1 compatible method
    size_t length = lua_objlen(L, index);
    positions.reserve(length);
    
    for (size_t i = 1; i <= length; ++i)  // Lua 1-based indexing
    {
        lua_rawgeti(L, index, static_cast<int>(i));
        
        if (lua_istable(L, -1))
        {
            // Get x coordinate
            lua_rawgeti(L, -1, 1);  // Assume array format {x, y}
            int32_t x = static_cast<int32_t>(luaL_checkinteger(L, -1));
            lua_pop(L, 1);
            
            // Get y coordinate
            lua_rawgeti(L, -1, 2);
            int32_t y = static_cast<int32_t>(luaL_checkinteger(L, -1));
            lua_pop(L, 1);
            
            positions.emplace_back(x, y);
        }
        
        lua_pop(L, 1);  // Pop the table
    }
    
    return positions;
}