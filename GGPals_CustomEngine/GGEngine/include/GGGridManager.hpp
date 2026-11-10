/*************************************************************************
  \file       GGGridManager.hpp
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
    - 1.1 (05 November 2025): Fixed includes, types, and Matrix3x3 method names.
    - 1.2 (05 November 2025): Added Render2D integration with actual rendering.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include "GGGrid.hpp"
#include "GGECSWorld.hpp"
#include "GGRender2d.hpp"
#include "GGMath/Vector2.hpp"
#include "GGMath/Vector4.hpp"
#include "GGMath/Matrix3x3.hpp"

#include <memory>
#include <unordered_map>
#include <cmath>

struct GGTilemap;

namespace Grid
{
    /*************************************************************************
      \brief
        Grid rendering configuration
    *************************************************************************/
    struct GridRenderConfig
    {
        GGMath::float32 tileSize = 1.0f;
        GGMath::float32 tileSpacing = 0.02f;
        GGMath::float32 gridOffsetX = 100.0f;
        GGMath::float32 gridOffsetY = 100.0f;

        GGMath::Vector4 blueColor = GGMath::Vector4(0.3f, 0.5f, 1.0f, 1.0f);
        GGMath::Vector4 greenColor = GGMath::Vector4(0.3f, 1.0f, 0.5f, 1.0f);
        GGMath::Vector4 destroyedColor = GGMath::Vector4(0.2f, 0.2f, 0.2f, 0.8f);
        GGMath::Vector4 fogColor = GGMath::Vector4(0.5f, 0.5f, 0.5f, 0.7f);
        GGMath::Vector4 highlightColor = GGMath::Vector4(1.0f, 1.0f, 0.3f, 0.5f);
        GGMath::Vector4 borderColor = GGMath::Vector4(0.1f, 0.1f, 0.1f, 1.0f);

        GGMath::float32 highlightPulseSpeed = 2.0f;
        GGMath::float32 damageFlashSpeed = 5.0f;
    };

    /*************************************************************************
      \brief
        Grid interaction result
    *************************************************************************/
    struct GridInteractionResult
    {
        bool success = false;
        Tile* targetTile = nullptr;
        std::vector<Tile*> affectedTiles;
        std::string message;
    };

    /*************************************************************************
      \brief
        GridManager singleton - manages all grids and rendering
    *************************************************************************/
    class GridManager
    {
    private:
        static GridManager* instance;

        std::unordered_map<uint32_t, std::unique_ptr<GridComponent>> grids;
        uint32_t orderGridID = 0;
        uint32_t chaosGridID = 0;

        GridComponent* activeGrid = nullptr;
        bool isOrderTurn = true;

        GridRenderConfig renderConfig;

        GGMath::Vector2Int hoveredTile;
        bool hasHoveredTile = false;

        GGMath::Matrix3x3 gridToWorldMatrix;
        GGMath::Matrix3x3 worldToGridMatrix;

        GridManager() = default;

    public:
        friend class GGECSGridSystem;

        static inline GridManager* GetInstance()
        {
            if (!instance)
                instance = new GridManager();
            return instance;
        }

        static inline void Destroy()
        {
            delete instance;
            instance = nullptr;
        }

        GridManager(const GridManager&) = delete;
        GridManager& operator=(const GridManager&) = delete;

        /*************************************************************************
          \brief
            Initialize grid system - creates Order and Chaos grids
        *************************************************************************/
        inline void Initialize()
        {
            orderGridID = GenerateEntityID();
            grids[orderGridID] = std::make_unique<GridComponent>(true, orderGridID);

            chaosGridID = GenerateEntityID();
            grids[chaosGridID] = std::make_unique<GridComponent>(false, chaosGridID);

            activeGrid = grids[orderGridID].get();
            UpdateTransformMatrices();
        }

        inline uint32_t CreateGrid(bool isOrder)
        {
            uint32_t gridID = GenerateEntityID();
            grids[gridID] = std::make_unique<GridComponent>(isOrder, gridID);
            return gridID;
        }

        inline GridComponent* GetGrid(uint32_t gridID)
        {
            auto it = grids.find(gridID);
            return (it != grids.end()) ? it->second.get() : nullptr;
        }

        inline void DestroyGrid(uint32_t gridID)
        {
            if (grids.contains(gridID)) grids.erase(gridID);
        }

        /*************************************************************************
          \brief
            Update all grids - handles animations and tile states
        *************************************************************************/
        inline void Update(GGMath::float32 deltaTime)
        {
            for (auto& [id, grid] : grids)
            {
                if (!grid->IsActive())
                    continue;

                for (auto& tile : grid->GetAllTiles())
                {
                    if (tile.damageFlashTimer > 0)
                        tile.damageFlashTimer -= deltaTime;

                    if (tile.state & TILE_HIGHLIGHTED)
                    {
                        tile.highlightIntensity = 0.5f + 0.5f *
                            std::sin(renderConfig.highlightPulseSpeed * GetTime());
                    }
                }
            }
        }

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
        void UpdateGrid(uint32_t gridID, GGMath::float32 perspectiveFactor);

        /*************************************************************************
          \brief
            Render grid
        *************************************************************************/
        void RenderGrid(uint32_t gridID, GGMath::float32 perspectiveFactor);

        /*************************************************************************
          \brief
            Render individual tile
        *************************************************************************/
        void RenderTile(const Tile& tile, GGMath::float32 baseX, GGMath::float32 baseY, GGTilemap& tilemapInstance, GGMath::float32 perspectiveFactor);

        inline GGMath::Vector2Int WorldToGrid(const GGMath::Vector2& worldPos, uint32_t gridID)
        {
            GridComponent* grid = GetGrid(gridID);
            if (!grid)
                return GGMath::Vector2Int(-1, -1);

            GGMath::float32 baseX = renderConfig.gridOffsetX + grid->GetPosition().x;
            GGMath::float32 baseY = renderConfig.gridOffsetY + grid->GetPosition().y;

            GGMath::float32 relX = worldPos.x - baseX;
            GGMath::float32 relY = worldPos.y - baseY;

            if (relX < 0.0f || relY < 0.0f)
                return GGMath::Vector2Int(-1, -1);

            int32_t gridX = static_cast<int32_t>(relX / (renderConfig.tileSize + renderConfig.tileSpacing));
            int32_t gridY = static_cast<int32_t>(relY / (renderConfig.tileSize + renderConfig.tileSpacing));

            if (gridX < 0 || gridX >= GridComponent::GRID_WIDTH ||
                gridY < 0 || gridY >= GridComponent::GRID_HEIGHT)
            {
                return GGMath::Vector2Int(-1, -1);
            }

            return GGMath::Vector2Int(gridX, gridY);
        }

        inline GGMath::Vector2 GridToWorld(const GGMath::Vector2Int& gridPos, uint32_t gridID)
        {
            GridComponent* grid = GetGrid(gridID);
            if (!grid)
                return GGMath::Vector2::zero();

            if (gridPos.x < 0 || gridPos.x >= GridComponent::GRID_WIDTH ||
                gridPos.y < 0 || gridPos.y >= GridComponent::GRID_HEIGHT)
            {
                return GGMath::Vector2(-1000.f, -1000.f);
            }

            GGMath::float32 baseX = renderConfig.gridOffsetX + grid->GetPosition().x;
            GGMath::float32 baseY = renderConfig.gridOffsetY + grid->GetPosition().y;

            GGMath::float32 worldX = baseX + gridPos.x * (renderConfig.tileSize + renderConfig.tileSpacing)
                + renderConfig.tileSize * 0.5f;
            GGMath::float32 worldY = baseY + gridPos.y * (renderConfig.tileSize + renderConfig.tileSpacing)
                + renderConfig.tileSize * 0.5f;

            return GGMath::Vector2(worldX, worldY);
        }

        inline void HandleMouseHover(const GGMath::Vector2& mousePos)
        {
            GGMath::Vector2Int orderPos = WorldToGrid(mousePos, orderGridID);
            if (orderPos.x >= 0)
            {
                hoveredTile = orderPos;
                hasHoveredTile = true;
                return;
            }

            GGMath::Vector2Int chaosPos = WorldToGrid(mousePos, chaosGridID);
            if (chaosPos.x >= 0)
            {
                hoveredTile = chaosPos;
                hasHoveredTile = true;
                return;
            }

            hasHoveredTile = false;
        }

        inline GridInteractionResult ApplyAreaDamage(uint32_t gridID,
            const GGMath::Vector2Int& center,
            int32_t width, int32_t height,
            int32_t damage)
        {
            GridInteractionResult result;

            GridComponent* grid = GetGrid(gridID);
            if (!grid)
            {
                result.message = "Invalid grid";
                return result;
            }

            result.affectedTiles = grid->GetTilesInArea(center, width, height);

            int32_t tilesDestroyed = 0;
            for (auto* tile : result.affectedTiles)
            {
                if (tile->TakeDamage(damage))
                    tilesDestroyed++;

                tile->Reveal();
            }

            result.success = true;
            result.message = std::to_string(tilesDestroyed) + " tiles destroyed";
            return result;
        }

        inline bool IsGridDestroyed(uint32_t gridID)
        {
            GridComponent* grid = GetGrid(gridID);
            return grid ? grid->IsGridDestroyed() : false;
        }

        // Getters
        uint32_t GetOrderGridID() const { return orderGridID; }
        uint32_t GetChaosGridID() const { return chaosGridID; }
        GridComponent* GetActiveGrid() { return activeGrid; }
        void SetActiveGrid(uint32_t gridID) { activeGrid = GetGrid(gridID); }
        bool IsOrdersTurn() const { return isOrderTurn; }
        void SwitchTurn() { isOrderTurn = !isOrderTurn; }
        GridRenderConfig& GetRenderConfig() { return renderConfig; }

    private:
        inline void UpdateTransformMatrices()
        {
            gridToWorldMatrix.identity();
        }

        inline uint32_t GenerateEntityID()
        {
            static uint32_t nextID = 1000;
            return nextID++;
        }

        inline GGMath::float32 GetTime()
        {
            static GGMath::float32 time = 0;
            time += 0.016f;
            return time;
        }
    };

    inline GridManager* GridManager::instance = nullptr;
}