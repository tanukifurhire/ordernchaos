/*************************************************************************
  \file       GGTilemapRenderer.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       15 November 2025
  \version    1.1
  \brief
    Renderer for tilemap visualization using sprite quads.
  \details
    Provides rendering methods for GGTilemap structures using OpenGL.
    Renders each tile as a textured quad using the sprite asset system.
    Supports layer visibility, camera transforms, and world positioning.
    For the fixed 6x4 grid, performance is not a concern so implementation
    is straightforward without complex batching or culling.

    NOTE: NO namespace used to avoid collision with GGEngine class.
  \dependencies
    GGTilemapData.hpp, GGShader.hpp, GGAssetManager.hpp, Matrix3x3.hpp,
    Vector2.hpp, GL/glew.h
  \see
    GGTilemapData.hpp, GGAssetManager, GGRender2d
  \history
    - 1.0 (15 November 2025): Initial version with basic quad rendering.
    - 1.1 (15 November 2025): Removed namespace, fixed APIs.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef GG_TILEMAP_RENDERER_HPP
#define GG_TILEMAP_RENDERER_HPP

#pragma region Includes
#include "GGTilemapData.hpp"
#include "GGShader.hpp"
#include "GGAssetManager.hpp"
#include "GGMath/Matrix3x3.hpp"
#include "GGMath/Vector2.hpp"
#include <GL/glew.h>
#pragma endregion

/*************************************************************************
  \brief
    Renderer for tilemap layers
  \details
    Handles rendering of GGTilemap structures to screen. Creates textured
    quads for each visible tile using sprite assets. Simple immediate-mode
    approach suitable for small 6x4 grid.
*************************************************************************/
class GGTilemapRenderer
{
public:
    /*************************************************************************
      \brief
        Default constructor
    *************************************************************************/
    GGTilemapRenderer();

    /*************************************************************************
      \brief
        Destructor cleans up OpenGL resources
    *************************************************************************/
    ~GGTilemapRenderer();

    /*************************************************************************
      \brief
        Initialize renderer with shader and asset manager
      \param[in] shader
        Pointer to shader for sprite rendering
      \param[in] assetManager
        Pointer to asset manager for loading tile sprites
      \par Usage
      \code
        GGShader spriteShader;
        GGAssetManager assetMgr;
        GGTilemapRenderer renderer;
        renderer.Initialize(&spriteShader, &assetMgr);
      \endcode
      \par Purpose
        Setup renderer with required dependencies
    *************************************************************************/
    void Initialize(GGShader* shader, GGAssetManager* assetManager);

    /*************************************************************************
      \brief
        Render tilemap to screen
      \param[in] tilemap
        Tilemap data structure to render
      \param[in] viewProjection
        Camera view-projection matrix for transforming tiles
      \param[in] worldPosition
        World-space offset for the entire tilemap
      \par Usage
      \code
        renderer.RenderTilemap(myMap, camera.GetViewProjection(), Vector2(0, 0));
      \endcode
      \par Purpose
        Draw all visible layers of the tilemap
    *************************************************************************/
    void RenderTilemap(const GGTilemap& tilemap,
        const GGMath::Matrix3x3& viewProjection,
        const GGMath::Vector2& worldPosition,
        const GGMath::float32& perspectiveFactor);

    /*************************************************************************
      \brief
        Render single layer to screen
      \param[in] layer
        Layer data structure to render
      \param[in] viewProjection
        Camera view-projection matrix
      \param[in] worldPosition
        World-space offset for the layer
      \param[in] tileSize
        Size of each tile in world units
      \par Purpose
        Render a specific layer (used internally and for editor previews)
    *************************************************************************/
    void RenderLayer(const GGTilemapLayer& layer,
        const GGMath::Matrix3x3& viewProjection,
        const GGMath::Vector2& worldPosition,
        float tileSize,
        const GGMath::float32& perspectiveFactor,
        uint32_t layerIndex);

    /*************************************************************************
      \brief
        Enable/disable grid overlay for editor
      \param[in] enabled
        True to show grid lines, false to hide
      \par Purpose
        Toggle grid visualization during editing
    *************************************************************************/
    void SetGridOverlay(bool enabled);

private:
    /*************************************************************************
      \brief
        Render single tile quad
      \param[in] tile
        Tile data containing sprite reference
      \param[in] position
        World-space position of tile center
      \param[in] size
        Tile dimensions
      \param[in] viewProjection
        Camera transform matrix
      \par Purpose
        Internal method for rendering individual tiles
    *************************************************************************/
    void RenderTileQuad(const GGTileData& tile,
        const GGMath::Vector2& position,
        float size,
        const GGMath::Matrix3x3& viewProjection,
        const GGMath::float32& perspectiveFactor,
        uint32_t layerIndex);

    /*************************************************************************
      \brief
        Render grid overlay lines
      \param[in] gridWidth
        Number of columns
      \param[in] gridHeight
        Number of rows
      \param[in] tileSize
        Size of each tile
      \param[in] worldPosition
        Offset for grid
      \param[in] viewProjection
        Camera transform
      \par Purpose
        Draw debug grid for editor
    *************************************************************************/
    void RenderGridLines(int32_t gridWidth, int32_t gridHeight,
        float tileSize,
        const GGMath::Vector2& worldPosition,
        const GGMath::Matrix3x3& viewProjection);

    GGShader* m_shader;
    GGAssetManager* m_assetManager;
    bool m_gridOverlayEnabled;

    GLuint m_vao;
    GLuint m_vbo;
};

#endif // GG_TILEMAP_RENDERER_HPP