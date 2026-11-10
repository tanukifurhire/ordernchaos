/*************************************************************************
  \file       TilemapEditorState.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       16 November 2025
  \version    1.2
  \brief
    State management for tilemap editor.
  \details
    Defines data structures for editor state including current tool,
    selected tiles, active layer, and file metadata. Provides centralized
    state for the tilemap editor UI and operations.

    NOTE: NO namespace used to avoid collision with GGEngine class.
  \dependencies
    GGTilemapData.hpp, GGMath/Vector2.hpp, <string>
  \see
    TilemapNode.hpp, GGTilemapData.hpp
  \history
    - 1.0 (14 November 2025): Initial version.
    - 1.1 (15 November 2025): Removed namespace to fix collision.
    - 1.2 (16 November 2025): Added Select tool tile tracking (inspectedTileRow,
                              inspectedTileCol, hasTileSelected) for Option B.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef TILEMAP_EDITOR_STATE_HPP
#define TILEMAP_EDITOR_STATE_HPP

#pragma region Includes
#include "GGTilemapData.hpp"
#include "GGMath/Vector2.hpp"
#include <string>
#include <vector>
#pragma endregion

#pragma region Editor Tool Enum
/*************************************************************************
  \brief
    Available editing tools in tilemap editor
*************************************************************************/
enum class EditorTool
{
    Select,     // Selection tool for editing properties
    Paint,      // Paint tool for placing tiles
    Erase,      // Erase tool for removing tiles
    Fill        // Fill tool for flood-fill operations
};
#pragma endregion

#pragma region Tile Palette Entry
/*************************************************************************
  \brief
    Entry in tile palette representing an available tile type
  \details
    Contains metadata for tiles that can be placed in the map.
    Stores default properties and display information for editor UI.
*************************************************************************/
struct TilePaletteEntry
{
    std::string spritePath;
    std::string displayName;
    uint32_t spriteID;
    int32_t defaultTileType;
    bool defaultDestructible;
    uint32_t defaultVariation;

    /*************************************************************************
      \brief
        Default constructor creates empty palette entry
    *************************************************************************/
    TilePaletteEntry()
        : spritePath("")
        , displayName("Empty")
        , spriteID(0)
        , defaultTileType(0)
        , defaultDestructible(false)
        , defaultVariation(0)
    {
    }

    /*************************************************************************
      \brief
        Constructor with parameters
      \param[in] path
        Path to tile sprite
      \param[in] name
        Display name in palette
      \param[in] type
        Default tile type for gameplay
      \param[in] destructible
        Default destructible flag
      \param[in] variation
        Default visual variation index
    *************************************************************************/
    TilePaletteEntry(const std::string& path, const std::string& name,
        uint32_t id, int32_t type, bool destructible, uint32_t variation = 0)
        : spritePath(path)
        , displayName(name)
        , spriteID(id)
        , defaultTileType(type)
        , defaultDestructible(destructible)
        , defaultVariation(variation)
    {
    }
};
#pragma endregion

#pragma region Tilemap Editor State
/*************************************************************************
  \brief
    Complete state for tilemap editor
  \details
    Maintains current tool selection, active layer, selected tiles,
    file information, and dirty flag for unsaved changes. Centralized
    state makes editor operations and UI rendering straightforward.
*************************************************************************/
struct TilemapEditorState
{
    GGTilemap currentTilemap;
    std::string currentFilePath;
    uint32_t currentTextureID;
    bool isDirty;

    EditorTool currentTool;
    int32_t selectedLayerIndex;
    TilePaletteEntry selectedTile;
    std::vector<TilePaletteEntry> tilePalette;

    // Canvas view state
    GGMath::Vector2 canvasOffset;
    float canvasZoom{ 64.f };

    // Selected tile for inspection (Select tool - Option B)
    int32_t inspectedTileRow;
    int32_t inspectedTileCol;
    bool hasTileSelected;

    /*************************************************************************
      \brief
        Default constructor initializes editor to default state
    *************************************************************************/
    TilemapEditorState()
        : currentTilemap()
        , currentFilePath("")
        , currentTextureID(0)
        , isDirty(false)
        , currentTool(EditorTool::Paint)
        , selectedLayerIndex(0)
        , selectedTile()
        , canvasOffset(0.0f, 0.0f)
        , canvasZoom(64.0f)
        , inspectedTileRow(-1)
        , inspectedTileCol(-1)
        , hasTileSelected(false)
    {
    }
};
#pragma endregion

#endif // TILEMAP_EDITOR_STATE_HPP