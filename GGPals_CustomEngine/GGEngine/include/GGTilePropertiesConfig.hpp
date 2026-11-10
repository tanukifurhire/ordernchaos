/*************************************************************************
  \file       GGTilePropertiesConfig.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       16 November 2025
  \version    1.0
  \brief
    Configuration system for tile gameplay properties.
  \details
    Loads GGTileProperties.json config file to define gameplay behavior for
    each tileType. Provides runtime lookup of properties like walkability,
    vision blocking, cover values, and movement costs. Designed for easy
    extension - add new properties to config and this system without
    touching serialization or core tilemap code.

    Usage: Call GGTilePropertiesConfig::LoadFromFile() on startup, then
    query properties via GetTileProperties(tileType) during gameplay.

    NOTE: NO namespace used to avoid collision with GGEngine class.
  \dependencies
    RapidJSONWrapper.hpp, GGLogger.hpp, <string>, <unordered_map>
  \see
    GGTilemapData.hpp, TileProperties.json
  \history
    - 1.0 (16 November 2025): Initial version with core gameplay properties.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef GG_TILE_PROPERTIES_CONFIG_HPP
#define GG_TILE_PROPERTIES_CONFIG_HPP

#pragma region Includes
#include "RapidJSONWrapper.hpp"
#include <string>
#include <unordered_map>
#pragma endregion

#pragma region TileTypeProperties Structure
/*************************************************************************
  \brief
    Complete set of gameplay properties for a tile type
  \details
    Defines all gameplay behavior for a specific tileType ID. Loaded from
    TileProperties.json at runtime. Properties affect pathfinding, combat,
    vision systems, and destructibility.

    TO ADD NEW PROPERTIES:
    1. Add member variable here
    2. Add JSON field to TileProperties.json
    3. Update LoadFromFile() in .cpp to deserialize new field
    4. Use in gameplay systems (GGGrid, pathfinding, combat, etc.)
*************************************************************************/
struct TileTypeProperties
{
    int32_t tileTypeId;           // Unique ID matching GGTileData.tileType
    std::string name;             // Human-readable name
    std::string description;      // Detailed explanation

    // Core gameplay properties
    bool isWalkable;              // Can units move through this tile?
    bool blocksVision;            // Does this block line of sight?
    float coverValue;             // Defense bonus (0.0 = none, 1.0 = full)
    float movementCost;           // Movement point multiplier (1.0 = normal)
    bool isDestructible;          // Can be destroyed by attacks?
    std::string defaultSpritePath; // Default texture for this type

    /*************************************************************************
      \brief
        Default constructor initializes to safe empty tile values
    *************************************************************************/
    TileTypeProperties()
        : tileTypeId(0)
        , name("Unknown")
        , description("")
        , isWalkable(true)
        , blocksVision(false)
        , coverValue(0.0f)
        , movementCost(1.0f)
        , isDestructible(false)
        , defaultSpritePath("")
    {
    }
};
#pragma endregion

#pragma region TilePropertiesConfig Class
/*************************************************************************
  \brief
    Singleton configuration manager for tile properties
  \details
    Loads TileProperties.json on initialization, stores properties in
    hash map for O(1) lookup by tileType ID. Provides safe fallback
    for unknown tile types. Thread-safe for read-only access after
    initialization.
*************************************************************************/
class GGTilePropertiesConfig
{
public:
    /*************************************************************************
      \brief
        Load tile properties from JSON config file
      \param[in] filepath
        Path to TileProperties.json (typically "../../../Assets/Config/TileProperties.json")
      \return
        True if file loaded successfully, false on error
      \par Usage
      \code
        // Call once during engine initialization
        GGTilePropertiesConfig::LoadFromFile("../../../Assets/Config/TileProperties.json");
      \endcode
      \par Purpose
        Initialize config system before gameplay systems need tile data
    *************************************************************************/
    static bool LoadFromFile(const std::string& filepath);

    /*************************************************************************
      \brief
        Get properties for a specific tile type
      \param[in] tileTypeId
        Tile type ID to query
      \return
        Const reference to properties struct, or default "Unknown" properties if not found
      \par Usage
      \code
        const auto& props = GGTilePropertiesConfig::GetTileProperties(1);
        if (props.isWalkable)
        {
            // Allow unit to move to this tile
        }
      \endcode
      \par Purpose
        Runtime lookup of tile behavior for gameplay systems
    *************************************************************************/
    static const TileTypeProperties& GetTileProperties(int32_t tileTypeId);

    /*************************************************************************
      \brief
        Check if a tile type ID is defined in config
      \param[in] tileTypeId
        Tile type ID to check
      \return
        True if properties exist for this type
    *************************************************************************/
    static bool HasTileType(int32_t tileTypeId);

    /*************************************************************************
      \brief
        Get all loaded tile type IDs
      \return
        Vector of all valid tile type IDs
      \par Purpose
        Useful for editor dropdowns or validation
    *************************************************************************/
    static std::vector<int32_t> GetAllTileTypeIds();

    /*************************************************************************
      \brief
        Clear all loaded properties
      \par Purpose
        Cleanup on shutdown or for hot-reloading config
    *************************************************************************/
    static void Clear();

private:
    // Singleton storage - hash map for O(1) lookup
    static std::unordered_map<int32_t, TileTypeProperties> s_properties;

    // Fallback for unknown tile types
    static TileTypeProperties s_defaultProperties;
};
#pragma endregion

#endif // GG_TILE_PROPERTIES_CONFIG_HPP