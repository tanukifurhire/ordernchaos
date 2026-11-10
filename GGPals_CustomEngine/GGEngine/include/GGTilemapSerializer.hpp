/*************************************************************************
  \file       GGTilemapSerializer.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       15 November 2025
  \version    1.1
  \brief
    JSON serialization and deserialization for tilemap data.
  \details
    Provides static methods to save and load GGTilemap structures to/from
    JSON files using RapidJSON. Handles conversion between runtime data
    structures and JSON representation for editor workflow. Supports
    all tilemap properties including layers, tiles, and metadata.

    NOTE: NO namespace used to avoid collision with GGEngine class.
  \dependencies
    GGTilemapData.hpp, RapidJSONWrapper.hpp, <string>, <fstream>
  \see
    GGTilemapData.hpp, GGJSONUtils.hpp, GGAssetManager
  \history
    - 1.0 (15 November 2025): Initial version with load/save support.
    - 1.1 (15 November 2025): Removed namespace to fix collision.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef GG_TILEMAP_SERIALIZER_HPP
#define GG_TILEMAP_SERIALIZER_HPP

#pragma region Includes
#include "GGTilemapData.hpp"
#include "RapidJSONWrapper.hpp"
#include <string>
#include <fstream>
#pragma endregion

/*************************************************************************
  \brief
    Static utility class for tilemap JSON serialization
  \details
    Provides SaveToFile and LoadFromFile methods for converting between
    GGTilemap structs and JSON files. Uses RapidJSON for performance.
    All methods are static - no instance needed.
*************************************************************************/
class GGTilemapSerializer
{
public:
    /*************************************************************************
      \brief
        Save tilemap to JSON file
      \param[in] tilemap
        Tilemap data structure to serialize
      \param[in] filepath
        Destination file path (relative to working directory)
      \return
        True if save succeeded, false on error
      \par Usage
      \code
        GGTilemap myMap;
        if (GGTilemapSerializer::SaveToFile(myMap, "Assets/Maps/level1.json"))
        {
            // Save successful
        }
      \endcode
      \par Purpose
        Export tilemap from editor to JSON for runtime loading
    *************************************************************************/
    static bool SaveToFile(const GGTilemap& tilemap, const std::string& filepath);

    /*************************************************************************
      \brief
        Load tilemap from JSON file
      \param[in] filepath
        Source file path (relative to working directory)
      \param[out] outTilemap
        Tilemap structure to populate with loaded data
      \return
        True if load succeeded, false on error
      \par Usage
      \code
        GGTilemap loadedMap;
        if (GGTilemapSerializer::LoadFromFile("Assets/Maps/level1.json", loadedMap))
        {
            // Map loaded successfully
        }
      \endcode
      \par Purpose
        Import tilemap from JSON file for editing or runtime use
    *************************************************************************/
    static bool LoadFromFile(const std::string& filepath, GGTilemap& outTilemap);

private:
    /*************************************************************************
      \brief
        Serialize single tile to JSON object
      \param[in] tile
        Tile data to serialize
      \param[in,out] allocator
        RapidJSON allocator for creating JSON objects
      \return
        JSON object representing the tile
    *************************************************************************/
    static rapidjson::Value SerializeTile(const GGTileData& tile,
        rapidjson::Document::AllocatorType& allocator);

    /*************************************************************************
      \brief
        Deserialize single tile from JSON object
      \param[in] tileJson
        JSON object containing tile data
      \param[out] outTile
        Tile structure to populate
      \return
        True if deserialization succeeded, false on error
    *************************************************************************/
    static bool DeserializeTile(const rapidjson::Value& tileJson, GGTileData& outTile);

    /*************************************************************************
      \brief
        Serialize layer to JSON object
      \param[in] layer
        Layer data to serialize
      \param[in,out] allocator
        RapidJSON allocator for creating JSON objects
      \return
        JSON object representing the layer
    *************************************************************************/
    static rapidjson::Value SerializeLayer(const GGTilemapLayer& layer,
        rapidjson::Document::AllocatorType& allocator);

    /*************************************************************************
      \brief
        Deserialize layer from JSON object
      \param[in] layerJson
        JSON object containing layer data
      \param[out] outLayer
        Layer structure to populate
      \return
        True if deserialization succeeded, false on error
    *************************************************************************/
    static bool DeserializeLayer(const rapidjson::Value& layerJson, GGTilemapLayer& outLayer);
};

#endif // GG_TILEMAP_SERIALIZER_HPP