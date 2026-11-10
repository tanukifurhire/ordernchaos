/*************************************************************************
  \file       GGAssetID.cpp
  \author     Tok Duan Jie Nelson
  \par        Email: t.duanjienelson@digipen.edu
  \date       25 October 2025
  \version    1.0
  \brief
    Provides functionality for managint asset UUIDs and meta files.
  \details
    This file implements the GGAssetID system responsible for assigning
    and tracking Unqiue IDs for assets. It supports generation new UUIDs,
    reading/writing meta files, and looking up asset paths by UUID.
  \dependencies
    GGAssetID.hpp, <random>, <iostream>, <fstream>, <filesystem>, 
    <iomapip>, <uuid.h>, <unordered_map>, <optional>
  \see
    GGAssetID.hpp
  \history
    - 1.0 (25 October 2025): Initial version.
    - 1.1 (30 October 2025): Added new function to use with JSON
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include "GGAssetID.hpp"
#include <random>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>

std::string GGAssetID::ASSET_BASE_PATH = "";

/*************************************************************************
 \brief
    set ASSET_BASE_PATH as the selected path location
 *************************************************************************/
void GGAssetID::SetBasePath(const std::string& path)
{
    ASSET_BASE_PATH = path;
}

/*************************************************************************
 \brief
    Clears all UUID-to-path mappings from the registry.
    Call when switching projects so stale UUIDs from the old project
    cannot trigger loads of files that don't exist in the new project.
*************************************************************************/
void GGAssetID::ClearRegistry()
{
    uuidRegistry.clear();
}

/*************************************************************************
 \brief 
    Default constructor for GGAssetID

 \details 
    Initializes the random number generator engine with a random
    device seed and creates a UUID generator using the engine.
    This ensures each instance can generate unique UUIDs.
 *************************************************************************/
GGAssetID::GGAssetID()
      : engine(std::random_device{}()),            // member std::mt19937 engine
        uuidGen(engine)                            // member uuids::basic_uuid_random_generator<std::mt19937> uuidGen
{
}

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
bool GGAssetID::LoadOrGenerateMeta(const std::string& assetPath)
{
    std::filesystem::path metaPath = ComputeMetaPath(assetPath);

    // If meta exists, read UUID
    if (std::filesystem::exists(metaPath))
    {
        std::ifstream metaFile(metaPath);
        if (metaFile.is_open())
        {
            std::string uuidStr;
            std::getline(metaFile, uuidStr);                    // Read UUID string from the file
            auto uuid = uuids::uuid::from_string(uuidStr);      // Convert string to UUID type
            if (uuid.has_value())
            {
                uuidRegistry[uuid.value()] = assetPath;         // Store UUID in registry
                return true;
            }
        }
    }

    // No meta exists, generate new UUID
    auto uuid = uuidGen();                                      // Generate a new UUID
    uuidRegistry[uuid] = assetPath;                             // Store UUID in the registry
    SaveMetaFile(metaPath, uuid);                               // Save the new UUID into a .meta file
    return false;
}

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
std::filesystem::path GGAssetID::ComputeMetaPath(const std::string& assetPath) const
{
    std::filesystem::path metaPath = std::filesystem::path(ASSET_BASE_PATH) / assetPath;

    // For shaders, use base name to combine multiple files into a meta file
    if (metaPath.extension() == ".glsl" || metaPath.extension() == ".vert" || metaPath.extension() == ".frag")
    {
        metaPath = metaPath.parent_path() / metaPath.stem();
    }

    metaPath += ".meta";
    return metaPath;
}

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
bool GGAssetID::SaveMetaFile(const std::filesystem::path& metaPath, const AssetUUID& uuid)
{

    std::ofstream metaFile(metaPath);
    std::cout << "Saving meta file at: " << std::filesystem::absolute(metaPath) << "\n";
    if (!metaFile.is_open())
    {
        return false;
    }

    metaFile << uuids::to_string(uuid);
    return true;
}


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
const AssetUUID& GGAssetID::GetUUID(const std::string& relativePath) const
{
    for (const auto& [uuid, path] : uuidRegistry)
    {
        if (path == relativePath) 
        {
            return uuid;
        }
    }
    throw std::runtime_error("No UUID registerd for: " + relativePath);
}

/*************************************************************************
 \brief 
    Print all registered assets and their UUIDs to stdout

 \details 
    Outputs a formatted table showing all asset paths and their
    corresponding UUIDs. Used for debugging and verification of
    the asset registry state.
 *************************************************************************/
void GGAssetID::PrintAll() const
{
    std::cout << std::setw(33) << std::left << " " << "=== Registered Asset UUIDs ===" << std::endl;
    for (const auto& [uuid, path] : uuidRegistry)
    {
        std::cout << "  "
            << std::setw(50) << std::left << path
            << std::setw(10) << std::left << " -> "
            << uuids::to_string(uuid)
            << std::endl;
    }
    std::cout << "====================================================================================================" << std::endl;
}

/*************************************************************************
 \brief 
    Check if a UUID exists in the asset registry

 \param[in] 
    uuid The UUID to search for
 \return 
    true if the UUID is registered, false otherwise
 *************************************************************************/
bool GGAssetID::DoesUUIDExist(uuids::uuid const& uuid) const
{
    return uuidRegistry.find(uuid) != uuidRegistry.end();
}

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
const std::string& GGAssetID::GetFilePath(const uuids::uuid& uuid) const
{
    auto it = uuidRegistry.find(uuid);
    if (it == uuidRegistry.end())
    {
        static const std::string empty = "";
        std::cerr << "GGAssetID::GetFilePath - UUID not found.\n";
        return empty;
    }
    return it->second;
}

/*************************************************************************
    \brief
       Update the Asset Path to not detect a phantom asset
 *************************************************************************/
void GGAssetID::UpdateAssetPath(const std::string& oldPath, const std::string& newPath)
{
    for (auto& [uuid, path] : uuidRegistry)
    {
        if (path == oldPath)
        {
            path = newPath; // Update the registry to new path
            return;
        }
    }
}

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
std::optional<uuids::uuid> GGAssetID::GetUUIDFromPath(const std::string& path) const
{
    for (const auto& [uuid, p] : uuidRegistry)
    {
        if (p == path)
            return uuid;
    }
    return std::nullopt;
}

/*************************************************************************
 \brief
    Remove a UUID entry from the registry
 *************************************************************************/
bool GGAssetID::RemoveUUID(const std::string& relativePath)
{
    for (auto it = uuidRegistry.begin(); it != uuidRegistry.end(); ++it)
    {
        if (it->second == relativePath)
        {
            std::cout << "Removing UUID for asset: " << relativePath << std::endl;
            uuidRegistry.erase(it);
            return true;
        }
    }

    std::cout << "WARNING: No UUID found for asset: " << relativePath << std::endl;
    return false;
}