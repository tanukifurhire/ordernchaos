/*************************************************************************
  \file       GGGridPathfinding.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       05 November 2025
  \version    1.0
  \brief
    Defines pathfinding and movement validation for the grid system.
  \details
    Implements both Simple Validation and BFS pathfinding with profiling
    to compare performance. Handles unit movement validation, checking
    for destroyed tiles and occupied positions. Only one algorithm runs
    at a time based on configuration.
  \dependencies
    GGGrid.hpp, Vector2Int.hpp, GGPerformanceProfiler.hpp
  \see
    GridComponent, Tile, GGPerformanceProfiler
  \history
    - 1.0 (05 November 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include "GGGrid.hpp"
#include "GGMath/Vector2Int.hpp"
#include "GGPerformanceProfiler.hpp"
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <chrono>

namespace Grid
{
    /*************************************************************************
      \brief
        Pathfinding algorithm type selector
    *************************************************************************/
    enum class PathfindingAlgorithm : uint8_t
    {
        SIMPLE_VALIDATION = 0,  // Direct tile checking
        BFS_PATHFINDING = 1     // Breadth-First Search
    };

    /*************************************************************************
      \brief
        Movement validation result structure
      \details
        Contains validation results and performance metrics
    *************************************************************************/
    struct MovementValidationResult
    {
        bool canMove = false;                      // Whether movement is valid
        std::vector<GGMath::Vector2Int> path;      // Path to target (if using BFS)
        std::string reason;                         // Reason for invalid movement
        GGMath::float32 validationTimeMs = 0.0f;           // Time taken for validation
        int32_t tilesChecked = 0;                                       // Number of tiles examined
        PathfindingAlgorithm algorithmUsed = PathfindingAlgorithm::SIMPLE_VALIDATION;        // Which algorithm was used
    };

    /*************************************************************************
      \brief
        Profiling data for pathfinding algorithms
    *************************************************************************/
    struct PathfindingProfileData
    {
        // Simple validation metrics
        GGMath::float32 simpleAvgTimeMs = 0.0f;
        GGMath::float32 simpleMinTimeMs = FLT_MAX;
        GGMath::float32 simpleMaxTimeMs = 0.0f;
        int32_t simpleCallCount = 0;
        int32_t simpleSuccessCount = 0;

        // BFS metrics
        GGMath::float32 bfsAvgTimeMs = 0.0f;
        GGMath::float32 bfsMinTimeMs = FLT_MAX;
        GGMath::float32 bfsMaxTimeMs = 0.0f;
        int32_t bfsCallCount = 0;
        int32_t bfsSuccessCount = 0;
        int32_t bfsAvgPathLength = 0;

        // Comparison metrics
        GGMath::float32 totalSimpleTime = 0.0f;
        GGMath::float32 totalBfsTime = 0.0f;

        /*************************************************************************
          \brief
            Reset all profiling data
        *************************************************************************/
        void Reset()
        {
            simpleAvgTimeMs = 0.0f;
            simpleMinTimeMs = FLT_MAX;
            simpleMaxTimeMs = 0.0f;
            simpleCallCount = 0;
            simpleSuccessCount = 0;

            bfsAvgTimeMs = 0.0f;
            bfsMinTimeMs = FLT_MAX;
            bfsMaxTimeMs = 0.0f;
            bfsCallCount = 0;
            bfsSuccessCount = 0;
            bfsAvgPathLength = 0;

            totalSimpleTime = 0.0f;
            totalBfsTime = 0.0f;
        }

        /*************************************************************************
          \brief
            Get profiling report as string
          \return
            Formatted profiling statistics
        *************************************************************************/
        std::string GetReport() const
        {
            std::string report = "=== Pathfinding Profile Report ===\n";

            report += "\nSimple Validation:\n";
            report += "  Calls: " + std::to_string(simpleCallCount) + "\n";
            report += "  Success Rate: " + std::to_string(simpleSuccessCount) + "/" +
                std::to_string(simpleCallCount) + "\n";
            report += "  Avg Time: " + std::to_string(simpleAvgTimeMs) + " ms\n";
            report += "  Min Time: " + std::to_string(simpleMinTimeMs) + " ms\n";
            report += "  Max Time: " + std::to_string(simpleMaxTimeMs) + " ms\n";
            report += "  Total Time: " + std::to_string(totalSimpleTime) + " ms\n";

            report += "\nBFS Pathfinding:\n";
            report += "  Calls: " + std::to_string(bfsCallCount) + "\n";
            report += "  Success Rate: " + std::to_string(bfsSuccessCount) + "/" +
                std::to_string(bfsCallCount) + "\n";
            report += "  Avg Time: " + std::to_string(bfsAvgTimeMs) + " ms\n";
            report += "  Min Time: " + std::to_string(bfsMinTimeMs) + " ms\n";
            report += "  Max Time: " + std::to_string(bfsMaxTimeMs) + " ms\n";
            report += "  Avg Path Length: " + std::to_string(bfsAvgPathLength) + "\n";
            report += "  Total Time: " + std::to_string(totalBfsTime) + " ms\n";

            if (simpleCallCount > 0 && bfsCallCount > 0)
            {
                GGMath::float32 speedup = bfsAvgTimeMs / simpleAvgTimeMs;
                report += "\nComparison:\n";
                report += "  Simple is " + std::to_string(speedup) + "x ";
                report += (speedup > 1.0f) ? "faster" : "slower";
                report += " than BFS on average\n";
            }

            return report;
        }
    };

    /*************************************************************************
      \brief
        Grid pathfinding and movement validation system
      \details
        Provides two pathfinding algorithms with profiling:
        1. Simple Validation - Direct tile checking
        2. BFS - Breadth-First Search for optimal path
    *************************************************************************/
    class GridPathfinding
    {
    private:
        PathfindingAlgorithm currentAlgorithm;
        PathfindingProfileData profileData;
        bool profilingEnabled;

        // BFS cache to avoid recreating containers
        std::queue<GGMath::Vector2Int> bfsQueue;
        std::unordered_set<int32_t> visitedTiles;
        std::unordered_map<int32_t, GGMath::Vector2Int> parentMap;

    public:
        /*************************************************************************
          \brief
            Constructor
          \param[in] algorithm
            Initial pathfinding algorithm to use
          \param[in] enableProfiling
            Whether to collect profiling data
        *************************************************************************/
        GridPathfinding(PathfindingAlgorithm algorithm = PathfindingAlgorithm::SIMPLE_VALIDATION,
            bool enableProfiling = true) :
            currentAlgorithm(algorithm),
            profilingEnabled(enableProfiling)
        {
            profileData.Reset();
        }

        /*************************************************************************
          \brief
            Validate if a unit can move from one tile to another
          \param[in] grid
            Grid component to check
          \param[in] from
            Starting position
          \param[in] to
            Target position
          \param[in] maxRange
            Maximum movement range (Manhattan distance)
          \param[in] unitID
            ID of moving unit (to ignore self)
          \return
            Movement validation result with path and metrics
        *************************************************************************/
        MovementValidationResult ValidateMovement(
            GridComponent* grid,
            const GGMath::Vector2Int& from,
            const GGMath::Vector2Int& to,
            int32_t maxRange,
            uint32_t unitID = 0)
        {
            if (currentAlgorithm == PathfindingAlgorithm::SIMPLE_VALIDATION)
                return ValidateMovementSimple(grid, from, to, maxRange, unitID);
            else
                return ValidateMovementBFS(grid, from, to, maxRange, unitID);
        }

        /*************************************************************************
          \brief
            Simple validation - checks direct path availability
          \details
            Fast validation that checks if target is in range and accessible.
            Does not guarantee a valid path exists.
        *************************************************************************/
        MovementValidationResult ValidateMovementSimple(
            GridComponent* grid,
            const GGMath::Vector2Int& from,
            const GGMath::Vector2Int& to,
            int32_t maxRange,
            uint32_t unitID = 0)
        {
            MovementValidationResult result;
            result.algorithmUsed = PathfindingAlgorithm::SIMPLE_VALIDATION;

            // Start profiling
            auto startTime = std::chrono::high_resolution_clock::now();
            int32_t tilesChecked = 0;

            // Check if grid is valid
            if (!grid)
            {
                result.reason = "Invalid grid";
                RecordSimpleProfile(startTime, false, tilesChecked);
                return result;
            }

            // Check if positions are within bounds
            Tile* fromTile = grid->GetTile(from);
            Tile* toTile = grid->GetTile(to);
            tilesChecked = 2;

            if (!fromTile || !toTile)
            {
                result.reason = "Position out of bounds";
                RecordSimpleProfile(startTime, false, tilesChecked);
                return result;
            }

            // Check Manhattan distance
            int32_t distance = from.manhattanDistance(to);
            if (distance > maxRange)
            {
                result.reason = "Target out of range (distance: " +
                    std::to_string(distance) + ", max: " +
                    std::to_string(maxRange) + ")";
                RecordSimpleProfile(startTime, false, tilesChecked);
                return result;
            }

            // Check if target tile is passable
            if (!toTile->IsPassable())
            {
                result.reason = "Target tile is destroyed";
                RecordSimpleProfile(startTime, false, tilesChecked);
                return result;
            }

            // Check if target is occupied by another unit
            if (toTile->unitID != 0 && toTile->unitID != unitID)
            {
                result.reason = "Target tile is occupied by unit " +
                    std::to_string(toTile->unitID);
                RecordSimpleProfile(startTime, false, tilesChecked);
                return result;
            }

            // Simple path check - verify tiles along path are passable
            // This is a simplified check, not guaranteeing optimal path
            GGMath::Vector2Int current = from;
            result.path.push_back(current);

            while (current != to)
            {
                // Move towards target one step at a time
                GGMath::Vector2Int next = current;

                if (current.x < to.x)
                    next.x++;
                else if (current.x > to.x)
                    next.x--;
                else if (current.y < to.y)
                    next.y++;
                else if (current.y > to.y)
                    next.y--;

                Tile* nextTile = grid->GetTile(next);
                tilesChecked++;

                if (!nextTile || !nextTile->IsPassable())
                {
                    // Try alternate path
                    next = current;
                    if (current.y < to.y)
                        next.y++;
                    else if (current.y > to.y)
                        next.y--;
                    else if (current.x < to.x)
                        next.x++;
                    else if (current.x > to.x)
                        next.x--;

                    nextTile = grid->GetTile(next);
                    tilesChecked++;

                    if (!nextTile || !nextTile->IsPassable())
                    {
                        result.reason = "Path blocked at " +
                            std::to_string(next.x) + "," +
                            std::to_string(next.y);
                        RecordSimpleProfile(startTime, false, tilesChecked);
                        return result;
                    }
                }

                current = next;
                result.path.push_back(current);
            }

            // Movement is valid
            result.canMove = true;
            result.reason = "Movement valid";
            result.tilesChecked = tilesChecked;

            RecordSimpleProfile(startTime, true, tilesChecked);

            // Calculate time
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            result.validationTimeMs = duration.count() / 1000.0f;

            return result;
        }

        /*************************************************************************
          \brief
            BFS validation - finds optimal path if exists
          \details
            More thorough but slower validation that finds the shortest
            valid path between two points.
        *************************************************************************/
        MovementValidationResult ValidateMovementBFS(
            GridComponent* grid,
            const GGMath::Vector2Int& from,
            const GGMath::Vector2Int& to,
            int32_t maxRange,
            uint32_t unitID = 0)
        {
            MovementValidationResult result;
            result.algorithmUsed = PathfindingAlgorithm::BFS_PATHFINDING;

            // Start profiling
            auto startTime = std::chrono::high_resolution_clock::now();
            int32_t tilesChecked = 0;

            // Check if grid is valid
            if (!grid)
            {
                result.reason = "Invalid grid";
                RecordBFSProfile(startTime, false, tilesChecked, 0);
                return result;
            }

            // Check bounds
            Tile* fromTile = grid->GetTile(from);
            Tile* toTile = grid->GetTile(to);

            if (!fromTile || !toTile)
            {
                result.reason = "Position out of bounds";
                RecordBFSProfile(startTime, false, tilesChecked, 0);
                return result;
            }

            // Check if target is passable
            if (!toTile->IsPassable())
            {
                result.reason = "Target tile is destroyed";
                RecordBFSProfile(startTime, false, tilesChecked, 0);
                return result;
            }

            // Check if target is occupied
            if (toTile->unitID != 0 && toTile->unitID != unitID)
            {
                result.reason = "Target tile is occupied";
                RecordBFSProfile(startTime, false, tilesChecked, 0);
                return result;
            }

            // Clear BFS containers
            while (!bfsQueue.empty())
                bfsQueue.pop();
            visitedTiles.clear();
            parentMap.clear();

            // BFS initialization
            bfsQueue.push(from);
            visitedTiles.insert(TileToIndex(from));
            parentMap[TileToIndex(from)] = GGMath::Vector2Int(-1, -1);

            bool pathFound = false;

            // BFS search
            while (!bfsQueue.empty() && !pathFound)
            {
                GGMath::Vector2Int current = bfsQueue.front();
                bfsQueue.pop();
                tilesChecked++;

                // Check if we reached the target
                if (current == to)
                {
                    pathFound = true;
                    break;
                }

                // Check distance constraint
                int32_t currentDistance = from.manhattanDistance(current);
                if (currentDistance >= maxRange)
                    continue;

                // Check all 4 adjacent tiles
                static const GGMath::Vector2Int directions[4] = {
                    GGMath::Vector2Int(0, -1),  // Up
                    GGMath::Vector2Int(1, 0),   // Right
                    GGMath::Vector2Int(0, 1),   // Down
                    GGMath::Vector2Int(-1, 0)   // Left
                };

                for (const auto& dir : directions)
                {
                    GGMath::Vector2Int next = current + dir;
                    int32_t nextIndex = TileToIndex(next);

                    // Skip if already visited
                    if (visitedTiles.find(nextIndex) != visitedTiles.end())
                        continue;

                    // Get tile and check if valid
                    Tile* nextTile = grid->GetTile(next);
                    if (!nextTile)
                        continue;

                    tilesChecked++;

                    // Check if tile is passable
                    if (!nextTile->IsPassable())
                        continue;

                    // Check if tile is occupied (but allow target tile)
                    if (nextTile->unitID != 0 && nextTile->unitID != unitID && next != to)
                        continue;

                    // Check if within range
                    if (from.manhattanDistance(next) > maxRange)
                        continue;

                    // Add to queue
                    visitedTiles.insert(nextIndex);
                    parentMap[nextIndex] = current;
                    bfsQueue.push(next);
                }
            }

            if (pathFound)
            {
                // Reconstruct path
                std::vector<GGMath::Vector2Int> reversePath;
                GGMath::Vector2Int current = to;

                while (current.x != -1 && current.y != -1)
                {
                    reversePath.push_back(current);
                    current = parentMap[TileToIndex(current)];
                }

                // Reverse to get path from start to end
                result.path.reserve(reversePath.size());
                for (auto it = reversePath.rbegin(); it != reversePath.rend(); ++it)
                {
                    result.path.push_back(*it);
                }

                result.canMove = true;
                result.reason = "Path found (length: " + std::to_string(result.path.size()) + ")";

                RecordBFSProfile(startTime, true, tilesChecked, static_cast<int32_t>(result.path.size()));
            }
            else
            {
                result.reason = "No valid path within range";
                RecordBFSProfile(startTime, false, tilesChecked, 0);
            }

            result.tilesChecked = tilesChecked;

            // Calculate time
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            result.validationTimeMs = duration.count() / 1000.0f;

            return result;
        }

        /*************************************************************************
          \brief
            Switch pathfinding algorithm
          \param[in] algorithm
            Algorithm to use for future validations
        *************************************************************************/
        void SetAlgorithm(PathfindingAlgorithm algorithm)
        {
            currentAlgorithm = algorithm;
        }

        /*************************************************************************
          \brief
            Get current algorithm
          \return
            Currently active pathfinding algorithm
        *************************************************************************/
        PathfindingAlgorithm GetAlgorithm() const
        {
            return currentAlgorithm;
        }

        /*************************************************************************
          \brief
            Enable/disable profiling
          \param[in] enable
            Whether to collect profiling data
        *************************************************************************/
        void SetProfilingEnabled(bool enable)
        {
            profilingEnabled = enable;
        }

        /*************************************************************************
          \brief
            Get profiling data
          \return
            Reference to profiling statistics
        *************************************************************************/
        const PathfindingProfileData& GetProfileData() const
        {
            return profileData;
        }

        /*************************************************************************
          \brief
            Reset profiling data
        *************************************************************************/
        void ResetProfileData()
        {
            profileData.Reset();
        }

        /*************************************************************************
          \brief
            Run performance comparison
          \details
            Runs both algorithms on the same query for comparison
          \return
            Comparison results as string
        *************************************************************************/
        std::string RunPerformanceComparison(
            GridComponent* grid,
            const GGMath::Vector2Int& from,
            const GGMath::Vector2Int& to,
            int32_t maxRange,
            uint32_t unitID = 0)
        {
            std::string report = "=== Performance Comparison ===\n";
            report += "From: (" + std::to_string(from.x) + "," + std::to_string(from.y) + ")\n";
            report += "To: (" + std::to_string(to.x) + "," + std::to_string(to.y) + ")\n";
            report += "Max Range: " + std::to_string(maxRange) + "\n\n";

            // Run simple validation
            auto simpleResult = ValidateMovementSimple(grid, from, to, maxRange, unitID);
            report += "Simple Validation:\n";
            report += "  Result: " + std::string(simpleResult.canMove ? "SUCCESS" : "FAILED") + "\n";
            report += "  Time: " + std::to_string(simpleResult.validationTimeMs) + " ms\n";
            report += "  Tiles Checked: " + std::to_string(simpleResult.tilesChecked) + "\n";
            report += "  Path Length: " + std::to_string(simpleResult.path.size()) + "\n";
            report += "  Reason: " + simpleResult.reason + "\n\n";

            // Run BFS
            auto bfsResult = ValidateMovementBFS(grid, from, to, maxRange, unitID);
            report += "BFS Pathfinding:\n";
            report += "  Result: " + std::string(bfsResult.canMove ? "SUCCESS" : "FAILED") + "\n";
            report += "  Time: " + std::to_string(bfsResult.validationTimeMs) + " ms\n";
            report += "  Tiles Checked: " + std::to_string(bfsResult.tilesChecked) + "\n";
            report += "  Path Length: " + std::to_string(bfsResult.path.size()) + "\n";
            report += "  Reason: " + bfsResult.reason + "\n\n";

            // Comparison
            report += "Comparison:\n";
            GGMath::float32 speedup = bfsResult.validationTimeMs / simpleResult.validationTimeMs;
            report += "  Simple is " + std::to_string(speedup) + "x ";
            report += (speedup > 1.0f) ? "faster" : "slower";
            report += " for this query\n";

            if (simpleResult.canMove != bfsResult.canMove)
            {
                report += "  WARNING: Algorithms disagree on result!\n";
            }

            return report;
        }

    private:
        /*************************************************************************
          \brief
            Convert 2D tile position to 1D index for hashing
          \param[in] pos
            Tile position
          \return
            Unique index for tile
        *************************************************************************/
        int32_t TileToIndex(const GGMath::Vector2Int& pos) const
        {
            return pos.y * GridComponent::GRID_WIDTH + pos.x;
        }

        /*************************************************************************
          \brief
            Record profiling data for simple validation
        *************************************************************************/
        void RecordSimpleProfile(
            const std::chrono::high_resolution_clock::time_point& startTime,
            bool success,
            int32_t)
        {
            if (!profilingEnabled)
                return;

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            GGMath::float32 timeMs = duration.count() / 1000.0f;

            profileData.simpleCallCount++;
            if (success)
                profileData.simpleSuccessCount++;

            profileData.totalSimpleTime += timeMs;
            profileData.simpleAvgTimeMs = profileData.totalSimpleTime / profileData.simpleCallCount;
            profileData.simpleMinTimeMs = std::min(profileData.simpleMinTimeMs, timeMs);
            profileData.simpleMaxTimeMs = std::max(profileData.simpleMaxTimeMs, timeMs);
        }

        /*************************************************************************
          \brief
            Record profiling data for BFS
        *************************************************************************/
        void RecordBFSProfile(
            const std::chrono::high_resolution_clock::time_point& startTime,
            bool success,
            int32_t,
            int32_t pathLength)
        {
            if (!profilingEnabled)
                return;

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            GGMath::float32 timeMs = duration.count() / 1000.0f;

            profileData.bfsCallCount++;
            if (success)
            {
                profileData.bfsSuccessCount++;
                profileData.bfsAvgPathLength =
                    ((profileData.bfsAvgPathLength * (profileData.bfsSuccessCount - 1)) + pathLength)
                    / profileData.bfsSuccessCount;
            }

            profileData.totalBfsTime += timeMs;
            profileData.bfsAvgTimeMs = profileData.totalBfsTime / profileData.bfsCallCount;
            profileData.bfsMinTimeMs = std::min(profileData.bfsMinTimeMs, timeMs);
            profileData.bfsMaxTimeMs = std::max(profileData.bfsMaxTimeMs, timeMs);
        }
    };
}