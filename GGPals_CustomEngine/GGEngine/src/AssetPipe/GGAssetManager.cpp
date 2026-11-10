/*************************************************************************
  \file       GGAssetManager.cpp
  \author     Zafir Haqqym, Tok Duan Jie Nelson
  \par        Email: zafir.h@digipen.edu, t.duanjienelson@digipen.edu
  \date       04 November 2025
  \version    4.0
  \contribute Zafir(40% base GGAssetManager template), Nelson(60%)
  \brief
    Asset manager implementation.
  \details
    Implements load, cache, and cleanup paths with logging and safe error
    handling. Supports discovery for all asset types and hot reload for
    development iteration. Lua scripts are loaded into the engine's shared
    VM to ensure proper binding availability.
  \dependencies
    GGAssetManager.hpp, GGAudioAsset.hpp, GGAudioManager.hpp, GGLuaVM.hpp,
    GGEngine.hpp, <SOIL2/SOIL2.h>, <iostream>, <fstream>, <algorithm>,
    <filesystem>
  \see
    GGAssetManager.hpp, GGLuaVM.hpp, GGEngine.hpp
  \history
    - 1.0 (16 September 2025): Initial version.
    - 1.1 (20 September 2025): Fixed path resolution, added debug toggles.
    - 2.0 (23 September 2025): Updated for object-based audio system.
    - 2.1 (23 September 2025): Added SystemManagerInterface support.
    - 2.2 (24 September 2025): Added RunAssetTests implementation.
    - 2.3 (27 September 2025): Added ScanAssetDirectories() and asset discovery
      methods for filesystem scanning and on-demand loading capabilities.
    - 2.4 (29 September 2025): Added RegisterAssetsWithUUIDs() to register Assets
      with UUID
    - 3.0 (30 September 2025): Removed unused generic asset loader infrastructure.
      Simplified to direct typed loaders, removed dead code (RegisterAssetType,
      LoadAsset, LoadAssetByType, extensionToType map, assetLoaders map).
    - 3.1 (04 November 2025): Implemented GetLuaScript() using engine's shared
      lua VM for proper binding support, added discovery getters for all asset
      types, and enhanced HotReload() with shader and lua script support.
    - 4.0 (13 November 2025): Added Drag and drop of assets logic from 
      File Explorer into Game engine window.
    
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

// Debug toggle
#ifndef ASSET_DEBUG_ENABLED
#define ASSET_DEBUG_ENABLED 0
#endif

// Debug macro
#if ASSET_DEBUG_ENABLED
#define ASSET_DEBUG(msg) std::cout << "DEBUG: " << msg << std::endl
#else
#define ASSET_DEBUG(msg)
#endif

#include <GGAssetManager.hpp>
#include <GGAudioAsset.hpp>
#include <GGAudioManager.hpp>
#include <GGEngine.hpp>
#include <SOIL2/SOIL2.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <GGLogger.hpp>

#include <GGSystemManagers.hpp>
#include <GGECSAssetInterface.hpp>

// Forward declaration - we'll get audio system from engine when needed
class GGEngine;

// Starts empty - no project is loaded at engine startup
std::string GGAssetManager::ASSET_BASE_PATH = "";

/*************************************************************************
  \brief
    Default constructor initializing member variables
*************************************************************************/
GGAssetManager::GGAssetManager()
    : isInitialized(false)
{
}

/*************************************************************************
  \brief
    Destructor ensuring proper cleanup of all resources
*************************************************************************/
GGAssetManager::~GGAssetManager()
{
    Shutdown();
}

/*************************************************************************
  \brief
    Initialize asset manager - implements SystemManagerInterface
*************************************************************************/
bool GGAssetManager::Init()
{
    // Delegate to existing initialization method for compatibility
    return Initialize();
}

/*************************************************************************
  \brief
    Update method for system interface - intentionally empty stub
*************************************************************************/
void GGAssetManager::Update()
{
    // Intentionally empty
    // Asset management doesn't require per frame updates
    // Assets are loaded on-demand when requested through GetTexture(), 
    // GetAudioAsset(), GetShader()
}

/*************************************************************************
  \brief
    Initialize asset manager without singleton dependencies
*************************************************************************/
bool GGAssetManager::Initialize()
{
    if (isInitialized)
    {
        //std::cout << "Asset Manager already initialized" << std::endl;
        return true;
    }

    std::cout << "=== Initializing Asset Manager ===" << std::endl;
    ASSET_DEBUG("Asset base path: " << ASSET_BASE_PATH);

    // Note: Audio system initialization is handled by the engine
    // We don't initialize it here to avoid circular dependencies

    isInitialized = true;
    std::cout << "SUCCESS: Asset Manager initialized" << std::endl;

    return true;
}

/*************************************************************************
  \brief
    Shutdown asset manager and cleanup all cached resources
*************************************************************************/
void GGAssetManager::Shutdown()
{
    if (!isInitialized)
    {
        return;
    }

    std::cout << "=== Shutting down Asset Manager ===" << std::endl;

    // Clear all caches
    ClearCache();

    isInitialized = false;
    std::cout << "SUCCESS: Asset Manager shutdown complete" << std::endl;
}

/*************************************************************************
  \brief
    Run comprehensive asset loading tests for development validation
*************************************************************************/
void GGAssetManager::RunAssetTests() const
{
    std::cout << "=== Asset Manager Test ===" << std::endl;

    // Print current working directory for debugging path issues
    std::cout << "Current working dir: "
        << std::filesystem::current_path().string() << std::endl;

    if (!isInitialized)
    {
        std::cout << "ERROR: AssetManager not initialized" << std::endl;
        std::cout << "=== Asset Manager Test Complete ===" << std::endl;
        return;
    }
}

/*************************************************************************
  \brief
    Resolve relative path to full filesystem path
*************************************************************************/
std::string GGAssetManager::ResolvePath(const std::string& relativePath) const
{
    // Simple concatenation - relativePath should NOT include "Assets/" prefix
    std::string fullPath = ASSET_BASE_PATH + relativePath;
    ASSET_DEBUG("Resolved '" << relativePath << "' to '" << fullPath << "'");
    return fullPath;
}

/*************************************************************************
  \brief
    Built-in texture loading using references for safety
*************************************************************************/
const GLuint& GGAssetManager::GetTexture(const std::string& relativePath)
{
    // Check cache first
    auto cacheIterator = textureCache.find(relativePath);
    if (cacheIterator != textureCache.end())
    {
        return cacheIterator->second;
    }


    // Load new texture using proper path resolution
    std::string fullPath = ResolvePath(relativePath);
    ASSET_DEBUG("Loading texture from: " << fullPath);

    GLuint textureId = LoadTextureFromFile(fullPath);
    if (textureId != 0)
    {
        auto insertResult = textureCache.emplace(relativePath, textureId);
        //std::cout << "SUCCESS: Loaded texture: " << relativePath << " (GLuint TextureID: " << textureId << ")" << std::endl;
        return insertResult.first->second;
    }
    else
    {
        std::cout << "ERROR: Failed to load texture from: " << fullPath << std::endl;
        throw std::runtime_error("Failed to load texture: " + relativePath + " (resolved: " + fullPath + ")");
    }
}

/*************************************************************************
  \brief
    Built-in audio loading with unique_ptr ownership
*************************************************************************/
const GGAudioAsset& GGAssetManager::GetAudioAsset(const std::string& relativePath)
{
    // Check cache first
    auto cacheIterator = audioAssetCache.find(relativePath);
    if (cacheIterator != audioAssetCache.end())
    {
        return *(cacheIterator->second);
    }

    // Load new audio asset using proper path resolution
    std::string fullPath = ResolvePath(relativePath);

    // Get audio system from engine - now returns raw pointer
    extern GGEngine& GetEngineInstance();
    GGFMODWrapper* audioSystem = GetEngineInstance().GetAudioSystem();

    if (!audioSystem)
    {
        std::cout << "ERROR: Audio system not available for: " << relativePath << std::endl;
        throw std::runtime_error("Audio system not available for: " + relativePath);
    }

    auto audioAsset = std::make_unique<GGAudioAsset>(fullPath, audioSystem);

    if (audioAsset->IsValid())
    {
        auto& assetRef = *audioAsset;
        audioAssetCache[relativePath] = std::move(audioAsset);
        //std::cout << "SUCCESS: Loaded audio asset: " << relativePath << std::endl;
        return assetRef;
    }
    else
    {
        std::cout << "ERROR: Failed to load audio asset: " << relativePath << std::endl;
        throw std::runtime_error("Failed to load audio asset: " + relativePath + " (resolved: " + fullPath + ")");
    }
}

/*************************************************************************
  \brief
    Built-in shader loading with unique_ptr ownership
*************************************************************************/
const GGShader& GGAssetManager::GetShader(const std::string& vertexShaderPath,
    const std::string& fragmentShaderPath)
{
    // Generate cache key for this shader pair
    std::string shaderKey = GenerateShaderKey(vertexShaderPath, fragmentShaderPath);

    // Check cache first
    auto cacheIterator = shaderCache.find(shaderKey);
    if (cacheIterator != shaderCache.end())
    {
        return *(cacheIterator->second);
    }

    // Load new shader using proper path resolution
    std::string vFull = ResolvePath(vertexShaderPath);
    std::string fFull = ResolvePath(fragmentShaderPath);
    auto shader = std::make_unique<GGShader>(vFull, fFull);

    if (shader->GetShaderID() > 0)
    {
        // Get reference before moving shader into cache
        auto& shaderRef = *shader;
        shaderCache[shaderKey] = std::move(shader);
        //std::cout << "SUCCESS: Loaded shader: " << vertexShaderPath << " + " << fragmentShaderPath << std::endl;
        return shaderRef;
    }
    else
    {
        std::cout << "ERROR: Failed to load shader: " << vertexShaderPath << " + " << fragmentShaderPath << std::endl;
        throw std::runtime_error("Failed to load shader: " + vertexShaderPath + " + " + fragmentShaderPath);
    }
}

/*************************************************************************
  \brief
    Retrieve a shader using a single key path instead of separate vertex
    and fragment paths.

  \details
    This function looks up an existing shader in the shader cache using
    a single string key. If not found, it attempts to load
    `<key>.vert` and `<key>.frag` from the filesystem. If either file is
    missing, a dummy fallback shader is returned and an error is logged.
    A successfully loaded shader is cached and returned.

  \param keyPath
    The base name of the shader without file extension.

  \return
    A const reference to the loaded shader instance. Returns a static
    dummy shader if the shader files cannot be found.
*************************************************************************/
const GGShader& GGAssetManager::GetShaderByKey(const std::string& keyPath)
{
    auto cacheIterator = shaderCache.find(keyPath);
    if (cacheIterator != shaderCache.end())
    {
        return *(cacheIterator->second);
    }

    std::string vertexPath = keyPath + ".vert";
    std::string fragmentPath = keyPath + ".frag";

    std::string vFull = ResolvePath(vertexPath);
    std::string fFull = ResolvePath(fragmentPath);

    if (!std::filesystem::exists(vFull) || !std::filesystem::exists(fFull))
    {
        GGLogger::Error("Shader files not found for key: " + keyPath + ", using default shader");
        static GGShader dummyShader; // safe fallback
        return dummyShader;
    }
    /*
    size_t pos = keyPath.find('|');

    if (pos == std::string::npos)
    {
        throw std::runtime_error("Failed to load Shader from key: " + keyPath);
    }

    std::string vertexPath = keyPath.substr(0, pos);
    std::string fragmentPath = keyPath.substr(pos + 1);
    */

    //std::string vFull = ResolvePath(vertexPath);
    //std::string fFull = ResolvePath(fragmentPath);
    auto shader = std::make_unique<GGShader>(vFull, fFull);

    if (shader->GetShaderID() > 0)
    {
        // Get reference before moving shader into cache
        auto& shaderRef = *shader;
        shaderCache[keyPath] = std::move(shader);
        //std::cout << "SUCCESS: Loaded shader by key: " << vertexPath << " + " << fragmentPath << std::endl;
        return shaderRef;
    }
    else
    {
        std::cout << "ERROR: Failed to load shader by key: " << vertexPath << " + " << fragmentPath << std::endl;
        throw std::runtime_error("Failed to load shader by key: " + vertexPath + " + " + fragmentPath);
    }
}

/*************************************************************************
  \brief
    Retrieve a Lua script asset from the asset system.

  \details
    Looks up the script asset in the script cache. If not found, this
    function resolves the script's full path and loads it into the
    engine's shared Lua VM to ensure proper binding and environment
    context. Loaded scripts are stored for fast subsequent access.

  \param keyPath
    Relative path to the Lua script inside the Assets directory.

  \return
    A const reference to the loaded script asset.

  \note
    TODO by Zafir: The return type should eventually be changed from
    `const GGScriptAsset&` to a more appropriate type depending on the
    planned refactor of the script system.
*************************************************************************/
const GGScriptAsset& GGAssetManager::GetScript(const std::string& keyPath) // TODO by ZAF: Should change return type from const GGScriptAsset& to const GGScriptAsset (return by value) to prevent dangling references and undefined behavior
{
    return Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().GetScriptAsset(keyPath);
}



const std::string& GGAssetManager::GetFontPath(const std::string& keyPath)
{
    // TODO: insert return statement here
    std::string ful = keyPath;
    std::replace(ful.begin(), ful.end(), '\\', '/');
    //if (!ful.find("Fonts"))return 0;
    try {
        
        auto& atlas = GGFontAtlasManager::GetFontAtlas(ful, 48);

       
        return   atlas.GetFontPath();
    }
    catch (...) {
       ;
        static std::string empty = "";
        return empty; // Return a static empty string to avoid dangling refs
    }
}

/*************************************************************************
  \brief
    Clear all cached assets with explicit audio cleanup ordering
*************************************************************************/
void GGAssetManager::ClearCache()
{
    //std::cout << "Clearing asset caches..." << std::endl;

    // Clear textures
    for (auto& texturePair : textureCache)
    {
        if (texturePair.second != 0)
        {
            glDeleteTextures(1, &texturePair.second);
        }
    }
    textureCache.clear();

    // Explicitly release audio assets BEFORE clearing cache
    // This ensures audio cleanup happens while audio system is still alive
    extern GGEngine& GetEngineInstance();
    GGFMODWrapper* audioSystem = GetEngineInstance().GetAudioSystem();

    if (audioSystem)
    {
        for (auto& audioPair : audioAssetCache)
        {
            if (audioPair.second)
            {
                audioPair.second->Release(audioSystem);
            }
        }
    }

    // Now safe to clear audio asset cache
    audioAssetCache.clear();

    // Shaders clean up automatically via unique_ptr
    shaderCache.clear();

    //prefabCache.clear();

    luaCache.clear();

    //sceneCache.clear();

      // Clear font atlas cache
    GGFontAtlasManager::ClearCache();

    std::cout << "Asset caches cleared" << std::endl;
}

/*************************************************************************
  \brief
    Hot reload an asset for development
*************************************************************************/
bool GGAssetManager::HotReload(const std::string& relativePath)
{
    bool found = false;

    // Remove from caches
    if (textureCache.erase(relativePath) > 0)
    {
        //std::cout << "Hot reload: Cleared texture cache for " << relativePath << std::endl;
        found = true;
    }

    if (audioAssetCache.erase(relativePath) > 0)
    {
        //std::cout << "Hot reload: Cleared audio cache for " << relativePath << std::endl;
        found = true;
    }

    // Check shader cache (need to check both individual paths and combined keys)
    std::vector<std::string> shaderKeysToRemove;
    for (const auto& pair : shaderCache)
    {
        if (pair.first.find(relativePath) != std::string::npos)
        {
            shaderKeysToRemove.push_back(pair.first);
        }
    }

    for (const auto& key : shaderKeysToRemove)
    {
        shaderCache.erase(key);
        //std::cout << "Hot reload: Cleared shader cache for " << key << std::endl;
        found = true;
    }

    if (luaCache.erase(relativePath) > 0)
    {
        //std::cout << "Hot reload: Cleared lua script cache for " << relativePath << std::endl;
        found = true;
    }

    if (found)
    {
        //std::cout << "Hot reload complete for: " << relativePath << std::endl;
        //std::cout << "Call GetTexture/GetAudioAsset/GetShader/GetLuaScript to reload" << std::endl;
    }
    else
    {
        std::cout << "Hot reload: Asset not found in any cache: " << relativePath << std::endl;
    }

    return found;
}

/*************************************************************************
  \brief
    Get asset statistics for debugging
*************************************************************************/
std::string GGAssetManager::GetCacheStats() const
{
    std::stringstream stats;
    stats << "Asset Cache Statistics:\n";
    ASSET_DEBUG("Asset Base Path: " << ASSET_BASE_PATH);
    stats << "  Textures: " << textureCache.size() << "\n";
    stats << "  Audio Assets: " << audioAssetCache.size() << "\n";
    stats << "  Shaders: " << shaderCache.size() << "\n";
    //stats << "  Prefabs: " << prefabCache.size() << "\n";
    stats << "  Lua Scripts: " << luaCache.size() << "\n";
    //stats << "  Scene: " << sceneCache.size() << "\n";

    // Note: Audio system stats would need to be retrieved from engine
    stats << "  Audio System: Available through engine";

    return stats.str();
}

/*************************************************************************
  \brief
    Internal texture loading implementation using SOIL2
*************************************************************************/
GLuint GGAssetManager::LoadTextureFromFile(const std::string& fullPath)
{
    /*GLuint textureId = SOIL_load_OGL_texture(
        fullPath.c_str(),
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_NTSC_SAFE_RGB | SOIL_FLAG_COMPRESS_TO_DXT
    );*/

    GLuint textureId = SOIL_load_OGL_texture(
        fullPath.c_str(),
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_INVERT_Y
    );

    if (textureId == 0)
    {
        std::cout << "SOIL loading error: " << SOIL_last_result() << std::endl;
        return 0;
    }

    // Set texture parameters for pixel art / general use
    glBindTexture(GL_TEXTURE_2D, textureId);
    // changed texture loading as previous one was too pixelated
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureId;
}

/*************************************************************************
  \brief
    Generate unique cache key for shader pairs
*************************************************************************/
std::string GGAssetManager::GenerateShaderKey(const std::string& vertexPath,
    const std::string& fragmentPath) const
{
    return vertexPath + "|" + fragmentPath;
}

/*************************************************************************
  \brief
    Get list of all loaded texture asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedTexturePaths() const
{
    std::vector<std::string> paths;
    paths.reserve(textureCache.size());

    for (const auto& pair : textureCache)
    {
        paths.push_back(pair.first);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded audio asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedAudioPaths() const
{
    std::vector<std::string> paths;
    paths.reserve(audioAssetCache.size());

    for (const auto& pair : audioAssetCache)
    {
        paths.push_back(pair.first);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded shader asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedShaderPaths() const
{
    std::vector<std::string> paths;
    paths.reserve(shaderCache.size());

    for (const auto& pair : shaderCache)
    {
        paths.push_back(pair.first);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded Prefab asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedPrefabPaths() const
{
    std::vector<std::string> paths;

    // Get engine instance for Prefabs
    const auto& prefabMap = GGEngine::GetEngine().GetPrefabManager().GetPrefabs();

    paths.reserve(prefabMap.size());
    for (const auto& [prefabName, prefab] : prefabMap)
    {
        paths.push_back(prefabName);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded Lua script asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedLuaPaths() const
{
    std::vector<std::string> paths;
    paths.reserve(luaCache.size());

    for (const auto& pair : luaCache)
    {
        paths.push_back(pair.first);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded scene asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedScenePaths() const
{
    std::vector<std::string> paths;

    // Get engine instance for Scenes
    const auto& sceneMap = GGEngine::GetEngine().GetSceneManager().GetSceneMap();

    paths.reserve(sceneMap.size());
    for (const auto& [sceneName, world] : sceneMap)
    {
        paths.push_back(sceneName);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded animation asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedAnimationPaths() const
{
    std::vector<std::string> paths;

    const auto& animationSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();

    const auto& pathMap = animationSystem.GetPathToIndexMap();

    paths.reserve(pathMap.size());

    for (const auto& [path, id] : pathMap)
    {
        paths.push_back(path);
    }

    return paths;
}

/*************************************************************************
  \brief
    Get list of all loaded tilemap asset paths
*************************************************************************/
std::vector<std::string> GGAssetManager::GetLoadedTilemapPaths() const
{
    std::vector<std::string> paths;

    const auto& animationSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();

    const auto& pathMap = animationSystem.GetPathToIndexMap();

    paths.reserve(pathMap.size());

    for (const auto& [path, id] : pathMap)
    {
        paths.push_back(path);
    }

    return paths;
}
std::vector<std::string> GGAssetManager::GetLoadedFontPaths() const
{
    std::vector<std::string> paths;
    // Get font atlas manager from engine
    std::unordered_set<std::string> uniquePaths; // Use set to avoid duplicates

    // Get the atlas cache from font manager
    const auto& atlasCache = GGFontAtlasManager::GetAtlasCache();

    // Extract unique font paths from loaded atlases
    for (const auto& [key, atlas] : atlasCache)
    {
        if (atlas && atlas->IsValid())
        {
            // Use the stored font path from the atlas itself
            uniquePaths.insert(atlas->GetFontPath());
        }
    }

    // Convert set to vector
    paths.assign(uniquePaths.begin(), uniquePaths.end());
    return paths;
}
/*************************************************************************
  \brief
    Scan asset directories and discover all available files
*************************************************************************/
void GGAssetManager::ScanAssetDirectories()
{
    if (ASSET_BASE_PATH.empty()) {
        std::cout << "ScanAssetDirectories: No project loaded, skipping." << std::endl;
        return;
    }

    discoveredAudioPaths.clear();
    discoveredTexturePaths.clear();
    discoveredShaderPaths.clear();
    discoveredPrefabPaths.clear();
    discoveredLuaPaths.clear();
    discoveredScenePaths.clear();
    discoveredAnimationPaths.clear();
    discoveredTilemapPaths.clear();
    discoveredFontPaths.clear();
    
    std::string assetsPath = ASSET_BASE_PATH;

    try
    {
        if (std::filesystem::exists(assetsPath))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath))
            {
                if (entry.is_regular_file())
                {
                    std::string relativePath = std::filesystem::relative(entry.path(), assetsPath).string();
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    // Audio files
                    if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
                    {
                        discoveredAudioPaths.push_back(relativePath);
                    }
                    // Texture files
                    else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga")
                    {
                        discoveredTexturePaths.push_back(relativePath);
                    }
                    // Shader files
                    else if (extension == ".vert" || extension == ".frag")
                    {
                        discoveredShaderPaths.push_back(relativePath);
                    }
                    else if (extension == ".prefab")
                    {
                        discoveredPrefabPaths.push_back(relativePath);
                    }
                    else if (extension == ".lua")
                    {
                        discoveredLuaPaths.push_back(relativePath);
                    }
                    else if (extension == ".scene")
                    {
                        discoveredScenePaths.push_back(relativePath);
                    }
                    else if (extension == ".gganim")
                    {
                        discoveredAnimationPaths.push_back(relativePath);
                    }
                    else if (extension == ".tilemap")
                    {
                        discoveredTilemapPaths.push_back(relativePath);
                    }
                    else if (extension == ".ttf") {
                        discoveredFontPaths.push_back(relativePath);
                    }
                }
            }
        }

        std::cout << "Asset discovery complete: "
            << discoveredAudioPaths.size() << " audio, "
            << discoveredTexturePaths.size() << " textures, "
            << discoveredShaderPaths.size() << " shaders, " 
            << discoveredPrefabPaths.size() << " prefabs, "
            << discoveredLuaPaths.size() << " lua scripts, "
            << discoveredScenePaths.size() << " scenes, "
            << discoveredAnimationPaths.size() << " animations (.gganim)"
            << discoveredTilemapPaths.size() << " tilemaps"
            << discoveredFontPaths.size() << " fontTypes"
            << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cout << "Asset discovery failed: " << e.what() << std::endl;
    }
}

/*************************************************************************
 \brief 
    Register all discovered assets with the UUID system

 \details 
    Iterates through all discovered asset paths (textures, audio,
    shaders, prefabs, lua scripts, and scenes) and ensures each
    has a UUID assigned. Loads existing UUIDs from meta files or
    generates new ones if they don't exist. Prints a summary of
    all registered assets after completion.

 \note 
    This should be called after ScanAssetDirectories() to ensure all
    discovered assets are properly tracked by the UUID system.
 *************************************************************************/
void GGAssetManager::RegisterAssetsWithUUIDs()
{
    auto& assetRegistry = GGAssetID::GetGGAssetID();

    // Register texture assets
    for (const auto& path : discoveredTexturePaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    // Register audio assets
    for (const auto& path : discoveredAudioPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
        if (auto uuidOpt = assetRegistry->GetUUIDFromPath(path))
        {
            uint32_t audioAssetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
            std::cout << "[GGAssetManager] Audio registered: " << path
                << " -> audioAssetID: " << audioAssetID << std::endl;
        }
    }
    // Register shader assets
    for (const auto& path : discoveredShaderPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    // Register Prefab assets
    for (const auto& path : discoveredPrefabPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    // Register Lua scripts assets
    for (const auto& path : discoveredLuaPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    // Register scene assets
    for (const auto& path : discoveredScenePaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    // Register animation assets
    for (const auto& path : discoveredAnimationPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    // Register tilemap assets
    for (const auto& path : discoveredTilemapPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    for (const auto& path : discoveredFontPaths)
    {
        assetRegistry->LoadOrGenerateMeta(path);
    }
    //std::cout << "UUIDs assigned for all discovered assets.\n";
    //assetRegistry->PrintAll(); // optional debug

    // Register fonts with ECS runtime ID system
    // This runs in ALL builds (including ReleaseGame) so fonts are available
    for (const auto& path : discoveredFontPaths)
    {
        if (auto uuidOpt = assetRegistry->GetUUIDFromPath(path))
        {
            GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);
            /*std::cout << "[GGAssetManager] Font registered: " << path
                << " -> fontAssetID: " << fontAssetID << std::endl;*/
        }
    }
}

void GGAssetManager::PreloadAllAssets()
{
    // Textures
    for (const auto& path : discoveredTexturePaths)
        if (textureCache.find(path) == textureCache.end())
        {
            GetTexture(path);
        }

    // Audio
    for (const auto& path : discoveredAudioPaths)
        if (audioAssetCache.find(path) == audioAssetCache.end())
        {
            GetAudioAsset(path);
        }

    // Shaders (vertex + fragment pair example)
    for (const auto& path : discoveredShaderPaths)
    {
        if (path.ends_with(".vert"))
        {
            std::string baseName = path.substr(0, path.find_last_of('.'));
            vertexShaders[baseName] = path;
        }
        else if (path.ends_with(".frag"))
        {
            std::string baseName = path.substr(0, path.find_last_of('.'));
            fragmentShaders[baseName] = path;
        }
    }

    // Match vertex + fragment pairs and preload
    for (const auto& [baseName, vertPath] : vertexShaders)
    {
        auto fragIt = fragmentShaders.find(baseName);
        if (fragIt != fragmentShaders.end())
        {
            [[maybe_unused]] const std::string& fragPath = fragIt->second;
            std::string key = baseName;

            if (shaderCache.find(key) == shaderCache.end())
            {
                GetShaderByKey(key);
            }
        }
    }

    // Lua - Load into cache for Asset Manager display
    // Note: GetLuaScript() executes the file but doesn't register as component script
    // Component script registration happens separately via GGECSScriptSystem::RegisterScript()
    for (const auto& path : discoveredLuaPaths)
    {
        if (luaCache.find(path) == luaCache.end())
        {
            try
            {
                GetLuaScript(path);
            }
            catch (const std::exception& e)
            {
                std::cout << "WARNING: Failed to preload lua script " << path << ": " << e.what() << std::endl;
                // Still add to cache so it shows in Asset Manager even if execution failed
                luaCache[path] = nullptr;
            }
        }
    }

    auto& animSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();
    for (const auto& path : discoveredAnimationPaths)
    {
        try
        {
            animSystem.RegisterAnimationByPath(path);
        }
        catch (const std::exception& e)
        {
            std::cout << "WARNING: Failed to preload Animation " << path << ": " << e.what() << std::endl;
        }
    }
}

/*************************************************************************
  \brief
    Get list of all discovered audio files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredAudioPaths() const
{
    return discoveredAudioPaths;
}

/*************************************************************************
  \brief
    Get list of all discovered texture files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredTexturePaths() const
{
    return discoveredTexturePaths;
}

/*************************************************************************
  \brief
    Get list of all discovered shader files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredShaderPaths() const
{
    return discoveredShaderPaths;
}

/*************************************************************************
  \brief
    Get list of all discovered prefab files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredPrefabPaths() const
{
    return discoveredPrefabPaths;
}

/*************************************************************************
  \brief
    Get list of all discovered lua script files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredLuaPaths() const
{
    return discoveredLuaPaths;
}

/*************************************************************************
  \brief
    Get list of all discovered scene files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredScenePaths() const
{
    return discoveredScenePaths;
}

/*************************************************************************
  \brief
    Get list of all discovered animation files
*************************************************************************/
std::vector<std::string> GGAssetManager::GetDiscoveredAnimationPaths() const
{
    return discoveredAnimationPaths;
}

std::vector<std::string> GGAssetManager::GetDiscoveredTilemapPaths() const
{
    return discoveredTilemapPaths;
}

//for font paths

std::vector<std::string> GGAssetManager::GetDiscoveredFontPaths() const
{
    return discoveredFontPaths;
}


/*************************************************************************
  \brief
    Load lua script into engine's shared VM
  \param[in] relativePath
    Clean relative path to lua script file
  \return
    Reference to engine's shared lua VM
  \details
    Uses the engine's shared lua VM (with all bindings registered) to load
    and execute the script. The script's globals and functions become
    available in the shared VM. Tracks loaded scripts in cache to avoid
    reloading.
  \note
    All scripts share the same lua VM and global state (this is intentional
    for gameplay scripts that need to interact).
*************************************************************************/
const GGLuaVM& GGAssetManager::GetLuaScript(const std::string& relativePath)
{
    // Get engine's shared lua VM
    GGLuaVM* engineVM = GGEngine::GetEngine().GetLuaVM();
    if (!engineVM)
    {
        throw std::runtime_error("Engine lua VM not initialized");
    }

    // Check if already loaded
    if (luaCache.find(relativePath) != luaCache.end())
    {
        std::cout << "Lua script already loaded: " << relativePath << std::endl;
        return *engineVM;
    }

    // Not in cache - load script into shared VM
    std::string fullPath = ResolvePath(relativePath);

    if (!engineVM->ExecuteFile(fullPath))
    {
        throw std::runtime_error("Failed to load lua script: " + relativePath);
    }

    //std::cout << "Loaded lua script: " << relativePath << std::endl;

    // Mark as loaded (we don't store the VM, just track that we loaded this script)
    luaCache[relativePath] = nullptr;

    return *engineVM;
}

/*************************************************************************
  \brief
    Handles a file dropped into the editor and imports it into the asset
    directory.

  \details
    Determines the file type based on extension and routes it to the
    appropriate asset folder (Textures, Audio, Scripts, Shaders, Prefabs,
    Scenes, Animations, Tilemaps). If the file is unsupported, the name
    is recorded for an editor popup. If the file already exists, the
    duplicate filename and path are stored and a confirmation popup is
    triggered.

    When import succeeds, the file is copied into the asset directory,
    a relative path is generated, and the asset is registered with the
    UUID/meta system.

  \param absolutePath
    Absolute path to the file dropped into the editor.
*************************************************************************/
void GGAssetManager::HandleDroppedFile(const std::string& absolutePath)
{

    try
    {
        std::filesystem::path droppedFile(absolutePath);

        if (!std::filesystem::exists(droppedFile)) {
            std::cout << "Dropped file does not exist: " + absolutePath;
            return;
        }


        std::string extension = droppedFile.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        std::filesystem::path targetDir;
        std::string assetType;
        std::string assetTypeKey;

        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga")
        {
            assetTypeKey = "Textures";
            assetType = "Texture";
        }
        else if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
        {
            assetTypeKey = "Audio";
            assetType = "Audio";
        }
        else if (extension == ".lua")
        {
            assetTypeKey = "Scripts";
            assetType = "Script";
        }
        else if (extension == ".vert" || extension == ".frag" || extension == ".glsl")
        {
            assetTypeKey = "Shaders";
            assetType = "Shader";
        }
        else if (extension == ".prefab")
        {
            assetTypeKey = "Prefabs";
            assetType = "Prefab";
        }
        else if (extension == ".scene")
        {
            assetTypeKey = "Scenes";
            assetType = "Scene";
        }
        else if (extension == ".gganim")
        {
            assetTypeKey = "Animations";
            assetType = "Animation";
        }
        else if (extension == ".tilemap")
        {
            assetTypeKey = "Maps";
            assetType = "Tilemap";
        }
        else if (extension == ".ttf")
        {
            assetTypeKey = "Fonts";
            assetType = "Font";
        }
        else
        {
            std::cout << "Unsupported file type dropped: " + extension << std::endl;
            unsupportedFileNames.push_back(droppedFile.filename().string());
            isUnsupportedFileDropped = true;
            isDroppedFileFromFolder = true;
            return;
        }

        // Get target folder from drop target map
        std::string targetFolder = GetDropTargetFolder(assetTypeKey);

        // Build target directory
        if (!targetFolder.empty())
        {
            targetDir = std::filesystem::path(ASSET_BASE_PATH) / targetFolder;
        }
        else
        {
            targetDir = std::filesystem::path(ASSET_BASE_PATH) / assetTypeKey;
        }

        // Ensure directory exists
        if (!std::filesystem::exists(targetDir))
        {
            std::filesystem::create_directories(targetDir);
        }

        // Build target path
        std::filesystem::path targetPath = targetDir / droppedFile.filename();

        // Check if file already exists
        if (std::filesystem::exists(targetPath))
        {
            // Add to duplicate list for user confirmation
            duplicatedFileNames.push_back(droppedFile.filename().string());
            duplicatedFilePaths.push_back(absolutePath);
            isDroppedFileDuplicated = true;
            isDroppedFileFromFolder = true;
            std::cerr << "File already exists, asking for overwrite: " << droppedFile.filename().string() << std::endl;
            return;
        }

        // Generate relative path for asset system
        std::string relativePath = std::filesystem::relative(targetPath, ASSET_BASE_PATH).string();
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

        // Copy the file to the target location
        std::filesystem::copy_file(droppedFile, targetPath);

        // Generate or load meta file for the asset
        GGAssetID::GetGGAssetID()->LoadOrGenerateMeta(relativePath);

        // trigger asset refresh in editor
        isDroppedFileFromFolder = true;
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Failed to handle dropped file: " + std::string(e.what());
    }

}

/*************************************************************************
 \brief
    to set the drop location of the folder path for the asset
 *************************************************************************/
void GGAssetManager::SetDropTargetFolder(const std::string& assetType, const std::string& folderPath)
{
    dropTargetFolders[assetType] = folderPath;

    /*std::cout << "[AssetManager] SetDropTargetFolder(" << assetType << ", \""
        << (folderPath.empty() ? "[ROOT]" : folderPath) << "\")" << std::endl;*/
}

/*************************************************************************
 \brief
    get the drop location of the folder path for the asset
 *************************************************************************/
std::string GGAssetManager::GetDropTargetFolder(const std::string& assetType) const
{
    auto it = dropTargetFolders.find(assetType);
    if (it != dropTargetFolders.end())
    {
        //std::cout << "[AssetManager] GetDropTargetFolder(" << assetType << ") -> \"" << it->second << "\"" << std::endl;
        return it->second;
    }
    //std::cout << "[ASSET_MGR] GetDropTargetFolder(" << assetType << ") -> [NOT SET, using root]" << std::endl;
    return "";
}

/*************************************************************************
  \brief
    Overwrites an existing asset file after the user confirms replacement.

  \details
    Uses the same file-type routing rules as HandleDroppedFile to determine
    the correct asset folder. The dropped file is then copied into the
    asset directory using overwrite mode. After replacement, the relative
    path is recomputed and the asset metadata (UUID) is loaded or generated.

    This function is triggered only after user confirmation from the
    duplicate-file popup.

  \param absolutePath
    Absolute path of the file to replace the existing asset.

  \note
    If the path is empty, the operation is aborted with an error log.
*************************************************************************/
void GGAssetManager::ConfirmAndReplaceFile(const std::string& absolutePath, const std::string& targetFolderPath)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "CONFIRM AND REPLACE FILE CALLED" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Absolute path: " << absolutePath << std::endl;
    std::cout << "Target folder param: " << (targetFolderPath.empty() ? "[NOT PROVIDED]" : targetFolderPath) << std::endl;
    if (absolutePath.empty()) {
        GGLogger::Error("No file path");
        return;
    }

    try
    {
        std::filesystem::path droppedFile(absolutePath);
        std::string extension = droppedFile.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        std::cout << "File name: " << droppedFile.filename() << std::endl;
        std::cout << "Extension: " << extension << std::endl;

        std::filesystem::path targetDir;
        std::string assetType;
        std::string assetTypeKey;

        // Determine target directory (same logic as HandleDroppedFile)
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga")
        {
            assetTypeKey = "Textures";
            assetType = "Texture";
        }
        else if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
        {
            assetTypeKey = "Audio";
            assetType = "Audio";
        }
        else if (extension == ".lua")
        {
            assetTypeKey = "Scripts";
            assetType = "Script";
        }
        else if (extension == ".vert" || extension == ".frag" || extension == ".glsl")
        {
            assetTypeKey = "Shaders";
            assetType = "Shader";
        }
        else if (extension == ".prefab")
        {
            assetTypeKey = "Prefabs";
            assetType = "Prefab";
        }
        else if (extension == ".scene")
        {
            assetTypeKey = "Scenes";
            assetType = "Scene";
        }
        else if (extension == ".gganim")
        {
            assetTypeKey = "Animations";
            assetType = "Animation";
        }
        else if (extension == ".tilemap")
        {
            assetTypeKey = "Maps";
            assetType = "Tilemaps";
        }
        else if (extension == ".ttf")
        {
            assetTypeKey = "Fonts";
            assetType = "Fonts";
        }
        else
        {
            GGLogger::Error("Unsupported file type: " + extension);
            return;
        }

        std::cout << "Asset type key: " << assetTypeKey << std::endl;
        std::cout << "Asset type: " << assetType << std::endl;

        // use provided target folder, drop target or default
        std::string finalTargetFolder;
        if (!targetFolderPath.empty())
        {
            finalTargetFolder = targetFolderPath;
            std::cout << "Using provided target folder: " << finalTargetFolder << std::endl;
        }
        else
        {
            std::cout << "No target folder provided, checking drop target map..." << std::endl;
            finalTargetFolder = GetDropTargetFolder(assetTypeKey);
        }

        // build the target directory
        if (!finalTargetFolder.empty())
        {
            targetDir = std::filesystem::path(ASSET_BASE_PATH) / finalTargetFolder;
            std::cout << "Target directory (from folder): " << targetDir << std::endl;
        }
        else
        {
            targetDir = std::filesystem::path(ASSET_BASE_PATH) / assetTypeKey;
            std::cout << "Target directory (default root): " << targetDir << std::endl;
        }

        // Ensure that directory exists
        if (!std::filesystem::exists(targetDir))
        {
            std::cout << "Target directory doesn't exist, creating: " << targetDir << std::endl;
            std::filesystem::create_directories(targetDir);
        }

        std::filesystem::path targetPath = targetDir / droppedFile.filename();
        // Generate relative path
        std::string relativePath = std::filesystem::relative(targetPath, ASSET_BASE_PATH).string();
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

        // If file exists, move it via MoveAsset (handles .meta and UUID registry)
        if (std::filesystem::exists(targetPath))
        {
            MoveAsset(relativePath, relativePath); // effectively overwrites and updates UUID/meta
        }
        else
        {
            // Just copy new file and generate meta
            std::filesystem::copy_file(droppedFile, targetPath, std::filesystem::copy_options::overwrite_existing);
            GGAssetID::GetGGAssetID()->LoadOrGenerateMeta(relativePath);
        }

        GGLogger::Info("Added " + assetType + " to " + finalTargetFolder + ": " + droppedFile.filename().string());

    }
    catch (const std::exception& e)
    {
        GGLogger::Error("Failed to replace file: " + std::string(e.what()));
    }
}

 /*************************************************************************
 \brief
    Remove an asset from cache without deleting the file
 *************************************************************************/
bool GGAssetManager::UnloadAsset(const std::string& relativePath)
{
    bool wasUnloaded = false;

    // Unload from texture cache
    auto texIt = textureCache.find(relativePath);
    if (texIt != textureCache.end())
    {
        if (texIt->second != 0)
        {
            glDeleteTextures(1, &texIt->second);
        }
        textureCache.erase(texIt);
        //std::cout << "Unloaded texture from cache: " << relativePath << std::endl;
        wasUnloaded = true;
    }

    // Unload from audio cache (with proper FMOD cleanup)
    auto audioIt = audioAssetCache.find(relativePath);
    if (audioIt != audioAssetCache.end())
    {
        extern GGEngine& GetEngineInstance();
        GGFMODWrapper* audioSystem = GetEngineInstance().GetAudioSystem();

        if (audioSystem && audioIt->second)
        {
            audioIt->second->Release(audioSystem);
        }
        audioAssetCache.erase(audioIt);
        //std::cout << "Unloaded audio asset from cache: " << relativePath << std::endl;
        wasUnloaded = true;
    }

    // Unload from shader cache
    auto shaderIt = shaderCache.find(relativePath);
    if (shaderIt != shaderCache.end())
    {
        // Shaders clean up automatically via unique_ptr destructor
        shaderCache.erase(shaderIt);
        //std::cout << "Unloaded shader from cache: " << relativePath << std::endl;
        wasUnloaded = true;
    }

    // Unload from Lua cache
    auto luaIt = luaCache.find(relativePath);
    if (luaIt != luaCache.end())
    {
        // Lua VMs clean up automatically via unique_ptr destructor
        luaCache.erase(luaIt);
        //std::cout << "Unloaded Lua script from cache: " << relativePath << std::endl;
        wasUnloaded = true;
    }

    return wasUnloaded;
}

/*************************************************************************
 \brief
    Delete an asset from the filesystem, cache, and UUID registry
 *************************************************************************/
bool GGAssetManager::DeleteAsset(const std::string& relativePath)
{
    //std::cout << "Deleting asset: " << relativePath << std::endl;

    UnloadAsset(relativePath);

    std::string fullPath = ResolvePath(relativePath);
    std::filesystem::path assetPath(fullPath);

    // delete asset file
    bool fileDeleted = false;
    if (std::filesystem::exists(assetPath))
    {
        try
        {
            std::filesystem::remove(assetPath);
            //std::cout << "Deleted asset file: " << fullPath << std::endl;
            fileDeleted = true;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "ERROR: Failed to delete asset file: " << e.what() << std::endl;
            return false;
        }
    }
    else
    {
        std::cout << "WARNING: Asset file not found: " << fullPath << std::endl;
    }

    // delete .meta file
    auto& assetID = GGAssetID::GetGGAssetID();
    std::filesystem::path metaPath = assetID->ComputeMetaPath(relativePath);

    if (std::filesystem::exists(metaPath))
    {
        try
        {
            std::filesystem::remove(metaPath);
            std::cout << "Deleted meta file: " << metaPath.string() << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "ERROR: Failed to delete meta file: " << e.what() << std::endl;
            // Don't return false here - continue with cleanup
        }
    }

    // remove asset from discovery lists
    auto removeFromVector = [](std::vector<std::string>& vec, const std::string& path) {
        auto it = std::find(vec.begin(), vec.end(), path);
        if (it != vec.end())
        {
            vec.erase(it);
        }
        };

    removeFromVector(discoveredTexturePaths, relativePath);
    removeFromVector(discoveredAudioPaths, relativePath);
    removeFromVector(discoveredShaderPaths, relativePath);
    removeFromVector(discoveredPrefabPaths, relativePath);
    removeFromVector(discoveredLuaPaths, relativePath);
    removeFromVector(discoveredScenePaths, relativePath);
    removeFromVector(discoveredAnimationPaths, relativePath);
    removeFromVector(discoveredTilemapPaths, relativePath);
    removeFromVector(discoveredFontPaths, relativePath);

    std::cout << "Asset deletion complete: " << relativePath << std::endl;
    return fileDeleted;
}

void GGAssetManager::MoveAsset(const std::string& oldRelativePath, const std::string& newRelativePath)
{
    try
    {
        std::filesystem::path oldPath = std::filesystem::path(ASSET_BASE_PATH) / oldRelativePath;
        std::filesystem::path newPath = std::filesystem::path(ASSET_BASE_PATH) / newRelativePath;

        // Move the file
        std::filesystem::rename(oldPath, newPath);

        // Move the .meta file
        std::filesystem::path oldMeta = GGAssetID::GetGGAssetID()->ComputeMetaPath(oldRelativePath);
        if (std::filesystem::exists(oldMeta))
        {
            std::filesystem::path newMeta = GGAssetID::GetGGAssetID()->ComputeMetaPath(newRelativePath);
            std::filesystem::rename(oldMeta, newMeta);
        }

        // Update texture cache
        auto texIt = textureCache.find(oldRelativePath);
        if (texIt != textureCache.end())
        {
            GLuint textureID = texIt->second;
            textureCache.erase(texIt);
            textureCache[newRelativePath] = textureID;
        }

        // Update audio cache
        auto audioIt = audioAssetCache.find(oldRelativePath);
        if (audioIt != audioAssetCache.end())
        {
            auto audioAsset = std::move(audioIt->second);
            audioAssetCache.erase(audioIt);
            audioAssetCache[newRelativePath] = std::move(audioAsset);
        }

        // Update shader cache
        std::vector<std::string> shaderKeysToUpdate;
        for (const auto& pair : shaderCache)
        {
            if (pair.first.find(oldRelativePath) != std::string::npos)
            {
                shaderKeysToUpdate.push_back(pair.first);
            }
        }
        for (const auto& key : shaderKeysToUpdate)
        {
            auto shader = std::move(shaderCache[key]);
            shaderCache.erase(key);
            std::string newKey = key;
            size_t pos = newKey.find(oldRelativePath);
            if (pos != std::string::npos)
            {
                newKey.replace(pos, oldRelativePath.length(), newRelativePath);
            }
            shaderCache[newKey] = std::move(shader);
        }

        // Update lua cache
        auto luaIt = luaCache.find(oldRelativePath);
        if (luaIt != luaCache.end())
        {
            auto luaAsset = std::move(luaIt->second);
            luaCache.erase(luaIt);
            luaCache[newRelativePath] = std::move(luaAsset);
        }

        // Update the UUID registry
        GGAssetID::GetGGAssetID()->UpdateAssetPath(oldRelativePath, newRelativePath);

        // Reload asset in cache if already loaded
        HotReload(newRelativePath);

        std::cout << "Moved asset: " << oldRelativePath << " -> " << newRelativePath << std::endl;
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("Failed to move asset: " + std::string(e.what()));
    }
}

/*************************************************************************
 \brief
    Set the base asset path when opening a project.
    Clears all caches and discovery lists, then sets new path.
 *************************************************************************/
void GGAssetManager::SetAssetBasePath(const std::string& newBasePath)
{
    ClearCache();

    discoveredAudioPaths.clear();
    discoveredTexturePaths.clear();
    discoveredShaderPaths.clear();
    discoveredPrefabPaths.clear();
    discoveredLuaPaths.clear();
    discoveredScenePaths.clear();
    discoveredAnimationPaths.clear();
    discoveredTilemapPaths.clear();
    discoveredFontPaths.clear();

    //Clear ECS asset ID registries for all asset types
    GGECSStandardAssetManager<GLuint>::ClearAll();
    GGECSStandardAssetManager<GGAudioAsset>::ClearAll();
    GGECSStandardAssetManager<GGShader>::ClearAll();
    GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::ClearAll();
    GGECSStandardAssetManager<std::string>::ClearAll();
    GGECSStandardAssetManager<GGScriptAsset>::ClearAll();
    GGECSStandardAssetManager<GGTilemap>::ClearAll();
    GGECSStandardAssetManager<PrefabManager::Prefab>::ClearAll();
    GGECSStandardAssetManager<World>::ClearAll();

    //Clear animation system
    Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>().ClearAnimations();
    GGAssetID::GetGGAssetID()->ClearRegistry();

    ASSET_BASE_PATH = newBasePath;
    GGAssetID::GetGGAssetID()->SetBasePath(newBasePath);
    isProjectLoaded = true;

    ScanAssetDirectories();
}
/*************************************************************************
 \brief
    Return the current asset base path.
*************************************************************************/
const std::string& GGAssetManager::GetAssetBasePath() const
{
    return ASSET_BASE_PATH;
}

/*************************************************************************
 \brief
    true after SetAssetBasePath() has been called
*************************************************************************/
bool GGAssetManager::IsProjectLoaded()const
{
    return isProjectLoaded;
}

/*************************************************************************
 \brief
    Unload project - clear everything, reset to empty.
*************************************************************************/
void GGAssetManager::UnloadProject()
{
    ClearCache();
    discoveredAudioPaths.clear();
    discoveredTexturePaths.clear();
    discoveredShaderPaths.clear();
    discoveredPrefabPaths.clear();
    discoveredLuaPaths.clear();
    discoveredScenePaths.clear();
    discoveredAnimationPaths.clear();
    discoveredTilemapPaths.clear();
    discoveredFontPaths.clear();
    ASSET_BASE_PATH = "";
    isProjectLoaded = false;

}