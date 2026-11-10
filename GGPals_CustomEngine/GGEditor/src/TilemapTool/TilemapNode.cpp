/*************************************************************************
  \file       TilemapNode.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       30 November 2025
  \version    2.2
  \brief
    Implementation of tilemap editor UI and functionality.
  \details
    Implements complete ImGui-based tilemap editor with tile palette,
    layer management, canvas editing, and file operations. Provides
    visual authoring tools for creating 6x4 tactical grid maps for
    Order N Chaos gameplay.
  \dependencies
    TilemapNode.hpp, GGLogger.hpp, GGInputManager.hpp, GGSystemManagers.hpp, imgui.h
  \see
    TilemapNode.hpp, GGTilemapData.hpp
  \history
    - 1.0 (13 November 2025): Initial implementation.
    - 1.1 (13 November 2025): Select tool with tile inspection.
    - 1.2 (13 November 2025): Yellow border highlight for selected tiles.
    - 1.3 (14 November 2025): Comprehensive validation system.
    - 1.4 (14 November 2025): Error dialog rendering and validation pipeline.
    - 1.5 (15 November 2025): Fixed GetTextureID -> GetTexture for AssetManager API.
    - 1.6 (15 November 2025): Fixed ImTextureID cast (void*)(intptr_t) pattern.
    - 1.7 (15 November 2025): Added Refresh Palette button, auto-loads textures.
    - 1.8 (16 November 2025): Fixed DockBuild, auto texture scanning.
    - 1.9 (16 November 2025): Added file dialog integration for Open/Save.
    - 2.0 (16 November 2025): Canvas rendering checks IsTextureLoaded() before GetTexture().
    - 2.1 (16 November 2025): IsTextureLoaded() properly checks cache only (no GetTexture call).
    - 2.2 (30 November 2025): Fixed Fill tool not rendering tiles (missing spriteID assignment).
                          Fixed crash in Select tool when inspecting populated tiles (wrong
                          format specifier %s changed to %u for spriteID in RenderPropertiesPanel).
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma region Includes
#include "TilemapNode.hpp"
#include "GGLogger.hpp"
#include "GGInputManager.hpp"
#include "GGEngine.hpp"
#include "GGSystemManagers.hpp"
#include <GGECSAssetInterface.hpp>

#include <algorithm>
#include <queue>
#include <set>
#include <fstream>
#include <imgui_internal.h>
#pragma endregion


/*************************************************************************
  \brief
    Default constructor
*************************************************************************/
TilemapNode::TilemapNode()
    : m_renderer(nullptr)
    , m_assetManager(nullptr)
    , m_canvasSize(800.0f, 600.0f)
    , m_canvasPos(0.0f, 0.0f)
    , m_showErrorDialog(false)
    , m_errorDialogTitle("")
{
    initCallbackHandle = [this]() -> void { this->Init(); };
    updateCallbackHandle = [this]() -> void { this->Update(); };
    dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
    Initialize editor
*************************************************************************/
void TilemapNode::Init()
{
    GGLogger::Info("[TilemapEditor] Initializing TilemapNode");

    // Get asset manager from engine
    m_assetManager = GGEngine::GetEngine().GetAssetManager();

    // Initialize with default state
    m_state = TilemapEditorState();

    // Subscribe to tilemap open event
    onTilemapOpenCallback =
        [&](uint32_t id)
        {
            if (id == 0) return;

            m_state.currentTilemap = GGECSStandardAssetManager<GGTilemap>::GetAsset(id);
            auto uuidOpt = GGECSStandardAssetManager<GGTilemap>::GetUUIDFromID(id);
            if (uuidOpt.has_value())
            {
                m_state.currentFilePath =
                    GetEngineInstance().GetAssetManager()->ResolvePath(
                        GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value())
                    );
            }
        };

    EditorEventBus::onTilemapOpen += onTilemapOpenCallback;

    RefreshPalette();
}

/*************************************************************************
  \brief
    Initialize editor with dependencies
*************************************************************************/
void TilemapNode::Initialize(GGTilemapRenderer* renderer, GGAssetManager* assetManager)
{
    m_renderer = renderer;
    m_assetManager = assetManager;

    GGLogger::Info("TilemapNode dependencies initialized");
}

/*************************************************************************
  \brief
    Build docking layout
*************************************************************************/
void TilemapNode::DockBuild()
{
    ImGui::DockBuilderDockWindow("Tilemap Designer",
        Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockBottom);
}

#pragma endregion

#pragma region Update and Main Rendering

/*************************************************************************
  \brief
    Update editor UI
*************************************************************************/
void TilemapNode::Update()
{
    RenderEditorWindow();

    // Render file dialog and handle file selection
    if (m_fileDialog.Render())
    {
        std::string selectedPath = m_fileDialog.GetSelectedPath();

        // Check if file exists to determine if this is Open or Save operation
        std::ifstream testFile(selectedPath);
        bool fileExists = testFile.good();
        testFile.close();

        if (fileExists)
        {
            // File exists - Open operation
            GGLogger::Info("[TilemapEditor] Loading file: " + selectedPath);

            GGTilemap loadedTilemap;
            if (GGTilemapSerializer::LoadFromFile(selectedPath, loadedTilemap))
            {
                m_state.currentTilemap = loadedTilemap;
                m_state.currentFilePath = selectedPath;

                m_state.isDirty = false;
                m_state.selectedLayerIndex = 0;

                GGLogger::Info("[TilemapEditor] Opened tilemap: " + selectedPath);
            }
            else
            {
                GGLogger::Error("[TilemapEditor] Failed to open tilemap: " + selectedPath);
            }
        }
        else
        {
            // File doesn't exist - Save operation
            GGLogger::Info("[TilemapEditor] Saving file: " + selectedPath);

            if (GGTilemapSerializer::SaveToFile(m_state.currentTilemap, selectedPath))
            {
                m_state.currentFilePath = selectedPath;
                m_state.isDirty = false;

                GGLogger::Info("[TilemapEditor] Saved tilemap as: " + selectedPath);
            }
            else
            {
                GGLogger::Error("[TilemapEditor] Failed to save tilemap: " + selectedPath);
            }
        }
    }
}

/*************************************************************************
  \brief
    Render main editor window
*************************************************************************/
void TilemapNode::RenderEditorWindow()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Tilemap Designer", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar))
    {
        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New", "Ctrl+N"))
                {
                    NewTilemap();
                }
                if (ImGui::MenuItem("Open", "Ctrl+O"))
                {
                    OpenTilemap();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    SaveTilemap();
                }
                if (ImGui::MenuItem("Save As", "Ctrl+Shift+S"))
                {
                    SaveTilemapAs();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Main editor layout
        RenderToolbar();
        ImGui::Columns(3, "EditorColumns", true);

        // Left column - Tile Palette
        RenderTilePalette();
        ImGui::NextColumn();

        // Center column - Canvas
        RenderCanvas();
        ImGui::NextColumn();

        // Right column - Layers and Properties
        RenderLayersPanel();
        RenderPropertiesPanel();

        ImGui::Columns(1);

        // Render error dialog if needed
        if (m_showErrorDialog)
        {
            ImGui::OpenPopup("Validation Error");
            m_showErrorDialog = false; // Only open once
        }

        if (ImGui::BeginPopupModal("Validation Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_errorDialogTitle.c_str());
            ImGui::Separator();

            ImGui::BeginChild("ErrorList", ImVec2(500, 300), true);
            for (const auto& error : m_errorDialogMessages)
            {
                ImGui::TextWrapped("%s", error.c_str());
            }
            ImGui::EndChild();

            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                m_errorDialogMessages.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

#pragma endregion

#pragma region UI Panels

/*************************************************************************
  \brief
    Render toolbar
*************************************************************************/
void TilemapNode::RenderToolbar()
{
    ImGui::Text("Tools:");
    ImGui::SameLine();

    // Tool selection buttons
    if (ImGui::RadioButton("Select", m_state.currentTool == EditorTool::Select))
    {
        m_state.currentTool = EditorTool::Select;
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Paint", m_state.currentTool == EditorTool::Paint))
    {
        m_state.currentTool = EditorTool::Paint;
        m_state.hasTileSelected = false;  // Clear selection highlight
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Erase", m_state.currentTool == EditorTool::Erase))
    {
        m_state.currentTool = EditorTool::Erase;
        m_state.hasTileSelected = false;  // Clear selection highlight
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Fill", m_state.currentTool == EditorTool::Fill))
    {
        m_state.currentTool = EditorTool::Fill;
        m_state.hasTileSelected = false;  // Clear selection highlight
    }

    ImGui::Separator();
}

/*************************************************************************
  \brief
    Render tile palette
*************************************************************************/
void TilemapNode::RenderTilePalette()
{
    ImGui::BeginChild("Palette", ImVec2(0, 0), true);
    ImGui::Text("Tile Palette");

    // Refresh button
    if (ImGui::Button("Refresh Palette"))
    {
        RefreshPalette();
    }

    ImGui::Separator();

    // Display available tiles
    if (m_state.tilePalette.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No tiles loaded");
        ImGui::Text("Add tiles in Init()");
    }
    else
    {
        const float thumbnailSize = 64.0f;

        for (size_t i = 0; i < m_state.tilePalette.size(); ++i)
        {
            const auto& entry = m_state.tilePalette[i];

            // Draw thumbnail if texture available
            if (!entry.spriteID == 0)
            {
                GLuint textureID = GGECSStandardAssetManager<GLuint>::GetAsset(entry.spriteID);
                if (textureID != 0 && GGECSStandardAssetManager<GLuint>::HasAsset(entry.spriteID))
                {
                    // Clickable image button
                    bool isSelected = (m_state.selectedTile.spritePath == entry.spritePath);
                    ImVec4 tintColor = isSelected ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                    if (ImGui::ImageButton(entry.displayName.c_str(),
                        (ImTextureID)(intptr_t)textureID,
                        ImVec2(thumbnailSize, thumbnailSize),
                        ImVec2(0, 1), ImVec2(1, 0),
                        ImVec4(0, 0, 0, 0), // no background
                        tintColor))
                    {
                        m_state.selectedTile = entry;
                        m_state.currentTool = EditorTool::Paint; // Auto-switch to paint tool
                    }

                    // Label below thumbnail
                    ImGui::Text("%s", entry.displayName.c_str());

                    // Show properties on hover
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Type: %d", entry.defaultTileType);
                        ImGui::Text("Destructible: %s", entry.defaultDestructible ? "Yes" : "No");
                        ImGui::EndTooltip();
                    }
                }
                else
                {
                    // Fallback to text if texture fails
                    bool isSelected = (m_state.selectedTile.spritePath == entry.spritePath);
                    if (ImGui::Selectable(entry.displayName.c_str(), isSelected))
                    {
                        m_state.selectedTile = entry;
                    }
                }
            }
        }
    }

    ImGui::EndChild();
}

/*************************************************************************
  \brief
    Render canvas
*************************************************************************/
void TilemapNode::RenderCanvas()
{
    ImGui::BeginChild("Canvas", ImVec2(0, 0), true);
    ImGui::Text("Canvas - Layer: %s",
        m_state.selectedLayerIndex >= 0 &&
        m_state.selectedLayerIndex < static_cast<int32_t>(m_state.currentTilemap.layers.size())
        ? m_state.currentTilemap.layers[m_state.selectedLayerIndex].layerName.c_str()
        : "None");
    ImGui::Separator();

    // Get canvas region
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    m_canvasPos = canvasPos;
    m_canvasSize = canvasSize;

    // Draw grid background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(40, 40, 40, 255));

    // Draw grid lines
    const float tileSize = m_state.currentTilemap.tileSize * m_state.canvasZoom;
    const int32_t gridWidth = GGTilemapLayer::GRID_WIDTH;
    const int32_t gridHeight = GGTilemapLayer::GRID_HEIGHT;

    // Draw tiles if layer is selected
    if (m_state.selectedLayerIndex >= 0 &&
        m_state.selectedLayerIndex < static_cast<int32_t>(m_state.currentTilemap.layers.size()))
    {
        const auto& layer = m_state.currentTilemap.layers[m_state.selectedLayerIndex];

        for (int32_t row = 0; row < gridHeight; ++row)
        {
            for (int32_t col = 0; col < gridWidth; ++col)
            {
                const auto& tile = layer.tiles[row][col];

                // Only draw non-empty tiles
                if (!tile.spriteID == 0)
                {
                    float x = canvasPos.x + m_state.canvasOffset.x + (col * tileSize);
                    float y = canvasPos.y + m_state.canvasOffset.y + (row * tileSize);

                    // Check if texture is loaded before trying to get it
                    bool isLoaded = IsTextureLoaded(tile.spritePath);

                    // Try to get texture from asset manager
                    if (!tile.spriteID == 0)
                    {
                        GLuint textureID = GGECSStandardAssetManager<GLuint>::GetAsset(tile.spriteID);
                        if (textureID != 0)
                        {
                            // Draw actual texture using ImGui
                            drawList->AddImage((ImTextureID)(intptr_t)textureID,
                                ImVec2(x, y),
                                ImVec2(x + tileSize, y + tileSize),
                                ImVec2(0, 1), ImVec2(1, 0));
                        }
                        else
                        {
                            // Texture failed to load - draw colored placeholder
                            ImU32 errorColor = IM_COL32(200, 50, 50, 255); // Red for error
                            drawList->AddRectFilled(ImVec2(x, y),
                                ImVec2(x + tileSize, y + tileSize),
                                errorColor);

                            // Show filename
                            if (tileSize > 32.0f)
                            {
                                std::string filename = tile.spritePath.substr(tile.spritePath.find_last_of("/\\") + 1);
                                drawList->AddText(ImVec2(x + 2, y + 2),
                                    IM_COL32(255, 255, 255, 255),
                                    ("ERROR: " + filename).c_str());
                            }

                            // Log once per unique path
                            static std::set<std::string> loggedErrorPaths;
                            if (loggedErrorPaths.find(tile.spritePath) == loggedErrorPaths.end())
                            {
                                GGLogger::Warning("[TilemapEditor] Texture load failed: " + tile.spritePath);
                                loggedErrorPaths.insert(tile.spritePath);
                            }
                        }
                    }
                    else if (!isLoaded)
                    {
                        // Texture is missing - draw red placeholder with X pattern
                        ImU32 missingColor = IM_COL32(150, 30, 30, 255); // Dark red for missing
                        drawList->AddRectFilled(ImVec2(x, y),
                            ImVec2(x + tileSize, y + tileSize),
                            missingColor);

                        // Draw X pattern to indicate missing
                        drawList->AddLine(ImVec2(x, y),
                            ImVec2(x + tileSize, y + tileSize),
                            IM_COL32(255, 100, 100, 255), 2.0f);
                        drawList->AddLine(ImVec2(x + tileSize, y),
                            ImVec2(x, y + tileSize),
                            IM_COL32(255, 100, 100, 255), 2.0f);

                        // Show filename if tile is big enough
                        if (tileSize > 32.0f)
                        {
                            std::string filename = tile.spritePath.substr(tile.spritePath.find_last_of("/\\") + 1);
                            drawList->AddText(ImVec2(x + 2, y + 2),
                                IM_COL32(255, 200, 200, 255),
                                ("MISSING\n" + filename).c_str());
                        }

                        // Log once per unique path
                        static std::set<std::string> loggedMissingPaths;
                        if (loggedMissingPaths.find(tile.spritePath) == loggedMissingPaths.end())
                        {
                            GGLogger::Warning("[TilemapEditor] Texture not loaded: " + tile.spritePath);
                            loggedMissingPaths.insert(tile.spritePath);
                        }
                    }
                    else
                    {
                        // No asset manager - draw colored placeholder
                        ImU32 tileColor;
                        switch (tile.tileType)
                        {
                        case 1: tileColor = IM_COL32(100, 150, 100, 255); break;
                        case 2: tileColor = IM_COL32(150, 150, 150, 255); break;
                        case 3: tileColor = IM_COL32(150, 100, 50, 255); break;
                        default: tileColor = IM_COL32(200, 200, 200, 255); break;
                        }

                        drawList->AddRectFilled(ImVec2(x, y),
                            ImVec2(x + tileSize, y + tileSize),
                            tileColor);
                    }
                }
            }
        }
    }

    ImU32 gridColor = IM_COL32(80, 80, 80, 255);

    // Vertical lines
    for (int32_t col = 0; col <= gridWidth; ++col)
    {
        float x = canvasPos.x + m_state.canvasOffset.x + (col * tileSize);
        drawList->AddLine(ImVec2(x, canvasPos.y + m_state.canvasOffset.y),
            ImVec2(x, canvasPos.y + m_state.canvasOffset.y + (gridHeight * tileSize)),
            gridColor);
    }

    // Horizontal lines
    for (int32_t row = 0; row <= gridHeight; ++row)
    {
        float y = canvasPos.y + m_state.canvasOffset.y + (row * tileSize);
        drawList->AddLine(ImVec2(canvasPos.x + m_state.canvasOffset.x, y),
            ImVec2(canvasPos.x + m_state.canvasOffset.x + (gridWidth * tileSize), y),
            gridColor);
    }

    // Draw selection highlight (yellow border for inspected tile)
    if (m_state.hasTileSelected &&
        GGTilemapLayer::IsValidPosition(m_state.inspectedTileRow, m_state.inspectedTileCol))
    {
        float x = canvasPos.x + m_state.canvasOffset.x + (m_state.inspectedTileCol * tileSize);
        float y = canvasPos.y + m_state.canvasOffset.y + (m_state.inspectedTileRow * tileSize);

        ImU32 highlightColor = IM_COL32(255, 255, 0, 255); // Yellow
        float thickness = 3.0f;

        drawList->AddRect(ImVec2(x, y),
            ImVec2(x + tileSize, y + tileSize),
            highlightColor,
            0.0f,
            ImDrawFlags_None,
            thickness);
    }

    // Handle input
    ImGui::InvisibleButton("CanvasRegion", canvasSize);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        // Pan canvas with middle mouse
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_state.canvasOffset.x += delta.x;
        m_state.canvasOffset.y += delta.y;
    }

    // Mouse wheel zoom
    if (ImGui::IsItemHovered())
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            m_state.canvasZoom += wheel * 1.f;
            m_state.canvasZoom = std::max(10.f, std::min(64.0f, m_state.canvasZoom));
        }
    }

    // Handle tile painting/erasing
    if (ImGui::IsItemHovered())
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        bool leftClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool rightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

        if (leftClick || leftDown || rightClick)
        {
            HandleCanvasInput(mousePos, leftClick || leftDown, rightClick);
        }
    }

    ImGui::EndChild();
}

/*************************************************************************
  \brief
    Render layers panel
*************************************************************************/
void TilemapNode::RenderLayersPanel()
{
    ImGui::BeginChild("Layers", ImVec2(0, 200), true);
    ImGui::Text("Layers");
    ImGui::Separator();

    // Add layer button
    if (ImGui::Button("Add Layer"))
    {
        m_state.currentTilemap.AddLayer("New Layer");
        m_state.isDirty = true;
    }
    ImGui::SameLine();

    // Remove layer button
    if (ImGui::Button("Remove Layer"))
    {
        if (m_state.selectedLayerIndex >= 0 &&
            m_state.currentTilemap.layers.size() > 1)
        {
            m_state.currentTilemap.RemoveLayer(m_state.selectedLayerIndex);
            m_state.selectedLayerIndex = std::max(0, m_state.selectedLayerIndex - 1);
            m_state.isDirty = true;
        }
    }

    ImGui::Separator();

    // Layer list
    for (int32_t i = 0; i < static_cast<int32_t>(m_state.currentTilemap.layers.size()); ++i)
    {
        auto& layer = m_state.currentTilemap.layers[i];

        bool isSelected = (i == m_state.selectedLayerIndex);
        if (ImGui::Selectable(layer.layerName.c_str(), isSelected))
        {
            m_state.selectedLayerIndex = i;
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Rename"))
            {
                // TODO: Open rename dialog
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                // TODO: Duplicate layer
            }
            ImGui::Separator();
            if (ImGui::MenuItem(layer.isVisible ? "Hide" : "Show"))
            {
                layer.isVisible = !layer.isVisible;
                m_state.isDirty = true;
            }
            if (ImGui::MenuItem(layer.isLocked ? "Unlock" : "Lock"))
            {
                layer.isLocked = !layer.isLocked;
                m_state.isDirty = true;
            }
            ImGui::EndPopup();
        }
    }

    ImGui::EndChild();
}

/*************************************************************************
  \brief
    Render properties panel
*************************************************************************/
void TilemapNode::RenderPropertiesPanel()
{
    ImGui::BeginChild("Properties", ImVec2(0, 0), true);
    ImGui::Text("Properties");
    ImGui::Separator();

    // Map properties
    static char mapNameBuffer[256];
    strncpy_s(mapNameBuffer, m_state.currentTilemap.mapName.c_str(), 256);
    if (ImGui::InputText("Map Name", mapNameBuffer, 256))
    {
        m_state.currentTilemap.mapName = mapNameBuffer;
        m_state.isDirty = true;
    }

    float tileSize = m_state.currentTilemap.tileSize;
    if (ImGui::DragFloat("Tile Size", &tileSize, 1.0f, 8.0f, 512.0f))
    {
        m_state.currentTilemap.tileSize = tileSize;
        m_state.isDirty = true;
    }

    ImGui::Separator();

    // Selected tile properties (if tool is Paint)
    if (m_state.currentTool == EditorTool::Paint)
    {
        ImGui::Text("Selected Tile:");
        ImGui::Text("  Name: %s", m_state.selectedTile.displayName.c_str());
        ImGui::Text("  Type: %d", m_state.selectedTile.defaultTileType);
        ImGui::Text("  Destructible: %s", m_state.selectedTile.defaultDestructible ? "Yes" : "No");
    }

    // Inspected tile properties (if Select tool has selected a tile)
    if (m_state.currentTool == EditorTool::Select && m_state.hasTileSelected)
    {
        ImGui::Text("Inspected Tile:");
        ImGui::Text("  Position: (%d, %d)", m_state.inspectedTileRow, m_state.inspectedTileCol);

        if (m_state.selectedLayerIndex >= 0 &&
            m_state.selectedLayerIndex < static_cast<int32_t>(m_state.currentTilemap.layers.size()))
        {
            const auto& layer = m_state.currentTilemap.layers[m_state.selectedLayerIndex];

            if (GGTilemapLayer::IsValidPosition(m_state.inspectedTileRow, m_state.inspectedTileCol))
            {
                const GGTileData& tile = layer.GetTile(m_state.inspectedTileRow, m_state.inspectedTileCol);

                ImGui::Text("  Layer: %s", layer.layerName.c_str());

                if (tile.IsEmpty())
                {
                    ImGui::Text("  [Empty Tile]");
                }
                else
                {
                    ImGui::Text("  Sprite: %s", tile.spritePath.c_str());
                    // NOTE: spriteID is uint32_t, use %u format specifier.
                    // Using %s causes crash because ImGui interprets the integer as a pointer.
                    ImGui::Text("  Sprite ID: %u", tile.spriteID);
                    ImGui::Text("  Type: %d", tile.tileType);
                    ImGui::Text("  Destructible: %s", tile.isDestructible ? "Yes" : "No");
                    ImGui::Text("  Variation: %u", tile.variation);

                    // Show preview thumbnail if texture loaded
                    if (!tile.spriteID == 0)
                    {
                        GLuint textureID = GGECSStandardAssetManager<GLuint>::GetAsset(tile.spriteID);
                        if (textureID != 0)
                        {
                            ImGui::Image(
                                (void*)(intptr_t)textureID,
                                ImVec2(64, 64)
                            );
                        }
                    }
                    else if (!tile.spritePath.empty())
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Texture Missing!");
                    }
                }

                ImGui::Separator();

                // Delete button for non-empty tiles
                if (!tile.IsEmpty())
                {
                    if (ImGui::Button("Delete This Tile"))
                    {
                        EraseTile(m_state.inspectedTileRow, m_state.inspectedTileCol);
                    }
                }
            }
        }
    }

    // Canvas controls
    ImGui::Separator();
    ImGui::Text("Canvas:");
    ImGui::Text("  Zoom: %.2f", m_state.canvasZoom);
    if (ImGui::Button("Reset View"))
    {
        m_state.canvasOffset = GGMath::Vector2(50.0f, 50.0f);
        m_state.canvasZoom = 1.0f;
    }

    ImGui::EndChild();
}

#pragma endregion

#pragma region Input Handling

/*************************************************************************
  \brief
    Handle canvas input
*************************************************************************/
void TilemapNode::HandleCanvasInput(const ImVec2& mousePos, bool isLeftClick, bool isRightClick)
{
    // Check if we have a valid layer selected
    if (m_state.selectedLayerIndex < 0 ||
        m_state.selectedLayerIndex >= static_cast<int32_t>(m_state.currentTilemap.layers.size()))
    {
        return;
    }

    auto& layer = m_state.currentTilemap.layers[m_state.selectedLayerIndex];

    // Check if layer is locked
    if (layer.isLocked)
    {
        return;
    }

    // Convert mouse position to grid coordinates
    int32_t row, col;
    if (!ScreenToGrid(mousePos, row, col))
    {
        return;
    }

    // Perform action based on current tool
    switch (m_state.currentTool)
    {
    case EditorTool::Paint:
        if (isLeftClick)
        {
            PaintTile(row, col);
        }
        break;

    case EditorTool::Erase:
        if (isLeftClick || isRightClick)
        {
            EraseTile(row, col);
        }
        break;

    case EditorTool::Fill:
        if (isLeftClick)
        {
            FillTiles(row, col);
        }
        break;

    case EditorTool::Select:
        if (isLeftClick)
        {
            // Select tile for inspection
            m_state.inspectedTileRow = row;
            m_state.inspectedTileCol = col;
            m_state.hasTileSelected = true;
        }
        break;
    }
}

/*************************************************************************
  \brief
    Convert screen position to grid coordinates
*************************************************************************/
bool TilemapNode::ScreenToGrid(const ImVec2& mousePos, int32_t& outRow, int32_t& outCol)
{
    const float tileSize = m_state.currentTilemap.tileSize * m_state.canvasZoom;

    float relX = mousePos.x - m_canvasPos.x - m_state.canvasOffset.x;
    float relY = mousePos.y - m_canvasPos.y - m_state.canvasOffset.y;

    outCol = static_cast<int32_t>(relX / tileSize);
    outRow = static_cast<int32_t>(relY / tileSize);

    return GGTilemapLayer::IsValidPosition(outRow, outCol);
}

#pragma endregion

#pragma region Editing Operations

/*************************************************************************
  \brief
    Paint tile at grid position
*************************************************************************/
void TilemapNode::PaintTile(int32_t row, int32_t col)
{
    if (m_state.selectedLayerIndex < 0 ||
        m_state.selectedLayerIndex >= static_cast<int32_t>(m_state.currentTilemap.layers.size()))
    {
        return;
    }

    auto& layer = m_state.currentTilemap.layers[m_state.selectedLayerIndex];

    if (!GGTilemapLayer::IsValidPosition(row, col))
    {
        return;
    }

    GGTileData& tile = layer.GetTile(row, col);
    tile.spritePath = m_state.selectedTile.spritePath;
    tile.spriteID = m_state.selectedTile.spriteID;
    tile.tileType = m_state.selectedTile.defaultTileType;
    tile.isDestructible = m_state.selectedTile.defaultDestructible;
    tile.variation = m_state.selectedTile.defaultVariation;

    m_state.isDirty = true;
}

/*************************************************************************
  \brief
    Erase tile at grid position
*************************************************************************/
void TilemapNode::EraseTile(int32_t row, int32_t col)
{
    if (m_state.selectedLayerIndex < 0 ||
        m_state.selectedLayerIndex >= static_cast<int32_t>(m_state.currentTilemap.layers.size()))
    {
        return;
    }

    auto& layer = m_state.currentTilemap.layers[m_state.selectedLayerIndex];

    if (!GGTilemapLayer::IsValidPosition(row, col))
    {
        return;
    }

    GGTileData& tile = layer.GetTile(row, col);
    tile = GGTileData(); // Reset to default empty tile

    m_state.isDirty = true;
}

/*************************************************************************
  \brief
    Fill connected tiles with selected tile (flood fill)
*************************************************************************/
void TilemapNode::FillTiles(int32_t row, int32_t col)
{
    if (m_state.selectedLayerIndex < 0 ||
        m_state.selectedLayerIndex >= static_cast<int32_t>(m_state.currentTilemap.layers.size()))
    {
        return;
    }

    auto& layer = m_state.currentTilemap.layers[m_state.selectedLayerIndex];

    if (!GGTilemapLayer::IsValidPosition(row, col))
    {
        return;
    }

    // Get original tile to match
    GGTileData originalTile = layer.GetTile(row, col);

    // Don't fill if already same tile
    if (originalTile.spritePath == m_state.selectedTile.spritePath)
    {
        return;
    }

    // BFS flood fill
    std::queue<std::pair<int32_t, int32_t>> queue;
    bool visited[GGTilemapLayer::GRID_HEIGHT][GGTilemapLayer::GRID_WIDTH] = {};

    queue.push({ row, col });
    visited[row][col] = true;

    while (!queue.empty())
    {
        auto [r, c] = queue.front();
        queue.pop();

        // Paint this tile
        GGTileData& tile = layer.GetTile(r, c);
        tile.spritePath = m_state.selectedTile.spritePath;
        tile.spriteID = m_state.selectedTile.spriteID;
        tile.tileType = m_state.selectedTile.defaultTileType;
        tile.isDestructible = m_state.selectedTile.defaultDestructible;
        tile.variation = m_state.selectedTile.defaultVariation;

        // Check neighbors (4-directional)
        const int32_t dirs[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

        for (int32_t i = 0; i < 4; ++i)
        {
            int32_t newR = r + dirs[i][0];
            int32_t newC = c + dirs[i][1];

            if (GGTilemapLayer::IsValidPosition(newR, newC) && !visited[newR][newC])
            {
                const GGTileData& neighborTile = layer.GetTile(newR, newC);

                // Check if neighbor matches original tile
                if (neighborTile.spritePath == originalTile.spritePath)
                {
                    visited[newR][newC] = true;
                    queue.push({ newR, newC });
                }
            }
        }
    }

    m_state.isDirty = true;
}

#pragma endregion

#pragma region File Operations

/*************************************************************************
  \brief
    Create new tilemap
*************************************************************************/
void TilemapNode::NewTilemap()
{
    if (m_state.isDirty)
    {
        // TODO: Show save confirmation dialog
        GGLogger::Warning("Unsaved changes will be lost");
    }

    m_state.currentTilemap = GGTilemap();
    m_state.currentFilePath.clear();
    m_state.isDirty = false;
    m_state.selectedLayerIndex = 0;

    GGLogger::Info("Created new tilemap");
}

/*************************************************************************
  \brief
    Open tilemap from file
*************************************************************************/
/*************************************************************************
  \brief
    Open tilemap from file
*************************************************************************/
void TilemapNode::OpenTilemap()
{
    // Open file dialog in Open mode
    m_fileDialog.Open(SimpleFileDialog::Mode::Open);
    GGLogger::Info("[TilemapEditor] Opening file dialog");
}

/*************************************************************************
  \brief
    Save tilemap to current file
*************************************************************************/
void TilemapNode::SaveTilemap()
{
    if (m_state.currentFilePath.empty())
    {
        SaveTilemapAs();
        return;
    }

    // Run comprehensive validation
    std::vector<std::string> allErrors;
    bool valid = true;

    // 1. Validate dimensions
    std::vector<std::string> dimErrors;
    if (!ValidateDimensions(dimErrors))
    {
        allErrors.insert(allErrors.end(), dimErrors.begin(), dimErrors.end());
        valid = false;
    }

    // 2. Validate layers
    std::vector<std::string> layerErrors;
    if (!ValidateLayers(layerErrors))
    {
        allErrors.insert(allErrors.end(), layerErrors.begin(), layerErrors.end());
        valid = false;
    }

    // 3. Validate tile bounds
    std::vector<std::string> boundsErrors;
    if (!ValidateTileBounds(boundsErrors))
    {
        allErrors.insert(allErrors.end(), boundsErrors.begin(), boundsErrors.end());
        valid = false;
    }

    // 4. Validate textures (warning only, not blocking)
    std::vector<std::string> missingTextures;
    if (!ValidateTilemap(missingTextures))
    {
        GGLogger::Warning("Some textures are missing but saving anyway:");
        for (const auto& path : missingTextures)
        {
            GGLogger::Warning("  - " + path);
        }

        // Add to errors for dialog
        allErrors.push_back("--- Texture Warnings (not blocking save) ---");
        for (const auto& path : missingTextures)
        {
            allErrors.push_back("Missing texture: " + path);
        }
    }

    // Show errors if validation failed (excluding texture warnings)
    if (!valid)
    {
        ShowErrorDialog("Tilemap Validation Failed", allErrors);
        GGLogger::Error("Cannot save tilemap: Validation failed");
        return;
    }

    // All critical validation passed - save
    if (GGTilemapSerializer::SaveToFile(m_state.currentTilemap, m_state.currentFilePath))
    {
        m_state.isDirty = false;
        GGLogger::Info("Saved tilemap: " + m_state.currentFilePath);

        // Show warning dialog if textures missing (but save succeeded)
        if (!missingTextures.empty())
        {
            ShowErrorDialog("Save Successful - Texture Warnings", allErrors);
        }

        GlobalEventBus::onReloadAssets.Emit();
    }
    else
    {
        std::vector<std::string> saveError;
        saveError.push_back("Failed to write file to disk");
        saveError.push_back("Check file permissions and disk space");
        ShowErrorDialog("Save Failed", saveError);
        GGLogger::Error("Failed to save tilemap: " + m_state.currentFilePath);
    }
}

/*************************************************************************
  \brief
    Save tilemap to new file
*************************************************************************/
void TilemapNode::SaveTilemapAs()
{
    // Open file dialog in SaveAs mode with suggested filename
    std::string suggestedName = "new_map.tilemap";
    if (!m_state.currentTilemap.mapName.empty())
    {
        suggestedName = m_state.currentTilemap.mapName + ".tilemap";
    }

    m_fileDialog.Open(SimpleFileDialog::Mode::SaveAs, suggestedName);
    GlobalEventBus::onReloadAssets.Emit();
    GGLogger::Info("[TilemapEditor] Opening save dialog");
}

/*************************************************************************
  \brief
    Refresh palette from AssetManager
*************************************************************************/
void TilemapNode::RefreshPalette()
{
    if (!m_assetManager)
    {
        GGLogger::Warning("Cannot refresh palette: No asset manager");
        return;
    }

    m_state.tilePalette.clear();

    // Get all loaded textures from AssetManager
    std::vector<std::string> texturePaths = m_assetManager->GetLoadedTexturePaths();

    std::cout << "[TilemapNode] Refreshing Palette..." << std::endl;

    // Filter to only Textures/ directory
    for (const auto& path : texturePaths)
    {
        //std::cout << "[TilemapNode] Discovered: " << path << std::endl;
        // Only add textures from Textures/ folder (skip UI, sprites, etc.)
        if (path.find("Textures\\") != std::string::npos)
        {
            // Extract filename for display name
            size_t lastSlash = path.find_last_of("/\\");
            size_t lastDot = path.find_last_of(".");
            std::string displayName = (lastSlash != std::string::npos)
                ? path.substr(lastSlash + 1, lastDot - lastSlash - 1)
                : path;

            auto& assetSystem = GGAssetID::GetGGAssetID();
            uint32_t assetID = 0;

            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);

            // Add to palette with default properties
            m_state.tilePalette.emplace_back(
                path,           // spritePath
                displayName,    // displayName
                assetID,        // runtime assetID
                1,              // defaultTileType (wall by default)
                false,          // destructible
                0               // variation
            );
        }
    }

    // If no textures loaded yet, add brick wall as fallback
    if (m_state.tilePalette.empty())
    {
        m_state.tilePalette.emplace_back(
            "../../../EngineAssets/Textures/Brick_Wall.png",
            "Brick Wall",
            0,
            1,
            false,
            0
        );

        GGLogger::Info("Palette refreshed with 1 default tile (Brick_Wall)");
        GGLogger::Info("Tip: Load more textures first, then click Refresh Palette");
    }
    else
    {
        GGLogger::Info("Refreshed palette with " + std::to_string(m_state.tilePalette.size()) + " tiles");
    }

    // Set default selected tile to first in palette if available
    if (!m_state.tilePalette.empty())
    {
        m_state.selectedTile = m_state.tilePalette[0];
    }
}

/*************************************************************************
  \brief
    Validate tilemap for missing textures
*************************************************************************/
bool TilemapNode::ValidateTilemap(std::vector<std::string>& missingTextures)
{
    missingTextures.clear();
    std::set<std::string> checkedPaths;

    for (const auto& layer : m_state.currentTilemap.layers)
    {
        for (int32_t row = 0; row < GGTilemapLayer::GRID_HEIGHT; ++row)
        {
            for (int32_t col = 0; col < GGTilemapLayer::GRID_WIDTH; ++col)
            {
                const GGTileData& tile = layer.tiles[row][col];

                if (!tile.spritePath.empty() && checkedPaths.find(tile.spritePath) == checkedPaths.end())
                {
                    checkedPaths.insert(tile.spritePath);

                    if (!IsTextureLoaded(tile.spritePath))
                    {
                        missingTextures.push_back(tile.spritePath);
                    }
                }
            }
        }
    }

    return missingTextures.empty();
}

/*************************************************************************
  \brief
    Check if texture exists in AssetManager
*************************************************************************/
bool TilemapNode::IsTextureLoaded(const std::string& texturePath)
{
    if (!m_assetManager || texturePath.empty())
    {
        return false;
    }

    // Normalize path - convert backslashes to forward slashes
    std::string normalizedInput = texturePath;
    std::replace(normalizedInput.begin(), normalizedInput.end(), '\\', '/');

    // Get all loaded texture paths from AssetManager
    std::vector<std::string> loadedPaths = m_assetManager->GetLoadedTexturePaths();

    // Check if texture path is in loaded list (case-insensitive)
    for (const auto& loadedPath : loadedPaths)
    {
        // Normalize loaded path too
        std::string normalizedLoaded = loadedPath;
        std::replace(normalizedLoaded.begin(), normalizedLoaded.end(), '\\', '/');

        // Case-insensitive comparison (Windows)
        if (_stricmp(normalizedLoaded.c_str(), normalizedInput.c_str()) == 0)
        {
            return true;  // Found in cache
        }
    }

    return false;  // Not in cache - don't try to load it!
}

/*************************************************************************
  \brief
    Validate map dimensions
*************************************************************************/
bool TilemapNode::ValidateDimensions(std::vector<std::string>& errors)
{
    errors.clear();

    // Check if layers exist
    if (m_state.currentTilemap.layers.empty())
    {
        errors.push_back("Map has no layers");
        return false;
    }

    // All layers must be 6x4 (enforced by GGTilemapLayer structure at compile-time)
    // Validate this guarantee with static_assert
    static_assert(GGTilemapLayer::GRID_WIDTH == 6, "Grid width must be 6 for Order N Chaos");
    static_assert(GGTilemapLayer::GRID_HEIGHT == 4, "Grid height must be 4 for Order N Chaos");

    // Since dimensions are compile-time enforced, just log success
    GGLogger::Info("Dimension validation passed: All layers are 6x4 (compile-time enforced)");

    return true;
}

/*************************************************************************
  \brief
    Validate layer requirements
*************************************************************************/
bool TilemapNode::ValidateLayers(std::vector<std::string>& errors)
{
    errors.clear();

    if (m_state.currentTilemap.layers.empty())
    {
        errors.push_back("Map must have at least one layer");
        return false;
    }

    // Check for duplicate layer names
    std::set<std::string> layerNames;
    for (const auto& layer : m_state.currentTilemap.layers)
    {
        if (layerNames.find(layer.layerName) != layerNames.end())
        {
            errors.push_back("Duplicate layer name: '" + layer.layerName + "'");
        }
        layerNames.insert(layer.layerName);
    }

    bool valid = errors.empty();
    if (valid)
    {
        GGLogger::Info("Layer validation passed: " + std::to_string(m_state.currentTilemap.layers.size()) + " layers OK");
    }

    return valid;
}

/*************************************************************************
  \brief
    Validate tile bounds
*************************************************************************/
bool TilemapNode::ValidateTileBounds(std::vector<std::string>& errors)
{
    errors.clear();

    // All tiles are guaranteed within bounds by fixed array structure
    // This validation is redundant but included for completeness
    bool valid = true;

    for (size_t layerIdx = 0; layerIdx < m_state.currentTilemap.layers.size(); ++layerIdx)
    {
        const auto& layer = m_state.currentTilemap.layers[layerIdx];

        for (int32_t row = 0; row < GGTilemapLayer::GRID_HEIGHT; ++row)
        {
            for (int32_t col = 0; col < GGTilemapLayer::GRID_WIDTH; ++col)
            {
                if (!GGTilemapLayer::IsValidPosition(row, col))
                {
                    errors.push_back("Invalid tile position in layer '" + layer.layerName +
                        "': (" + std::to_string(row) + ", " + std::to_string(col) + ")");
                    valid = false;
                }
            }
        }
    }

    if (valid)
    {
        GGLogger::Info("Bounds validation passed: All tiles within 6x4 grid");
    }

    return valid;
}

/*************************************************************************
  \brief
    Show error dialog popup
*************************************************************************/
void TilemapNode::ShowErrorDialog(const std::string& title, const std::vector<std::string>& errors)
{
    m_showErrorDialog = true;
    m_errorDialogTitle = title;
    m_errorDialogMessages = errors;
}

#pragma endregion