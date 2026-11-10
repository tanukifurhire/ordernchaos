/*************************************************************************
  \file       AssetManagerNode.cpp
  \author     Zafir Haqqym, Tok Duan Jie Nelson
  \par        Email: zafir.h@digipen.edu, t.duanjienelson@digipen.edu
  \date       04 November 2025
  \version    5.2
  \contribute Zafir(20%, base AssetManagerNode template), Nelson(80%)
  \brief
    Implementation of asset manager UI panel.
  \details
    Implements asset browser with collapsible sections for each asset type,
    refresh functionality, selection tracking, and load-on-demand behavior.
    Integrates with GGAssetManager for discovery and loading operations.
    Provides visual feedback for asset states and comprehensive statistics.
  \dependencies
    AssetManagerNode.hpp, GGSystemManagers.hpp, GGECSAssetInterface.hpp,
    EditorEventBus.hpp, imgui_internal.h
  \see
    AssetManagerNode.hpp, GGAssetManager.hpp
  \history
    - 1.0 (04 November 2025): Initial implementation with asset discovery,
      click-to-load lua scripts, asset statistics display, and UUID generation
      for discovered assets.
    - 2.0 (05 November 2025): Added Prefabs, Scripts and Scenes implementation
    - 2.1 (06 November 2025): Creating new scene file with right click logic
    - 3.0 (13 November 2025): Popup handling of duplicated/unsupported files
    - 4.0 (15 Jan 2026): New asset browser with thumbnail
    - 5.0 (18 Mar 2026): Added "Assets Used" tab for per-scene asset reporting
    - 5.1 (19 Mar 2026): Fixed LoadProject calling RefreshAssetLists synchronously
      instead of deferring via needsAssetRefresh flag. Deferred refresh caused
      AddNewScene (called immediately after LoadProject returns) to deserialize
      ScriptComponent UUIDs before the asset registry was populated, leaving
      scriptAssetID as 0 on every entity and spamming CreateScriptInstance errors.
    - 5.2 (05 Apr 2026): Assets Used tab now uses checkboxes for multi-scene
      selection with Select All / Deselect All; combined report deduplicates
      textures by filename and always writes to combinedScenes.txt
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <AssetManagerNode.hpp>
#include <GGSystemManagers.hpp>
#include <GGECSAssetInterface.hpp>
#include <EditorEventBus.hpp>
#include <EditorPlatform.hpp>
#include <GGECSComponents.hpp>
#include <GGECSScriptSystem.hpp>

#include <set>
#include <fstream>
#include <filesystem>

#include <imgui_internal.h>

#define DEBUG_DROP_TO_FOLDER 0  // Set to 0 to disable debug output

#if DEBUG_DROP_TO_FOLDER
#define DROP_DEBUG(msg) std::cout << "[DROP_DEBUG] " << msg << std::endl
#else
#define DROP_DEBUG(msg)
#endif

/*************************************************************************
 \brief
    Constructor for AssetManagerNode

 \details
    Initializes callback handles for Init, Update, and DockBuild
    functions. These callbacks are used by the editor system to
    manage the asset manager's lifecycle and rendering.
 *************************************************************************/
AssetManagerNode::AssetManagerNode()
{
    initCallbackHandle = [this]() -> void { this->Init(); };
    updateCallbackHandle = [this]() -> void { this->Update(); };
    dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
 \brief
    Initialize the asset manager node

 \details
    Currently empty but reserved for future initialization logic
    such as loading persistent settings or setting up initial state.
 *************************************************************************/
void AssetManagerNode::Init()
{
    InitializeDefaultThumbnails();
    //RefreshAssetLists();

    // subscribe to refresh requests
    Callback<> refreshCallback = [this]()
        {
            this->needsAssetRefresh = true;
        };
    EditorEventBus::onAssetSystemRefresh += refreshCallback;

    // play mode events to track when game is playing
    onPlayModeCallback = [this]()
        {
            isPlaying = true;
        };

    onStopPlayModeCallback = [this]()
        {
            isPlaying = false;
        };

    GlobalEventBus::onPlayModeStart += onPlayModeCallback;
    GlobalEventBus::onStopPlayMode += onStopPlayModeCallback;

    // Initialize folder structures
    const std::string ASSET_BASE = "";
    textureRootFolder = std::make_shared<FolderNode>("Textures", ASSET_BASE);
    audioRootFolder = std::make_shared<FolderNode>("Audio", ASSET_BASE);
    shaderRootFolder = std::make_shared<FolderNode>("Shaders", ASSET_BASE);
    scriptRootFolder = std::make_shared<FolderNode>("Scripts", ASSET_BASE);
    animationRootFolder = std::make_shared<FolderNode>("Animations", ASSET_BASE);
    prefabRootFolder = std::make_shared<FolderNode>("Prefabs", ASSET_BASE);
    sceneRootFolder = std::make_shared<FolderNode>("Scenes", ASSET_BASE);
    tilemapRootFolder = std::make_shared<FolderNode>("Maps", ASSET_BASE);
    fontRootFolder = std::make_shared<FolderNode>("Fonts", ASSET_BASE);

    // initialize current folder pointers
    currentTextureFolder = textureRootFolder;
    currentAudioFolder = audioRootFolder;
    currentShaderFolder = shaderRootFolder;
    currentScriptFolder = scriptRootFolder;
    currentAnimationFolder = animationRootFolder;
    currentPrefabFolder = prefabRootFolder;
    currentSceneFolder = sceneRootFolder;
    currentTilemapFolder = tilemapRootFolder;
    currentFontFolder = fontRootFolder;
}

/*************************************************************************
 \brief
    Configure ImGui docking layout for the asset manager window

 \details
    Splits the left dock vertically to create space for both the
    hierarchy (top) and asset manager (bottom). The asset manager
    window is docked to the bottom portion, taking 50% of the height.
 *************************************************************************/
void AssetManagerNode::DockBuild()
{
    auto& editor = Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>();

    // Split the left dock into top (hierarchy) and bottom (asset manager)
    ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(
        editor.dockLeft,           // Split the left dock
        ImGuiDir_Down,             // Split downward
        0.5f,                      // 50% of height for bottom
        nullptr,                   // Don't need the bottom ID returned here
        &editor.dockLeft           // Update dockLeft to be the top part
    );

    // Now dock Asset Manager to the bottom part
    ImGui::DockBuilderDockWindow("Asset Manager", dock_left_bottom);
}

/*************************************************************************
 \brief
    Update and render the asset manager window each frame

 \details
    Handles the asset refresh flag and renders the main asset
    manager ImGui window with a fixed size. The window contains
    the resource browser UI.
 *************************************************************************/
void AssetManagerNode::Update()
{
    GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();

    // set drop target based on the current folder path
    if (assetManager)
    {
        DROP_DEBUG("=== UPDATE: Setting Drop Targets ===");

        auto setTargetIfChanged = [&](const std::string& assetType,
            std::shared_ptr<FolderNode> folder)
            {
                if (folder)
                {
                    std::string relativePath = std::filesystem::relative(
                        folder->fullPath, GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath()).string();
                    std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

                    // Only set if changed
                    if (lastSetDropTargets[assetType] != relativePath)
                    {
                        assetManager->SetDropTargetFolder(assetType, relativePath);
                        lastSetDropTargets[assetType] = relativePath;
                        DROP_DEBUG(">>> Drop target changed for " << assetType << " to: " << relativePath);
                    }
                }
            };

        setTargetIfChanged("Textures", currentTextureFolder);
        setTargetIfChanged("Audio", currentAudioFolder);
        setTargetIfChanged("Shaders", currentShaderFolder);
        setTargetIfChanged("Scripts", currentScriptFolder);
        setTargetIfChanged("Animations", currentAnimationFolder);
        setTargetIfChanged("Prefabs", currentPrefabFolder);
        setTargetIfChanged("Scenes", currentSceneFolder);
        setTargetIfChanged("Maps", currentTilemapFolder);
        setTargetIfChanged("Fonts", currentFontFolder);

        DROP_DEBUG("=== END Setting Drop Targets ===\n");
    }

    if (assetManager && assetManager->HasDroppedFiles())
    {
        DROP_DEBUG("****************************************");
        DROP_DEBUG("*** FILES WERE DROPPED INTO WINDOW! ***");
        DROP_DEBUG("****************************************");

        bool hasDuplicates = assetManager->HasDuplicatedFiles();
        bool hasUnsupported = assetManager->HasUnsupportedFiles();

        if (hasDuplicates)
        {
            DROP_DEBUG("Processing duplicated files...");
            // collect all duplicated files
            duplicatedFiles.clear();

            // get all duplicated file info from asset manager
            auto dupNames = assetManager->GetDuplicateFileNames();
            auto dupPaths = assetManager->GetDuplicateFilePaths();

            for (size_t i = 0; i < dupNames.size(); ++i)
            {
                duplicatedFiles.push_back({ dupNames[i], dupPaths[i] });
                DROP_DEBUG("  Duplicate file: " << dupNames[i] << " at " << dupPaths[i]);
            }

            openDuplicatePopupNextFrame = true;
        }

        if (hasUnsupported)
        {
            DROP_DEBUG("Processing unsupported files...");
            // collect all unsupported files
            unsupportedFiles.clear();

            auto unsupportedNames = assetManager->GetUnsupportedFileName();

            for (const auto& name : unsupportedNames)
            {
                unsupportedFiles.push_back({ name, "" });
            }

            if (!hasDuplicates)
            {
                openUnsupportedPopUpNextFrame = true;
            }
        }

        if (!hasDuplicates && !hasUnsupported)
        {
            DROP_DEBUG("No duplicates or unsupported files - triggering asset refresh");
            needsAssetRefresh = true;
        }

        assetManager->ClearDroppedFilesFlag();
        DROP_DEBUG("****************************************\n");
    }

    if (needsAssetRefresh)
    {
        RefreshAssetLists();
        needsAssetRefresh = false;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

    if (ImGui::Begin("Asset Manager", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        if (!projectLoaded)
        {
            ImGui::SetCursorPosY(ImGui::GetContentRegionAvail().y * 0.4f);
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 300.f) * 0.5f);
            ImGui::TextDisabled("No project loaded.");
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 300.f) * 0.5f);
            ImGui::TextDisabled("Use 'New Project' or 'Open Project' to get started.");
        }
        else
        {
            if (ImGui::BeginTabBar("AssetManagerTabs"))
            {
                if (ImGui::BeginTabItem("Asset Browser"))
                {
                    RenderThumbnailBrowser();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("ID Reference"))
                {
                    ImGui::BeginChild(
                        "IDReferenceScroll",
                        ImVec2(0, 0),
                        false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar
                    );
                    RenderResourceBrowser();
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Assets Used"))
                {
                    RenderAssetsUsedTab();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        if (openDuplicatePopupNextFrame)
        {
            ImGui::OpenPopup("Warning, duplicated files detected!");
            openDuplicatePopupNextFrame = false; // reset after opening
        }

        if (openUnsupportedPopUpNextFrame)
        {
            ImGui::OpenPopup("Error!");
            openUnsupportedPopUpNextFrame = false;
        }

        RenderDuplicatedFilePopUp();
        RenderUnsupportedFilePopUp();

        if (openIndividualSelectionPopup)
        {
            ImGui::OpenPopup("Select Files to replace");
            openIndividualSelectionPopup = false;
        }

        RenderDeleteConfirmationPopup();

        if (showDeleteConfirmationPopup)
        {
            ImGui::OpenPopup("Delete Asset?");
            showDeleteConfirmationPopup = false;
        }

    }

    ImGui::End();
}

/*************************************************************************
 \brief
    Refresh all asset lists from disk and memory

  \details
    Performs a complete refresh by:
      1. Scanning asset directories for new files.
      2. Preloading textures, audio, shaders, Lua scripts, and animations.
      3. Updating cached and discovered asset lists.
      4. Ensuring all assets have UUID meta files.
      5. Pre-registering assets with the ECS system.

 \note
    This operation can be expensive as it scans directories and
    generates UUIDs for new assets. Should not be called every frame.
 *************************************************************************/
void AssetManagerNode::RefreshAssetLists()
{
    GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();

    if (assetManager)
    {
        // Scan directories for new files
        assetManager->ScanAssetDirectories();
        assetManager->PreloadAllAssets();         // This loads only textures, audio, shaders, lua Assets and animations

        // Cache all Loaded asset paths
        cachedTexturePaths = assetManager->GetLoadedTexturePaths();
        cachedAudioPaths = assetManager->GetLoadedAudioPaths();
        cachedShaderPaths = assetManager->GetLoadedShaderPaths();
        cachedPrefabPaths = assetManager->GetLoadedPrefabPaths();
        cachedLuaPaths = assetManager->GetLoadedLuaPaths();
        cachedScenePaths = assetManager->GetLoadedScenePaths();
        cachedAnimationPaths = assetManager->GetLoadedAnimationPaths();
        cachedTilemapPaths = assetManager->GetLoadedTilemapPaths();
        cachedFontPaths = assetManager->GetLoadedFontPaths();

        // Update discovered lists (on-disk assets)
        discoveredTexturePaths = assetManager->GetDiscoveredTexturePaths();
        discoveredAudioPaths = assetManager->GetDiscoveredAudioPaths();
        discoveredShaderPaths = assetManager->GetDiscoveredShaderPaths();
        discoveredPrefabPaths = assetManager->GetDiscoveredPrefabPaths();
        discoveredLuaPaths = assetManager->GetDiscoveredLuaPaths();
        discoveredScenePaths = assetManager->GetDiscoveredScenePaths();
        discoveredAnimationPaths = assetManager->GetDiscoveredAnimationPaths();
        discoveredTilemapPaths = assetManager->GetDiscoveredTilemapPaths();
        discoveredFontPaths = assetManager->GetDiscoveredFontPaths();

        // Build folder hierarchies after getting fresh asset lists
        BuildFolderHierarchy("Textures");
        BuildFolderHierarchy("Audio");
        BuildFolderHierarchy("Shaders");
        BuildFolderHierarchy("Scripts");
        BuildFolderHierarchy("Animations");
        BuildFolderHierarchy("Prefabs");
        BuildFolderHierarchy("Scenes");
        BuildFolderHierarchy("Tilemaps");
        BuildFolderHierarchy("Fonts");

        if (currentTextureFolder)
        {
            currentTextureFolder =
                FindFolderByPath(textureRootFolder,
                    currentTextureFolder->fullPath);
        }
        if (currentAudioFolder)
        {
            currentAudioFolder =
                FindFolderByPath(audioRootFolder,
                    currentAudioFolder->fullPath);
        }
        if (currentShaderFolder)
        {
            currentShaderFolder =
                FindFolderByPath(shaderRootFolder,
                    currentShaderFolder->fullPath);
        }
        if (currentScriptFolder)
        {
            currentScriptFolder =
                FindFolderByPath(scriptRootFolder,
                    currentScriptFolder->fullPath);
        }
        if (currentAnimationFolder)
        {
            currentAnimationFolder =
                FindFolderByPath(animationRootFolder,
                    currentAnimationFolder->fullPath);
        }
        if (currentPrefabFolder)
        {
            currentPrefabFolder =
                FindFolderByPath(prefabRootFolder,
                    currentPrefabFolder->fullPath);
        }
        if (currentSceneFolder)
        {
            currentSceneFolder =
                FindFolderByPath(sceneRootFolder,
                    currentSceneFolder->fullPath);
        }
        if (currentTilemapFolder)
        {
            currentTilemapFolder =
                FindFolderByPath(tilemapRootFolder,
                    currentTilemapFolder->fullPath);
        }
        if (currentFontFolder)
        {
            currentFontFolder =
                FindFolderByPath(fontRootFolder,
                    currentFontFolder->fullPath);
        }
        // Make sure all assets have UUIDs
        auto& assetSystem = GGAssetID::GetGGAssetID();
        for (const auto& path : cachedTexturePaths)         assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : cachedAudioPaths)           assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : cachedShaderPaths)          assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : discoveredPrefabPaths)      assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : cachedLuaPaths)             assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : discoveredScenePaths)       assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : cachedAnimationPaths)       assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : discoveredTilemapPaths)     assetSystem->LoadOrGenerateMeta(path);
        for (const auto& path : discoveredFontPaths)        assetSystem->LoadOrGenerateMeta(path);


        // Preregister all assets with ECS system
#pragma region Preregister assets for ECS
        // register textures
        for (const auto& path : cachedTexturePaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);
            }
        }

        // register audio
        for (const auto& path : cachedAudioPaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
            }
        }

        // register shaders
        for (const auto& path : cachedShaderPaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                GGECSStandardAssetManager<GGShader>::RegisterAsset(*uuidOpt);
            }
        }

        // register prefabs
        for (const auto& path : discoveredPrefabPaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                GGECSStandardAssetManager<PrefabManager::Prefab>::RegisterAsset(*uuidOpt);
            }
        }


        // register animations
        for (const auto& path : cachedAnimationPaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::RegisterAsset(*uuidOpt);
            }
        }

        for (const auto& path : discoveredFontPaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                //uint32_t fontAssetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);
                GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);
                //std::cout << "[AssetManager] Font Runtime ID: " << path << " -> fontAssetID: " << fontAssetID << std::endl;
            }
        }

        // register scripts - ensures all Lua types are registered before CreateScriptInstance runs
        for (const auto& path : cachedLuaPaths)
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
            }
        }
#pragma endregion

        // Call individual event handlers here
        auto& scriptSys = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
        //auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
        //auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
        //sceneManager.BackupAllScenes();
        scriptSys.ValidateCompositionDependencies();
        //sceneManager.RestoreAllScenes(prefabManager);
        GlobalEventBus::onReloadAssets.Emit();
    }
    else
    {
        GGLogger::Error("Asset manager not available for refresh");
    }
}

void AssetManagerNode::LoadProject(const std::string& projectAssetsPath)
{

    GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();
    if (!assetManager)
    {
        GGLogger::Error("AssetMaangerNOde::LoadProject - asset manager unavailable");
        return;
    }

    // Clear current world and all loaded scenes and thumbnails from old project
    auto& engine = GGEngine::GetEngine();
    auto& sceneManager = engine.GetSceneManager();
    auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();

    // Force-stop play mode first if active.
    if (isPlaying)
    {
        GlobalEventBus::onStopPlayMode.Emit();
        isPlaying = false;
    }

    // Use ClearScriptCache here, not ReloadScriptsWithoutRestore.
    // ReloadScriptsWithoutRestore replays compiledScriptsCache, which still
    // holds the previous project's absolute script paths at this point.
    // Those paths get re-registered before SetAssetBasePath changes the base,
    // polluting the new project's script system with stale entries.
    scriptSystem.ClearScriptCache();

    sceneManager.ClearAllLoadedScenes(engine.GetActiveWorld(), engine.GetObjectsFactory());
    sceneManager.ClearBackup();
    thumbnailCache.clear();

    std::string basePath = projectAssetsPath;
    if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\')
    {
        basePath += '/';
    }

    assetManager->SetAssetBasePath(basePath);

    textureRootFolder = std::make_shared<FolderNode>("Textures", basePath + "Textures");
    audioRootFolder = std::make_shared<FolderNode>("Audio", basePath + "Audio");
    shaderRootFolder = std::make_shared<FolderNode>("Shaders", basePath + "Shaders");
    scriptRootFolder = std::make_shared<FolderNode>("Scripts", basePath + "Scripts");
    animationRootFolder = std::make_shared<FolderNode>("Animations", basePath + "Animations");
    prefabRootFolder = std::make_shared<FolderNode>("Prefabs", basePath + "Prefabs");
    sceneRootFolder = std::make_shared<FolderNode>("Scenes", basePath + "Scenes");
    tilemapRootFolder = std::make_shared<FolderNode>("Maps", basePath + "Maps");
    fontRootFolder = std::make_shared<FolderNode>("Fonts", basePath + "Fonts");

    currentTextureFolder = textureRootFolder;
    currentAudioFolder = audioRootFolder;
    currentShaderFolder = shaderRootFolder;
    currentScriptFolder = scriptRootFolder;
    currentAnimationFolder = animationRootFolder;
    currentPrefabFolder = prefabRootFolder;
    currentSceneFolder = sceneRootFolder;
    currentTilemapFolder = tilemapRootFolder;
    currentFontFolder = fontRootFolder;

    lastSetDropTargets.clear();

    // Refresh must happen synchronously here -- the caller (StopPlayToolbar) calls
    // AddNewScene immediately after LoadProject returns. If we defer via
    // needsAssetRefresh, scene deserialization runs before the asset registry is
    // populated and every ScriptComponent ends up with scriptAssetID = 0.
    RefreshAssetLists();

    projectLoaded = true;
    GGLogger::Info("Project loaded: " + projectAssetsPath);
}


#pragma region Old Asset Browser
/*************************************************************************
 \brief
    Render the main resource browser UI

 \details
    Displays an ImGui interface showing all asset categories
    (textures, audio, shaders, prefabs, scripts, scenes) in
    collapsible sections. Each asset is displayed as a selectable
    item with drag-and-drop support and shows its load status and
    ECS asset ID. Includes a refresh button and asset statistics.

 \note
    Assets can be selected to emit events and support drag-and-drop
    operations to other editor windows for easy asset assignment.
 *************************************************************************/
void AssetManagerNode::RenderResourceBrowser()
{
    ImGui::Text("===[ID REFERENCE]===");

    // Refresh button
    ImGui::BeginDisabled(isPlaying);
    if (ImGui::Button("Refresh Asset Lists"))
    {
        needsAssetRefresh = true;
        GGLogger::Info("Asset refresh requested");
    }
    ImGui::EndDisabled();
    ImGui::Separator();

    auto& assetSystem = GGAssetID::GetGGAssetID();

#pragma region Textures Section
    // Textures
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_None))
    {
        if (cachedTexturePaths.empty())
        {
            ImGui::Text("No textures loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedTextures;
            for (const auto& path : cachedTexturePaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);
                sortedTextures.emplace_back(assetID, path);
            }
            std::sort(sortedTextures.begin(), sortedTextures.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedTextures.size(); ++i)
            {
                uint32_t assetID = sortedTextures[i].first;
                const std::string& path = sortedTextures[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(cachedTexturePaths.begin(), cachedTexturePaths.end(), path)
                    - cachedTexturePaths.begin());
                bool isSelected = (selectedTextureIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected))
                {
                    selectedTextureIndex = origIndex;
                    if (assetID != 0)
                    {
                        EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                    }
                    selectedAudioIndex = -1;
                    selectedShaderIndex = -1;
                    selectedPrefabIndex = -1;
                    selectedLuaIndex = -1;
                    selectedSceneIndex = -1;
                    selectedAnimationIndex = -1;
                    selectedTilemapIndex = -1;
                    selectedFontIndex = -1;
                    GGLogger::Info("Selected texture asset ID:" + std::to_string(assetID) + " (" + path + ")");
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_TEXTURE", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
#pragma endregion

#pragma region Animation Section

    // Animations 
    if (ImGui::CollapsingHeader("Animations", ImGuiTreeNodeFlags_None))
    {
        if (cachedAnimationPaths.empty())
        {
            ImGui::Text("No animations loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedAnims;
            for (const auto& path : cachedAnimationPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>
                    ::RegisterAsset(*uuidOpt);
                sortedAnims.emplace_back(assetID, path);
            }
            std::sort(sortedAnims.begin(), sortedAnims.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedAnims.size(); ++i)
            {
                uint32_t assetID = sortedAnims[i].first;
                const std::string& path = sortedAnims[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(cachedAnimationPaths.begin(), cachedAnimationPaths.end(), path)
                    - cachedAnimationPaths.begin());
                bool isSelected = (selectedAnimationIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected))
                {
                    selectedAnimationIndex = origIndex;
                    if (assetID != 0)
                    {
                        EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                    }
                    selectedTextureIndex = -1;
                    selectedAudioIndex = -1;
                    selectedShaderIndex = -1;
                    selectedPrefabIndex = -1;
                    selectedLuaIndex = -1;
                    selectedSceneIndex = -1;
                    selectedTilemapIndex = -1;
                    selectedFontIndex = -1;
                    GGLogger::Info("Selected animation asset ID:" + std::to_string(assetID) + " (" + path + ")");
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_ANIM", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }

#pragma endregion

#pragma region Audio Section
    //Audio files section
    if (ImGui::CollapsingHeader("Audio Files", ImGuiTreeNodeFlags_None))
    {
        if (cachedAudioPaths.empty())
        {
            ImGui::Text("No audio loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedAudio;
            for (const auto& path : cachedAudioPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
                sortedAudio.emplace_back(assetID, path);
            }
            std::sort(sortedAudio.begin(), sortedAudio.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedAudio.size(); ++i)
            {
                uint32_t assetID = sortedAudio[i].first;
                const std::string& path = sortedAudio[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(cachedAudioPaths.begin(), cachedAudioPaths.end(), path)
                    - cachedAudioPaths.begin());
                bool isSelected = (selectedAudioIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected))
                {
                    selectedAudioIndex = origIndex;
                    if (assetID != 0)
                    {
                        EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                        // Also emit audio-specific event with path for Audio Controls
                        uint32_t idCopy = assetID;
                        std::string pathCopy = path;
                        EditorEventBus::onAudioAssetSelected.Emit(std::move(idCopy), std::move(pathCopy));
                    }
                    selectedTextureIndex = -1;
                    selectedShaderIndex = -1;
                    selectedPrefabIndex = -1;
                    selectedLuaIndex = -1;
                    selectedSceneIndex = -1;
                    selectedAnimationIndex = -1;
                    selectedTilemapIndex = -1;
                    selectedFontIndex = -1;
                    GGLogger::Info("Selected Audio asset ID:" + std::to_string(assetID) + " (" + path + ")");
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_AUDIO", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }

#pragma endregion

#pragma region Shaders section
    // Shaders section
    if (ImGui::CollapsingHeader("Shaders", ImGuiTreeNodeFlags_None))
    {
        if (cachedShaderPaths.empty())
        {
            ImGui::Text("No shaders loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedShaders;
            for (const auto& path : cachedShaderPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(*uuidOpt);
                sortedShaders.emplace_back(assetID, path);
            }
            std::sort(sortedShaders.begin(), sortedShaders.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedShaders.size(); ++i)
            {
                uint32_t assetID = sortedShaders[i].first;
                const std::string& path = sortedShaders[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(cachedShaderPaths.begin(), cachedShaderPaths.end(), path)
                    - cachedShaderPaths.begin());
                bool isSelected = (selectedShaderIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected))
                {
                    selectedShaderIndex = origIndex;
                    if (assetID != 0)
                    {
                        EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                    }
                    selectedTextureIndex = -1;
                    selectedAudioIndex = -1;
                    selectedPrefabIndex = -1;
                    selectedLuaIndex = -1;
                    selectedSceneIndex = -1;
                    selectedAnimationIndex = -1;
                    selectedTilemapIndex = -1;
                    selectedFontIndex = -1;
                    GGLogger::Info("Selected Shader asset ID:" + std::to_string(assetID) + " (" + path + ")");
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_SHADER", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
#pragma endregion

#pragma region Prefabs section
    // Prefabs section
    if (ImGui::CollapsingHeader("Prefabs", ImGuiTreeNodeFlags_None))
    {
        auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
        auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
        auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();

        if (discoveredPrefabPaths.empty())
        {
            ImGui::Text("No Prefabs loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedPrefabs;
            for (const auto& path : discoveredPrefabPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<PrefabManager::Prefab>::RegisterAsset(*uuidOpt);
                sortedPrefabs.emplace_back(assetID, path);
            }
            std::sort(sortedPrefabs.begin(), sortedPrefabs.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedPrefabs.size(); ++i)
            {
                uint32_t assetID = sortedPrefabs[i].first;
                const std::string& path = sortedPrefabs[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(discoveredPrefabPaths.begin(), discoveredPrefabPaths.end(), path)
                    - discoveredPrefabPaths.begin());
                bool isSelected = (selectedPrefabIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        sceneManager.SetActiveScene("PrefabEditorWorld");
                        sceneManager.GetActiveScene().ResetWorld();
                        auto& world = sceneManager.GetActiveScene();
                        std::string const& resolved = prefabManager.ResolvePrefabName(path);
                        prefabManager.SetCurrentlyEditedPrefabName(resolved);
                        objectsFactory.InstantiateObjects(resolved, prefabManager, world, 1);
                    }
                    else
                    {
                        selectedPrefabIndex = origIndex;
                        if (assetID != 0)
                        {
                            EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                        }
                        selectedTextureIndex = -1;
                        selectedAudioIndex = -1;
                        selectedShaderIndex = -1;
                        selectedLuaIndex = -1;
                        selectedSceneIndex = -1;
                        selectedAnimationIndex = -1;
                        selectedTilemapIndex = -1;
                        selectedFontIndex = -1;
                        GGLogger::Info("Selected prefab asset ID:" + std::to_string(assetID) + " (" + path + ")");
                    }
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    std::string resolvedName = prefabManager.ResolvePrefabName(path);
                    prefabManager.SetCurrentlyEditedPrefabName(resolvedName);
                    ImGui::SetDragDropPayload("ASSET_PREFAB", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
#pragma endregion

#pragma region Scripts section
    // Lua Scripts section
    if (ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_None))
    {
        if (cachedLuaPaths.empty())
        {
            ImGui::Text("No lua scripts found - try clicking Refresh");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedScripts;
            for (const auto& path : cachedLuaPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
                sortedScripts.emplace_back(assetID, path);
            }
            std::sort(sortedScripts.begin(), sortedScripts.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedScripts.size(); ++i)
            {
                uint32_t assetID = sortedScripts[i].first;
                const std::string& path = sortedScripts[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(cachedLuaPaths.begin(), cachedLuaPaths.end(), path)
                    - cachedLuaPaths.begin());
                bool isSelected = (selectedLuaIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if (!path.empty()) Platform::OpenInEditor(path);
                    }
                    else {
                        selectedLuaIndex = origIndex;
                        if (assetID != 0)
                        {
                            EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                        }
                        selectedTextureIndex = -1;
                        selectedAudioIndex = -1;
                        selectedShaderIndex = -1;
                        selectedPrefabIndex = -1;
                        selectedSceneIndex = -1;
                        selectedAnimationIndex = -1;
                        selectedTilemapIndex = -1;
                        selectedFontIndex = -1;
                        GGLogger::Info("Selected script asset ID:" + std::to_string(assetID) + " (" + path + ")");
                    }
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_SCRIPT", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
#pragma endregion

#pragma region Scene Section
    // Scene section
    if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_None))
    {
        bool createScenePopup = false;
        // rightclick logic to create new Scene
        if (ImGui::BeginPopupContextWindow("ScenesHeader")) // Right-click on the header text/bar
        {
            if (ImGui::MenuItem("Create New Scene"))
            {
                createScenePopup = true;
            }

            if (ImGui::MenuItem("Refresh Asset List"))
            {
                RefreshAssetLists();
            }

            ImGui::EndPopup();
        }

        if (createScenePopup)
        {
            ImGui::OpenPopup("Create New Scene");
            createScenePopup = false; // reset the flag
        }

        // Popup for typing new scene name
        if (ImGui::BeginPopupModal("Create New Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char sceneFileName[128] = "MyScene"; // Default text

            ImGui::Text("Enter your new scene name:");
            ImGui::InputText("##sceneFileName", sceneFileName, IM_ARRAYSIZE(sceneFileName));

            ImGui::Separator();

            const std::string sceneFolderPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Scenes";

            // Create button logic for creating scene name
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                std::string fileNameStr(sceneFileName);

                if (fileNameStr.find(".scene") == std::string::npos)
                {
                    fileNameStr += ".scene";
                }

                const std::filesystem::path fullPath = std::filesystem::path(sceneFolderPath) / fileNameStr;

                // Ensure the folder exists before creating the file
                std::filesystem::create_directories(sceneFolderPath);

                if (!std::filesystem::exists(fullPath))
                {

                    std::ofstream outFileStream(fullPath);
                    if (outFileStream.is_open())
                    {
                        std::string sceneNameOnly = fileNameStr.substr(0, fileNameStr.find(".scene"));
                        outFileStream << "{\n"
                            << "  \"sceneName\": \"" << sceneNameOnly << "\",\n"
                            << "  \"entities\": []\n"
                            << "}";
                        GGLogger::Info("Created new scene file: " + fullPath.string() + "!");
                        outFileStream.close();
                    }
                    else
                    {
                        GGLogger::Error("Failed to create " + fileNameStr + " Scene File!");
                    }

                }
                PrefabManager& prefabManager = GGEngine::GetEngine().GetPrefabManager();
                SceneManager& sceneManager = GGEngine::GetEngine().GetSceneManager();
                if (std::filesystem::exists(fullPath))
                {
                    if (!sceneManager.AddNewScene(fullPath.string(), prefabManager))
                    {
                        GGLogger::Error("Failed to load scene: " + fullPath.string());
                    }
                    else
                    {
                        GGLogger::Info("New scene loaded: " + fullPath.string());
                    }
                }

                RefreshAssetLists();

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (discoveredScenePaths.empty())
        {
            ImGui::Text("No Scenes loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedScenes;
            for (const auto& path : discoveredScenePaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<World>::RegisterAsset(*uuidOpt);
                sortedScenes.emplace_back(assetID, path);
            }
            std::sort(sortedScenes.begin(), sortedScenes.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedScenes.size(); ++i)
            {
                uint32_t assetID = sortedScenes[i].first;
                const std::string& path = sortedScenes[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(discoveredScenePaths.begin(), discoveredScenePaths.end(), path)
                    - discoveredScenePaths.begin());
                bool isSelected = (selectedSceneIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        SceneManager& sceneManager = GGEngine::GetEngine().GetSceneManager();
                        sceneManager.SetActiveScene(path.c_str());
                    }
                    else {
                        selectedSceneIndex = origIndex;
                        if (assetID != 0)
                        {
                            EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                        }
                        selectedTextureIndex = -1;
                        selectedAudioIndex = -1;
                        selectedShaderIndex = -1;
                        selectedPrefabIndex = -1;
                        selectedLuaIndex = -1;
                        selectedAnimationIndex = -1;
                        selectedTilemapIndex = -1;
                        selectedFontIndex = -1;
                        GGLogger::Info("Selected scene asset ID:" + std::to_string(assetID) + " (" + path + ")");
                    }
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_SCENE", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
#pragma endregion

#pragma region Tilemaps
    if (ImGui::CollapsingHeader("Tilemaps", ImGuiTreeNodeFlags_None))
    {
        if (discoveredTilemapPaths.empty())
        {
            ImGui::Text("No Tilemaps loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedTilemaps;
            for (const auto& path : discoveredTilemapPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuidOpt);
                sortedTilemaps.emplace_back(assetID, path);
            }
            std::sort(sortedTilemaps.begin(), sortedTilemaps.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedTilemaps.size(); ++i)
            {
                uint32_t assetID = sortedTilemaps[i].first;
                const std::string& path = sortedTilemaps[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(discoveredTilemapPaths.begin(), discoveredTilemapPaths.end(), path)
                    - discoveredTilemapPaths.begin());
                bool isSelected = (selectedTilemapIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        EditorEventBus::onTilemapOpen.Emit(uint32_t(assetID));
                    }
                    else {
                        selectedTilemapIndex = origIndex;
                        if (assetID != 0)
                        {
                            EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                        }
                        selectedTextureIndex = -1;
                        selectedAudioIndex = -1;
                        selectedShaderIndex = -1;
                        selectedPrefabIndex = -1;
                        selectedLuaIndex = -1;
                        selectedAnimationIndex = -1;
                        selectedSceneIndex = -1;
                        selectedFontIndex = -1;
                        GGLogger::Info("Selected tilemap asset ID:" + std::to_string(assetID) + " (" + path + ")");
                    }
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_TILEMAP", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }

#pragma endregion

#pragma region Fonts section
    // Fonts section
    if (ImGui::CollapsingHeader("Fonts", ImGuiTreeNodeFlags_None))
    {
        if (discoveredFontPaths.empty())
        {
            ImGui::Text("No fonts loaded");
        }
        else
        {
            // Build sorted list by asset ID
            std::vector<std::pair<uint32_t, std::string>> sortedFonts;
            for (const auto& path : discoveredFontPaths)
            {
                uint32_t assetID = 0;
                if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                    assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);
                sortedFonts.emplace_back(assetID, path);
            }
            std::sort(sortedFonts.begin(), sortedFonts.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (size_t i = 0; i < sortedFonts.size(); ++i)
            {
                uint32_t assetID = sortedFonts[i].first;
                const std::string& path = sortedFonts[i].second;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED]");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextUnformatted(std::string("[" + std::to_string(assetID) + "]").c_str());
                ImGui::SameLine();
                int origIndex = static_cast<int>(
                    std::find(discoveredFontPaths.begin(), discoveredFontPaths.end(), path)
                    - discoveredFontPaths.begin());
                bool isSelected = (selectedFontIndex == origIndex);
                uint32_t tempAssetID = assetID;
                if (ImGui::Selectable(path.c_str(), isSelected))
                {
                    selectedFontIndex = origIndex;
                    if (assetID != 0)
                    {
                        EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                    }
                    selectedTextureIndex = -1;
                    selectedAudioIndex = -1;
                    selectedShaderIndex = -1;
                    selectedPrefabIndex = -1;
                    selectedLuaIndex = -1;
                    selectedSceneIndex = -1;
                    selectedAnimationIndex = -1;
                    selectedTilemapIndex = -1;
                    GGLogger::Info("Selected font asset ID:" + std::to_string(assetID) + " (" + path + ")");
                }

                // Drag & Drop attached to the selectable
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::Text("%s", path.c_str());
                    ImGui::SetDragDropPayload("ASSET_FONT", &tempAssetID, sizeof(uint32_t));
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
#pragma endregion

    // Asset statistics
    ImGui::Separator();
    ImGui::Text("Discovered: %zu textures, %zu audio, %zu shaders, %zu prefabs, %zu scripts, %zu scenes, %zu animations, %zu tilemaps, %zu fonts",
        discoveredTexturePaths.size(),
        discoveredAudioPaths.size(),
        discoveredShaderPaths.size(),
        discoveredPrefabPaths.size(),
        discoveredLuaPaths.size(),
        discoveredScenePaths.size(),
        discoveredAnimationPaths.size(),
        discoveredTilemapPaths.size(),
        discoveredFontPaths.size());
}
#pragma endregion
/*************************************************************************
 \brief
    Build folder hierarchy from discovered assets
 *************************************************************************/
void AssetManagerNode::BuildFolderHierarchy(const std::string& assetType)
{
    std::shared_ptr<FolderNode> rootFolder;
    std::vector<std::string>* assetList = nullptr;

    // Select appropriate root folder and asset list
    if (assetType == "Textures") {
        rootFolder = textureRootFolder;
        assetList = &cachedTexturePaths;
        if (!currentTextureFolder) currentTextureFolder = rootFolder;
    }
    else if (assetType == "Audio") {
        rootFolder = audioRootFolder;
        assetList = &cachedAudioPaths;
        if (!currentAudioFolder) currentAudioFolder = rootFolder;
    }
    else if (assetType == "Shaders") {
        rootFolder = shaderRootFolder;
        assetList = &cachedShaderPaths;
        if (!currentShaderFolder) currentShaderFolder = rootFolder;
    }
    else if (assetType == "Scripts") {
        rootFolder = scriptRootFolder;
        assetList = &cachedLuaPaths;
        if (!currentScriptFolder) currentScriptFolder = rootFolder;
    }
    else if (assetType == "Animations") {
        rootFolder = animationRootFolder;
        assetList = &cachedAnimationPaths;
        if (!currentAnimationFolder) currentAnimationFolder = rootFolder;
    }
    else if (assetType == "Prefabs") {
        rootFolder = prefabRootFolder;
        assetList = &discoveredPrefabPaths;
        if (!currentPrefabFolder) currentPrefabFolder = rootFolder;
    }
    else if (assetType == "Scenes") {
        rootFolder = sceneRootFolder;
        assetList = &discoveredScenePaths;
        if (!currentSceneFolder) currentSceneFolder = rootFolder;
    }
    else if (assetType == "Tilemaps") {
        rootFolder = tilemapRootFolder;
        assetList = &discoveredTilemapPaths;
        if (!currentTilemapFolder) currentTilemapFolder = rootFolder;
    }
    else if (assetType == "Fonts") {
        rootFolder = fontRootFolder;
        assetList = &discoveredFontPaths;
        if (!currentFontFolder) currentFontFolder = rootFolder;
    }

    if (!rootFolder || !assetList) return;

    // Clear existing structure
    rootFolder->assets.clear();
    rootFolder->subfolders.clear();

    // Recursive function to build folder tree
    std::function<void(std::shared_ptr<FolderNode>, const std::filesystem::path&)> buildTree;
    buildTree = [&](std::shared_ptr<FolderNode> node, const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) return;

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory())
            {
                auto subfolder = std::make_shared<FolderNode>(
                    entry.path().filename().string(),
                    entry.path().string()
                );
                subfolder->parent = node;

                // Recursively build subfolders
                buildTree(subfolder, entry.path());

                node->subfolders.push_back(subfolder);
            }
        }
        };

    // Scan filesystem for folders
    std::filesystem::path rootPath(rootFolder->fullPath);
    buildTree(rootFolder, rootPath);

    // Distribute assets into folders
    const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
    for (const auto& assetPath : *assetList) {
        std::filesystem::path fullAssetPath = std::filesystem::path(ASSET_BASE) / assetPath;
        std::filesystem::path assetDir = fullAssetPath.parent_path();

        // Find the correct folder for this asset
        std::function<bool(std::shared_ptr<FolderNode>)> placeAsset;
        placeAsset = [&](std::shared_ptr<FolderNode> node) -> bool {
            if (assetDir == std::filesystem::path(node->fullPath)) {
                node->assets.push_back(assetPath);
                return true;
            }

            for (auto& subfolder : node->subfolders) {
                if (placeAsset(subfolder)) {
                    return true;
                }
            }

            return false;
            };

        if (!placeAsset(rootFolder))
        {
            // If not placed in any subfolder, add to root
            rootFolder->assets.push_back(assetPath);
        }
    }
}

/*************************************************************************
 \brief
    Create new folder with right click
 *************************************************************************/
void AssetManagerNode::CreateNewFolder(
    std::shared_ptr<FolderNode> parentFolder,
    const std::string& folderName)
{
    if (!parentFolder || folderName.empty()) return;

    std::filesystem::path newFolderPath =
        std::filesystem::path(parentFolder->fullPath) / folderName;

    try {
        if (std::filesystem::exists(newFolderPath)) {
            GGLogger::Warning("Folder already exists: " + folderName);
            return;
        }

        std::filesystem::create_directories(newFolderPath);

        auto newFolder = std::make_shared<FolderNode>(
            folderName,
            newFolderPath.string()
        );
        newFolder->parent = parentFolder;
        parentFolder->subfolders.push_back(newFolder);

        GGLogger::Info("Created folder: " + newFolderPath.string());
        needsAssetRefresh = true;
    }
    catch (const std::exception& e) {
        GGLogger::Error("Failed to create folder: " + std::string(e.what()));
    }
}

/*************************************************************************
 \brief
    Rename folder with right click
 *************************************************************************/
void AssetManagerNode::RenameFolder(
    std::shared_ptr<FolderNode> folder,
    const std::string& newName)
{
    if (!folder || newName.empty()) return;

    std::filesystem::path oldPath(folder->fullPath);
    std::filesystem::path newPath = oldPath.parent_path() / newName;

    try {
        if (std::filesystem::exists(newPath)) {
            GGLogger::Warning("Folder name already exists: " + newName);
            return;
        }

        // Create the new folder first
        std::filesystem::create_directories(newPath);

        const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
        std::string oldRelativePath = std::filesystem::relative(oldPath, ASSET_BASE).string();
        std::string newRelativePath = std::filesystem::relative(newPath, ASSET_BASE).string();

        // Collect all assets in this folder
        std::vector<std::string> assetsToMove;

        auto collectAssets = [&](const std::vector<std::string>& assetList) {
            for (const auto& assetPath : assetList) {
                if (assetPath.find(oldRelativePath) == 0) {
                    assetsToMove.push_back(assetPath);
                }
            }
            };

        collectAssets(cachedTexturePaths);
        collectAssets(cachedAudioPaths);
        collectAssets(cachedShaderPaths);
        collectAssets(cachedLuaPaths);
        collectAssets(cachedAnimationPaths);
        collectAssets(discoveredPrefabPaths);
        collectAssets(discoveredScenePaths);
        collectAssets(discoveredTilemapPaths);
        collectAssets(discoveredFontPaths);

        // Move each asset individually using GGAssetManager::MoveAsset
        // This ensures all internal caches are updated correctly
        GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();
        if (assetManager) {
            for (const std::string& oldAssetPath : assetsToMove) {
                std::string newAssetPath = newRelativePath + oldAssetPath.substr(oldRelativePath.length());
                assetManager->MoveAsset(oldAssetPath, newAssetPath);
            }
        }

        // Remove the old folder (should be empty now)
        if (std::filesystem::is_empty(oldPath)) {
            std::filesystem::remove(oldPath);
        }

        // Update the folder node
        folder->name = newName;
        folder->fullPath = newPath.string();

        GGLogger::Info("Renamed folder to: " + newName);

        // Trigger refresh to rebuild the tree
        needsAssetRefresh = true;
    }
    catch (const std::exception& e) {
        GGLogger::Error("Failed to rename folder: " + std::string(e.what()));
    }
}

/*************************************************************************
 \brief
    Rename asset with right click
 *************************************************************************/
void AssetManagerNode::RenameAsset(
    const std::string& assetPath,
    const std::string& newName,
    const std::string& assetType)
{
    if (assetPath.empty() || newName.empty()) return;

    const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
    std::filesystem::path oldPath = std::filesystem::path(ASSET_BASE) / assetPath;

    // Get extension from old path
    std::string extension = oldPath.extension().string();

    // Create new path with new name but same extension and directory
    std::filesystem::path newPath = oldPath.parent_path() / (newName + extension);

    try {
        if (std::filesystem::exists(newPath)) {
            GGLogger::Warning("Asset name already exists: " + newName + extension);
            return;
        }

        // Calculate new relative path
        std::string newRelativePath = std::filesystem::relative(newPath, ASSET_BASE).string();

        // Rename the actual file
        std::filesystem::rename(oldPath, newPath);

        // Update UUID registry
        auto& assetSystem = GGAssetID::GetGGAssetID();
        assetSystem->UpdateAssetPath(assetPath, newRelativePath);

        // Update in-memory structures
        std::shared_ptr<FolderNode> rootFolder = nullptr;
        std::vector<std::string>* assetList = nullptr;

        if (assetType == "Textures") { rootFolder = textureRootFolder; assetList = &cachedTexturePaths; }
        else if (assetType == "Audio") { rootFolder = audioRootFolder; assetList = &cachedAudioPaths; }
        else if (assetType == "Shaders") { rootFolder = shaderRootFolder; assetList = &cachedShaderPaths; }
        else if (assetType == "Scripts") { rootFolder = scriptRootFolder; assetList = &cachedLuaPaths; }
        else if (assetType == "Animations") { rootFolder = animationRootFolder; assetList = &cachedAnimationPaths; }
        else if (assetType == "Prefabs") { rootFolder = prefabRootFolder; assetList = &discoveredPrefabPaths; }
        else if (assetType == "Scenes") { rootFolder = sceneRootFolder; assetList = &discoveredScenePaths; }
        else if (assetType == "Tilemaps") { rootFolder = tilemapRootFolder; assetList = &discoveredTilemapPaths; }
        else if (assetType == "Fonts") { rootFolder = fontRootFolder; assetList = &discoveredFontPaths; }

        if (rootFolder && assetList)
        {
            // Find the folder containing this asset
            auto folderPath = oldPath.parent_path();
            auto folder = FindFolderByPath(rootFolder, folderPath.string());

            if (folder)
            {
                // Update folder's asset list
                auto it = std::find(folder->assets.begin(), folder->assets.end(), assetPath);
                if (it != folder->assets.end())
                {
                    *it = newRelativePath;
                }
            }

            // Update cached list
            auto listIt = std::find(assetList->begin(), assetList->end(), assetPath);
            if (listIt != assetList->end())
            {
                *listIt = newRelativePath;
            }
        }

        // Update thumbnail cache if it exists
        auto thumbIt = thumbnailCache.find(assetPath);
        if (thumbIt != thumbnailCache.end())
        {
            GLuint thumbnail = thumbIt->second;
            thumbnailCache.erase(thumbIt);
            thumbnailCache[newRelativePath] = thumbnail;
        }

        GGLogger::Info("Renamed asset to: " + newName + extension);
        needsAssetRefresh = true;
    }
    catch (const std::exception& e) {
        GGLogger::Error("Failed to rename asset: " + std::string(e.what()));
    }
}

/*************************************************************************
 \brief
    Move asset to folder with drag and drop
 *************************************************************************/
void AssetManagerNode::MoveAssetToFolder(
    uint32_t assetID,
    std::shared_ptr<FolderNode> targetFolder,
    const std::string& assetType)
{
    if (!targetFolder) return;

    std::string assetPath;

    // Resolve the path from assetID depending on asset type
    if (assetType == "Textures")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<GLuint>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Audio")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<GGAudioAsset>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Shaders")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<GGShader>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Scripts")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<GGScriptAsset>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Prefabs")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<PrefabManager::Prefab>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Scenes")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<World>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Animations")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Tilemaps")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<GGTilemap>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }
    else if (assetType == "Fonts")
    {
        if (auto uuidOpt = GGECSStandardAssetManager<std::string>::GetUUIDFromID(assetID))
            assetPath = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt);
    }


    if (assetPath.empty()) return;

    const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();;
    std::filesystem::path sourcePath = std::filesystem::path(ASSET_BASE) / assetPath;
    std::filesystem::path destPath = std::filesystem::path(targetFolder->fullPath) / sourcePath.filename();

    try
    {
        if (std::filesystem::exists(destPath))
        {
            GGLogger::Warning("Asset already exists in target folder: " + destPath.string());
            return;
        }

        std::string newRelativePath = std::filesystem::relative(destPath, ASSET_BASE).string();

        // Determine root folder & cached list
        std::shared_ptr<FolderNode> rootFolder = nullptr;
        std::vector<std::string>* assetList = nullptr;

        if (assetType == "Textures") { rootFolder = textureRootFolder; assetList = &cachedTexturePaths; }
        else if (assetType == "Audio") { rootFolder = audioRootFolder; assetList = &cachedAudioPaths; }
        else if (assetType == "Shaders") { rootFolder = shaderRootFolder; assetList = &cachedShaderPaths; }
        else if (assetType == "Scripts") { rootFolder = scriptRootFolder; assetList = &cachedLuaPaths; }
        else if (assetType == "Animations") { rootFolder = animationRootFolder; assetList = &cachedAnimationPaths; }
        else if (assetType == "Prefabs") { rootFolder = prefabRootFolder; assetList = &discoveredPrefabPaths; }
        else if (assetType == "Scenes") { rootFolder = sceneRootFolder; assetList = &discoveredScenePaths; }
        else if (assetType == "Tilemaps") { rootFolder = tilemapRootFolder; assetList = &discoveredTilemapPaths; }
        else if (assetType == "Fonts") { rootFolder = fontRootFolder; assetList = &discoveredFontPaths; }

        if (!rootFolder || !assetList) return;

        // Remove asset from old folder in memory
        auto oldFolderPath = sourcePath.parent_path();
        auto oldFolder = FindFolderByPath(rootFolder, oldFolderPath.string());
        if (oldFolder)
        {
            oldFolder->assets.erase(
                std::remove(oldFolder->assets.begin(), oldFolder->assets.end(), assetPath),
                oldFolder->assets.end()
            );
        }

        // Remove asset from cached list
        assetList->erase(
            std::remove(assetList->begin(), assetList->end(), assetPath),
            assetList->end()
        );

        // Move actual files
        GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();
        if (assetManager)
            assetManager->MoveAsset(assetPath, newRelativePath);

        // Update UUID registry
        auto& assetSystem = GGAssetID::GetGGAssetID();
        assetSystem->UpdateAssetPath(assetPath, newRelativePath);

        // Add to target folder in memory
        targetFolder->assets.push_back(newRelativePath);

        // Add to cached list
        assetList->push_back(newRelativePath);

        GGLogger::Info("Moved asset '" + assetPath + "' to folder: " + targetFolder->fullPath);

        needsAssetRefresh = true;
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("Failed to move asset: " + std::string(e.what()));
    }
}

/*************************************************************************
 \brief
    Render folder tree with assets
 *************************************************************************/
void AssetManagerNode::RenderFolderTree(
    std::shared_ptr<FolderNode> folder,
    const std::string& assetType,
    GLuint defaultThumbnail,
    [[maybe_unused]] const char* dragDropPayload)
{
    if (!folder) return;

    // Update current folder pointer and log it
    static std::unordered_map<std::string, std::string> lastFolderPaths;
    std::string currentPath = folder->fullPath;

    bool folderChanged = false;
    if (lastFolderPaths[assetType] != currentPath)
    {
        folderChanged = true;
        lastFolderPaths[assetType] = currentPath;
    }

    if (assetType == "Textures")
    {
        currentTextureFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Textures folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Audio")
    {
        currentAudioFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Audio folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Shaders")
    {
        currentShaderFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Shader folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Scripts")
    {
        currentScriptFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Script folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Animations")
    {
        currentAnimationFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Animation folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Prefabs")
    {
        currentPrefabFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Prefab folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Scenes")
    {
        currentSceneFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Scene folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Tilemaps")
    {
        currentTilemapFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Tilemap folder changed to: " << folder->fullPath);
    }
    else if (assetType == "Fonts")
    {
        currentFontFolder = folder;
        //if (folderChanged) DROP_DEBUG(">>> Font folder changed to: " << folder->fullPath);
    }

    auto& assetSystem = GGAssetID::GetGGAssetID();

    const float thumbnailSize = 80.0f;
    const float padding = 10.0f;
    const float cellSize = thumbnailSize + padding;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(panelWidth / cellSize);
    if (columns < 1) columns = 1;

    int itemIndex = 0;

    ImGui::Text("Current folder: %s", folder->name.c_str());
    ImGui::Separator();

    if (auto parentShared = folder->parent.lock())
    {
        ImGui::PushID("back_button");
        ImGui::BeginGroup();

        ImVec4 backColour(0.7f, 0.7f, 0.7f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, backColour);

        ImGui::Button("Back", ImVec2(thumbnailSize * 1.08f, thumbnailSize * 1.08f));

        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (assetType == "Textures") currentTextureFolder = parentShared;
            else if (assetType == "Audio") currentAudioFolder = parentShared;
            else if (assetType == "Shaders") currentShaderFolder = parentShared;
            else if (assetType == "Scripts") currentScriptFolder = parentShared;
            else if (assetType == "Animations") currentAnimationFolder = parentShared;
            else if (assetType == "Prefabs") currentPrefabFolder = parentShared;
            else if (assetType == "Scenes") currentSceneFolder = parentShared;
            else if (assetType == "Tilemaps") currentTilemapFolder = parentShared;
            else if (assetType == "Fonts") currentFontFolder = parentShared;
        }

        if (ImGui::BeginDragDropTarget())
        {
            const char* acceptedPayloads[] = {
                "ASSET_TEXTURE",
                "ASSET_AUDIO",
                "ASSET_SHADER",
                "ASSET_SCRIPT",
                "ASSET_PREFAB",
                "ASSET_SCENE",
                "ASSET_ANIM",
                "ASSET_TILEMAP",
                 "ASSET_FONT"
            };

            for (const char* payloadType : acceptedPayloads)
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType))
                {
                    uint32_t droppedAssetID = *static_cast<const uint32_t*>(payload->Data);

                    // Move asset into parent folder
                    MoveAssetToFolder(droppedAssetID, parentShared, assetType);
                    break;
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::Text("Go back");
        ImGui::EndGroup();
        ImGui::PopID();

        itemIndex++;
    }

    // Folder context menu
    if (ImGui::BeginPopupContextWindow(("FolderCtx_" + folder->name).c_str(), ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Create New folder"))
        {
            contextMenuFolder = folder;
            createFolderAssetType = assetType;
            showCreateFolderPopup = true;
        }

        if (assetType == "Scenes") // only for Scenes tab
        {
            if (ImGui::MenuItem("Create New Scene"))
            {
                showCreateScenePopup = true; // local flag
            }
        }

        if (assetType == "Scripts")
        {
            if (ImGui::MenuItem("Create New Script"))
            {
                showCreateScriptPopup = true;
            }
        }

        ImGui::EndPopup();
    }

    // Render subfolders
    for (auto& subfolder : folder->subfolders)
    {
        if (itemIndex % columns != 0) ImGui::SameLine();

        ImGui::PushID(subfolder->name.c_str());
        ImGui::BeginGroup();

        // Folder thumbnail button
        ImVec4 folderColour(1.0f, 0.8f, 0.0f, 1.0f); // Gold colour
        ImGui::PushStyleColor(ImGuiCol_Button, folderColour);
        ImGui::Button("Open", ImVec2(thumbnailSize * 1.08f, thumbnailSize * 1.08f));
        ImGui::PopStyleColor();

        // Check for double-click to enter folder
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (assetType == "Textures") currentTextureFolder = subfolder;
            else if (assetType == "Audio") currentAudioFolder = subfolder;
            else if (assetType == "Shaders") currentShaderFolder = subfolder;
            else if (assetType == "Scripts") currentScriptFolder = subfolder;
            else if (assetType == "Animations") currentAnimationFolder = subfolder;
            else if (assetType == "Prefabs") currentPrefabFolder = subfolder;
            else if (assetType == "Scenes") currentSceneFolder = subfolder;
            else if (assetType == "Tilemaps") currentTilemapFolder = subfolder;
            else if (assetType == "Fonts") currentFontFolder = subfolder;
        }


        // Context menu for subfolder
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Rename")) {
                contextMenuFolder = subfolder;
                showRenameFolderPopup = true;
                newFolderName = subfolder->name;
            }
            ImGui::EndPopup();
        }

        // Drop target
        if (ImGui::BeginDragDropTarget())
        {
            const char* acceptedPayloads[] = {
                "ASSET_TEXTURE",
                "ASSET_AUDIO",
                "ASSET_SHADER",
                "ASSET_SCRIPT",
                "ASSET_PREFAB",
                "ASSET_SCENE",
                "ASSET_ANIM",
                "ASSET_TILEMAP",
                "ASSET_FONT"
            };

            for (const char* payloadType : acceptedPayloads)
            {
                if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload(payloadType))
                {
                    uint32_t droppedAssetID = *static_cast<const uint32_t*>(payload->Data);

                    MoveAssetToFolder(droppedAssetID, subfolder, assetType);
                    break;
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);
        ImGui::TextWrapped("%s", subfolder->name.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();

        itemIndex++;
    }

    for (size_t i = 0; i < folder->assets.size(); ++i)
    {
        const std::string& path = folder->assets[i];

        uint32_t preRegisteredAssetID = 0;
        if (assetType == "Scenes")
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
                preRegisteredAssetID = GGECSStandardAssetManager<World>::RegisterAsset(*uuidOpt);
        }
        if (itemIndex % columns != 0) ImGui::SameLine();

        ImGui::PushID((path + std::to_string(i)).c_str());
        ImGui::BeginGroup();

        // Asset thumbnail
        GLuint thumbnail = GetThumbnailForAsset(path, defaultThumbnail);
        void* texID = reinterpret_cast<void*>(static_cast<intptr_t>(thumbnail));

        ImVec2 uv0(0, 1);
        ImVec2 uv1(1, 0);

        bool clicked = ImGui::ImageButton(path.c_str(), texID, ImVec2(thumbnailSize, thumbnailSize), uv0, uv1);
        if (clicked)
        {
            // Handle selection
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                uint32_t assetID = 0;
                // Register based on type
                if (assetType == "Scenes" && preRegisteredAssetID != 0)
                    assetID = preRegisteredAssetID;
                else if (assetType == "Textures")
                    assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);
                else if (assetType == "Audio")
                    assetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
                else if (assetType == "Animations")
                    assetID = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::RegisterAsset(*uuidOpt);
                else if (assetType == "Shaders")
                    assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(*uuidOpt);
                else if (assetType == "Prefabs")
                    assetID = GGECSStandardAssetManager<PrefabManager::Prefab>::RegisterAsset(*uuidOpt);
                else if (assetType == "Scripts")
                    assetID = GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
                else if (assetType == "Tilemaps")
                    assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuidOpt);
                else if (assetType == "Fonts")
                    assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);

                if (assetID != 0)
                {
                    EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                    if (assetType == "Audio")
                    {
                        uint32_t idCopy = assetID;
                        std::string pathCopy = path;
                        EditorEventBus::onAudioAssetSelected.Emit(std::move(idCopy), std::move(pathCopy));
                    }
                }
            }
        }

        if (ImGui::BeginPopupContextItem())
        {
            // Add right-click context menu for assets
            if (ImGui::MenuItem("Rename"))
            {
                renameAssetPath = path;
                renameAssetType = assetType;
                showRenameAssetPopup = true;
                assetToRename = renameAssetPath;
                std::filesystem::path assetFilePath(path);
                renameAssetNewName = assetFilePath.stem().string();
            }

            if (ImGui::MenuItem("Delete"))
            {
                DeleteAssetWithConfirmation(path, assetType);
            }

            ImGui::EndPopup();
        }

        //(use loaded stuff)
        // Double Clicking asset logic
        auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
        auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
        auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();

        if (assetType == "Prefabs" && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            sceneManager.SetActiveScene("PrefabEditorWorld");
            sceneManager.GetActiveScene().ResetWorld();
            auto& world = sceneManager.GetActiveScene();
            std::string const& resolved = prefabManager.ResolvePrefabName(path);
            std::cout << resolved << std::endl;
            prefabManager.SetCurrentlyEditedPrefabName(resolved);
            objectsFactory.InstantiateObjects(resolved, prefabManager, world, 1);
        }

        if (assetType == "Scenes" && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            sceneManager.SetActiveScene(path.c_str());
        }

        if (assetType == "Scripts" && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (!path.empty()) Platform::OpenInEditor(path);
        }

        if (assetType == "Tilemaps" && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path)) {
                uint32_t assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuidOpt);
                EditorEventBus::onTilemapOpen.Emit(uint32_t(assetID));
            }
        }

        // Drag source for moving AND component assignment
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::Text("%s", path.c_str());

            // Determine AssetID
            uint32_t assetID = 0;
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                if (assetType == "Scenes" && preRegisteredAssetID != 0)
                    assetID = preRegisteredAssetID;
                else if (assetType == "Textures")
                    assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);
                else if (assetType == "Audio")
                    assetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
                else if (assetType == "Animations")
                    assetID = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::RegisterAsset(*uuidOpt);
                else if (assetType == "Shaders")
                    assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(*uuidOpt);
                else if (assetType == "Prefabs")
                {
                    assetID = GGECSStandardAssetManager<PrefabManager::Prefab>::RegisterAsset(*uuidOpt);
                    auto& prefabMgr = GGEngine::GetEngine().GetPrefabManager();
                    std::string resolvedName = prefabMgr.ResolvePrefabName(path);
                    prefabMgr.SetCurrentlyEditedPrefabName(resolvedName);
                }
                else if (assetType == "Scripts")
                    assetID = GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
                else if (assetType == "Tilemaps")
                    assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuidOpt);
                else if (assetType == "Scenes")
                    assetID = GGECSStandardAssetManager<World>::RegisterAsset(*uuidOpt);
                else if (assetType == "Fonts")
                    assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);
            }

            // Determine payload type string for editor
            const char* payloadType = nullptr;
            if (assetType == "Scenes") payloadType = "ASSET_SCENE";
            else if (assetType == "Textures") payloadType = "ASSET_TEXTURE";
            else if (assetType == "Audio") payloadType = "ASSET_AUDIO";
            else if (assetType == "Shaders") payloadType = "ASSET_SHADER";
            else if (assetType == "Prefabs") payloadType = "ASSET_PREFAB";
            else if (assetType == "Scripts") payloadType = "ASSET_SCRIPT";
            else if (assetType == "Animations") payloadType = "ASSET_ANIM";
            else if (assetType == "Tilemaps") payloadType = "ASSET_TILEMAP";
            else if (assetType == "Fonts") payloadType = "ASSET_FONT";

            // Set payload as AssetID
            if (payloadType)
                ImGui::SetDragDropPayload(payloadType, &assetID, sizeof(uint32_t));

            ImGui::EndDragDropSource();
        }

        // Display name
        std::string displayName = GetAssetDisplayName(path);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);
        ImGui::TextWrapped("%s", displayName.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();

        itemIndex++;
    }
}

/*************************************************************************
 \brief
    Find current folder by path
 *************************************************************************/
std::shared_ptr<AssetManagerNode::FolderNode>
AssetManagerNode::FindFolderByPath(
    std::shared_ptr<FolderNode> root,
    const std::string& path)
{
    if (!root)
        return nullptr;

    std::filesystem::path rootPath = std::filesystem::path(root->fullPath).lexically_normal();
    std::filesystem::path targetPath = std::filesystem::path(path).lexically_normal();

    if (rootPath == targetPath)
        return root;

    for (auto& child : root->subfolders)
    {
        if (auto result = FindFolderByPath(child, path))
            return result;
    }

    return nullptr;
}

/*************************************************************************
 \brief
    Initialize default thumbnails for each asset type
 *************************************************************************/
void AssetManagerNode::InitializeDefaultThumbnails()
{
    // Create colored thumbnails for different asset types
    defaultTextureThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultAudioThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultShaderThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultPrefabThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultScriptThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultSceneThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultAnimationThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultTilemapThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
    defaultFontThumbnail = CreateColoredThumbnail(0.0f, 0.0f, 0.0f);
}

/*************************************************************************
 \brief
    Create a simple colored texture to use as a placeholder thumbnail
 *************************************************************************/
GLuint AssetManagerNode::CreateColoredThumbnail(float r, float g, float b)
{
    const int size = 64;
    std::vector<unsigned char> pixels(size * size * 4);

    for (int i = 0; i < size * size; ++i)
    {
        pixels[i * 4 + 0] = static_cast<unsigned char>(r * 255);
        pixels[i * 4 + 1] = static_cast<unsigned char>(g * 255);
        pixels[i * 4 + 2] = static_cast<unsigned char>(b * 255);
        pixels[i * 4 + 3] = 255;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

/*************************************************************************
 \brief
    Get thumbnail texture for an asset (or return default)
 *************************************************************************/
GLuint AssetManagerNode::GetThumbnailForAsset(const std::string& assetPath, GLuint defaultThumbnail)
{
    // Check cache first
    auto it = thumbnailCache.find(assetPath);
    if (it != thumbnailCache.end())
    {
        return it->second;
    }

    // For textures, try to load the actual texture
    if (assetPath.find(".png") != std::string::npos ||
        assetPath.find(".jpg") != std::string::npos ||
        assetPath.find(".jpeg") != std::string::npos ||
        assetPath.find(".tga") != std::string::npos)
    {
        try
        {
            GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();
            if (assetManager)
            {
                GLuint textureID = assetManager->GetTexture(assetPath);
                thumbnailCache[assetPath] = textureID;
                return textureID;
            }
        }
        catch (...)
        {
            // If loading fails, use default
        }
    }

    if (assetPath.find(".gganim") != std::string::npos)
    {
        try
        {
            auto& animSystem = Systems::GetECSSystemsRegistry()
                .GetSystem<Systems::ECSSpriteAnimator>();

            // Register (or find existing) animation
            uint32_t animIndex = animSystem.RegisterAnimationByPath(assetPath);
            const auto& anim = animSystem.GetAnimation(animIndex);

            // Use first texture page if available
            if (!anim.texturePages.empty() && anim.texturePages[0] != 0
                && GGECSStandardAssetManager<GLuint>::HasAsset(anim.texturePages[0]))
            {
                GLuint textureID = GGECSStandardAssetManager<GLuint>::GetAsset(anim.texturePages[0]);
                thumbnailCache[assetPath] = textureID;
                return textureID;
            }
        }
        catch (...) {}
    }

    // Cache and return default
    thumbnailCache[assetPath] = defaultThumbnail;
    return defaultThumbnail;
}

/*************************************************************************
 \brief
    Extract display name from full asset path
 *************************************************************************/
std::string AssetManagerNode::GetAssetDisplayName(const std::string& path)
{
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

    // Remove extension
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos)
    {
        filename = filename.substr(0, lastDot);
    }

    return filename;
}

/*************************************************************************
 \brief
    Render assets in a grid layout with thumbnails
 *************************************************************************/
void AssetManagerNode::RenderAssetGrid(const char* label,
    const std::vector<std::string>& assets,
    GLuint defaultThumbnail,
    int& selectedIndex,
    [[maybe_unused]] const char* dragDropPayload)
{
    if (assets.empty())
    {
        ImGui::TextDisabled("No %s found", label);
        return;
    }

    auto& assetSystem = GGAssetID::GetGGAssetID();

    const float thumbnailSize = 80.0f;
    const float padding = 20.0f;
    const float cellSize = thumbnailSize + padding;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(panelWidth / cellSize);
    if (columns < 1) columns = 1;

    for (size_t i = 0; i < assets.size(); ++i)
    {
        const std::string& path = assets[i];
        ImGui::PushID(static_cast<int>(i));

        // Start new row
        if (i % columns != 0)
        {
            ImGui::SameLine();
        }

        ImGui::BeginGroup();

        // Get thumbnail
        GLuint thumbnail = GetThumbnailForAsset(path, defaultThumbnail);

        // Thumbnail button
        bool isSelected = (selectedIndex == static_cast<int>(i));
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }
        // Image button with unique ID
        ImGui::PushID(("img_" + std::to_string(i)).c_str());
        void* texID = reinterpret_cast<void*>(static_cast<intptr_t>(thumbnail));

        ImVec2 uv0(0, 1);
        ImVec2 uv1(1, 0);

        bool clicked = ImGui::ImageButton(path.c_str(), texID, ImVec2(thumbnailSize, thumbnailSize), uv0, uv1);
        ImGui::PopID();

        if (clicked)
        {
            selectedIndex = static_cast<int>(i);

            // Get asset ID and emit event
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                uint32_t assetID = 0;

                // Register asset based on type
                if (strcmp(label, "Textures") == 0)
                    assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Audio") == 0)
                    assetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Shaders") == 0)
                    assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Prefabs") == 0)
                    assetID = GGECSStandardAssetManager<PrefabManager::Prefab>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Scripts") == 0)
                    assetID = GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Scenes") == 0)
                    assetID = GGECSStandardAssetManager<World>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Animations") == 0)
                    assetID = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Tilemaps") == 0)
                    assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Fonts") == 0)
                    assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);

                if (assetID != 0)
                {
                    EditorEventBus::onAssetSelected.Emit(std::move(assetID));
                }
            }

            GGLogger::Info("Selected " + std::string(label) + ": " + path);
        }

        if (isSelected)
        {
            ImGui::PopStyleColor();
        }

        // Drag & Drop
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
            {
                uint32_t assetID = 0;

                if (strcmp(label, "Textures") == 0)
                    assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Audio") == 0)
                    assetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Shaders") == 0)
                    assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Prefabs") == 0)
                {
                    assetID = GGECSStandardAssetManager<PrefabManager::Prefab>::RegisterAsset(*uuidOpt);
                    auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
                    std::string resolvedName = prefabManager.ResolvePrefabName(path);
                    prefabManager.SetCurrentlyEditedPrefabName(resolvedName);
                }
                else if (strcmp(label, "Scripts") == 0)
                    assetID = GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Animations") == 0)
                    assetID = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Tilemaps") == 0)
                    assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuidOpt);
                else if (strcmp(label, "Fonts") == 0)
                    assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuidOpt);

                // Determine ImGui payload type string for the editor
                const char* payloadType = nullptr;
                if (strcmp(label, "Scenes") == 0) payloadType = "ASSET_SCENE";
                else if (strcmp(label, "Textures") == 0) payloadType = "ASSET_TEXTURE";
                else if (strcmp(label, "Audio") == 0) payloadType = "ASSET_AUDIO";
                else if (strcmp(label, "Shaders") == 0) payloadType = "ASSET_SHADER";
                else if (strcmp(label, "Prefabs") == 0) payloadType = "ASSET_PREFAB";
                else if (strcmp(label, "Scripts") == 0) payloadType = "ASSET_SCRIPT";
                else if (strcmp(label, "Animations") == 0) payloadType = "ASSET_ANIM";
                else if (strcmp(label, "Tilemaps") == 0) payloadType = "ASSET_TILEMAP";
                else if (strcmp(label, "Fonts") == 0) payloadType = "ASSET_FONT";

                ImGui::Text("%s", path.c_str());

                // Send AssetID to editor, not the path string
                if (payloadType)
                    ImGui::SetDragDropPayload(payloadType, &assetID, sizeof(uint32_t));
            }

            ImGui::EndDragDropSource();
        }

        // Display name below thumbnail
        std::string displayName = GetAssetDisplayName(path);
        float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
        float textPosX = (thumbnailSize - textWidth) * 0.5f;
        if (textPosX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textPosX);

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);
        ImGui::TextWrapped("%s", displayName.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();
    }
}

/*************************************************************************
 \brief
    Render thumbnail-based asset browser
 *************************************************************************/
void AssetManagerNode::RenderThumbnailBrowser()
{
    ImGui::BeginDisabled(isPlaying);
    if (ImGui::Button("Refresh Assets"))
    {
        needsAssetRefresh = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    {
        GGAssetManager* assetManager = GGEngine::GetEngine().GetAssetManager();
        if (assetManager)
        {
            std::filesystem::path projectPath = std::filesystem::path(assetManager->GetAssetBasePath()).parent_path().parent_path();
            std::string projectName = projectPath.filename().string();

            ImGui::TextDisabled("Project: ");
            ImGui::SameLine();
            ImGui::Text("%s", projectName.c_str());
        }
    }

    ImGui::Separator();



    if (ImGui::BeginTabBar("AssetTabs"))
    {
        if (ImGui::BeginTabItem("Textures"))
        {
            ImGui::BeginChild(
                "TextureScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentTextureFolder) currentTextureFolder = textureRootFolder;
            RenderFolderTree(currentTextureFolder, "Textures",
                defaultTextureThumbnail, "ASSET_TEXTURE");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Animations"))
        {
            ImGui::BeginChild(
                "AnimationScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentAnimationFolder) currentAnimationFolder = animationRootFolder;
            RenderFolderTree(currentAnimationFolder, "Animations",
                defaultAnimationThumbnail, "ASSET_ANIM");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio"))
        {
            ImGui::BeginChild(
                "AudioScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentAudioFolder) currentAudioFolder = audioRootFolder;
            RenderFolderTree(currentAudioFolder, "Audio",
                defaultAudioThumbnail, "ASSET_AUDIO");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Shaders"))
        {
            ImGui::BeginChild(
                "ShaderScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentShaderFolder) currentShaderFolder = shaderRootFolder;
            RenderFolderTree(currentShaderFolder, "Shaders",
                defaultShaderThumbnail, "ASSET_SHADER");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Prefabs"))
        {
            ImGui::BeginChild(
                "PrefabScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentPrefabFolder) currentPrefabFolder = prefabRootFolder;
            RenderFolderTree(currentPrefabFolder, "Prefabs",
                defaultPrefabThumbnail, "ASSET_PREFAB");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scripts"))
        {
            ImGui::BeginChild(
                "ScriptScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentScriptFolder) currentScriptFolder = scriptRootFolder;
            RenderFolderTree(currentScriptFolder, "Scripts",
                defaultScriptThumbnail, "ASSET_SCRIPT");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scenes"))
        {
            ImGui::BeginChild(
                "SceneScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentSceneFolder) currentSceneFolder = sceneRootFolder;
            RenderFolderTree(currentSceneFolder, "Scenes",
                defaultSceneThumbnail, "ASSET_SCENE");

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tilemaps"))
        {
            ImGui::BeginChild(
                "TilemapScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentTilemapFolder) currentTilemapFolder = tilemapRootFolder;
            RenderFolderTree(currentTilemapFolder, "Tilemaps",
                defaultTilemapThumbnail, "ASSET_TILEMAP");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fonts"))
        {
            ImGui::BeginChild(
                "FontScroll",
                ImVec2(0, 0),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );
            if (!currentFontFolder) currentFontFolder = fontRootFolder;
            RenderFolderTree(currentFontFolder, "Fonts",
                defaultFontThumbnail, "ASSET_FONT");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    // Create folder popup
    char buffer[40];
    strncpy(buffer, newFolderName.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    if (showCreateFolderPopup) {
        ImGui::OpenPopup("Create New Folder");
        showCreateFolderPopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Create New Folder", NULL,
        ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name:");
        if (ImGui::InputText("##folderName", buffer, sizeof(buffer)))
        {
            newFolderName = std::string(buffer);
        }

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (contextMenuFolder) {
                CreateNewFolder(contextMenuFolder, newFolderName);
                contextMenuFolder = nullptr;
                newFolderName = defaultFolderName;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }


    // Popup for creating new scene
    if (showCreateScenePopup) {
        ImGui::OpenPopup("Create New Scene");
        showCreateScenePopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Create New Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char sceneFileName[128] = "MyScene"; // Default text

        ImGui::Text("Enter your new scene name:");
        ImGui::InputText("##sceneFileName", sceneFileName, IM_ARRAYSIZE(sceneFileName));

        ImGui::Separator();

        const std::string sceneFolderPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Scenes";

        // Create button logic for creating scene name
        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::string fileNameStr(sceneFileName);

            if (fileNameStr.find(".scene") == std::string::npos)
            {
                fileNameStr += ".scene";
            }

            const std::filesystem::path fullPath = std::filesystem::path(sceneFolderPath) / fileNameStr;

            // Ensure the folder exists before creating the file
            std::filesystem::create_directories(sceneFolderPath);

            if (!std::filesystem::exists(fullPath))
            {

                std::ofstream outFileStream(fullPath);
                if (outFileStream.is_open())
                {
                    std::string sceneNameOnly = fileNameStr.substr(0, fileNameStr.find(".scene"));
                    outFileStream << "{\n"
                        << "  \"sceneName\": \"" << sceneNameOnly << "\",\n"
                        << "  \"entities\": []\n"
                        << "}";
                    GGLogger::Info("Created new scene file: " + fullPath.string() + "!");
                    outFileStream.close();
                }
                else
                {
                    GGLogger::Error("Failed to create " + fileNameStr + " Scene File!");
                }

            }
            PrefabManager& prefabManager = GGEngine::GetEngine().GetPrefabManager();
            SceneManager& sceneManager = GGEngine::GetEngine().GetSceneManager();
            if (std::filesystem::exists(fullPath))
            {
                if (!sceneManager.AddNewScene(fullPath.string(), prefabManager))
                {
                    GGLogger::Error("Failed to load scene: " + fullPath.string());
                }
                else
                {
                    GGLogger::Info("New scene loaded: " + fullPath.string());
                }
            }

            RefreshAssetLists();

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Popup for creating new script
    if (showCreateScriptPopup)
    {
        ImGui::OpenPopup("Create New Script");
        showCreateScriptPopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Create New Script", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char scriptFileName[128] = "MyScript";

        ImGui::Text("Enter your new script name:");
        ImGui::InputText("##scriptFileName", scriptFileName, IM_ARRAYSIZE(scriptFileName));

        ImGui::Separator();

        const std::string scriptFolderPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Scripts";

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::string fileNameStr(scriptFileName);

            if (fileNameStr.find(".lua") == std::string::npos)
            {
                fileNameStr += ".lua";
            }

            const std::filesystem::path fullPath = std::filesystem::path(scriptFolderPath) / fileNameStr;

            std::filesystem::create_directories(scriptFolderPath);

            if (!std::filesystem::exists(fullPath))
            {
                std::ofstream outFileStream(fullPath);
                if (outFileStream.is_open())
                {
                    std::string scriptNameOnly = fileNameStr.substr(0, fileNameStr.find(".lua"));

                    outFileStream << "--[[************************************************************************" << std::endl
                        << "  \\file       " << fileNameStr << std::endl
                        << "  \\author     " << std::endl
                        << "  \\par        Email: " << std::endl
                        << "  \\date       01 February 2026" << std::endl
                        << "  \\version    1.0" << std::endl
                        << "  \\brief" << std::endl
                        << "    EDIT YOUR BRIEF HERE" << std::endl
                        << "  \\details" << std::endl
                        << "    EDIT YOUR DETAILS HERE" << std::endl
                        << "  \\dependencies" << std::endl
                        << "    " << std::endl
                        << "  \\see" << std::endl
                        << "    " << std::endl
                        << "  \\history" << std::endl
                        << "    - 1.0 (01 February 2026): Created this script." << std::endl
                        << "  \\copyright" << std::endl
                        << "    Copyright (C) 2026 DigiPen Institute of Technology." << std::endl
                        << "************************************************************************--]]" << std::endl;

                    GGLogger::Info("Created new script file: " + fullPath.string() + "!");
                    outFileStream.close();
                }
                else
                {
                    GGLogger::Error("Failed to create " + fileNameStr + " Script File!");
                }
            }
            else
            {
                GGLogger::Warning("Script file already exists: " + fullPath.string());
            }

            RefreshAssetLists();

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Rename folder popup
    if (showRenameFolderPopup)
    {
        newFolderName = std::string(renameFolderBuffer);
        strncpy(renameFolderBuffer, newFolderName.c_str(), sizeof(renameFolderBuffer) - 1);
        ImGui::OpenPopup("Rename Folder");
        showRenameFolderPopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Rename Folder", NULL,
        ImGuiWindowFlags_AlwaysAutoResize))
    {

        ImGui::Text("Enter new folder name:");
        ImGui::InputText("##renameFolder", renameFolderBuffer, sizeof(renameFolderBuffer));

        if (ImGui::Button("Rename", ImVec2(120, 0)))
        {
            newFolderName = std::string(renameFolderBuffer);
            if (contextMenuFolder)
            {
                // save old path before renaming
                std::string oldPath = contextMenuFolder->fullPath;

                RenameFolder(contextMenuFolder, newFolderName);

                if (currentTextureFolder && currentTextureFolder->fullPath == oldPath)
                    currentTextureFolder = contextMenuFolder;
                if (currentAudioFolder && currentAudioFolder->fullPath == oldPath)
                    currentAudioFolder = contextMenuFolder;
                if (currentShaderFolder && currentShaderFolder->fullPath == oldPath)
                    currentShaderFolder = contextMenuFolder;
                if (currentScriptFolder && currentScriptFolder->fullPath == oldPath)
                    currentScriptFolder = contextMenuFolder;
                if (currentAnimationFolder && currentAnimationFolder->fullPath == oldPath)
                    currentAnimationFolder = contextMenuFolder;
                if (currentPrefabFolder && currentPrefabFolder->fullPath == oldPath)
                    currentPrefabFolder = contextMenuFolder;
                if (currentSceneFolder && currentSceneFolder->fullPath == oldPath)
                    currentSceneFolder = contextMenuFolder;
                if (currentTilemapFolder && currentTilemapFolder->fullPath == oldPath)
                    currentTilemapFolder = contextMenuFolder;
                if (currentFontFolder && currentFontFolder->fullPath == oldPath)
                    currentFontFolder = contextMenuFolder;

                contextMenuFolder = nullptr;
            }
            memset(renameFolderBuffer, 0, sizeof(renameFolderBuffer));
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            memset(renameFolderBuffer, 0, sizeof(renameFolderBuffer));
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Rename asset popup
    if (showRenameAssetPopup) {
        ImGui::OpenPopup("Rename Asset");
        showRenameAssetPopup = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Rename Asset", NULL,
        ImGuiWindowFlags_AlwaysAutoResize)) {

        if (ImGui::IsPopupOpen("Rename Asset") && assetToRename.length() > 0)
        {
            std::filesystem::path assetPath(assetToRename);
            std::string stem = assetPath.stem().string();
            strncpy(renameAssetBuffer, stem.c_str(), sizeof(renameAssetBuffer) - 1);
            renameAssetBuffer[sizeof(renameAssetBuffer) - 1] = '\0';
            assetToRename.clear();
        }

        ImGui::Text("Enter new asset name:");
        ImGui::InputText("##renameAsset", renameAssetBuffer, sizeof(renameAssetBuffer));

        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            renameAssetNewName = std::string(renameAssetBuffer);

            const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();

            std::filesystem::path oldFullPath = ASSET_BASE + renameAssetPath;
            std::filesystem::path newFullPath = oldFullPath.parent_path() / (renameAssetNewName + oldFullPath.extension().string());

            // Relative paths (for internal asset tracking)
            std::filesystem::path oldRelPath(renameAssetPath);
            std::string newRelativePath = (oldRelPath.parent_path() / (renameAssetNewName + oldRelPath.extension().string())).string();

            // Rename the asset file on disk
            if (std::filesystem::exists(oldFullPath))
            {
                std::filesystem::rename(oldFullPath, newFullPath);
            }
            std::filesystem::path oldMetaPath = GGAssetID::GetGGAssetID()->ComputeMetaPath(renameAssetPath);
            std::filesystem::path newMetaPath = GGAssetID::GetGGAssetID()->ComputeMetaPath(newRelativePath);
            if (std::filesystem::exists(oldMetaPath))
            {
                std::filesystem::rename(oldMetaPath, newMetaPath);
            }

            auto assetManager = GGEngine::GetEngine().GetAssetManager();
            assetManager->UnloadAsset(renameAssetPath);
            GGAssetID::GetGGAssetID()->UpdateAssetPath(renameAssetPath, newRelativePath);

            renameAssetPath.clear();
            renameAssetType.clear();
            memset(renameAssetBuffer, 0, sizeof(renameAssetBuffer));
            needsAssetRefresh = true;
            ImGui::CloseCurrentPopup();

        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            renameAssetPath.clear();
            renameAssetType.clear();
            memset(renameAssetBuffer, 0, sizeof(renameAssetBuffer));
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Asset statistics
    ImGui::Separator();
    ImGui::Text("Total: %zu textures, %zu audio, %zu shaders, %zu prefabs, %zu scripts, %zu scenes, %zu animations, %zu tilemaps",
        discoveredTexturePaths.size(),
        discoveredAudioPaths.size(),
        discoveredShaderPaths.size(),
        discoveredPrefabPaths.size(),
        discoveredLuaPaths.size(),
        discoveredScenePaths.size(),
        discoveredAnimationPaths.size(),
        discoveredTilemapPaths.size(),
        discoveredFontPaths.size());
}

/*************************************************************************
  \brief
    Find the currently expanded folder in the hierarchy
*************************************************************************/
//std::shared_ptr<AssetManagerNode::FolderNode> AssetManagerNode::FindExpandedFolder(
//    std::shared_ptr<FolderNode> root)
//{
//    if (!root) return nullptr;
//
//    // If this folder is expanded, return it
//    if (root->isExpanded) return root;
//
//    // Check subfolders recursively
//    for (auto& subfolder : root->subfolders)
//    {
//        if (subfolder->isExpanded)
//        {
//            return subfolder;
//        }
//
//        // Check deeper levels
//        auto found = FindExpandedFolder(subfolder);
//        if (found) return found;
//    }
//
//    return nullptr;
//}
#pragma endregion

/*************************************************************************
  \brief
    Renders the popup for duplicated files.
  \details
    Displays a modal listing all duplicated files and allows the user
    to replace all, skip all, or select files individually for replacement.
*************************************************************************/
void AssetManagerNode::RenderDuplicatedFilePopUp()
{
    auto assetManager = GGEngine::GetEngine().GetAssetManager();

    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f) // pivot: center of window
    );

    // Popup for typing new scene name
    if (ImGui::BeginPopupModal("Warning, duplicated files detected!", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("The following files already exist:");
        ImGui::Separator();

        ImGui::BeginChild("DuplicateFilesList", ImVec2(400, 200), true);
        for (size_t i = 0; i < duplicatedFiles.size(); ++i)
        {
            ImGui::BulletText("%s", duplicatedFiles[i].filename.c_str());
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("Do you want to replace All of the files?");
        ImGui::Spacing();

        // button row
        if (ImGui::Button("Replace All", ImVec2(120, 0)))
        {
            if (assetManager)
            {
                // replace all duplicated files
                for (const auto& fileInfo : duplicatedFiles)
                {
                    assetManager->ConfirmAndReplaceFile(fileInfo.fullPath, "");
                }

                needsAssetRefresh = true;
                assetManager->ClearDuplicatedFileFlag();
                duplicatedFiles.clear();
            }

            // open unsupported popup if there are unsupported files
            if (!unsupportedFiles.empty())
            {
                openUnsupportedPopUpNextFrame = true;
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Skip All", ImVec2(120, 0)))
        {
            if (assetManager)
            {
                assetManager->ClearDuplicatedFileFlag();
                duplicatedFiles.clear();
            }

            // open unsupported popup if there are unsupported files
            if (!unsupportedFiles.empty())
            {
                openUnsupportedPopUpNextFrame = true;
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Choose Individually", ImVec2(150, 0)))
        {
            // initialize selection vector before opening popup
            selectedFilesForReplacement.clear();
            selectedFilesForReplacement.resize(duplicatedFiles.size(), 1);

            ImGui::CloseCurrentPopup();

            // open individual selection popup
            openIndividualSelectionPopup = true;
        }

        ImGui::EndPopup();
    }

    // individual file selection popup
    if (ImGui::BeginPopupModal("Select Files to replace", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Select which files to replace:");
        ImGui::Separator();

        // ensure selection vector is properly sized      
        if (selectedFilesForReplacement.size() != duplicatedFiles.size())
        {
            selectedFilesForReplacement.resize(duplicatedFiles.size(), 1);
        }

        ImGui::BeginChild("IndividualFilesList", ImVec2(400, 250), true);
        for (size_t i = 0; i < duplicatedFiles.size(); ++i)
        {
            bool isSelected = selectedFilesForReplacement[i] != 0;
            if (ImGui::Checkbox(duplicatedFiles[i].filename.c_str(), &isSelected))
            {
                selectedFilesForReplacement[i] = isSelected ? 1 : 0;
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        if (ImGui::Button("Replace Selected", ImVec2(150, 0)))
        {
            if (assetManager)
            {
                for (size_t i = 0; i < duplicatedFiles.size(); ++i)
                {
                    if (selectedFilesForReplacement[i])
                    {
                        assetManager->ConfirmAndReplaceFile(duplicatedFiles[i].fullPath, "");
                    }
                }
                needsAssetRefresh = true;
                assetManager->ClearDuplicatedFileFlag();
                duplicatedFiles.clear();
                selectedFilesForReplacement.clear();
            }

            // open unsupported popup if there are unsupported files
            if (!unsupportedFiles.empty())
            {
                openUnsupportedPopUpNextFrame = true;
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            if (assetManager)
            {
                assetManager->ClearDuplicatedFileFlag();
                duplicatedFiles.clear();
                selectedFilesForReplacement.clear();
            }

            // open unsupported popup if there are unsupported files
            if (!unsupportedFiles.empty())
            {
                openUnsupportedPopUpNextFrame = true;
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    /*std::string fileName = assetManager ? assetManager->GetDuplicateFileName() : "[unknown]";
    ImGui::Text("%s already exists.", fileName.c_str());
    ImGui::Text("Do you want to replace it?");

    ImGui::Separator();

    if (ImGui::Button("Yes", ImVec2(120, 0)))
    {
        if (assetManager) {
            assetManager->ConfirmAndReplaceFile();
            needsAssetRefresh = true;
            assetManager->ClearDuplicatedFileFlag();
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
        if (assetManager) {
            assetManager->ClearDuplicatedFileFlag();
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}*/

}

/*************************************************************************
  \brief
    Renders the popup for unsupported files.
  \details
    Displays a modal listing all unsupported files and the supported
    file types. Provides an "Okay" button to close the popup and
    clear the unsupported file flags.
*************************************************************************/
void AssetManagerNode::RenderUnsupportedFilePopUp()
{
    auto assetManager = GGEngine::GetEngine().GetAssetManager();

    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f)
    );

    if (ImGui::BeginPopupModal("Error!", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("The following files are not supported:");
        ImGui::Separator();

        // display all unsupported files in a scrollable list
        ImGui::BeginChild("UnsupportedFilesList", ImVec2(400, 200), true);
        for (size_t i = 0; i < unsupportedFiles.size(); ++i)
        {
            ImGui::BulletText("%s", unsupportedFiles[i].filename.c_str());
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("Supported file type:");
        ImGui::BulletText(".png, .jpg, .jpeg, .tga (Textures)");
        ImGui::BulletText(".wav, .ogg (Audio)");
        ImGui::BulletText(".lua (Scripts)");
        ImGui::BulletText(".vert, .frag (Shaders)");
        ImGui::BulletText(".prefab (Prefabs)");
        ImGui::BulletText(".scene (Scenes)");
        ImGui::BulletText(".gganim (Animations)");
        ImGui::BulletText(".ttf (Fonts)");
        ImGui::Separator();

        if (ImGui::Button("Okay", ImVec2(120, 0)))
        {
            if (assetManager)
            {
                assetManager->ClearUnsupportedFileFlag();
                unsupportedFiles.clear();
            }
            ImGui::CloseCurrentPopup();
        }


        ImGui::EndPopup();

        /*std::string fileName = assetManager ? assetManager->GetUnsupportedFileName() : "[unknown]";
        ImGui::Text("%s is not supported!", fileName.c_str());
        ImGui::Text(" ");
        ImGui::Text("Current Supported files: .png, .jpg, .jpeg, .tga, .wav, .ogg,");
        ImGui::Text("                         .lua, .vert, .frag, .prefab, .scene.");
        ImGui::Separator();

        if (ImGui::Button("Close", ImVec2(120, 0)))
        {
            if (assetManager)
            {
                assetManager->ClearUnsupportedFileFlag();
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();*/
    }
}

void AssetManagerNode::DeleteAssetWithConfirmation(const std::string& assetPath, const std::string& assetType)
{
    assetToDelete = assetPath;
    assetTypeToDelete = assetType;
    showDeleteConfirmationPopup = true;
}

void AssetManagerNode::RenderDeleteConfirmationPopup()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Delete Asset?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Do you want to delete this asset?");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("%s", assetToDelete.c_str());

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Action cannot be undone!");
        ImGui::Spacing();

        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            auto assetManager = GGEngine::GetEngine().GetAssetManager();
            auto& assetID = GGAssetID::GetGGAssetID();

            if (assetManager->DeleteAsset(assetToDelete))
            {
                //Remove UUID from registry
                assetID->RemoveUUID(assetToDelete);

                // Clean up thumbnail if it exists
                auto thumbIt = thumbnailCache.find(assetToDelete);
                if (thumbIt != thumbnailCache.end())
                {
                    GLuint thumbnail = thumbIt->second;

                    // Check if this is not one of the shared default thumbnails
                    bool isDefaultThumbnail = (
                        thumbnail == defaultTextureThumbnail ||
                        thumbnail == defaultAudioThumbnail ||
                        thumbnail == defaultShaderThumbnail ||
                        thumbnail == defaultPrefabThumbnail ||
                        thumbnail == defaultScriptThumbnail ||
                        thumbnail == defaultSceneThumbnail ||
                        thumbnail == defaultAnimationThumbnail ||
                        thumbnail == defaultTilemapThumbnail ||
                        thumbnail == defaultFontThumbnail
                        );

                    if (!isDefaultThumbnail)
                    {
                        glDeleteTextures(1, &thumbnail);
                    }

                    thumbnailCache.erase(thumbIt);
                }

                // Refresh asset lists
                RefreshAssetLists();

                GGLogger::Info("Asset deleted: " + assetToDelete);
            }
            else
            {
                GGLogger::Error("Failed to delete asset: " + assetToDelete);
            }

            assetToDelete.clear();
            assetTypeToDelete.clear();

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        // cancel button
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            assetToDelete.clear();
            assetTypeToDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
#pragma region AssetsUsed section
/*************************************************************************
 \brief
    Classify a resolved relative asset path into one of the six tracked
    asset-type bucket names by its file extension.

 \param[in]  relativePath  e.g. "Textures/UI/button.png"
 \return     "Textures" | "Audio" | "Shaders" | "Scripts" |
             "Animations" | "Tilemaps" | "" (untracked / not in report)
 *************************************************************************/
static std::string ClassifyAssetPath(const std::string& relativePath)
{
    std::filesystem::path p(relativePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".png" || ext == ".jpg" ||
        ext == ".jpeg" || ext == ".tga")
    {
        return "Textures";
    }
    if (ext == ".wav" || ext == ".mp3" ||
        ext == ".ogg")
    {
        return "Audio";
    }
    if (ext == ".vert" || ext == ".frag" ||
        ext == ".glsl")
    {
        return "Shaders";
    }
    if (ext == ".lua")
    {
        return "Scripts";
    }
    if (ext == ".gganim")
    {
        return "Animations";
    }
    if (ext == ".tilemap")
    {
        return "Tilemaps";
    }
    return "";
}

/*************************************************************************
 \brief
    Try to resolve a UUID string through GGAssetID and, if the resulting
    path is a tracked asset type, insert it (deduplicated) into the map.

 \param[in]  uuidStr   Raw string value extracted from JSON
 \param[out] out       [assetType][subfolder] → [paths]
 *************************************************************************/
static void TryResolveAndInsert(
    const std::string& uuidStr,
    std::map<std::string, std::map<std::string, std::vector<std::string>>>& out)
{
    auto uuidOpt = uuids::uuid::from_string(uuidStr);
    if (!uuidOpt.has_value())
        return;

    auto& registry = GGAssetID::GetGGAssetID();
    if (!registry->DoesUUIDExist(uuidOpt.value()))
        return;

    const std::string& resolvedPath = registry->GetFilePath(uuidOpt.value());
    if (resolvedPath.empty())
        return;

    std::string assetType = ClassifyAssetPath(resolvedPath);
    if (assetType.empty())
        return;

    // Subfolder = parent directory relative to Assets/
    std::filesystem::path fsPath(resolvedPath);
    std::string subfolder = fsPath.parent_path().string();
    std::replace(subfolder.begin(), subfolder.end(), '\\', '/');
    if (subfolder.empty())
        subfolder = assetType;

    // Deduplicate
    auto& vec = out[assetType][subfolder];
    if (std::find(vec.begin(), vec.end(), resolvedPath) == vec.end())
        vec.push_back(resolvedPath);
}

/*************************************************************************
 \brief
    Walk a single component data JSON object and collect every asset UUID
    it contains, using the exact key names written by GGECSGameObjectBridge.

    Known UUID keys and the components that write them:
      "shaderAssetUUID"    - MeshRenderer, UIComponent, WorldTextComponent
      "textureAssetUUID"   - Sprite
      "animationAssetUUID" - SpriteAnimator
      "audioAssetUUID"     - AudioSource
      "scriptAssetUUID"    - ScriptComponent
      "tilemapAssetUUID"   - TilemapComponent
      "fontAssetUUID"      - WorldTextComponent (.ttf, not in report types;
                             TryResolveAndInsert returns "" for it silently)

    ScriptComponent also serializes user-defined textureAssetFields and
    audioAssetFields under arbitrary designer-chosen key names. We detect
    those by scanning every remaining string member of the script object
    and running TryResolveAndInsert - it is a no-op for non-UUID values.

 \param[in]  compObj  The component data object (value inside the wrapper)
 \param[out] out      Target output map to populate
 *************************************************************************/
static void CollectFromComponentObject(
    const rapidjson::Value& compObj,
    std::map<std::string, std::map<std::string, std::vector<std::string>>>& out)
{
    if (!compObj.IsObject())
        return;

    // Fixed well-known UUID keys
    static const char* KNOWN_UUID_KEYS[] = {
        "shaderAssetUUID",
        "textureAssetUUID",
        "animationAssetUUID",
        "audioAssetUUID",
        "scriptAssetUUID",
        "tilemapAssetUUID",
        "fontAssetUUID",    // ClassifyAssetPath returns "" for .ttf -> skipped
    };

    for (const char* key : KNOWN_UUID_KEYS)
    {
        if (compObj.HasMember(key) && compObj[key].IsString())
            TryResolveAndInsert(compObj[key].GetString(), out);
    }

    if (compObj.HasMember("scriptAssetUUID"))
    {
        static const std::unordered_set<std::string> ALREADY_HANDLED = {
            "scriptAssetUUID", "__composedType",
        };

        for (auto it = compObj.MemberBegin(); it != compObj.MemberEnd(); ++it)
        {
            if (!it->value.IsString())
                continue;
            if (ALREADY_HANDLED.count(it->name.GetString()))
                continue;

            TryResolveAndInsert(it->value.GetString(), out);
        }
    }
}

/*************************************************************************
 \brief
    Render the "Assets Used" tab contents.

 \details
    Presents a scrollable list of all discovered .scene files. The user
    selects one and presses "Generate Report". On confirmation the tab
    parses the scene JSON, resolves UUIDs to paths, and writes a .txt
    report into the project's Assets folder. Inline colour-coded feedback
    is shown.
 *************************************************************************/
void AssetManagerNode::RenderAssetsUsedTab()
{
    // Keep the checked-state vector in sync with discoveredScenePaths
    if (assetsUsedCheckedScenes.size() != discoveredScenePaths.size())
    {
        assetsUsedCheckedScenes.assign(discoveredScenePaths.size(), false);
        assetsUsedStatusMessage.clear();
    }

    ImGui::TextUnformatted("Select one or more scene files to generate a combined asset report:");
    ImGui::Separator();

    // select all/ deselect all buttons
    if (ImGui::Button("Select All"))
    {
        std::fill(assetsUsedCheckedScenes.begin(), assetsUsedCheckedScenes.end(), true);
        assetsUsedStatusMessage.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Deselect All"))
    {
        std::fill(assetsUsedCheckedScenes.begin(), assetsUsedCheckedScenes.end(), false);
        assetsUsedStatusMessage.clear();
    }

    ImGui::Spacing();

    // scrollable checkbox list
    ImGui::BeginChild("AssetsUsedSceneList",
        ImVec2(0.0f, 280.0f),
        true,
        ImGuiWindowFlags_None);

    if (discoveredScenePaths.empty())
    {
        ImGui::TextDisabled("No Scene files discovered. Try refreshing assets.");
    }
    else
    {
        for (int i = 0; i < static_cast<int>(discoveredScenePaths.size()); ++i)
        {
            const std::string& scenePath = discoveredScenePaths[i];
            std::string displayName = GetAssetDisplayName(scenePath);   // filename only

            bool checked = assetsUsedCheckedScenes[i];
            if (ImGui::Checkbox(displayName.c_str(), &checked))
            {
                assetsUsedCheckedScenes[i] = checked;
                assetsUsedStatusMessage.clear();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", scenePath.c_str());
        }
    }

    ImGui::EndChild();
    ImGui::Spacing();

    // count selected
    int checkedCount = 0;
    for (bool b : assetsUsedCheckedScenes)
        if (b) ++checkedCount;

    bool canGenerate = (checkedCount > 0);

    if (!canGenerate)
        ImGui::BeginDisabled();

    if (ImGui::Button("Generate Report", ImVec2(140.0f, 0.0f)))
    {
        const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();

        // Merged map: [assetType][subfolder] -> set of filename strings (deduped)
        std::map<std::string, std::map<std::string, std::set<std::string>>> mergedTextureNames;

        // We reuse the existing full map for the report writer, but we need
        // to deduplicate by filename across all selected scenes.
        // Strategy: collect into a set keyed by filename, then convert back.
        std::map<std::string, std::map<std::string, std::set<std::string>>> mergedSets;

        bool anyParseError = false;

        for (int i = 0; i < static_cast<int>(discoveredScenePaths.size()); ++i)
        {
            if (!assetsUsedCheckedScenes[i])
                continue;

            const std::string& relScenePath = discoveredScenePaths[i];
            std::string absScenePath = ASSET_BASE + relScenePath;

            std::map<std::string, std::map<std::string, std::vector<std::string>>> sceneAssets;
            if (!CollectAssetsFromScene(absScenePath, sceneAssets))
            {
                anyParseError = true;
                continue;
            }

            // Merge into the deduplicated set map (key = filename, not full path)
            for (const auto& [assetType, folderMap] : sceneAssets)
            {
                for (const auto& [folder, paths] : folderMap)
                {
                    for (const std::string& path : paths)
                    {
                        std::string filename = std::filesystem::path(path).filename().string();
                        mergedSets[assetType][folder].insert(filename);
                    }
                }
            }
        }

        if (anyParseError && mergedSets.empty())
        {
            assetsUsedStatusMessage = "ERROR: Could not read or parse one or more scene files.";
            assetsUsedStatusIsError = true;
        }
        else
        {
            // Convert merged sets back to the vector form WriteAssetsUsedReport expects.
            // We pass filenames (already deduplicated) as the path list.
            std::map<std::string, std::map<std::string, std::vector<std::string>>> mergedVecs;
            for (const auto& [assetType, folderMap] : mergedSets)
                for (const auto& [folder, nameSet] : folderMap)
                    mergedVecs[assetType][folder] = std::vector<std::string>(nameSet.begin(), nameSet.end());

            if (mergedVecs.empty())
            {
                assetsUsedStatusMessage = "No tracked assets found in the selected scenes.";
                assetsUsedStatusIsError = false;
            }
            else if (!WriteAssetsUsedReport("combinedScenes", mergedVecs))
            {
                assetsUsedStatusMessage = "ERROR: Could not write report file to project Assets/.";
                assetsUsedStatusIsError = true;
            }
            else
            {
                std::string msg = "Report written: combinedScenes.txt";
                if (anyParseError)
                    msg += "  (warning: some scenes could not be parsed)";
                assetsUsedStatusMessage = msg;
                assetsUsedStatusIsError = anyParseError;
            }
        }
    }

    if (!canGenerate)
        ImGui::EndDisabled();

    // status feedback
    if (!assetsUsedStatusMessage.empty())
    {
        ImGui::SameLine();
        if (assetsUsedStatusIsError)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", assetsUsedStatusMessage.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                "%s", assetsUsedStatusMessage.c_str());
        }
    }
}

/*************************************************************************
 \brief
    Parse a .scene JSON file and collect every asset path it references.

 \details
    For each entity's component array, we locate the component data object
    (the member whose value is an object - the one that is NOT "componentID")
    and pass it to CollectFromComponentObject() which looks up the exact
    UUID key names produced by GGECSGameObjectBridge serializers.

    Results are bucketed by [assetType][subfolder] and deduplicated.
    Each inner vector is sorted alphabetically for deterministic output.
 *************************************************************************/
bool AssetManagerNode::CollectAssetsFromScene(
    const std::string& sceneFilePath,
    std::map<std::string, std::map<std::string, std::vector<std::string>>>& outAssetsByTypeAndFolder)
{
    std::ifstream file(sceneFilePath);
    if (!file.is_open())
    {
        std::cout << "AssetsUsed: could not open scene file: " << sceneFilePath;
        return false;
    }

    std::string contents((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    rapidjson::Document doc;
    doc.Parse(contents.c_str());

    if (doc.HasParseError() || !doc.IsObject())
    {
        std::cout << "AssetsUsed: failed to parse JSON in: " << sceneFilePath;
        return false;
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray())
    {
        std::cout << "AssetsUsed: scene JSON missing 'entities' array: " << sceneFilePath;
        return false;
    }

    for (const auto& entity : doc["entities"].GetArray())
    {
        if (!entity.IsObject())
            continue;
        if (!entity.HasMember("components") || !entity["components"].IsArray())
            continue;

        for (const auto& compWrapper : entity["components"].GetArray())
        {
            // compWrapper looks like:
            //   { "componentID": 5, "Sprite": { "textureAssetUUID": "...", ... } }
            // We want the member whose value is an object (not the uint componentID).
            if (!compWrapper.IsObject())
                continue;

            for (auto memberIt = compWrapper.MemberBegin();
                memberIt != compWrapper.MemberEnd(); ++memberIt)
            {
                if (!memberIt->value.IsObject())
                    continue;   // skip "componentID" and any other non-objects

                CollectFromComponentObject(memberIt->value, outAssetsByTypeAndFolder);
            }
        }
    }

    // Sort each path list alphabetically for deterministic output
    for (auto& [type, folderMap] : outAssetsByTypeAndFolder)
        for (auto& [folder, paths] : folderMap)
            std::sort(paths.begin(), paths.end());

    return true;
}

/*************************************************************************
 \brief
    Write the collected asset report to <projectAssets>/<sceneName>.txt.

 \details
    Asset types appear in a fixed order: Textures, Audio, Shaders,
    Scripts, Animations, Tilemaps. Folders within each type are sorted
    alphabetically (by std::map key ordering). An existing file is
    silently overwritten.

    Uses the live project asset base path so the report always lands
    inside the currently open project, not a hardcoded relative path.
 *************************************************************************/
bool AssetManagerNode::WriteAssetsUsedReport(
    const std::string& sceneName,
    const std::map<std::string, std::map<std::string, std::vector<std::string>>>& assetsByTypeAndFolder)
{
    // Use the live project asset base path instead of hardcoded relative path
    const std::string ASSET_BASE = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
    std::string outputPath = ASSET_BASE + sceneName + ".txt";

    std::ofstream out(outputPath);
    if (!out.is_open())
    {
        std::cout << "AssetsUsed: could not create report file: " << outputPath;
        return false;
    }

    // header
    const std::string divider(64, '=');
    out << divider << "\n";
    out << "Assets Used Report: " << sceneName << "\n";
    out << divider << "\n\n";

    // body - fixed display order
    const std::vector<std::string> TYPE_ORDER = {
        "Textures", "Audio", "Shaders", "Scripts", "Animations", "Tilemaps"
    };

    int totalAssets = 0;

    for (const std::string& assetType : TYPE_ORDER)
    {
        auto typeIt = assetsByTypeAndFolder.find(assetType);
        if (typeIt == assetsByTypeAndFolder.end())
            continue;

        const auto& folderMap = typeIt->second;

        int typeCount = 0;
        for (const auto& [folder, paths] : folderMap)
            typeCount += static_cast<int>(paths.size());

        out << "[" << assetType << "]"
            << "  (" << typeCount
            << (typeCount == 1 ? " asset)" : " assets)") << "\n";

        for (const auto& [folder, paths] : folderMap)
        {
            out << "  Folder: " << folder << "\n";
            for (const std::string& path : paths)
                out << "    - " << std::filesystem::path(path).filename().string() << "\n";
            out << "\n";
        }

        out << "\n";
        totalAssets += typeCount;
    }

    // footer
    out << divider << "\n";
    out << "Total assets referenced: " << totalAssets << "\n";
    out << divider << "\n";

    out.close();
    std::cout << "AssetsUsed: report written to " << outputPath;
    return true;
}
#pragma endregion