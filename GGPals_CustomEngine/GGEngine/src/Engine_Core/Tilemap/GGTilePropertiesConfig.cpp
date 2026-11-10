/*************************************************************************
  \file       GGTilePropertiesConfig.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       16 November 2025
  \version    1.0
  \brief
    Implementation of tile properties configuration loader.
  \details
    Loads TileProperties.json using RapidJSON and populates hash map
    for fast runtime lookup. Provides robust error handling for missing
    or malformed config files.
  \dependencies
    GGTilePropertiesConfig.hpp, GGLogger.hpp, <fstream>, <sstream>
  \see
    GGTilePropertiesConfig.hpp, TileProperties.json
  \history
    - 1.0 (16 November 2025): Initial implementation.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma region Includes
#include "GGTilePropertiesConfig.hpp"
#include "GGLogger.hpp"
#include <fstream>
#include <sstream>
#pragma endregion

#pragma region Static Members
std::unordered_map<int32_t, TileTypeProperties> GGTilePropertiesConfig::s_properties;
TileTypeProperties GGTilePropertiesConfig::s_defaultProperties;
#pragma endregion

#pragma region Public Methods

/*************************************************************************
  \brief
    Load tile properties from JSON file
*************************************************************************/
bool GGTilePropertiesConfig::LoadFromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        GGLogger::Error("Failed to open TileProperties.json: " + filepath);
        GGLogger::Error("Make sure the file exists at: ../../../Assets/Config/TileProperties.json");
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    rapidjson::Document doc;
    doc.Parse(buffer.str().c_str());

    if (doc.HasParseError())
    {
        GGLogger::Error("JSON parse error in TileProperties.json");
        return false;
    }

    if (!doc.HasMember("tileTypes") || !doc["tileTypes"].IsArray())
    {
        GGLogger::Error("TileProperties.json missing 'tileTypes' array");
        return false;
    }

    // Clear existing properties
    s_properties.clear();

    // Load each tile type
    const auto& tileTypesArray = doc["tileTypes"].GetArray();
    int32_t loadedCount = 0;

    for (const auto& tileTypeJson : tileTypesArray)
    {
        if (!tileTypeJson.IsObject())
        {
            GGLogger::Warning("Skipping invalid tile type entry in config");
            continue;
        }

        TileTypeProperties props;

        // Required fields
        if (!tileTypeJson.HasMember("tileTypeId"))
        {
            GGLogger::Warning("Tile type missing 'tileTypeId', skipping");
            continue;
        }

        props.tileTypeId = tileTypeJson["tileTypeId"].GetInt();

        // Optional fields with defaults
        if (tileTypeJson.HasMember("name"))
        {
            props.name = tileTypeJson["name"].GetString();
        }

        if (tileTypeJson.HasMember("description"))
        {
            props.description = tileTypeJson["description"].GetString();
        }

        if (tileTypeJson.HasMember("isWalkable"))
        {
            props.isWalkable = tileTypeJson["isWalkable"].GetBool();
        }

        if (tileTypeJson.HasMember("blocksVision"))
        {
            props.blocksVision = tileTypeJson["blocksVision"].GetBool();
        }

        if (tileTypeJson.HasMember("coverValue"))
        {
            props.coverValue = tileTypeJson["coverValue"].GetFloat();
        }

        if (tileTypeJson.HasMember("movementCost"))
        {
            props.movementCost = tileTypeJson["movementCost"].GetFloat();
        }

        if (tileTypeJson.HasMember("isDestructible"))
        {
            props.isDestructible = tileTypeJson["isDestructible"].GetBool();
        }

        if (tileTypeJson.HasMember("defaultSpritePath"))
        {
            props.defaultSpritePath = tileTypeJson["defaultSpritePath"].GetString();
        }

        // Store in map
        s_properties[props.tileTypeId] = props;
        ++loadedCount;

        // TO ADD NEW PROPERTIES:
        // 1. Add to TileProperties.json for each tileType
        // 2. Add member to TileTypeProperties struct
        // 3. Add deserialization here:
        //    if (tileTypeJson.HasMember("yourNewProperty"))
        //    {
        //        props.yourNewProperty = tileTypeJson["yourNewProperty"].GetBool/Int/Float/String();
        //    }
        // 4. Use in gameplay code via GetTileProperties()
    }

    GGLogger::Info("Loaded " + std::to_string(loadedCount) + " tile type definitions from " + filepath);
    return loadedCount > 0;
}

/*************************************************************************
  \brief
    Get properties for tile type
*************************************************************************/
const TileTypeProperties& GGTilePropertiesConfig::GetTileProperties(int32_t tileTypeId)
{
    auto it = s_properties.find(tileTypeId);
    if (it != s_properties.end())
    {
        return it->second;
    }

    // Return default if not found
    GGLogger::Warning("Tile type " + std::to_string(tileTypeId) + " not found in config, using defaults");
    return s_defaultProperties;
}

/*************************************************************************
  \brief
    Check if tile type exists
*************************************************************************/
bool GGTilePropertiesConfig::HasTileType(int32_t tileTypeId)
{
    return s_properties.find(tileTypeId) != s_properties.end();
}

/*************************************************************************
  \brief
    Get all tile type IDs
*************************************************************************/
std::vector<int32_t> GGTilePropertiesConfig::GetAllTileTypeIds()
{
    std::vector<int32_t> ids;
    ids.reserve(s_properties.size());

    for (const auto& pair : s_properties)
    {
        ids.push_back(pair.first);
    }

    return ids;
}

/*************************************************************************
  \brief
    Clear all properties
*************************************************************************/
void GGTilePropertiesConfig::Clear()
{
    s_properties.clear();
}

#pragma endregion