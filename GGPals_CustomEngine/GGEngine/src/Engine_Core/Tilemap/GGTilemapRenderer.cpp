/*************************************************************************
  \file       GGTilemapRenderer.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       15 November 2025
  \version    1.1
  \brief
    Implementation of tilemap rendering system.
  \details
    Implements quad-based rendering for tilemap tiles. Uses OpenGL to
    render textured quads for each tile in visible layers. Simple approach
    suitable for fixed 6x4 grid without need for complex optimization.
  \dependencies
    GGTilemapRenderer.hpp, GGLogger.hpp, <glm/glm.hpp>
  \see
    GGTilemapRenderer.hpp, GGTilemapData.hpp
  \history
    - 1.0 (15 November 2025): Initial implementation.
    - 1.1 (15 November 2025): Removed namespace, fixed APIs.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma region Includes
#include "GGTilemapRenderer.hpp"
#include "GGLogger.hpp"
#include <GGECSAssetInterface.hpp>
#include <GGSystemManagers.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GGLayerManager.hpp>
#pragma endregion

#pragma region Constructor and Destructor

/*************************************************************************
  \brief
    Default constructor
*************************************************************************/
GGTilemapRenderer::GGTilemapRenderer()
    : m_shader(nullptr)
    , m_assetManager(nullptr)
    , m_gridOverlayEnabled(false)
    , m_vao(0)
    , m_vbo(0)
{
}

/*************************************************************************
  \brief
    Destructor cleans up OpenGL resources
*************************************************************************/
GGTilemapRenderer::~GGTilemapRenderer()
{
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
    }
}

#pragma endregion

#pragma region Initialization

/*************************************************************************
  \brief
    Initialize renderer with shader and asset manager
*************************************************************************/
void GGTilemapRenderer::Initialize(GGShader* shader, GGAssetManager* assetManager)
{
    m_shader = shader;
    m_assetManager = assetManager;

    // Create unit quad (0,0 to 1,1) that we'll scale and position per tile
    // Vertex format: position (x, y), texcoord (u, v)
    float quadVertices[] =
    {
        // positions   // texcoords
        0.0f, 1.0f,    0.0f, 1.0f,
        1.0f, 0.0f,    1.0f, 0.0f,
        0.0f, 0.0f,    0.0f, 0.0f,

        0.0f, 1.0f,    0.0f, 1.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        1.0f, 0.0f,    1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // Texcoord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    GGLogger::Info("GGTilemapRenderer initialized");
}

#pragma endregion

#pragma region Rendering

/*************************************************************************
  \brief
    Render tilemap to screen
*************************************************************************/
void GGTilemapRenderer::RenderTilemap(const GGTilemap& tilemap,
    const GGMath::Matrix3x3& viewProjection,
    const GGMath::Vector2& worldPosition,
    const GGMath::float32& perspectiveFactor)
{
    /*if (!m_shader || !m_assetManager)
    {
        GGLogger::Warning("GGTilemapRenderer not initialized");
        return;
    }*/
    [[maybe_unused]] auto& layerMgr = GGLayerManager::Get();

#ifdef GG_EDITOR_BUILD
    // Check if TILEMAP layer type is enabled
    LayerKey tilemapKey{ 0, LayerType::TILEMAP };
    if (!layerMgr.IsEnabled(tilemapKey))
    {
        return; 
    }
#endif
    // Render each visible layer from back to front
    uint32_t layerCount = {};
    for (const auto& layer : tilemap.layers)
    {
        if (layer.isVisible)
        {
            RenderLayer(layer, viewProjection, worldPosition, tilemap.tileSize, perspectiveFactor, layerCount);
        }
        layerCount++;
    }

    // Render grid overlay if enabled
    if (m_gridOverlayEnabled && !tilemap.layers.empty())
    {
        RenderGridLines(GGTilemapLayer::GRID_WIDTH, GGTilemapLayer::GRID_HEIGHT,
            tilemap.tileSize, worldPosition, viewProjection);
    }
}

/*************************************************************************
  \brief
    Render single layer to screen
*************************************************************************/
void GGTilemapRenderer::RenderLayer(const GGTilemapLayer& layer,
    const GGMath::Matrix3x3& viewProjection,
    const GGMath::Vector2& worldPosition,
    float tileSize,
    const GGMath::float32& perspectiveFactor,
    uint32_t layerIndex)
{
    // Iterate through grid
    for (int32_t row = 0; row < GGTilemapLayer::GRID_HEIGHT; ++row)
    {
        for (int32_t col = 0; col < GGTilemapLayer::GRID_WIDTH; ++col)
        {
            const GGTileData& tile = layer.tiles[row][col];

            // Calculate tile position in world space
            // Origin at top-left, positive x right, positive y down
            GGMath::Vector2 tilePosition;
            tilePosition.x = worldPosition.x + (col * tileSize);
            tilePosition.y = worldPosition.y + (row * tileSize);

            RenderTileQuad(tile, tilePosition, tileSize, viewProjection, perspectiveFactor, layerIndex);
        }
    }
}

/*************************************************************************
  \brief
    Render single tile quad
*************************************************************************/
void GGTilemapRenderer::RenderTileQuad(const GGTileData& tile,
    const GGMath::Vector2& position,
    float size,
    const GGMath::Matrix3x3&,
    const GGMath::float32& perspectiveFactor,
    uint32_t layerIndex)
{
    // Get texture from asset manager
    GLuint textureID = GGECSStandardAssetManager<GLuint>::GetAsset(tile.spriteID);
    if (textureID == 0)
    {
        // Texture not loaded, skip
        return;
    }

    // Build model matrix (translate and scale)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position.x, position.y, 0.0f));
    model = glm::scale(model, glm::vec3(size, size, 1.0f));

    auto& renderer = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSRender>();

    InstanceData instance{};
    instance.useTexture = 1;
    instance.textureID = textureID;
    instance.layer = layerIndex - 10;

    // We use Z axis to simulate 3D perspective
    instance.translation = glm::vec3(position.x, position.y, perspectiveFactor);
    instance.scale = glm::vec3(size, size, 1.f);
    instance.rotation = 0.f;
    instance.uvBounds = glm::vec4(0.f, 0.f, 1.f, 1.f);
    
    instance.colorAdd = 
        glm::vec4(tile.colorAdd.x, 
            tile.colorAdd.y, 
            tile.colorAdd.z, 
            tile.colorAdd.w);

    instance.colorMultiply =
        glm::vec4(tile.colorMultiply.x,
            tile.colorMultiply.y,
            tile.colorMultiply.z,
            tile.colorMultiply.w);

    renderer.PushInstance(std::move(instance));
}

/*************************************************************************
  \brief
    Render grid overlay lines
*************************************************************************/
void GGTilemapRenderer::RenderGridLines(int32_t gridWidth, int32_t gridHeight,
    float tileSize,
    const GGMath::Vector2& worldPosition,
    const GGMath::Matrix3x3& viewProjection)
{
    // TODO: Implement grid line rendering using GGRender2d line primitives
    // For now, this is a placeholder for future implementation
    (void)gridWidth;
    (void)gridHeight;
    (void)tileSize;
    (void)worldPosition;
    (void)viewProjection;
}

#pragma endregion

#pragma region Settings

/*************************************************************************
  \brief
    Enable/disable grid overlay for editor
*************************************************************************/
void GGTilemapRenderer::SetGridOverlay(bool enabled)
{
    m_gridOverlayEnabled = enabled;
}

#pragma endregion