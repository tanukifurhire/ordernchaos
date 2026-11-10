/*************************************************************************
  \file       GGGrid.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       05 November 2025
  \version    1.1
  \brief
    Defines the Grid component and tile structures for Order N Chaos.
  \details
    This file implements the grid system for the turn-based tactical game.
    It includes tile states, colors, and grid configuration for managing
    the 6x4 game boards. Supports tile destruction, fog of war, and
    color-based ability casting mechanics.
  \dependencies
    GGMath/Vector2Int.hpp, GGMath/Types.hpp, GGECSComponents.hpp
  \see
    GGGridManager, GGECSComponents
  \history
    - 1.0 (05 November 2025): Initial version.
    - 1.1 (05 November 2025): Fixed include paths and type prefixes.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include "GGMath/Vector2Int.hpp"
#include "GGMath/Types.hpp"
#include <GGTilemapData.hpp>

#include <vector>
#include <memory>
#include <unordered_map>

namespace Grid
{
    /*************************************************************************
      \brief
        Tile color enumeration for gameplay mechanics
      \details
        Each color provides different ability effects:
        - Blue: Defense and vision abilities
        - Green: Healing and utility abilities
    *************************************************************************/
    enum class TileColor : uint8_t
    {
        BLUE = 0,    // Defense/Vision abilities
        GREEN = 1,   // Healing/Utility abilities
        NONE = 255   // No color (destroyed tile)
    };

    /*************************************************************************
      \brief
        Tile state flags for gameplay mechanics
      \details
        Bitfield flags allow combining multiple states
    *************************************************************************/
    enum TileState : uint8_t
    {
        TILE_NORMAL = 0,        // Normal tile state
        TILE_DESTROYED = 1,     // Tile is destroyed (blocks movement)
        TILE_FOGGED = 2,        // Hidden by fog of war
        TILE_REVEALED = 4,      // Permanently revealed
        TILE_HIGHLIGHTED = 8,   // Highlighted for movement/ability range
        TILE_OCCUPIED = 16,     // Has a unit on it
        TILE_TARGETED = 32      // Targeted by ability
    };

    /*************************************************************************
      \brief
        Individual tile data structure
      \details
        Represents a single tile in the game grid with health,
        color, and state information for gameplay mechanics
    *************************************************************************/
    struct Tile
    {
        // Core tile properties
        GGMath::Vector2Int position;        // Grid position (x, y)
        TileColor color;                    // Tile color for abilities
        uint8_t state;                      // Bitfield of TileState flags
        int32_t health;                     // Tile health (0 = destroyed)
        int32_t maxHealth;                  // Maximum tile health

        // Unit reference (if occupied)
        uint32_t unitID;                    // Entity ID of unit on tile (0 = empty)

        // Visual properties
        GGMath::float32 damageFlashTimer;   // Visual feedback timer
        GGMath::float32 highlightIntensity; // Highlight animation intensity

        /*************************************************************************
          \brief
            Default constructor - creates normal tile
          \par Usage
          \code
            Tile newTile;  // Creates blue tile with full health
          \endcode
        *************************************************************************/
        Tile() :
            position(0, 0),
            color(TileColor::BLUE),
            state(TILE_NORMAL | TILE_FOGGED),
            health(1),
            maxHealth(1),
            unitID(0),
            damageFlashTimer(0.0f),
            highlightIntensity(0.0f)
        {
        }

        /*************************************************************************
          \brief
            Check if tile is passable for unit movement
          \return
            True if units can move through this tile
          \par Usage
          \code
            if (tile.IsPassable())
                unit.MoveTo(tile.position);
          \endcode
        *************************************************************************/
        bool IsPassable() const
        {
            return !(state & TILE_DESTROYED) && health > 0;
        }

        /*************************************************************************
          \brief
            Check if tile is visible to player
          \return
            True if tile is not hidden by fog of war
        *************************************************************************/
        bool IsVisible() const
        {
            return !(state & TILE_FOGGED) || (state & TILE_REVEALED);
        }

        /*************************************************************************
          \brief
            Apply damage to tile
          \param[in] damage
            Amount of damage to apply
          \return
            True if tile was destroyed by damage
        *************************************************************************/
        bool TakeDamage(int32_t damage)
        {
            health -= damage;
            damageFlashTimer = 0.5f; // Flash for half second

            if (health <= 0)
            {
                health = 0;
                state |= TILE_DESTROYED;
                state &= ~TILE_OCCUPIED;  // Remove unit if destroyed
                unitID = 0;
                color = TileColor::NONE;
                return true;
            }
            return false;
        }

        /*************************************************************************
          \brief
            Reveal tile permanently (remove fog of war)
        *************************************************************************/
        void Reveal()
        {
            state |= TILE_REVEALED;
            state &= ~TILE_FOGGED;
        }
    };

    /*************************************************************************
      \brief
        Grid component for game entities
      \details
        Manages a 6x4 grid of tiles for each player's board.
        Handles tile states, unit positions, and grid-based mechanics.
    *************************************************************************/
    class GridComponent
    {
    public:
        // Grid dimensions (from game design doc)
        static constexpr int32_t GRID_WIDTH = 6;
        static constexpr int32_t GRID_HEIGHT = 4;
        static constexpr int32_t TOTAL_TILES = GRID_WIDTH * GRID_HEIGHT;

    private:
        // Grid data
        std::vector<Tile> tiles;                    // Flat array of tiles
        bool isOrderSide;                           // True = Order, False = Chaos
        uint32_t ownerEntityID;                     // Entity that owns this grid
        GGMath::Vector2 position;                   // World position of grid

        // Grid state
        int32_t destroyedTileCount;                 // Track for win condition
        bool gridActive;                            // Is grid currently active

        // Color distribution for random generation
        std::vector<GGMath::Vector2Int> blueTiles;  // Positions of blue tiles
        std::vector<GGMath::Vector2Int> greenTiles; // Positions of green tiles

        // Tilemap (for rendering)
        GGTilemap tilemap;

    public:
        /*************************************************************************
          \brief
            Constructor - creates grid with random color distribution
          \param[in] isOrder
            True for Order side, false for Chaos side
          \param[in] ownerID
            Entity ID of the grid owner
        *************************************************************************/
        GridComponent(bool isOrder = true, uint32_t ownerID = 0) :
            isOrderSide(isOrder),
            ownerEntityID(ownerID),
            destroyedTileCount(0),
            gridActive(true)
        {
            tiles.reserve(TOTAL_TILES);

            // Create tiles with checkerboard pattern (temporary)
            for (int32_t y = 0; y < GRID_HEIGHT; ++y)
            {
                for (int32_t x = 0; x < GRID_WIDTH; ++x)
                {
                    Tile tile;
                    tile.position = GGMath::Vector2Int(x, y);

                    // Simple alternating pattern
                    if ((x + y) % 2 == 0)
                        tile.color = TileColor::BLUE;
                    else
                        tile.color = TileColor::GREEN;

                    // Track color positions
                    if (tile.color == TileColor::BLUE)
                        blueTiles.push_back(tile.position);
                    else
                        greenTiles.push_back(tile.position);

                    // Set initial fog of war
                    tile.state = TILE_NORMAL | TILE_FOGGED;
                    tile.health = tile.maxHealth = 2;  // 2 HP per tile for M2

                    tiles.push_back(tile);
                }
            }
        }

        void SetTileMap(GGTilemap const& tilemapVal)
        {
            tilemap.layers = tilemapVal.layers;
            tilemap.mapName = tilemapVal.mapName;
            tilemap.tileSize = tilemapVal.tileSize;
        }

        GGTilemap& GetTileMap()
        {
            return tilemap;
        }

        GGMath::Vector2 GetPosition() const { return position; }

        void SetPosition(GGMath::Vector2 pos) { position = pos; }

        /*************************************************************************
          \brief
            Get tile at specific grid position
          \param[in] x
            X coordinate (0-5)
          \param[in] y
            Y coordinate (0-3)
          \return
            Pointer to tile, nullptr if out of bounds
        *************************************************************************/
        Tile* GetTile(int32_t x, int32_t y)
        {
            if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT)
                return nullptr;

            int32_t index = y * GRID_WIDTH + x;
            return &tiles[index];
        }

        /*************************************************************************
          \brief
            Get tile at vector position
          \param[in] pos
            Grid position as Vector2Int
          \return
            Pointer to tile, nullptr if out of bounds
        *************************************************************************/
        Tile* GetTile(const GGMath::Vector2Int& pos)
        {
            return GetTile(pos.x, pos.y);
        }

        /*************************************************************************
          \brief
            Get all tiles within range of a position
          \param[in] center
            Center position
          \param[in] range
            Manhattan distance range
          \return
            Vector of tile pointers within range
        *************************************************************************/
        std::vector<Tile*> GetTilesInRange(const GGMath::Vector2Int& center, int32_t range)
        {
            std::vector<Tile*> result;

            for (auto& tile : tiles)
            {
                if (tile.position.manhattanDistance(center) <= range)
                {
                    result.push_back(&tile);
                }
            }

            return result;
        }

        /*************************************************************************
          \brief
            Get tiles in a rectangular area
          \param[in] center
            Center position
          \param[in] width
            Width of area
          \param[in] height
            Height of area
          \return
            Vector of tiles in the area
        *************************************************************************/
        std::vector<Tile*> GetTilesInArea(const GGMath::Vector2Int& center,
            int32_t width, int32_t height)
        {
            std::vector<Tile*> result;

            int32_t halfWidth = width / 2;
            int32_t halfHeight = height / 2;

            for (int32_t y = center.y - halfHeight; y <= center.y + halfHeight; ++y)
            {
                for (int32_t x = center.x - halfWidth; x <= center.x + halfWidth; ++x)
                {
                    Tile* tile = GetTile(x, y);
                    if (tile)
                        result.push_back(tile);
                }
            }

            return result;
        }

        /*************************************************************************
          \brief
            Highlight tiles for ability/movement range
          \param[in] tiles
            Vector of tiles to highlight
          \param[in] clear
            Clear previous highlights first
        *************************************************************************/
        void HighlightTiles(const std::vector<Tile*>& tilesToHighlight, bool clear = true)
        {
            if (clear)
            {
                // Clear all highlights
                for (auto& tile : tiles)
                {
                    tile.state &= ~TILE_HIGHLIGHTED;
                    tile.highlightIntensity = 0.0f;
                }
            }

            // Set new highlights
            for (auto* tile : tilesToHighlight)
            {
                if (tile)
                {
                    tile->state |= TILE_HIGHLIGHTED;
                    tile->highlightIntensity = 1.0f;
                }
            }
        }

        /*************************************************************************
          \brief
            Check if all tiles are destroyed (win condition)
          \return
            True if all tiles are destroyed
        *************************************************************************/
        bool IsGridDestroyed() const
        {
            return destroyedTileCount >= TOTAL_TILES;
        }

        /*************************************************************************
          \brief
            Get percentage of grid destroyed
          \return
            Percentage (0.0 to 1.0) of tiles destroyed
        *************************************************************************/
        GGMath::float32 GetDestructionPercentage() const
        {
            return static_cast<GGMath::float32>(destroyedTileCount) / TOTAL_TILES;
        }

        /*************************************************************************
          \brief
            Apply fog of war to entire grid
        *************************************************************************/
        void ApplyFogOfWar()
        {
            for (auto& tile : tiles)
            {
                if (!(tile.state & TILE_REVEALED))
                {
                    tile.state |= TILE_FOGGED;
                }
            }
        }

        /*************************************************************************
          \brief
            Reveal tiles in cross pattern (vision ability)
          \param[in] center
            Center of cross pattern
          \param[in] size
            Size of cross arms
        *************************************************************************/
        void RevealCrossPattern(const GGMath::Vector2Int& center, int32_t size)
        {
            // Reveal horizontal line
            for (int32_t x = center.x - size; x <= center.x + size; ++x)
            {
                Tile* tile = GetTile(x, center.y);
                if (tile) tile->Reveal();
            }

            // Reveal vertical line
            for (int32_t y = center.y - size; y <= center.y + size; ++y)
            {
                Tile* tile = GetTile(center.x, y);
                if (tile) tile->Reveal();
            }
        }

        // Getters
        bool IsOrderGrid() const { return isOrderSide; }
        uint32_t GetOwnerID() const { return ownerEntityID; }
        const std::vector<Tile>& GetAllTiles() const { return tiles; }
        std::vector<Tile>& GetAllTiles() { return tiles; }
        bool IsActive() const { return gridActive; }
        void SetActive(bool active) { gridActive = active; }
    };
}