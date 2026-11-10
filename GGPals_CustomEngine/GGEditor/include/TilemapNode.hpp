/*************************************************************************
  \file       TilemapNode.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       16 November 2025
  \version    1.5
  \brief
    Tilemap editor node for visual map editing.
  \details
    Provides complete ImGui-based tilemap editor for creating and editing
    tilemaps. Follows EditorSceneBase pattern for integration into existing
    editor. Features include tile palette, layer management, canvas with
    pan/zoom, and file operations (new, open, save). Designed for 6x4
    tactical grid used in Order N Chaos gameplay.
  \dependencies
    TilemapEditorState.hpp, GGTilemapRenderer.hpp, GGTilemapSerializer.hpp,
    EditorSceneBase.hpp, GGAssetManager.hpp, SimpleFileDialog.hpp, imgui.h
  \see
    TilemapEditorState.hpp, GGTilemapData.hpp, GGTilemapRenderer.hpp
  \history
    - 1.0 (13 November 2025): Initial version with basic editing tools.
    - 1.1 (14 November 2025): Added Select tool implementation.
    - 1.2 (14 November 2025): Added comprehensive validation system.
    - 1.3 (15 November 2025): Added error dialog with ImGui modal popups.
    - 1.4 (16 November 2025): Integration with TilePropertiesConfig.
    - 1.5 (16 November 2025): Added SimpleFileDialog include and m_fileDialog member.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef TILEMAP_NODE_HPP
#define TILEMAP_NODE_HPP

#pragma region Includes
#include "TilemapEditorState.hpp"
#include "GGTilemapRenderer.hpp"
#include "GGTilemapSerializer.hpp"
#include "EditorSceneBase.hpp"
#include "GGAssetManager.hpp"
#include "SimpleFileDialog.hpp"
#include <EditorEventBus.hpp>
#include <imgui.h>
#include <memory>
#pragma endregion

/*************************************************************************
  \brief
    Tilemap editor node for visual map creation
  \details
    Provides complete editor interface for designing tactical grid maps.
    Integrates with existing editor framework through EditorSceneBase.
    Supports multiple layers, tile painting, and property editing.
*************************************************************************/
class TilemapNode : public EditorSceneBase
{
public:
    /*************************************************************************
      \brief
        Default constructor
    *************************************************************************/
    TilemapNode();

    /*************************************************************************
      \brief
        Virtual destructor
    *************************************************************************/
    virtual ~TilemapNode() = default;

    /*************************************************************************
      \brief
        Initialize editor node - implements EditorSceneBase
      \par Purpose
        Setup renderer, asset manager, and default state
    *************************************************************************/
    virtual void Init() override;

    /*************************************************************************
      \brief
        Build docking layout - implements EditorSceneBase
      \par Purpose
        Register tilemap editor window in dock system
    *************************************************************************/
    virtual void DockBuild() override;

    /*************************************************************************
      \brief
        Update editor UI - implements EditorSceneBase
      \par Purpose
        Render editor windows and handle user input each frame
    *************************************************************************/
    virtual void Update() override;

    /*************************************************************************
      \brief
        Initialize editor with dependencies
      \param[in] renderer
        Pointer to tilemap renderer
      \param[in] assetManager
        Pointer to asset manager for loading sprites
      \par Usage
      \code
        GGTilemapRenderer renderer;
        GGAssetManager assetMgr;
        node.Initialize(&renderer, &assetMgr);
      \endcode
      \par Purpose
        Manual initialization with required systems
    *************************************************************************/
    void Initialize(GGTilemapRenderer* renderer, GGAssetManager* assetManager);

private:
#pragma region UI Rendering
    /*************************************************************************
      \brief
        Render main editor window containing all panels
    *************************************************************************/
    void RenderEditorWindow();

    /*************************************************************************
      \brief
        Render toolbar with file operations and tool selection
    *************************************************************************/
    void RenderToolbar();

    /*************************************************************************
      \brief
        Render tile palette panel for tile selection
    *************************************************************************/
    void RenderTilePalette();

    /*************************************************************************
      \brief
        Render canvas where tilemap is drawn and edited
    *************************************************************************/
    void RenderCanvas();

    /*************************************************************************
      \brief
        Render layer management panel
    *************************************************************************/
    void RenderLayersPanel();

    /*************************************************************************
      \brief
        Render properties panel for selected tiles
    *************************************************************************/
    void RenderPropertiesPanel();
#pragma endregion

#pragma region Input Handling
    /*************************************************************************
      \brief
        Handle mouse input on canvas for painting/erasing
      \param[in] mousePos
        Mouse position in canvas space
      \param[in] isLeftClick
        True if left mouse button pressed
      \param[in] isRightClick
        True if right mouse button pressed
    *************************************************************************/
    void HandleCanvasInput(const ImVec2& mousePos, bool isLeftClick, bool isRightClick);

    /*************************************************************************
      \brief
        Convert screen space mouse position to grid coordinates
      \param[in] mousePos
        Mouse position in screen space
      \param[out] outRow
        Grid row index
      \param[out] outCol
        Grid column index
      \return
        True if mouse is over valid grid cell
    *************************************************************************/
    bool ScreenToGrid(const ImVec2& mousePos, int32_t& outRow, int32_t& outCol);
#pragma endregion

#pragma region Editing Operations
    /*************************************************************************
      \brief
        Paint tile at grid position with selected tile
      \param[in] row
        Grid row index
      \param[in] col
        Grid column index
    *************************************************************************/
    void PaintTile(int32_t row, int32_t col);

    /*************************************************************************
      \brief
        Erase tile at grid position
      \param[in] row
        Grid row index
      \param[in] col
        Grid column index
    *************************************************************************/
    void EraseTile(int32_t row, int32_t col);

    /*************************************************************************
      \brief
        Fill connected tiles with selected tile (flood fill)
      \param[in] row
        Starting grid row
      \param[in] col
        Starting grid column
    *************************************************************************/
    void FillTiles(int32_t row, int32_t col);
#pragma endregion

#pragma region File Operations
    /*************************************************************************
      \brief
        Create new tilemap (with unsaved changes confirmation)
    *************************************************************************/
    void NewTilemap();

    /*************************************************************************
      \brief
        Open tilemap from file
    *************************************************************************/
    void OpenTilemap();

    /*************************************************************************
      \brief
        Save tilemap to current file path
    *************************************************************************/
    void SaveTilemap();

    /*************************************************************************
      \brief
        Save tilemap to new file path
    *************************************************************************/
    void SaveTilemapAs();

    /*************************************************************************
      \brief
        Refresh tile palette from AssetManager
      \par Purpose
        Reloads all available textures from AssetManager into palette.
        Useful after adding new textures to Assets/Textures/ directory.
    *************************************************************************/
    void RefreshPalette();

    /*************************************************************************
      \brief
        Validate tilemap before saving
      \param[out] missingTextures
        Vector to store paths of missing textures
      \return
        True if all textures exist, false if any are missing
      \par Purpose
        Checks all tiles in all layers to ensure texture paths exist in AssetManager.
    *************************************************************************/
    bool ValidateTilemap(std::vector<std::string>& missingTextures);

    /*************************************************************************
      \brief
        Check if texture path exists in AssetManager
      \param[in] texturePath
        Path to texture to validate
      \return
        True if texture exists and is loaded
    *************************************************************************/
    bool IsTextureLoaded(const std::string& texturePath);

    /*************************************************************************
      \brief
        Validate map dimensions (6x4 grid enforcement)
      \param[out] errors
        Vector to store dimension error messages
      \return
        True if dimensions valid
    *************************************************************************/
    bool ValidateDimensions(std::vector<std::string>& errors);

    /*************************************************************************
      \brief
        Validate layer requirements (at least 1 layer)
      \param[out] errors
        Vector to store layer error messages
      \return
        True if layer count valid
    *************************************************************************/
    bool ValidateLayers(std::vector<std::string>& errors);

    /*************************************************************************
      \brief
        Validate tile bounds (all tiles within 0-5, 0-3)
      \param[out] errors
        Vector to store bounds error messages
      \return
        True if all tiles within bounds
    *************************************************************************/
    bool ValidateTileBounds(std::vector<std::string>& errors);

    /*************************************************************************
      \brief
        Show error dialog with validation results
      \param[in] title
        Dialog title
      \param[in] errors
        List of error messages to display
    *************************************************************************/
    void ShowErrorDialog(const std::string& title, const std::vector<std::string>& errors);
#pragma endregion

#pragma region Members
    TilemapEditorState m_state;
    GGTilemapRenderer* m_renderer;
    GGAssetManager* m_assetManager;

    // Canvas view state
    ImVec2 m_canvasSize;
    ImVec2 m_canvasPos;

    // Error dialog state
    bool m_showErrorDialog;
    std::string m_errorDialogTitle;
    std::vector<std::string> m_errorDialogMessages;

    // File dialog
    SimpleFileDialog m_fileDialog;

    // Events
    Callback<uint32_t> onTilemapOpenCallback;
#pragma endregion
};

/*************************************************************************
  \brief
    Get singleton instance of tilemap editor node
  \return
    Reference to unique_ptr of TilemapNode
  \par Purpose
    Ensures single editor node instance in application
*************************************************************************/
inline auto& GetTilemapNode()
{
    static std::unique_ptr<TilemapNode> tilemapNode =
        std::make_unique<TilemapNode>();
    return tilemapNode;
}

#endif // TILEMAP_NODE_HPP