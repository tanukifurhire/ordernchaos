/*************************************************************************
  \file       GGLuaGridBindings.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Exposes Grid system functionality to Lua scripts for runtime control
    of tile-based operations, pathfinding, and visual feedback.
  \details
    This header defines all Lua bindings for the Grid system, including:
    - Grid creation and active grid management
    - Tile queries and manipulation
    - Pathfinding and movement validation
    - Area operations and visual effects
    - Coordinate conversion between world and grid space
    - Helper functions to convert C++ structs to Lua tables
  \dependencies
    - GGGridManager.hpp
    - GGGrid.hpp
    - GGGridPathfinding.hpp
    - Lua 5.x headers (lua.h, lauxlib.h, lualib.h)
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <cstdint>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <GGGridManager.hpp>
#include <GGGrid.hpp>
#include <GGGridPathfinding.hpp>

class GGLuaVM;

inline Grid::GridPathfinding* g_pathfinding = nullptr;

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
void RegisterGridBindings(GGLuaVM& luaVM);

/*************************************************************************
  INTERNAL C FUNCTIONS (Lua C API callbacks)

  These implement the lua_CFunction signature: int(*)(lua_State*)

  Error handling pattern:
  1. Retrieve GridManager singleton
  2. Validate grid IDs and coordinates
  3. Call Grid API with bounds checking
  4. Push results as tables/primitives to Lua stack
  5. Return number of return values

  All errors use luaL_error() for safe script sandboxing.
*************************************************************************/

/*************************************************************************
  GRID MANAGEMENT FUNCTIONS
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Grid.getActiveGrid() -> gridID
  \details
    Gets currently active grid ID.
  \return
    Active grid ID as integer
*************************************************************************/
int lua_Grid_GetActiveGrid(lua_State* L);

/*************************************************************************
  \brief
    Lua: Grid.setActiveGrid(gridID)
  \param[in] 1 (Lua stack)
    Grid ID (integer)
  \details
    Sets the active grid for operations.
*************************************************************************/
int lua_Grid_SetActiveGrid(lua_State* L);

/*************************************************************************
  TILE QUERY FUNCTIONS
*************************************************************************/

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
int lua_Grid_GetTile(lua_State* L);

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
int lua_Grid_GetTilesInRange(lua_State* L);

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
int lua_Grid_GetTilesInArea(lua_State* L);

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
int lua_Grid_HighlightTiles(lua_State* L);

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
int lua_Grid_SetTileTexture(lua_State* L);

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
int lua_Grid_SetTileColorMultiply(lua_State* L);

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
int lua_Grid_SetTileColorAdd(lua_State* L);

/*************************************************************************
  AREA OPERATION FUNCTIONS
*************************************************************************/

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
int lua_Grid_ValidateMovement(lua_State* L);

/*************************************************************************
  \brief
    Lua: Grid.setPathfindingAlgorithm(algorithm)
  \param[in] 1 (Lua stack)
    Algorithm name (string: "SIMPLE" or "BFS")
  \details
    Sets pathfinding algorithm for future validations.
*************************************************************************/
int lua_Grid_SetPathfindingAlgorithm(lua_State* L);

/*************************************************************************
  COORDINATE CONVERSION FUNCTIONS
*************************************************************************/

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
int lua_Grid_GetMouseWorldPerspective(lua_State* L);

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
int lua_Grid_WorldToGrid(lua_State* L);

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
int lua_Grid_GridToWorld(lua_State* L);

/*************************************************************************
  HELPER FUNCTIONS
*************************************************************************/

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
void PushTileToLua(lua_State* L, const Grid::Tile& tile);

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
void PushMovementResultToLua(lua_State* L, const Grid::MovementValidationResult& result);

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
std::vector<GGMath::Vector2Int> GetPositionArrayFromLua(lua_State* L, int index);