/*************************************************************************
  \file       GGTilemapSerializer.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       15 November 2025
  \version    1.1
  \brief
    Implementation of JSON serialization for tilemap data.
  \details
    Implements SaveToFile and LoadFromFile methods using RapidJSON for
    efficient JSON parsing. Handles all tilemap properties including
    layers, tiles, and metadata. Provides robust error handling for
    malformed JSON or file I/O errors.
  \dependencies
    GGTilemapSerializer.hpp, GGLogger.hpp, <fstream>, <sstream>
  \see
    GGTilemapSerializer.hpp, GGTilemapData.hpp
  \history
    - 1.0 (15 November 2025): Initial implementation.
    - 1.1 (15 November 2025): Removed namespace, fixed GGLogger API.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma region Includes
#include "GGTilemapSerializer.hpp"
#include "GGLogger.hpp"
#include <sstream>

#include <GGECSAssetInterface.hpp>
#pragma endregion

#pragma region Public Methods

/*************************************************************************
  \brief
    Save tilemap to JSON file
*************************************************************************/
bool GGTilemapSerializer::SaveToFile(const GGTilemap& tilemap, const std::string& filepath)
{
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    // Serialize map metadata
    rapidjson::Value mapName(tilemap.mapName.c_str(), allocator);
    doc.AddMember("mapName", mapName, allocator);
    doc.AddMember("tileSize", tilemap.tileSize, allocator);

    // Serialize layers array
    rapidjson::Value layersArray(rapidjson::kArrayType);
    for (const auto& layer : tilemap.layers)
    {
        layersArray.PushBack(SerializeLayer(layer, allocator), allocator);
    }
    doc.AddMember("layers", layersArray, allocator);

    // Write to file
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream file(filepath);
    if (!file.is_open())
    {
        GGLogger::Error("Failed to open file for writing: " + filepath);
        return false;
    }

    file << buffer.GetString();
    file.close();

    GGLogger::Info("Tilemap saved successfully: " + filepath);
    return true;
}

/*************************************************************************
  \brief
    Load tilemap from JSON file
*************************************************************************/
bool GGTilemapSerializer::LoadFromFile(const std::string& filepath, GGTilemap& outTilemap)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        GGLogger::Error("Failed to open file for reading: " + filepath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    rapidjson::Document doc;
    doc.Parse(buffer.str().c_str());

    if (doc.HasParseError())
    {
        GGLogger::Error("JSON parse error in file: " + filepath);
        return false;
    }

    // Deserialize map metadata
    if (!doc.HasMember("mapName") || !doc.HasMember("tileSize"))
    {
        GGLogger::Error("Missing required tilemap fields");
        return false;
    }

    outTilemap.mapName = doc["mapName"].GetString();
    outTilemap.tileSize = doc["tileSize"].GetFloat();

    // Deserialize layers
    outTilemap.layers.clear();
    if (doc.HasMember("layers") && doc["layers"].IsArray())
    {
        const auto& layersArray = doc["layers"].GetArray();
        for (const auto& layerJson : layersArray)
        {
            GGTilemapLayer layer;
            if (DeserializeLayer(layerJson, layer))
            {
                outTilemap.layers.push_back(layer);
            }
            else
            {
                GGLogger::Warning("Failed to deserialize layer, skipping");
            }
        }
    }

    GGLogger::Info("Tilemap loaded successfully: " + filepath);
    return true;
}

#pragma endregion

#pragma region Private Serialization Methods

/*************************************************************************
  \brief
    Serialize single tile to JSON object
*************************************************************************/
rapidjson::Value GGTilemapSerializer::SerializeTile(const GGTileData& tile,
    rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value tileJson(rapidjson::kObjectType);

    // Serialize sprite path as string
    rapidjson::Value spritePathStr(tile.spritePath.c_str(), allocator);
    tileJson.AddMember("spritePath", spritePathStr, allocator);

    {
        auto uuidOpt = GGECSStandardAssetManager<GLuint>::GetUUIDFromID(tile.spriteID);
        if (uuidOpt.has_value())
        {
            rapidjson::Value uuidStr(uuids::to_string(uuidOpt.value()).c_str(), allocator);
            tileJson.AddMember("textureAssetUUID", uuidStr, allocator);
        }
        else
        {
            tileJson.AddMember("textureAssetUUID", "", allocator);
            std::cerr << "[SerializeTile] Warning: No UUID found for textureAssetUUID" << std::endl;
        }
    }

    tileJson.AddMember("isDestructible", tile.isDestructible, allocator);
    tileJson.AddMember("tileType", tile.tileType, allocator);
    tileJson.AddMember("variation", tile.variation, allocator);

    return tileJson;
}

/*************************************************************************
  \brief
    Deserialize single tile from JSON object
*************************************************************************/
bool GGTilemapSerializer::DeserializeTile(const rapidjson::Value& tileJson, GGTileData& outTile)
{
    if (!tileJson.IsObject())
    {
        return false;
    }

    if (tileJson.HasMember("spritePath"))
    {
        outTile.spritePath = tileJson["spritePath"].GetString();
    }

    if (tileJson.HasMember("textureAssetUUID"))
    {
        std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
            tileJson["textureAssetUUID"].GetString()
        );

        if (uuid)
        {
            auto& assetSystem = GGAssetID::GetGGAssetID();

            if (assetSystem->DoesUUIDExist(uuid.value()))
            {
                std::string const& filePath = assetSystem->GetFilePath(uuid.value());
                uint32_t assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(uuid.value());
                outTile.spriteID = assetID;

                if (assetID)
                {
                    std::cout << "[DeserializeTile] Registered and loaded texture " << filePath
                        << " with ID " << assetID << std::endl;
                }
                else
                {
                    std::cout << "[DeserializeTile] No Asset ID Registered, default to 0" << std::endl;
                }
            }
            else
            {
                std::cerr << "[DeserializeTile] Texture UUID does not exist: " << uuids::to_string(uuid.value())
                    << std::endl;

                assetSystem->PrintAll();
            }
        }
        else
        {
            std::cerr << "[DeserializeTile] Invalid texture UUID string: "
                << tileJson["textureAssetUUID"].GetString() << std::endl;
        }
    }

    if (tileJson.HasMember("isDestructible"))
    {
        outTile.isDestructible = tileJson["isDestructible"].GetBool();
    }

    if (tileJson.HasMember("tileType"))
    {
        outTile.tileType = tileJson["tileType"].GetInt();
    }

    if (tileJson.HasMember("variation"))
    {
        outTile.variation = tileJson["variation"].GetUint();
    }

    return true;
}

/*************************************************************************
  \brief
    Serialize layer to JSON object
*************************************************************************/
rapidjson::Value GGTilemapSerializer::SerializeLayer(const GGTilemapLayer& layer,
    rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value layerJson(rapidjson::kObjectType);

    rapidjson::Value layerName(layer.layerName.c_str(), allocator);
    layerJson.AddMember("layerName", layerName, allocator);
    layerJson.AddMember("isVisible", layer.isVisible, allocator);
    layerJson.AddMember("isLocked", layer.isLocked, allocator);

    // Serialize tiles as 2D array
    rapidjson::Value tilesArray(rapidjson::kArrayType);
    for (int32_t row = 0; row < GGTilemapLayer::GRID_HEIGHT; ++row)
    {
        rapidjson::Value rowArray(rapidjson::kArrayType);
        for (int32_t col = 0; col < GGTilemapLayer::GRID_WIDTH; ++col)
        {
            rowArray.PushBack(SerializeTile(layer.tiles[row][col], allocator), allocator);
        }
        tilesArray.PushBack(rowArray, allocator);
    }
    layerJson.AddMember("tiles", tilesArray, allocator);

    return layerJson;
}

/*************************************************************************
  \brief
    Deserialize layer from JSON object
*************************************************************************/
bool GGTilemapSerializer::DeserializeLayer(const rapidjson::Value& layerJson, GGTilemapLayer& outLayer)
{
    if (!layerJson.IsObject())
    {
        return false;
    }

    if (layerJson.HasMember("layerName"))
    {
        outLayer.layerName = layerJson["layerName"].GetString();
    }

    if (layerJson.HasMember("isVisible"))
    {
        outLayer.isVisible = layerJson["isVisible"].GetBool();
    }

    if (layerJson.HasMember("isLocked"))
    {
        outLayer.isLocked = layerJson["isLocked"].GetBool();
    }

    // Deserialize tiles 2D array
    if (layerJson.HasMember("tiles") && layerJson["tiles"].IsArray())
    {
        const auto& tilesArray = layerJson["tiles"].GetArray();
        int32_t row = 0;
        for (const auto& rowJson : tilesArray)
        {
            if (row >= GGTilemapLayer::GRID_HEIGHT)
            {
                break;
            }

            if (rowJson.IsArray())
            {
                const auto& rowArray = rowJson.GetArray();
                int32_t col = 0;
                for (const auto& tileJson : rowArray)
                {
                    if (col >= GGTilemapLayer::GRID_WIDTH)
                    {
                        break;
                    }

                    DeserializeTile(tileJson, outLayer.tiles[row][col]);
                    ++col;
                }
            }
            ++row;
        }
    }

    return true;
}

#pragma endregion