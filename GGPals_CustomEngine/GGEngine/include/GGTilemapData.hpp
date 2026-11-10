/*************************************************************************
  \file       GGTilemapData.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       15 November 2025
  \version    1.1
  \brief
    Core tilemap data structures for the tactical grid system.
  \details
    Defines fundamental data structures for the 6x4 tactical grid used in
    Order N Chaos. Includes tile data, layer management, and complete tilemap
    representation. Fixed grid dimensions match game requirements for
    turn-based tactical gameplay. Cache-friendly layout using contiguous
    2D arrays with [row][col] indexing to keep row data adjacent in memory.

    NOTE: NO namespace used to avoid collision with GGEngine class.
  \dependencies
    Types.hpp, <vector>, <string>
  \see
    GGTilemapSerializer.hpp, GGTilemapRenderer.hpp, GGAssetManager
  \history
    - 1.0 (15 November 2025): Initial version with fixed 6x4 grid support.
    - 1.1 (15 November 2025): Removed namespace to fix collision.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef GG_TILEMAP_DATA_HPP
#define GG_TILEMAP_DATA_HPP

#pragma region Includes
#include "GGMath/Types.hpp"
#include "GGMath/Vector4.hpp"
#include <vector>
#include <string>
#pragma endregion

#pragma region TileData Structure
/*************************************************************************
  \brief
    Represents a single tile in the tilemap
  \details
    Contains all data needed to render and interact with a tile,
    including visual representation, gameplay properties, and type
    information for the tactical grid system. Designed for efficient
    memory layout and quick access during rendering.

    Uses string path for sprite instead of AssetID for compatibility
    with existing asset management system.
*************************************************************************/
struct GGTileData
{
    std::string spritePath;     // Path to tile sprite asset
    bool isDestructible;        // Can this tile be destroyed?
    int32_t tileType;           // Gameplay type (0=empty, 1=solid, etc.)
    uint32_t variation;         // Visual variation index for same type

    uint32_t spriteID;          // Runtime Asset ID to use
    GGMath::Vector4 colorAdd = { 0.f, 0.f, 0.f, 0.f };
    GGMath::Vector4 colorMultiply = { 1.f, 1.f, 1.f, 1.f };

    /*************************************************************************
      \brief
        Default constructor initializes tile to empty state
      \details
        Creates an empty tile with empty sprite path, non-destructible,
        type 0 (empty), and variation 0.
    *************************************************************************/
    GGTileData()
        : spritePath("")
        , isDestructible(false)
        , tileType(0)
        , variation(0)
        , spriteID(0)
        , colorAdd({ 0.f, 0.f, 0.f, 0.f})
        , colorMultiply({ 1.f, 1.f, 1.f, 1.f })
    {
    }

    /*************************************************************************
      \brief
        Check if tile is empty (has no sprite)
      \return
        True if sprite path is empty
      \par Usage
      \code
        if (tile.IsEmpty())
        {
            // Skip rendering this tile
        }
      \endcode
    *************************************************************************/
    bool IsEmpty() const
    {
        return spritePath.empty();
    }

    void SetColorAdd(GGMath::Vector4 color) { colorAdd = color; }

    void SetColorMultiply(GGMath::Vector4 color) { colorMultiply = color; }
};
#pragma endregion

#pragma region TilemapLayer Structure
/*************************************************************************
  \brief
    Single layer of tiles in the tilemap
  \details
    Fixed 6x4 grid matching game requirements. Supports multiple layers
    for background decoration, gameplay tiles, and foreground effects.
    Uses contiguous 2D array for cache efficiency. [row][col] indexing
    keeps entire rows adjacent in memory for better cache line utilization
    during horizontal scanline rendering.
*************************************************************************/
struct GGTilemapLayer
{
    static constexpr int32_t GRID_WIDTH = 6;
    static constexpr int32_t GRID_HEIGHT = 4;

    std::string layerName;
    GGTileData tiles[GRID_HEIGHT][GRID_WIDTH];  // [row][col] indexing
    bool isVisible;
    bool isLocked;  // Prevent editing in editor

    /*************************************************************************
      \brief
        Default constructor initializes empty layer
      \details
        Creates layer with default name, all tiles empty (via GGTileData
        default constructor), visible, and unlocked for editing.
    *************************************************************************/
    GGTilemapLayer()
        : layerName("New Layer")
        , isVisible(true)
        , isLocked(false)
    {
    }

    /*************************************************************************
      \brief
        Get mutable tile at grid position
      \param[in] row
        Row index (0-3)
      \param[in] col
        Column index (0-5)
      \return
        Reference to tile data at specified position
      \par Usage
      \code
        layer.GetTile(2, 3).isDestructible = true;
        layer.GetTile(0, 0).tileType = 1;
      \endcode
      \par Purpose
        Direct access for tile modification during editor operations
    *************************************************************************/
    GGTileData& GetTile(int32_t row, int32_t col)
    {
        if (row >= GRID_HEIGHT || row < 0 || col >= GRID_WIDTH || col < 0)
        {
            std::cerr << "Error: Out of range access row: " << row << " col: " << col << std::endl;
            return tiles[0][0];
        }
        
        return tiles[row][col];
    }

    /*************************************************************************
      \brief
        Get immutable tile at grid position
      \param[in] row
        Row index (0-3)
      \param[in] col
        Column index (0-5)
      \return
        Const reference to tile data at specified position
      \par Purpose
        Read-only access during rendering and queries
    *************************************************************************/
    const GGTileData& GetTile(int32_t row, int32_t col) const
    {
        if (row >= GRID_HEIGHT || row < 0 || col >= GRID_WIDTH || col < 0)
        {
            std::cerr << "Error: Out of range access row: " << row << " col: " << col << std::endl;
            return tiles[0][0];
        }

        return tiles[row][col];
    }

    /*************************************************************************
      \brief
        Check if grid coordinates are valid
      \param[in] row
        Row index to validate
      \param[in] col
        Column index to validate
      \return
        True if coordinates are within grid bounds, false otherwise
      \par Usage
      \code
        if (GGTilemapLayer::IsValidPosition(row, col))
        {
            layer.GetTile(row, col).tileType = 1;
        }
      \endcode
      \par Purpose
        Bounds checking before tile access to prevent out-of-range errors
    *************************************************************************/
    static bool IsValidPosition(int32_t row, int32_t col)
    {
        return row >= 0 && row < GRID_HEIGHT && col >= 0 && col < GRID_WIDTH;
    }
};
#pragma endregion

#pragma region Tilemap Structure
/*************************************************************************
  \brief
    Complete tilemap containing all layers
  \details
    Manages multiple layers of tiles, metadata, and provides
    the complete data structure for editor workflow and runtime loading.
    Supports dynamic layer addition/removal while maintaining fixed
    grid dimensions per layer.
*************************************************************************/
struct GGTilemap
{
    std::string mapName;
    std::vector<GGTilemapLayer> layers;
    float tileSize;  // Size of each tile in world units

    /*************************************************************************
      \brief
        Default constructor initializes tilemap with one ground layer
      \details
        Creates map with untitled name, single default layer named "Ground",
        and 64-pixel tile size. This provides sensible defaults for new maps.
    *************************************************************************/
    GGTilemap()
        : mapName("Untitled Map")
        , tileSize(1.0f)
    {
        layers.emplace_back();
        layers[0].layerName = "Ground";
    }

    /*************************************************************************
      \brief
        Add a new layer to the tilemap
      \param[in] layerName
        Name for the new layer
      \return
        Index of newly created layer
      \par Usage
      \code
        int bgLayer = tilemap.AddLayer("Background");
        int fgLayer = tilemap.AddLayer("Foreground");
      \endcode
      \par Purpose
        Dynamically add layers during editor session
    *************************************************************************/
    int32_t AddLayer(const std::string& layerName)
    {
        GGTilemapLayer newLayer;
        newLayer.layerName = layerName;
        layers.push_back(newLayer);
        return static_cast<int32_t>(layers.size() - 1);
    }

    /*************************************************************************
      \brief
        Remove layer at index
      \param[in] layerIndex
        Index of layer to remove (0-based)
      \par Usage
      \code
        tilemap.RemoveLayer(2);  // Remove third layer
      \endcode
      \par Purpose
        Delete unwanted layers during editor session. No-op if index invalid.
    *************************************************************************/
    void RemoveLayer(int32_t layerIndex)
    {
        if (layerIndex >= 0 && layerIndex < static_cast<int32_t>(layers.size()))
        {
            layers.erase(layers.begin() + layerIndex);
        }
    }
};
#pragma endregion

#endif // GG_TILEMAP_DATA_HPP