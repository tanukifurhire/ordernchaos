/*************************************************************************
  \file       GGAssetID.cpp
  \author     Tok Duan Jie Nelson
  \par        Email: t.duanjienelson@digipen.edu
  \date       25 October 2025
  \version    1.0
  \brief
    Provides functionality for managint asset UUIDs and meta files.
  \details
    This file declares the GGAssetID system responsible for assigning
    and tracking Unqiue IDs for assets. It supports generation new UUIDs,
    reading/writing meta files, and looking up asset paths by UUID.
  \dependencies
    <random>, <iostream>, <fstream>, <filesystem>,
    <iomapip>, <uuid.h>, <unordered_map>, <optional>, <cstdint
  \see

  \history
    - 1.0 (25 October 2025): Initial version.
    - 1.1 (30 October 2025): Added new function to use with JSON
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <string>
#include <unordered_map>
#include <random>
#include <cstdint>
#include <uuid.h>   // your UUID library header
#include <filesystem>
#include <optional>

using AssetUUID = uuids::uuid;

class GGAssetID
{
public:
    GGAssetID();

    /*************************************************************************
     \brief
        Load UUID for an asset from its meta file, or generate a new one
        if it doesn't exist

     \param[in]
        assetPath Relative path of the asset within the assets folder
     \return
        true if an existing UUID was loaded, false if a new UUID was generated

     \details
        This function handles the complete lifecycle of an asset's UUID:
            1. Computes the meta file path for the given asset.
            2. If the meta file exists:
                - Reads the UUID string from the file.
                - Converts it to a UUID object and stores it in the registry.
            3. If the meta file does not exist or is invalid:
                - Generates a new UUID.
                - Stores it in the registry.
                - Saves a new meta file with this UUID.

     \note
        This function ensures that every asset always has a consistent UUID,
        and can be safely called without manually managing meta files or UUIDs.
     *************************************************************************/
    bool LoadOrGenerateMeta(const std::string& assetPath);

    /*************************************************************************
     \brief
        Get UUID for a given asset path

     \param[in]
        relativePath Relative path to the asset
     \return
        Reference to the UUID associated with the asset path

     \details
        Searches the UUID registry for an entry matching the provided
        path and returns its corresponding UUID.

     \note
        Throws std::runtime_error if no UUID is registered for the given path.
     *************************************************************************/
    const AssetUUID& GetUUID(const std::string& relativePath) const;

    /*************************************************************************
     \brief
        Print all registered assets and their UUIDs to stdout

     \details
        Outputs a formatted table showing all asset paths and their
        corresponding UUIDs. Used for debugging and verification of
        the asset registry state.
     *************************************************************************/
    void PrintAll() const;

    /*************************************************************************
     \brief 
        Get the singleton instance of GGAssetID

     \return 
        Reference to the unique pointer holding the GGAssetID singleton

     \details 
        Creates and returns a static singleton instance of GGAssetID
        using a unique pointer. The instance is initialized on first
        access and persists for the lifetime of the application.
    *************************************************************************/
    static inline auto& GetGGAssetID()
    {
        static std::unique_ptr<GGAssetID> assetID = std::make_unique<GGAssetID>();
        return assetID;
    }

    /*************************************************************************
     \brief
        Check if a UUID exists in the asset registry

     \param[in]
        uuid The UUID to search for
     \return
        true if the UUID is registered, false otherwise
     *************************************************************************/
    bool DoesUUIDExist(uuids::uuid const& uuid) const;

    /*************************************************************************
     \brief
        Retrieve the file path associated with a given UUID

     \param[in]
        uuid The asset's unique identifier
     \return
        Reference to the file path string, or empty string if not found

     \note
        Prints an error message to stderr if UUID is not found in registry.
        Returns a reference to a static empty string in the error case.
     *************************************************************************/
    const std::string& GetFilePath(const uuids::uuid& uuid) const;

    /*************************************************************************
     \brief
        Get UUID from an asset file path

     \param[in]
        path Relative path to the asset file
     \return
        Optional containing the UUID if found, std::nullopt otherwise

     \details
        Searches the UUID registry for an entry matching the provided
        path and returns its corresponding UUID wrapped in an optional.
     *************************************************************************/
    std::optional<AssetUUID> GetUUIDFromPath(const std::string& path) const;

    /*************************************************************************
     \brief
        Update the Asset Path to not detect a phantom asset
     *************************************************************************/
    void UpdateAssetPath(const std::string& oldPath, const std::string& newPath);

    /*************************************************************************
     \brief
        Compute the meta file path for a given asset

     \param[in]
        assetPath Relative path to the asset file
     \return
        Filesystem path to the corresponding .meta file

     \details
        Combines shader files (.glsl, .vert, .frag) with the same base
        name into a single meta file by using the stem name. For other
        file types, appends .meta directly to the full filename.

     \note
        This consolidation allows multiple shader files (vertex, fragment, etc.)
        that belong to the same shader program to share a single UUID.
     *************************************************************************/
    std::filesystem::path ComputeMetaPath(const std::string& assetPath) const;

    /*************************************************************************
     \brief
        Remove a UUID entry from the registry

     \param[in]
        relativePath Relative path to the asset
     \return
        true if UUID was found and removed, false otherwise

     \details
        Removes the UUID mapping for an asset from the registry.
        This should be called when an asset is deleted from the filesystem.
     *************************************************************************/
    bool RemoveUUID(const std::string& relativePath);

    void SetBasePath(const std::string& path);

    /*************************************************************************
     \brief
        Clears all UUID-to-path mappings from the registry.
        Call when switching projects so stale UUIDs from the old project
        cannot trigger loads of files that don't exist in the new project.
    *************************************************************************/
    void ClearRegistry();
private:
    std::mt19937 engine;   // persistent engine

    // 64-bit random engine generator for UUIDs
    uuids::basic_uuid_random_generator<std::mt19937> uuidGen;

    // Map of UUID -> asset path
    std::unordered_map<AssetUUID, std::string> uuidRegistry;
   
    static inline std::unordered_map<uint32_t, AssetUUID> uuidToIDMap;

    /*************************************************************************
     \brief
        Save UUID to a meta file for the given asset

     \param[in]
        metaPath Filesystem path where the meta file should be saved
     \param[in]
        uuid The UUID to write to the meta file
     \return
        true if the file was successfully written, false otherwise

     \details
        Creates a new meta file at the specified path and writes the
        UUID as a string. Prints the absolute path of the meta file
        being saved to stdout for debugging purposes.
     *************************************************************************/
    bool SaveMetaFile(const std::filesystem::path& metaPath, const AssetUUID& uuid);

    static std::string ASSET_BASE_PATH;
};