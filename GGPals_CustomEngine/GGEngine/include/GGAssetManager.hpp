/*************************************************************************
  \file       GGAssetManager.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       04 November 2025
  \version    4.0
  \brief
    Asset Manager for game assets at runtime.
  \details
    Provides typed asset loading and caching for textures, audio, shaders,
    and lua scripts. Uses direct loader methods for type-safe access with
    automatic caching and cleanup. Supports asset discovery for on-demand
    loading and hot reload for all asset types.
  \dependencies
    <string>, <unordered_map>, <memory>, <vector>, <GL/glew.h>,
    GGShader.hpp, GGAudioAsset.hpp, GGLuaVM.hpp, GGSystemInterface.hpp,
    <filesystem>
  \see
    GGAssetManager.cpp, GGShader.hpp, GGAudioAsset.hpp, GGLuaVM.hpp
  \history
    - 1.0 (16 September 2025): Initial version.
    - 1.1 (20 September 2025): Fixed path resolution.
    - 2.0 (23 September 2025): Updated for object-based audio system.
    - 2.1 (23 September 2025): Added SystemManagerInterface support.
    - 2.2 (24 September 2025): Added RunAssetTests method.
    - 2.3 (27 September 2025): Added asset discovery system with directory
      scanning capabilities for on-demand loading.
    - 2.4 (29 September 2025): Added RegisterAssetsWithUUIDs() to register Assets
      with UUID
    - 3.0 (30 September 2025): Removed unused generic asset loader system.
      Simplified to direct typed loaders only (GetTexture, GetAudioAsset, GetShader).
    - 3.1 (04 November 2025): Added lua script loading via GetLuaScript(),
      discovery getters for all asset types (textures, shaders, prefabs, lua,
      scenes), and extended HotReload() to support shaders and lua scripts.
    - 4.0 (13 November 2025): Added Drag and drop of assets logic from
      File Explorer into Game engine window.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <GL/glew.h>
#include <GGShader.hpp>
#include <GGSystemInterface.hpp>
#include <GGAssetID.hpp>
#include <GGLuaVM.hpp>

// Forward declarations
class GGAudioAsset;
struct GGScriptAsset;
class GGFMODWrapper;

/*************************************************************************
  \brief
    Asset management system with unique_ptr ownership model
  \details
    Provides typed asset loading and caching. Uses explicit cleanup
    for audio assets to ensure proper destruction order with audio system.
  \par Usage
  \code
    // Can be used through system registry
    auto& assetMgr = systemRegistry.GetSystem<GGAssetManager>();
    GLuint texture = assetMgr.GetTexture("Textures/player.png");

    // Or directly
    GGAssetManager assetMgr;
    assetMgr.Init();  // Calls Initialize() internally
  \endcode
*************************************************************************/
class GGAssetManager : public SystemManagerInterface
{
public:
    /*************************************************************************
      \brief
        Default constructor for asset manager
    *************************************************************************/
    GGAssetManager();

    /*************************************************************************
      \brief
        Destructor with proper cleanup
    *************************************************************************/
    ~GGAssetManager();

    /*************************************************************************
      \brief
        Initialize asset manager - implements SystemManagerInterface
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief
        Update method for system interface - intentionally empty
    *************************************************************************/
    void Update() override;

    /*************************************************************************
      \brief
        Initialize asset manager
    *************************************************************************/
    bool Initialize();

    /*************************************************************************
      \brief
        Shutdown and cleanup all resources
    *************************************************************************/
    void Shutdown();

    /*************************************************************************
      \brief
        Built-in texture loading - returns reference to cached texture ID
    *************************************************************************/
    const GLuint& GetTexture(const std::string& relativePath);

    /*************************************************************************
      \brief
        Built-in audio loading - returns reference to cached audio asset
      \param[in] relativePath
        Clean relative path (e.g., "Audio/SFX/Reload.wav")
      \return
        Reference to audio asset (throws if failed to load)
      \details
        Updated to use raw pointer to audio system instead of shared_ptr.
        Audio system lifetime is managed by engine's unique_ptr.
    *************************************************************************/
    const GGAudioAsset& GetAudioAsset(const std::string& relativePath);

    /*************************************************************************
      \brief
        Built-in shader loading - returns reference to cached shader
    *************************************************************************/
    const GGShader& GetShader(const std::string& vertexShaderPath,
        const std::string& fragmentShaderPath);

    /*************************************************************************
      \brief
        Built-in shader loading by key
    *************************************************************************/
    const GGShader& GetShaderByKey(const std::string& keyPath);

    /*************************************************************************
      \brief
        Built-in Script loading
    *************************************************************************/
    const GGScriptAsset& GetScript(const std::string& keyPath);

     

     const std::string& GetFontPath(const std::string& keyPath);

    /*************************************************************************
      \brief
        Clear all cached assets with explicit audio cleanup
      \details
        Updated to explicitly release audio assets before clearing cache
        to ensure proper cleanup order with audio system.
    *************************************************************************/
    void ClearCache();

    /*************************************************************************
      \brief
        Hot reload an asset for development
    *************************************************************************/
    bool HotReload(const std::string& relativePath);

    /*************************************************************************
      \brief
        Get asset statistics for debugging
    *************************************************************************/
    std::string GetCacheStats() const;

    /*************************************************************************
      \brief
        Resolve relative path to absolute filesystem path
    *************************************************************************/
    std::string ResolvePath(const std::string& relativePath) const;

    /*************************************************************************
      \brief
        Run comprehensive asset loading tests for development validation
    *************************************************************************/
    void RunAssetTests() const;

    /*************************************************************************
     \brief
       Get list of all loaded texture asset paths
     \return
       Vector of texture file paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedTexturePaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded audio asset paths
      \return
        Vector of audio file paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedAudioPaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded shader asset paths
      \return
        Vector of shader key paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedShaderPaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded prefab asset paths
      \return
        Vector of prefab key paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedPrefabPaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded lua script asset paths
      \return
        Vector of lua script key paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedLuaPaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded scene asset paths
      \return
        Vector of scene key paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedScenePaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded scene asset paths
      \return
        Vector of scene key paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedAnimationPaths() const;

    /*************************************************************************
      \brief
        Get list of all loaded tilemap asset paths
      \return
        Vector of tilemap key paths currently in cache
    *************************************************************************/
    std::vector<std::string> GetLoadedTilemapPaths() const;


    std::vector<std::string> GetLoadedFontPaths() const;

    /*************************************************************************
      \brief
        Scan asset directories and discover all available files
      \details
        Recursively scans the Assets directory to find all audio, texture,
        and shader files without loading them into memory.
    *************************************************************************/
    void ScanAssetDirectories();

    /*************************************************************************
      \brief
        Get list of all discovered audio files (not necessarily loaded)
      \return
        Vector of all audio file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredAudioPaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered texture files (not necessarily loaded)
      \return
        Vector of all texture file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredTexturePaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered shader files (not necessarily loaded)
      \return
        Vector of all shader file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredShaderPaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered prefab files (not necessarily loaded)
      \return
        Vector of all prefab file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredPrefabPaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered lua script files (not necessarily loaded)
      \return
        Vector of all lua script file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredLuaPaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered scene files (not necessarily loaded)
      \return
        Vector of all scene file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredScenePaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered animation files (not necessarily loaded)
      \return
        Vector of all animation file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredAnimationPaths() const;

    /*************************************************************************
      \brief
        Get list of all discovered tilemap files (not necessarily loaded)
      \return
        Vector of all tilemap file paths found in asset directories
    *************************************************************************/
    std::vector<std::string> GetDiscoveredTilemapPaths() const;
    //for font paths
    std::vector<std::string> GetDiscoveredFontPaths() const;

    void PreloadAllAssets();

    /*************************************************************************
      \brief
        Load lua script into engine's shared lua VM
      \param[in] relativePath
        Clean relative path to lua script (e.g., "Scripts/test_lua.lua")
      \return
        Reference to engine's shared lua VM (with all bindings registered)
      \details
        Loads and executes the lua script file in the engine's shared lua VM.
        All scripts share the same VM and global state, allowing them to
        interact. Tracks loaded scripts to avoid reloading. The VM has all
        ECS, Input, Debug, and Event bindings pre-registered.
      \note
        Scripts are NOT isolated - they share global state in the engine VM.
        This is intentional for gameplay scripts that need to interact.
    *************************************************************************/
    const GGLuaVM& GetLuaScript(const std::string& relativePath);

    void RegisterAssetsWithUUIDs();

    /*************************************************************************
      \brief
        Handles a file dropped into the editor/game window.

      \details
        Determines the type of the dropped file based on its extension and
        moves/copies it into the appropriate asset folder (Textures, Audio,
        Scripts, Shaders, Prefabs, Scenes, Animations, Tilemaps).

        If the file type is unsupported, its name is stored for the editor
        popup. If the file already exists in the target directory, its name
        and path are stored and a duplicate-file popup is triggered. Upon
        successful copy, the file is registered with the UUID/meta system
        and flagged for hot reload in the editor.

      \param absolutePath
        The absolute filesystem path of the dropped file.
    *************************************************************************/
    void HandleDroppedFile(const std::string& absolutePath);

    /*************************************************************************
      \brief
        Check if any files were dropped into the editor/game window.

      \return
        True if at least one file was dropped, false otherwise.
    *************************************************************************/
    bool HasDroppedFiles() const { return isDroppedFileFromFolder; }

    /*************************************************************************
      \brief
        Clears the flag that indicates dropped files.

      \details
        Resets the internal state after handling dropped files so that
        subsequent calls to HasDroppedFiles() return false.
    *************************************************************************/
    void ClearDroppedFilesFlag() { isDroppedFileFromFolder = false; }

    /*************************************************************************
      \brief
        Check if any dropped files were duplicates of existing assets.

      \return
        True if there are duplicated files, false otherwise.
    *************************************************************************/
    bool HasDuplicatedFiles() const { return isDroppedFileDuplicated; }

    /*************************************************************************
      \brief
        Clears the duplicated file flag and associated vectors.

      \details
        Resets the internal state after the user has handled duplicate files.
        Both duplicatedFileNames and duplicatedFilePaths vectors are cleared.
    *************************************************************************/
    void ClearDuplicatedFileFlag() 
    { 
        isDroppedFileDuplicated = false;
        duplicatedFileNames.clear();
        duplicatedFilePaths.clear();
    }

    /*************************************************************************
      \brief
        Get the names of files that were identified as duplicates.

      \return
        Vector of filenames that are duplicates.
    *************************************************************************/
    std::vector<std::string> GetDuplicateFileNames() const { return duplicatedFileNames; }

    /*************************************************************************
      \brief
        Get the absolute paths of files that were identified as duplicates.

      \return
        Vector of absolute file paths that are duplicates.
    *************************************************************************/
    std::vector<std::string> GetDuplicateFilePaths() const { return duplicatedFilePaths; }

    /*************************************************************************
      \brief
        Check if any unsupported files were dropped.

      \return
        True if unsupported files exist, false otherwise.
    *************************************************************************/
    bool HasUnsupportedFiles() const { return isUnsupportedFileDropped; }

    /*************************************************************************
      \brief
        Clears the unsupported file flag and associated vector.

      \details
        Resets the internal state after unsupported files have been handled.
        unsupportedFileNames vector is cleared.
    *************************************************************************/
    void ClearUnsupportedFileFlag() 
    {
        isUnsupportedFileDropped = false;
        unsupportedFileNames.clear();
    }
    std::vector<std::string> GetUnsupportedFileName() const { return unsupportedFileNames; }

    /*************************************************************************
      \brief
        Confirms and replaces an existing asset file after user approval.

      \details
        Copies the specified file to its corresponding asset folder
        (Textures, Audio, Scripts, Shaders, Prefabs, Scenes, Animations,
        Tilemaps) and overwrites the existing file. The asset is then
        registered with the UUID/meta system to update editor and runtime
        references. This is typically called after a duplicate-file popup
        confirms overwrite.

      \param absolutePath
        The absolute filesystem path of the file to replace.
      \param targetFolderPath
        Optional relative path to a specific subfolder where the asset
        should be placed. If empty, uses the default root folder for
        the asset type (e.g., "Textures" for .png files).
        Example: "Textures/Sprites" or "Audio/SFX"
    *************************************************************************/
    void ConfirmAndReplaceFile(const std::string& absolutePath, const std::string& targetFolderPath = "");

    /*************************************************************************
      \brief
        Set the target folder for the next dropped file operation

      \param assetType
        The type of asset (e.g., "Textures", "Audio", "Scripts")
      \param folderPath
        Relative path from asset base to target folder
        Example: "Textures/Sprites" or "Audio/Music"

      \details
        This method allows the AssetManagerNode to inform the AssetManager
        about which subfolder is currently active in the browser, so that
        dropped files are placed in the correct location.
    *************************************************************************/
    void SetDropTargetFolder(const std::string& assetType, const std::string& folderPath);

    /*************************************************************************
      \brief
        Get the current drop target folder for a given asset type

      \param assetType
        The type of asset (e.g., "Textures", "Audio", "Scripts")
      \return
        The relative path to the target folder, or empty string for root

      \details
        Returns the folder path that was set by SetDropTargetFolder.
        If no specific folder was set, returns empty string which means
        the root folder for that asset type should be used.
    *************************************************************************/
    std::string GetDropTargetFolder(const std::string& assetType) const;

    void MoveAsset(const std::string& oldRelativePath, const std::string& newRelativePath);

    /*************************************************************************
     \brief
        Delete an asset from the filesystem, cache, and UUID registry

     \param[in]
        relativePath Relative path to the asset (e.g., "Textures/player.png")
     \return
        true if asset was successfully deleted, false otherwise

     \details
        Performs complete asset deletion:
            1. Removes asset from runtime cache (with proper cleanup)
            2. Deletes asset file from filesystem
            3. Deletes corresponding .meta file
            4. Removes UUID from registry
            5. Updates discovery lists

     \note
        This operation cannot be undone. Ensure proper confirmation
        before calling this method.
     *************************************************************************/
    bool DeleteAsset(const std::string& relativePath);

    /*************************************************************************
     \brief
        Remove an asset from cache without deleting the file

     \param[in]
        relativePath Relative path to the asset
     \return
        true if asset was found and removed from cache

     \details
        Unloads an asset from memory but keeps the file on disk.
        Used internally by DeleteAsset and can be used for memory
        management without file deletion.
     *************************************************************************/
    bool UnloadAsset(const std::string& relativePath);

    /*************************************************************************
     \brief
        Set the base asset path when opening a project.
        Clears all caches and discovery lists, then sets new path.
     *************************************************************************/
    void SetAssetBasePath(const std::string& newBasePath);

    /*************************************************************************
     \brief
        Return the current asset base path.
    *************************************************************************/
    const std::string& GetAssetBasePath() const;

    /*************************************************************************
     \brief
        true after SetAssetBasePath() has been called
    *************************************************************************/
    bool IsProjectLoaded() const;

    /*************************************************************************
     \brief
        Unload project - clear everything, reset to empty.
    *************************************************************************/
    void UnloadProject();

private:
    // Built-in asset caches
    std::unordered_map<std::string, GLuint> textureCache;
    std::unordered_map<std::string, std::unique_ptr<GGAudioAsset>> audioAssetCache;
    std::unordered_map<std::string, std::unique_ptr<GGShader>> shaderCache;
    std::unordered_map<std::string, std::unique_ptr<GGLuaVM>> luaCache;


    bool isInitialized;

    std::unordered_map<std::string, std::string> dropTargetFolders; // the target folder for asset type to drop in
    bool isDroppedFileFromFolder = false;   // flag to track if files were dropped
    bool isDroppedFileDuplicated = false;   // flag to track if files were duplicated

    // vector storage for multiple files
    std::vector<std::string> duplicatedFileNames;
    std::vector<std::string> duplicatedFilePaths;
    
    bool isUnsupportedFileDropped = false;  // flag to track if files were unsupported
    
    // vector storage for multiple unsupported files
    std::vector<std::string> unsupportedFileNames;

    // Discovered assets (but not necessarily loaded)
    std::vector<std::string> discoveredAudioPaths;
    std::vector<std::string> discoveredTexturePaths;
    std::vector<std::string> discoveredShaderPaths;
    std::vector<std::string> discoveredPrefabPaths;
    std::vector<std::string> discoveredLuaPaths;
    std::vector<std::string> discoveredScenePaths;
    std::vector<std::string> discoveredAnimationPaths;
    std::vector<std::string> discoveredTilemapPaths;
    std::vector<std::string> discoveredFontPaths;


    // Shaders (auto-pair vertex and fragment by base name)
    std::unordered_map<std::string, std::string> vertexShaders;
    std::unordered_map<std::string, std::string> fragmentShaders;

    // Base path that changes at runtime each time a project is opened
    static std::string ASSET_BASE_PATH;

    // true once SetAssetBasePath() has been called for the 1st time
    bool isProjectLoaded = false;

    /*************************************************************************
      \brief
        Load texture using SOIL2
    *************************************************************************/
    GLuint LoadTextureFromFile(const std::string& fullPath);

    /*************************************************************************
      \brief
        Generate shader cache key
    *************************************************************************/
    std::string GenerateShaderKey(const std::string& vertexPath,
        const std::string& fragmentPath) const;
};