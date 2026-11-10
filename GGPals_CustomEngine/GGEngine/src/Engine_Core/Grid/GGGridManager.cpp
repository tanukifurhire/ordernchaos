/*************************************************************************
  \file       GGGridManager.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       05 November 2025
  \version    1.2
  \brief
    Defines the GridManager class for Order N Chaos grid management.
  \details
    This manager handles grid creation, updates, rendering, and game logic
    for the turn-based tactical grid system. It manages both Order and Chaos
    grids, handles unit placement, and processes grid-based abilities.

    RENDER2D INTEGRATION COMPLETE - Uses render2d::drawSquare() for tiles.
  \dependencies
    GGGrid.hpp, GGECSWorld.hpp, GGRender2d.hpp
  \see
    GridComponent, render2d
  \history
    - 1.0 (05 November 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGGridManager.hpp>
#include <GGTilemapData.hpp>
#include <GGTilemapRenderer.hpp>
#include <GGSystemManagers.hpp>
#include <GGMath/Vector4.hpp>

/*************************************************************************
  \brief
    Updates the grid by determining which tile is currently highlighted
    based on the mouse world position with perspective applied.
  \param gridID
    ID of the grid to update.
  \param perspectiveFactor
    Factor applied when converting mouse world position to perspective-correct space.
  \details
    Computes the tile under the mouse cursor and sets the highlight flag
    on that tile while clearing it from all others.
*************************************************************************/
void Grid::GridManager::UpdateGrid(
    uint32_t gridID,
    GGMath::float32 perspectiveFactor)
{
    GridComponent* grid = GetGrid(gridID);
    if (!grid) return;

    auto& input = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

    glm::vec2 worldPosPerspective =
        input.GetMouseWorldPositionWithPerspective(perspectiveFactor);

    GGMath::Vector2Int result = WorldToGrid(GGMath::Vector2(worldPosPerspective.x, worldPosPerspective.y), gridID);

    for (auto& tile : grid->GetAllTiles())
    {
        if (tile.position == result)
        {
            tile.state |= TILE_HIGHLIGHTED;
        }
        else
        {
            tile.state &= ~TILE_HIGHLIGHTED;
        }
    }
}

/*************************************************************************
  \brief
    Renders the entire grid and its associated tilemap.
  \param gridID
    ID of the grid to render.
  \param perspectiveFactor
    Perspective factor used when drawing tiles and tilemaps.
  \details
    Iterates through all tiles, calling RenderTile for each, and finally
    renders the full tilemap using the ECS renderer.
*************************************************************************/
void Grid::GridManager::RenderGrid(
    uint32_t gridID, 
    GGMath::float32 perspectiveFactor)
{
    GridComponent* grid = GetGrid(gridID);
    if (!grid) return;

    [[maybe_unused]] GGMath::float32 baseX = renderConfig.gridOffsetX;
    [[maybe_unused]] GGMath::float32 baseY = renderConfig.gridOffsetY;

    GGMath::Matrix3x3 transform;
    transform.identity();

    for (auto const& tile : grid->GetAllTiles())
    {
        RenderTile(tile, grid->GetPosition().x, grid->GetPosition().y, grid->GetTileMap(), perspectiveFactor);
    }

    Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().GetTilemapRenderer()
        .RenderTilemap(grid->GetTileMap(), transform, GGMath::Vector2(grid->GetPosition().x, grid->GetPosition().y), perspectiveFactor);
}

/*************************************************************************
  \brief
    Renders an individual tile, applying highlight visual changes if needed.
  \param tile
    Tile to render.
  \param baseX
    Base X world position of the grid.
  \param baseY
    Base Y world position of the grid.
  \param tilemapInstance
    Tilemap containing the visual representation of tiles.
  \param perspectiveFactor
    Factor affecting rendering perspective.
  \details
    Computes the tile’s world position and optionally applies highlight
    effects to the tile’s layers.
*************************************************************************/
void Grid::GridManager::RenderTile(
    const Tile& tile, 
    GGMath::float32 baseX, 
    GGMath::float32 baseY, 
    [[maybe_unused]] GGTilemap& tilemapInstance, 
    [[maybe_unused]] GGMath::float32 perspectiveFactor)
{
    [[maybe_unused]] GGMath::float32 x = baseX + tile.position.x * (renderConfig.tileSize + renderConfig.tileSpacing);
    [[maybe_unused]] GGMath::float32 y = baseY + tile.position.y * (renderConfig.tileSize + renderConfig.tileSpacing);
    
    // Highlight overlay
    if ((tile.state & TILE_HIGHLIGHTED) != 0)
    {
        // Highlight color logic
        /*for (GGTilemapLayer& layer : tilemapInstance.layers)
        {
            layer.GetTile(tile.position.y, tile.position.x).SetColorMultiply({ .8f, .8f, .8f, 1.f });
        }*/
    }
    else
    {
        /*for (GGTilemapLayer& layer : tilemapInstance.layers)
        {
            layer.GetTile(tile.position.y, tile.position.x).SetColorMultiply({ 1.f, 1.f, 1.f, 1.f });
        }*/
    }
}