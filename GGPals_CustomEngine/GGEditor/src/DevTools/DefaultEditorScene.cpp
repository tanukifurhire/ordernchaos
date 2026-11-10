/*************************************************************************
  \file       DefaultEditorScene.cpp
  \author     Tai Xiao Xuan, Zafir Haqqym
  \par        Email: xiaoxuan.tai@digipen.edu, zafir.h@digipen.edu,
  \date       1 October 2025
  \version    1.7
  \brief
    Implementation of the DefaultEditorScene class.
  \details
    Provides the concrete implementation for the default editor scene,
    including Dear ImGui integration for development tools interface.
    Implements physics simulation controls, performance monitoring, and
    visual debugging utilities for the GGEngine development environment.
  \dependencies
    DefaultEditorScene.hpp, GGSystemManagers.hpp, Dear ImGui headers,
    GGLogger.hpp
  \see
    DefaultEditorScene, EditorSceneBase, Systems::SimulationManager,
    GGAssetManager, GGLogger, GGPerformanceProfiler.hpp
  \history
    - 1.0 (26 September 2025): Initial implementation.
    - 1.1 (27 September 2025): Added basic Dev Tools window structure with
      tabbed interface and functional console integration using GGLogger.
      Includes colour coded message display and auto scroll functionality.
    - 1.2 (27 September 2025): Added physics controls tab with manual stepping,
      entity monitoring.
    - 1.3 (27 September 2025): Added audio controls tab with playback management.
    - 1.4 (27 September 2025): Added complete resource browser with asset discovery
      system, on-demand loading, and load status indicators.
    - 1.5 (30 September 2025): Added performance profiler tab with real-time system
      timing analysis, rolling average calculations, configurable sample window,
      color-coded performance indicators, and accounted/unaccounted time tracking.
      Integrated with centralized PerformanceProfiler singleton for accurate frame
      budget monitoring.
    - 1.6 (30 September 2025): Added Frame Rate tab with configurable frame
      cap slider. Frame limiting always active for consistent network timing
      in windowed multiplayer games.
    - 1.7 (1 October 2025): Added Font Test tab with FontAtlas integration
      testing. Includes static text rendering, dynamic FPS counter with
      window-relative positioning, and interactive custom text controls with
      position, scale, and color adjustments. FPS counter dynamically adjusts
      to window size for consistent top-right placement.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <DefaultEditorScene.hpp>
#include <GGSystemManagers.hpp>
#include <GGEngine.hpp>
#include <GGPerformanceProfiler.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGPhysics.hpp>
#include <GGECSGameObjectBridge.hpp>
#include <GameClient.hpp>
#include <EditorEventBus.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <EditorViewNode.hpp>
#include <GameViewNode.hpp>

/*************************************************************************
  \brief
    Constructor implementation configuring callback system
  \details
    Initializes the callback handles using lambda expressions that properly
    dispatch to the scene's virtual methods. This enables the EditorManager
    to invoke scene methods through the event system without direct coupling.
*************************************************************************/
DefaultEditorScene::DefaultEditorScene()
{
    initCallbackHandle = [this]() -> void { this->Init(); };
    updateCallbackHandle = [this]() -> void { this->Update(); };
    dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
    Scene initialization implementation
  \details
    Subscribes to asset selection events from AssetManagerNode to enable
    audio playback integration with the Asset Manager. When an audio asset
    is selected, updates the selectedAudioPath for Audio Controls.
*************************************************************************/
void DefaultEditorScene::Init()
{
    // Create callback that handles audio asset selection
    audioAssetSelectedCallback = [this](uint32_t& assetID, std::string& path)
        {
            selectedAudioPath = path;
            GGLogger::Info("Audio Controls: Selected audio " + selectedAudioPath + " (ID: " + std::to_string(assetID) + ")");
        };

    // Subscribe to audio-specific asset selection events
    EditorEventBus::onAudioAssetSelected += audioAssetSelectedCallback;

    // Future initialization code here
}

/*************************************************************************
  \brief
    Dock build implementation
  \details
    Performs one-time setup for the dock building for the dockable interface
    of ImGui.
  \note
    Called automatically through the callback system during the first update
    frame.
*************************************************************************/
void DefaultEditorScene::DockBuild()
{
    ImGui::DockBuilderDockWindow("Dev Tools", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockLeft);
}

/*************************************************************************
  \brief
    Per-frame update and ImGui rendering implementation
  \details
    Implements development tools interface including:
    - Resource browser showing loaded assets
    - Audio controls for playback management
    - Physics manual stepping controls
    - Performance profiler showing system timing data
    - Console output for debug messages

    Uses persistent UI state stored in member variables to maintain
    interface state between frames.
*************************************************************************/
void DefaultEditorScene::Update()
{
    // Refresh asset lists if needed
    if (needsAssetRefresh)
    {
        RefreshAssetLists();
        needsAssetRefresh = false;
    }

    // Force window to specific size and prevent auto-stretching
    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

    [[maybe_unused]] auto& sceneManager = GGEngine::GetEngine().GetSceneManager();

    // Single development tools window
    if (ImGui::Begin("Dev Tools", nullptr, ImGuiWindowFlags_NoResize))
    {
        // Upper section: Tabbed tools
        if (ImGui::BeginTabBar("DevToolsTabs"))
        {
            if (ImGui::BeginTabItem("Frame Rate"))
            {
                RenderFrameRateSettings();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Performance"))
            {
                RenderPerformanceProfiler();
                ImGui::EndTabItem();
            }

            /*if (ImGui::BeginTabItem("Resource Browser"))
            {
                RenderResourceBrowser();
                ImGui::EndTabItem();
            }*/

            if (ImGui::BeginTabItem("Audio Controls"))
            {
                RenderAudioControls();
                ImGui::EndTabItem();
            }

            /*if (ImGui::BeginTabItem("Physics"))
            {
                sceneManager.SetActiveScene("PhysicsScene");
                RenderPhysicsControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Entity Spawning"))
            {
                sceneManager.SetActiveScene("EntitySpawningScene");
                RenderEntitySpawningControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Prefab Editor"))
            {
                sceneManager.SetActiveScene("PrefabEditorWorld");
                RenderPrefabEditingControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Scene Management"))
            {
                // scene management test handles its own scene switching logic!
                // do not add any SetActiveScene here
                RenderSceneManagementControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Instancing"))
            {
                sceneManager.SetActiveScene("InstancingScene");
                RenderInstancingControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Networking"))
            {
                sceneManager.SetActiveScene("NetworkingScene");
                RenderNetworkingTestControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Font Atlas"))
            {
                sceneManager.SetActiveScene("FontAtlasScene");
                RenderFontTestControls();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Crash Log"))
            {
                sceneManager.SetActiveScene("CrashLogScene");
                RenderCrashLogging();
                ImGui::EndTabItem();
            }*/

            /*if (ImGui::BeginTabItem("Background"))
            {
                sceneManager.SetActiveScene("BackgroundScene");
                BackgroundSpriteScene();
                ImGui::EndTabItem();
            }*/

            //temp
            /*if (ImGui::BeginTabItem("picking"))
            {
                PickingFrameBuffer();
                ImGui::EndTabItem();
            }*/

            if (ImGui::BeginTabItem("CameraSettings"))
            {
                CameraSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GizmoSettings"))
            {
                GizmoSettings();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // Separator between tabs and console
        ImGui::Separator();
        ImGui::Spacing();

        // Lower section: Console (not in a tab)
        ImGui::Text("===[CONSOLE]===");
        ImGui::Separator();
        RenderConsole();
    }
    ImGui::End();

    // Render font test overlays to game screen
    RenderFontTestOverlays();
}

/*************************************************************************
  \brief
    Refresh cached asset lists from asset manager
  \details
    Updates the cached lists of loaded assets and discovers available assets
    on disk for display in the resource browser. Called when needsAssetRefresh
    flag is set or when user clicks refresh button.
*************************************************************************/
void DefaultEditorScene::RefreshAssetLists()
{
    auto* assetManager = GGEngine::GetEngine().GetAssetManager();
    if (assetManager)
    {
        // Scan for new files in directories first
        assetManager->ScanAssetDirectories();

        // Get loaded assets (in memory)
        cachedTexturePaths = assetManager->GetLoadedTexturePaths();
        cachedAudioPaths = assetManager->GetLoadedAudioPaths();
        cachedShaderPaths = assetManager->GetLoadedShaderPaths();

        

        // Get discovered assets (on disk)
        discoveredAudioPaths = assetManager->GetDiscoveredAudioPaths();

        GGLogger::Info("Asset lists refreshed - Found " +
            std::to_string(discoveredAudioPaths.size()) + " audio files");
    }
    else
    {
        GGLogger::Error("Asset manager not available for refresh");
    }
}

/*************************************************************************
  \brief
    Render the resource browser interface
  \details
    Displays lists of discovered and loaded assets with status indicators.
    Allows selection and provides refresh functionality. Supports on-demand
    loading for discovered assets.
*************************************************************************/
void DefaultEditorScene::RenderResourceBrowser()
{
    ImGui::Text("===[ASSET BROWSER]===");

    // Refresh button
    if (ImGui::Button("Refresh Asset Lists"))
    {
        needsAssetRefresh = true;
        GGLogger::Info("Asset refresh requested");
    }

    ImGui::Separator();

    // Textures section
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (cachedTexturePaths.empty())
        {
            ImGui::Text("No textures loaded");
        }
        else
        {
            for (size_t i = 0; i < cachedTexturePaths.size(); ++i)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED] ");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                bool isSelected = (selectedTextureIndex == static_cast<int>(i));
                if (ImGui::Selectable(cachedTexturePaths[i].c_str(), isSelected))
                {
                    selectedTextureIndex = static_cast<int>(i);
                    // Clear other selections
                    selectedAudioIndex = -1;
                    selectedShaderIndex = -1;
                }
            }
        }
    }

    // Audio files section - show discovered files with load status
    if (ImGui::CollapsingHeader("Audio Files", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (discoveredAudioPaths.empty())
        {
            ImGui::Text("No audio files found - try clicking Refresh");
        }
        else
        {
            for (size_t i = 0; i < discoveredAudioPaths.size(); ++i)
            {
                const std::string& path = discoveredAudioPaths[i];
                bool isLoaded = std::find(cachedAudioPaths.begin(), cachedAudioPaths.end(), path) != cachedAudioPaths.end();

                // Show load status
                if (isLoaded)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                    ImGui::Text("[LOADED] ");
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray for discovered
                    ImGui::Text("[DISK]   ");
                }

                ImGui::SameLine();
                bool isSelected = (selectedAudioIndex == static_cast<int>(i));
                if (ImGui::Selectable(path.c_str(), isSelected))
                {
                    selectedAudioIndex = static_cast<int>(i);
                    selectedAudioPath = path;

                    // Load on selection if not already loaded
                    if (!isLoaded)
                    {
                        try
                        {
                            GGEngine::GetEngine().GetAssetManager()->GetAudioAsset(path);
                            GGLogger::Info("Loaded audio asset: " + path);
                            // Refresh the cached list to show new loaded status
                            needsAssetRefresh = true;
                        }
                        catch (const std::exception& e)
                        {
                            GGLogger::Error("Failed to load: " + path + " - " + e.what());
                        }
                    }
                    else
                    {
                        GGLogger::Info("Selected loaded audio: " + path);
                    }

                    // Clear other selections
                    selectedTextureIndex = -1;
                    selectedShaderIndex = -1;
                }

                ImGui::PopStyleColor();
            }
        }
    }

    // Shaders section
    if (ImGui::CollapsingHeader("Shaders", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (cachedShaderPaths.empty())
        {
            ImGui::Text("No shaders loaded");
        }
        else
        {
            for (size_t i = 0; i < cachedShaderPaths.size(); ++i)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for loaded
                ImGui::Text("[LOADED] ");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                bool isSelected = (selectedShaderIndex == static_cast<int>(i));
                if (ImGui::Selectable(cachedShaderPaths[i].c_str(), isSelected))
                {
                    selectedShaderIndex = static_cast<int>(i);
                    // Clear other selections
                    selectedTextureIndex = -1;
                    selectedAudioIndex = -1;
                }
            }
        }
    }

    // Asset statistics
    ImGui::Separator();
    ImGui::Text("Loaded Assets: %zu textures, %zu audio, %zu shaders",
        cachedTexturePaths.size(),
        cachedAudioPaths.size(),
        cachedShaderPaths.size());
    ImGui::Text("Discovered Audio: %zu files", discoveredAudioPaths.size());
}

/*************************************************************************
  \brief
    Render the audio controls interface
  \details
    Provides play, pause, stop controls for audio playback.
    Shows currently playing audio and master volume control.
    Integrates with selected audio from resource browser.
*************************************************************************/
void DefaultEditorScene::RenderAudioControls()
{
    auto* audioSystem = GGEngine::GetEngine().GetAudioSystem();
    if (!audioSystem)
    {
        ImGui::Text("Audio system not available");
        return;
    }

    ImGui::Text("===[AUDIO PLAYBACK CONTROLS]===");

    // Audio system status
    ImGui::Text("FMOD Status: %s", audioSystem->IsInitialized() ? "READY" : "NOT INITIALIZED");
    ImGui::Text("Active Sounds: %zu", audioSystem->GetActiveSoundCount());

    ImGui::Separator();

    // Audio file selection
    ImGui::Text("Selected Audio: %s", selectedAudioPath.empty() ? "None - Select from Resource Browser" : selectedAudioPath.c_str());

    // Playback controls
    ImGui::BeginDisabled(selectedAudioPath.empty());

    if (ImGui::Button("Play"))
    {
        AudioResult result = audioSystem->PlayEditorAudioPreview(selectedAudioPath);
        if (result == AudioResult::SUCCESS)
        {
            GGLogger::Info("Playing audio: " + selectedAudioPath);
        }
        else
        {
            GGLogger::Error("Failed to play audio: " + selectedAudioPath + " - " + audioSystem->GetResultString(result));
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop All"))
    {
        AudioResult result = audioSystem->StopAllSounds();
        if (result == AudioResult::SUCCESS)
        {
            GGLogger::Info("Stopped all audio playback");
        }
        else
        {
            GGLogger::Error("Failed to stop audio: " + audioSystem->GetResultString(result));
        }
    }

    ImGui::EndDisabled();

    // Master volume control
    float currentVolume = audioSystem->GetMasterVolume();
    if (ImGui::SliderFloat("Master Volume", &currentVolume, 0.0f, 1.0f))
    {
        AudioResult result = audioSystem->SetMasterVolume(currentVolume);
        if (result != AudioResult::SUCCESS)
        {
            GGLogger::Error("Failed to set master volume: " + audioSystem->GetResultString(result));
        }
    }

    ImGui::Separator();

    // Currently playing audio
    ImGui::Text("===[CURRENTLY PLAYING]===");
    auto playingAudio = audioSystem->GetCurrentlyPreviewingAudio();
    //auto playingAudio = audioSystem->GetCurrentlyPreviewingAudio();

    if (playingAudio.empty())
    {
        ImGui::Text("No audio currently playing");
    }
    else
    {
        for (const auto& playing : playingAudio)
        {
            ImGui::Text("Playing: %s", playing.assetPath.c_str());
        }
    }
}

/*************************************************************************
  \brief
    Render the physics controls interface
  \details
    Provides manual physics stepping controls and displays physics
    system status and entity information.
*************************************************************************/
void DefaultEditorScene::RenderPhysicsControls()
{
    ImGui::Text("===[PHYSICS SYSTEM CONTROLS]===");

    auto& simManager = Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>();

    // Manual step mode toggle
    bool manualMode = simManager.ManualStepMode();
    if (ImGui::Checkbox("Manual Step Mode", &manualMode))
    {
        simManager.ManualStepMode() = manualMode;
        GGLogger::Info(manualMode ? "Physics: Manual step mode enabled" : "Physics: Real-time mode enabled");
    }

    // Step button (only enabled in manual mode)
    ImGui::BeginDisabled(!manualMode);
    if (ImGui::Button("Step Physics"))
    {
        simManager.ShouldStepThisFrame() = true;
        GGLogger::Info("Physics: Manual step executed");
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // Physics system information
    ImGui::Text("===[SYSTEM STATUS]===");
    ImGui::Text("Mode: %s", manualMode ? "MANUAL" : "REAL-TIME");

    // Entity count from test world
    auto& physicsWorld = GGEngine::GetEngine().GetSceneManager().GetActiveScene();
    ImGui::Text("Active Entities: %zu", physicsWorld.GetLivingEntities().size());

    // Performance information
    ImGui::Text("Frame Rate: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

    static Entity parent = NULL_ENTITY;
    static Entity child = NULL_ENTITY;
    auto& world = GGEngine::GetEngine().GetSceneManager().GetActiveScene();
    auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();

    hierarchySystem.Update(world);
    if (ImGui::Button("Spawn Hierarchy Test"))
    {
        parent = world.CreateEntity();
        child = world.CreateEntity();

        // Add components
        world.AddComponent<LocalTransform>(parent, LocalTransform());
        world.AddComponent<WorldTransform>(parent, WorldTransform());
        world.AddComponent<Rigidbody2DFloat>(parent, Rigidbody2DFloat());
        world.AddComponent<AABBFloat>(parent, AABBFloat());
        world.AddComponent<ScreenBounds>(parent, ScreenBounds());
        world.AddComponent<MeshRenderer>(parent, MeshRenderer());
        world.AddComponent<Sprite>(parent, Sprite());
        world.AddComponent<SpriteAnimator>(parent, SpriteAnimator());

        world.GetComponent<Rigidbody2DFloat>(parent)->vx = 0;
        world.GetComponent<Rigidbody2DFloat>(parent)->vy = 0;
        world.GetComponent<LocalTransform>(parent)->scale.x = 1.f;
        world.GetComponent<LocalTransform>(parent)->scale.y = 1.f;
        
        world.AddComponent<LocalTransform>(child, LocalTransform());
        world.AddComponent<WorldTransform>(child, WorldTransform());
        world.AddComponent<Rigidbody2DFloat>(child, Rigidbody2DFloat());
        world.AddComponent<AABBFloat>(child, AABBFloat());
        world.AddComponent<ScreenBounds>(child, ScreenBounds());
        world.AddComponent<MeshRenderer>(child, MeshRenderer());
        world.AddComponent<Sprite>(child, Sprite());
        world.AddComponent<SpriteAnimator>(child, SpriteAnimator());

        // Child starts 100 units to the right of parent
        world.GetComponent<Rigidbody2DFloat>(child)->vx = 0;
        world.GetComponent<Rigidbody2DFloat>(child)->vy = 0;
        world.GetComponent<LocalTransform>(child)->scale.x = 1.f;
        world.GetComponent<LocalTransform>(child)->scale.y = 1.f;
        world.GetComponent<LocalTransform>(child)->position = { 100.f, 0.f };

        // Link them
        hierarchySystem.SetParent(world, child, parent);
        hierarchySystem.Update(world);

        // Assign animation via uuid
        auto& animSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();

        uint32_t animID = animSystem.RegisterAnimationByPath("Animations/test_anim.gganim");
        uint32_t anim2ID = animSystem.RegisterAnimationByPath("Animations/test_anim2.gganim");

        // Assign animation to parent
        auto* parentAnimator = world.GetComponent<SpriteAnimator>(parent);
        parentAnimator->animationAssetID = animID;
        parentAnimator->currentAnimState = 0;
        parentAnimator->currentFrameIndex = 0;

        // Assign animation 2 to child
        auto* childAnimator = world.GetComponent<SpriteAnimator>(child);
        childAnimator->animationAssetID = anim2ID;
        childAnimator->currentAnimState = 0;
        childAnimator->currentFrameIndex = 0;

        GGLogger::Info("Parent & child created, linked, and animation assigned!");
    }

    if (parent != NULL_ENTITY && child != NULL_ENTITY)
    {
        // --- Local Transform editing (Unity-style) ---
        auto* parentLocal = world.GetComponent<LocalTransform>(parent);

        ImGui::Separator();
        ImGui::Text("Parent Local Transform (Edit Me)");
        bool moved = false;
        moved |= ImGui::DragFloat2("Local Position", &parentLocal->position.x, 1.0f);
        // Convert stored radians to degrees for display
        float rotDeg = parentLocal->rotation * GGMath::RADIANS_TO_DEGREES;

        // ImGui shows degrees instead of radians
        if (ImGui::DragFloat("Local Rotation (°)", &rotDeg, 1.0f, -360.0f, 360.0f))
        {
            // Convert degrees back to radians after editing
            parentLocal->rotation = rotDeg * GGMath::DEGREES_TO_RADIANS;
            moved = true;
        }
        moved |= ImGui::DragFloat2("Local Scale", &parentLocal->scale.x, 0.01f, 0.1f, 5.0f);

        if (moved)
            parentLocal->isDirty = true;

        // --- Apply Hierarchy Update ---
        hierarchySystem.Update(world);

        // --- Display computed results ---
        auto parentWorldPos = world.GetComponent<WorldTransform>(parent)->matrix.getTranslation();
        auto childWorldPos = world.GetComponent<WorldTransform>(child)->matrix.getTranslation();

        ImGui::Separator();
        ImGui::Text("Parent World (Computed): (%.2f, %.2f)", parentWorldPos.x, parentWorldPos.y);
        ImGui::Text("Child  World (Computed): (%.2f, %.2f)", childWorldPos.x, childWorldPos.y);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
            "(Drag Parent Local sliders — Child follows automatically!)");

        // --- Optional: world-space override slider ---
        static GGMath::Vector2 overrideWorld = { 0.f, 0.f };
        ImGui::Separator();
        ImGui::Text("Parent World Override (experimental):");
        if (ImGui::DragFloat2("World Position Override", &overrideWorld.x, 1.0f))
        {
            auto oldWorld = world.GetComponent<WorldTransform>(parent)->matrix.getTranslation();
            GGMath::Vector2 delta = overrideWorld - oldWorld;
            parentLocal->position += delta; // convert world change into local offset
            parentLocal->isDirty = true;
        }
    }


    if (ImGui::Button("Spawn Random Blocks"))
    {
        for (int j = 0; j < 8; ++j)  // spawn ? blocks
        {
            Entity block = physicsWorld.CreateEntity();

            // Add components
            physicsWorld.AddComponent<LocalTransform>(block, LocalTransform());
            physicsWorld.AddComponent<WorldTransform>(block, WorldTransform());
            physicsWorld.AddComponent<Rigidbody2DFloat>(block, Rigidbody2DFloat());
            physicsWorld.AddComponent<AABBFloat>(block, AABBFloat());
            physicsWorld.AddComponent<ScreenBounds>(block, ScreenBounds());
            physicsWorld.AddComponent<MeshRenderer>(block, MeshRenderer());

            
            int screenW = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowWidth();
            int screenH = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowHeight();

            
            float halfW = screenW * 0.5f;
            float halfH = screenH * 0.5f;

            float x = -halfW * 0.8f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (halfW * 1.6f);
            float y = -halfH * 0.8f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (halfH * 1.6f);
            physicsWorld.GetComponent<LocalTransform>(block)->position.x = x;
            physicsWorld.GetComponent<LocalTransform>(block)->position.y = y;
            physicsWorld.GetComponent<LocalTransform>(block)->scale.x = 1.f;
            physicsWorld.GetComponent<LocalTransform>(block)->scale.y = 1.f;


            float vx = -150.f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 200.f));
            float vy = -150.f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 200.f));
            physicsWorld.GetComponent<Rigidbody2DFloat>(block)->vx = vx;
            physicsWorld.GetComponent<Rigidbody2DFloat>(block)->vy = vy;
            physicsWorld.GetComponent<Rigidbody2DFloat>(block)->mass = 1.0f;
        }
    }

    if (ImGui::Button("Reset Rubric Log Flags")) {
        GGPhysics::ResetRubricLogFlags();
        GGLogger::Info("Rubric flags reset next observed events will be logged again.");
    }
}

/*************************************************************************
  \brief
    Render the console output interface
  \details
    Displays engine log messages with auto-scroll functionality.
    Provides clear button and message filtering capabilities with
    color-coded output for different message types.
*************************************************************************/
void DefaultEditorScene::RenderConsole()
{
    // Console controls
    if (ImGui::Button("Clear Console"))
    {
        GGLogger::Clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScrollConsole);

    ImGui::Separator();

    // Console output area
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

    const auto& messages = GGLogger::GetMessages();
    for (const auto& message : messages)
    {
        // Color code messages based on type
        if (message.find("[ERROR]") != std::string::npos)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
        }
        else if (message.find("[WARNING]") != std::string::npos)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Yellow
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f)); // Light gray
        }

        ImGui::TextWrapped("%s", message.c_str());
        ImGui::PopStyleColor();
    }

    // Auto-scroll to bottom
    if (autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::Text("Messages: %zu", messages.size());
}

/*************************************************************************
  \brief
    Render the performance profiler interface
  \details
    Displays real-time performance data for all registered systems
    including average frame time, percentage of total frame, and
    configurable display options. Provides controls for enabling/disabling
    profiling, clearing data, and sorting options.
*************************************************************************/
void DefaultEditorScene::RenderPerformanceProfiler()
{
    auto& profiler = Systems::PerformanceProfiler::GetInstance();

    ImGui::Text("===[PERFORMANCE PROFILER]===");
    ImGui::Separator();

    // Profiler controls
    bool enabled = profiler.IsEnabled();
    if (ImGui::Checkbox("Enable Profiling", &enabled))
    {
        profiler.SetEnabled(enabled);
        GGLogger::Info(enabled ? "Performance profiling enabled" : "Performance profiling disabled");
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Data"))
    {
        profiler.Clear();
        GGLogger::Info("Performance data cleared");
    }

    // Sample count control - adjusts rolling average window size
    // Lower values (1-30): responsive but jittery data
    // Higher values (60-120): smooth data but slower to detect performance changes
    int maxFrames = static_cast<int>(profiler.GetMaxSamples());
    if (ImGui::SliderInt("Sample Window", &maxFrames, 1, 120))
    {
        profiler.SetMaxSamples(static_cast<size_t>(maxFrames));
    }
    ImGui::Text("(Number of frames to average)");

    ImGui::Separator();

    // Sort control
    ImGui::Text("Sort By:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Time", sortByTime))
    {
        sortByTime = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Name", !sortByTime))
    {
        sortByTime = false;
    }

    ImGui::Separator();

    // Frame statistics
    double totalFrameTime = profiler.GetTotalFrameTime();

    ImGui::Text("Total Frame Time: %.4f ms", totalFrameTime * 1000.0);
    ImGui::Separator();

    // System performance data
    ImGui::BeginChild("PerformanceData", ImVec2(0, 180), true);

    auto sortedData = profiler.GetSortedData(sortByTime);

    if (sortedData.empty())
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No performance data collected yet.");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Data will appear after first frame update.");
    }
    else
    {
        // Table header
        ImGui::Columns(4, "perfColumns");
        ImGui::Separator();
        ImGui::Text("System Name");
        ImGui::NextColumn();
        ImGui::Text("Avg Time (ms)");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Total Frame %");
        ImGui::NextColumn();
        ImGui::Text("Frames");
        ImGui::NextColumn();
        ImGui::Separator();

        // System rows
        for (auto const& data : sortedData)
        {
            // Color code based on percentage
            ImVec4 color;
            if (data.percentage > 10.0)
            {
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red - high usage
            }
            else if (data.percentage > 5.0)
            {
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);  // Yellow - moderate usage
            }
            else if (data.percentage > 1.0)
            {
                color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);  // Light green - normal usage
            }
            else
            {
                color = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);  // Dark green - low usage
            }

            // System name (clean up typeid names)
            std::string cleanName = data.name;
            // Remove "class " prefix if present (MSVC)
            size_t classPos = cleanName.find("class ");
            if (classPos != std::string::npos)
            {
                cleanName = cleanName.substr(classPos + 6);
            }

            ImGui::TextColored(color, "%s", cleanName.c_str());
            ImGui::NextColumn();

            // Average time
            ImGui::TextColored(color, "%.4f", data.avgTime * 1000.0);
            ImGui::NextColumn();

            // Percentage with bar
            ImGui::TextColored(color, "%.2f%%", data.percentage);
            ImGui::NextColumn();

            // Sample count
            ImGui::Text("%zu", data.sampleCount);
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
    }

    ImGui::EndChild();

    // Summary statistics
    ImGui::Separator();
    ImGui::Text("Total Systems Tracked: %zu", sortedData.size());

    if (!sortedData.empty())
    {
        // Calculate total percentage (should be around 100%)
        // Accounted Time: sum of all measured system percentages
        // Should be close to 100% which represents profiled CPU work
        double totalPercentage = 0.0;
        for (auto const& data : sortedData)
        {
            totalPercentage += data.percentage;
        }

        ImGui::Text("Accounted Time: %.2f%%", totalPercentage);

        // Always show unaccounted time (moved below, no conditional)
        // Unaccounted Time: gap between measured systems and total frame time
        // Includes: OS overhead, driver calls, unmeasured engine code, measurement gaps
        double unaccounted = 100.0 - totalPercentage;
        ImGui::Text("Unaccounted Time: %.2f%%", unaccounted);
    }
}

/*************************************************************************
  \brief
    Render frame rate settings controls
  \details
    Provides frame rate cap slider for manual frame limiting.
    Frame cap is always active (VSync unreliable in windowed mode).

    Rationale for configurable frame cap:
    1. Network consistency - Adjustable tick rates for different connection qualities
    2. Hardware compatibility - Lower-end systems may need reduced frame rates
    3. Development testing - Verify game logic works correctly at various frame rates
    4. Performance debugging - Identify frame-rate dependent bugs
    5. Power management - Lower FPS extends laptop battery life
*************************************************************************/
void DefaultEditorScene::RenderFrameRateSettings()
{
    ImGui::Text("===[GRAPHICS SETTINGS]===");
    ImGui::Separator();

    auto& timeManager = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();

    // Frame rate cap slider (always active)
    // Range: 30-240 FPS covers common use cases from low-end to high-refresh monitors
    int frameCap = static_cast<int>(timeManager.GetTargetFPS());
    if (ImGui::SliderInt("Frame Rate Cap", &frameCap, 30, 240))
    {
        timeManager.SetTargetFPS(static_cast<uint32_t>(frameCap));
        GGLogger::Info("Frame rate cap: " + std::to_string(frameCap) + " FPS");
    }

    ImGui::Separator();

    // Status display - shows target vs actual FPS for monitoring frame limiter accuracy
    ImGui::Text("Mode: Manual Frame Cap");
    ImGui::Text("Target FPS: %d", frameCap);
    ImGui::Text("Actual FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

/*************************************************************************
  \brief
    Render entity spawning controls
  \details
    Provides an ImGui interface for spawning and destroying entities in the
    active scene. Supports:
    - Loading prefabs and entity types once.
    - Specifying spawn position and scale.
    - Selecting from loaded prefabs to spawn instances.
    - Destroying individual or all spawned entities.
    - Toggling debug draw flags (points, lines, AABBs, circles, squares).
*************************************************************************/
void DefaultEditorScene::RenderEntitySpawningControls() {
    auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
    auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();

    ImGui::Text("===[ENTITY SPAWNING TEST]===");
    ImGui::Separator();

    auto& prefabWorld = GGEngine::GetEngine().GetSceneManager().GetActiveScene();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.3f, 0.0f, 1.0f)); // dark green
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f)); // slightly lighter
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));  // even lighter
    if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float inputWidth{ 120.f };
        static float xPos{ 0.f }, yPos{ 0.f };
        static float scale{ 1.f };

        ImGui::Text("Position: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputFloat("xPos", &xPos);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputFloat("yPos", &yPos);
        ImGui::SetNextItemWidth(inputWidth);

        ImGui::Separator();

        ImGui::Text("Scale: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputFloat("", &scale);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Loaded Prefab Types", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (prefabManager.PrefabCount() == 0) {
                ImGui::Text("No prefabs loaded");
            }
            else {
                int i{ 0 };
                for (auto& [key, prefab] : prefabManager.GetPrefabs()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));
                    ImGui::Text("[LOADED] ");
                    ImGui::PopStyleColor();

                    ImGui::SameLine();

                    bool isSelected = (selectedSpawnPrefabIndex == static_cast<int>(i));
                    if (ImGui::Selectable(key.c_str(), isSelected)) {
                        selectedSpawnPrefabIndex = i + 1; // update selection;
                        // actually spawn an entity from a prefab
                        objectsFactory.InstantiateObjects(key, prefabManager, prefabWorld, 1);
                        std::string spawnMsg = "Spawned entity of type: " + key;
                        GGLogger::Info(spawnMsg);
                    }

                    ++i;

                }
            }
        }
    }
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.0f, 0.0f, 1.0f)); // dark red
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.45f, 0.0f, 0.0f, 1.0f)); // medium red
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));  // brighter red
    if (ImGui::CollapsingHeader("Destroy", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (prefabManager.PrefabCount() == 0) {
            ImGui::Text("No prefabs loaded");
        }
        else {
            int i{ 0 };
            for (auto& [key, prefab] : prefabManager.GetPrefabs()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.0f));
                ImGui::Text("[DESTROY] ");
                ImGui::PopStyleColor();
                ImGui::SameLine();

                bool isSelected = (selectedDestroyPrefabIndex == i);
                if (ImGui::Selectable(key.c_str(), isSelected)) {
                    selectedDestroyPrefabIndex = i; // update selection

                    // find the first entity to destroy
                    Entity toDestroy = NULL_ENTITY;
                    auto& livingEntities = prefabWorld.GetLivingEntities();
                    for (auto& ent : livingEntities) {
                        auto* inst = prefabWorld.GetComponent<PrefabInstance>(ent);
                        if (inst && inst->GetPrefabInstanceName() == key) {
                            toDestroy = ent;
                            break; // only destroy the first one
                        }
                        else {
                            GGLogger::Info("Entity of type: " + key + " does not exist");
                        }
                    }

                    // destroy that one entity if found
                    if (toDestroy.IsValid()) {
                        prefabWorld.DestroyEntity(toDestroy);
                        GGLogger::Info("Destroyed entity of type: " + key);
                    }
                }
                ++i;
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.0f));
        ImGui::Text("[DESTROY] ");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Button("DestroyAllObjects")) {
            if (!(prefabWorld.GetLivingEntities()).empty()) {
                objectsFactory.DestroyAllSpawnedObjects(prefabWorld);
                GGLogger::Info("All objects destroyed");
            }
            else {
                GGLogger::Info("No objects exist to be destroyed");
            }
        }
        ImGui::PopStyleColor();
    }
    ImGui::Text("===[DEBUG DRAW FLAGS]===");
    ImGui::Separator();
    if (ImGui::Button(GGMeshRendererSystem::ShowDebugPoints() ? "Points: ON" : "Points: OFF"))
        GGMeshRendererSystem::SetDebugPoints(!GGMeshRendererSystem::ShowDebugPoints());

    ImGui::SameLine();
    if (ImGui::Button(GGMeshRendererSystem::ShowDebugLines() ? "Lines: ON" : "Lines: OFF"))
        GGMeshRendererSystem::SetDebugLines(!GGMeshRendererSystem::ShowDebugLines());

    ImGui::SameLine();
    if (ImGui::Button(GGMeshRendererSystem::ShowDebugAABBs() ? "AABBs: ON" : "AABBs: OFF"))
        GGMeshRendererSystem::SetDebugAABBs(!GGMeshRendererSystem::ShowDebugAABBs());

    ImGui::SameLine();
    if (ImGui::Button(GGMeshRendererSystem::ShowDebugCircles() ? "Circles: ON" : "Circles: OFF"))
        GGMeshRendererSystem::SetDebugCircles(!GGMeshRendererSystem::ShowDebugCircles());

    ImGui::SameLine();
    if (ImGui::Button(GGMeshRendererSystem::ShowDebugSquares() ? "Squares: ON" : "Squares: OFF"))
        GGMeshRendererSystem::SetDebugSquares(!GGMeshRendererSystem::ShowDebugSquares());


    ImGui::Separator();

    ImGui::PopStyleColor(3);

}

/*************************************************************************
  \brief
    Render prefab editing controls
  \details
    Provides an ImGui interface for inspecting and modifying spawned prefab
    instances. Supports:
    - Loading prefabs and entity types.
    - Spawning entities from existing prefabs.
    - Selecting a root entity for editing.
    - Editing transform properties (position, scale) of the selected entity.
    - Saving the modified entity as a new prefab.
    - Destroying all spawned objects in the current scene.
*************************************************************************/
void DefaultEditorScene::RenderPrefabEditingControls() {
    auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
    auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
    auto& sceneManager = GGEngine::GetEngine().GetSceneManager();

    ImGui::Text("===[PREFAB EDITOR TEST]===");
    ImGui::Separator();

    auto& prefabWorld = GGEngine::GetEngine().GetSceneManager().GetActiveScene();
    static Entity selectedRootEntity = NULL_ENTITY;
    static std::string saveStatus;

    // spawn objects
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.3f, 0.0f, 1.0f)); // dark green
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f)); // slightly lighter
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));  // even lighter
    if (ImGui::CollapsingHeader("Spawn From Existing Prefabs", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float inputWidth{ 120.f };

        ImGui::Separator();

        if (prefabManager.PrefabCount() == 0) {
            ImGui::Text("No prefabs loaded");
        }
        else {
            int i{ 0 };
            for (auto& [key, prefab] : prefabManager.GetPrefabs()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));
                ImGui::Text("[LOADED] ");
                ImGui::PopStyleColor();

                ImGui::SameLine();

                bool isSelected = (selectedSpawnPrefabIndex == static_cast<int>(i));
                if (ImGui::Selectable(key.c_str(), isSelected)) {
                    selectedSpawnPrefabIndex = i + 1; // update selection;
                    // actually spawn an entity from a prefab
                    objectsFactory.InstantiateObjects(key, prefabManager, prefabWorld, 1);
                    std::string spawnMsg = "Spawned entity of type: " + key;
                    GGLogger::Info(spawnMsg);
                }

                ++i;

            }
        }
    }
    ImGui::PopStyleColor(3);

    // root entity selection (prefab to be edited)
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Spawned Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Select Root Entity to Edit/Save Prefab:");
        auto& livingEntities = prefabWorld.GetLivingEntities();
        if (livingEntities.empty()) {
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "No entities available. Spawn some first.");
        }
        else {
            for (auto ent : livingEntities) {
                bool isSelected = (selectedRootEntity == ent);
                
                std::string label = "Entity " + std::to_string(ent.id) + "(" + (prefabWorld.GetComponent<NameComponent>(ent) ? prefabWorld.GetComponent<NameComponent>(ent)->GetName() : "Unnamed Entity") + ")";
                
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedRootEntity = ent;
                    saveStatus.clear();
                    GGLogger::Info(label + " selected");
                }
            }
        }
    }

    // editing (using transform to test)
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Edit Prefab", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Separator();
        static float inputWidth{ 80.f };
        static float xPos{ 0.f }, yPos{ 0.f };
        static float sx{ 1.f }, sy{ 1.f };

        if (selectedRootEntity.IsValid()) {
            // get the entity's transform
            if (auto* transform = prefabWorld.GetComponent<LocalTransform>(selectedRootEntity)) {
                // initialize sliders with current values
                xPos = transform->position.x;
                yPos = transform->position.y;
                sx = transform->scale.x;
                sy = transform->scale.y;

                // sliders for position
                ImGui::Text("Position: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(inputWidth);
                if (ImGui::DragFloat("xPos", &xPos, 0.1f, -10.f, 10.f)) {
                    transform->position.x = xPos;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(inputWidth);
                if (ImGui::DragFloat("yPos", &yPos, 0.1f, -10.f, 10.f)) {
                    transform->position.y = yPos;
                }

                ImGui::Spacing();

                // sliders for scale
                ImGui::Text("Scale X: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(inputWidth);
                if (ImGui::DragFloat("sx", &sx, 0.1f, 0.1f, 10.f)) {
                    transform->scale.x = sx;
                }

                ImGui::Text("Scale Y: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(inputWidth);
                if (ImGui::DragFloat("sy", &sy, 0.1f, 0.1f, 10.f)) {
                    transform->scale.y = sy;
                }

                transform->isDirty = true;
            }
            else {
                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Selected entity has no Transform component!");
            }
        }
        else {
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "No root entity selected. Select one to edit.");
        }
    }

    // prefab save option
    ImGui::Separator();
    if (selectedRootEntity.IsValid()) {
        static char prefabSaveName[128] = ""; // buffer for prefab name input

        // show original prefab name if available
        if (auto* prefabInst = prefabWorld.GetComponent<PrefabInstance>(selectedRootEntity)) {
            auto originalName = (prefabWorld.GetComponent<EntityName>(selectedRootEntity) ? prefabWorld.GetComponent<NameComponent>(selectedRootEntity)->GetName() : "Unknown Prefab");
            ImGui::Text("Original Prefab: %s", originalName.c_str());
            // pre-fill input buffer with original name if empty
            if (strlen(prefabSaveName) == 0) {
                strncpy(prefabSaveName, originalName.c_str(), sizeof(prefabSaveName));
            }
        }

        ImGui::InputText("Save As", prefabSaveName, IM_ARRAYSIZE(prefabSaveName));

        ImGui::SameLine();
        if (ImGui::Button("Save Prefab")) {
            std::string newName(prefabSaveName);
            if (!newName.empty()) {
                bool success = prefabManager.SavePrefab(selectedRootEntity, newName, prefabWorld);
                if (success) {
                    GGLogger::Info("Prefab saved as: " + newName);
                    //sceneManager.PropagatePrefabChanges(prefabManager);
                    sceneManager.SyncPrefabInstances(newName,prefabManager);
                }
                else {
                    GGLogger::Error("Failed to save prefab: " + newName);
                }
            }
        }
    }
    else {
        ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Select a root entity first to save as prefab.");
    }

    // destroy all objects
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("DestroyAllObjects")) {
        if (!(prefabWorld.GetLivingEntities()).empty()) {
            objectsFactory.DestroyAllSpawnedObjects(prefabWorld);
            GGLogger::Info("All objects destroyed");
        }
        else {
            GGLogger::Info("No objects exist to be destroyed");
        }
    }
    ImGui::PopStyleColor();
}

/*************************************************************************
  \brief
    Render scene management controls
  \details
    Provides an ImGui interface for managing scenes in the editor. Supports:
    - Loading prefabs and entity types.
    - Adding new scenes to the SceneManager.
    - Switching the active scene.
    - Backing up and restoring all scenes.
    - Spawning test entities in the active scene.
    - Saving the currently active scene under a new name.
*************************************************************************/
void DefaultEditorScene::RenderSceneManagementControls() {
    auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
    auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
    auto& sceneManager = GGEngine::GetEngine().GetSceneManager();

    ImGui::Text("===[SCENES MANAGEMENT TEST]===");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.0f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.5f, 0.0f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.7f, 0.4f, 0.9f, 1.0f));
    if (ImGui::CollapsingHeader("Choose Scene To Instantiate", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (sceneManager.GetSceneCount() == 0) {
            ImGui::Text("No scenes loaded");
        }
        else {
            ImGui::Separator();

            std::string sceneToActivate;

            for (auto& scene : sceneManager.GetSceneMap()) {
                if (scene.second.GetLivingEntitiesCount() == 0) {
                    continue; // skip scenes with no spawned entities
                }

                std::string sceneName = scene.first;

                bool isSelected = (selectedSceneKey == sceneName);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));
                ImGui::Text("[LOADED] ");
                ImGui::PopStyleColor();

                ImGui::SameLine();

                if (ImGui::Selectable((scene.first).c_str(), isSelected)) {
                    sceneToActivate = sceneName; // store selected scene
                }
            }

            if (!sceneToActivate.empty()) {
                selectedSceneKey = sceneToActivate;
                sceneManager.SetActiveScene(sceneToActivate);
                GGLogger::Info("Switched to scene: " + sceneToActivate);
            }
        }
    }

    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Text("===[SCENE BACKUP/RESTORE TEST]===");
    ImGui::Separator();

    // get the current active scene key & world
    bool hasActive = sceneManager.HasActiveScene();

    if (ImGui::Button("Backup All Scenes")) {
        bool success = sceneManager.BackupAllScenes();
        if (success)
            GGLogger::Info("[SceneManager] Backup successful for all loaded scenes!");
        else
            GGLogger::Error("[SceneManager] Backup failed or no scenes loaded!");
    }

    ImGui::SameLine();

    if (ImGui::Button("Restore All Scenes")) {
        bool success = sceneManager.RestoreAllScenes(prefabManager);
        if (success)
            GGLogger::Info("[SceneManager] All scenes restored successfully!");
        else
            GGLogger::Error("[SceneManager] Restore failed! No backup data found.");
    }

    ImGui::Separator();

    if (hasActive) {
        if (ImGui::Button("Spawn Test Entity In Active Scene")) {
            World& activeWorld = sceneManager.GetActiveScene();

            std::string prefabName = "Tile";
            bool success = objectsFactory.InstantiateObjects(prefabName, prefabManager, activeWorld, 1);

            if (success) {
                Entity spawned = objectsFactory.GetLastSpawnedObject();
                GGLogger::Info("Spawned test entity: " + prefabName);
            }
            else {
                GGLogger::Error("[SceneManager] Failed to spawn test entity: " + prefabName);
            }
        }
    }
    else {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "No active scene selected!");
    }

    ImGui::Separator();
    ImGui::Text("===[SCENE SAVING/EDITING TEST]===");
    ImGui::Separator();

    static char sceneSaveName[64] = "";
    auto originalName = sceneManager.GetActiveSceneKey();
    ImGui::Text("Original Scene: %s", originalName.c_str());

    ImGui::InputText("##Save As", sceneSaveName, IM_ARRAYSIZE(sceneSaveName));

    if (ImGui::Button("Save Scene")) {
        SceneSerializer::SaveCurrentScene(originalName, sceneSaveName, sceneManager);
        GetAssetManagerNode()->RefreshAssetLists();
    }
}

void DefaultEditorScene::RenderInstancingControls()
{
    ImGui::Text("===[INSTANCING TEST]===");
    ImGui::Separator();

    auto& testWorld = GGEngine::GetEngine().GetSceneManager().GetActiveScene();

    static int32_t numToSpawn{ 2500 };
    static float xScale{ 1.0f }, yScale{ 1.0f }, rot{ 0.0f };

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("xScale", &xScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("yScale", &yScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("rotation", &rot);

    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("Num to Spawn", &numToSpawn, 0,
        std::min(2500, static_cast<int32_t>(World::MAX_ENTITIES) -
            static_cast<int32_t>(testWorld.GetLivingEntities().size()) - 1));

    ImGui::SameLine();
    if (ImGui::Button("Spawn Clone"))
    {
        if (numToSpawn > 0)
        {
            size_t spawnCount = static_cast<size_t>(numToSpawn);

            numToSpawn = 0;
            for (size_t i{}; i < spawnCount; i++)
            {
                Entity renderTest = testWorld.CreateEntity();
                testWorld.AddComponent<LocalTransform>(renderTest, LocalTransform());
                testWorld.AddComponent<WorldTransform>(renderTest, WorldTransform());
                testWorld.AddComponent<MeshRenderer>(renderTest, MeshRenderer());
                testWorld.AddComponent<Sprite>(renderTest, Sprite());

                testWorld.GetComponent<LocalTransform>(renderTest)->scale.x = xScale;
                testWorld.GetComponent<LocalTransform>(renderTest)->scale.y = yScale;
                testWorld.GetComponent<LocalTransform>(renderTest)->rotation = rot;

                testWorld.GetComponent<LocalTransform>(renderTest)->position.x = -960.0f + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / (960.0f - (-960.0f))));
                testWorld.GetComponent<LocalTransform>(renderTest)->position.y = -540.0f + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / (540.0f - (-540.0f))));

                testWorld.GetComponent<MeshRenderer>(renderTest)->shaderAssetID = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultShaderAssetID();
                
                testWorld.GetComponent<Sprite>(renderTest)->textureAssetID = 12;
            }
        }
    }

    ImGui::Separator();

    ImGui::Text("===[SYSTEM STATUS]===");
    ImGui::Text("Active Entities: %zu", testWorld.GetLivingEntities().size());

    // Performance information
    ImGui::Text("Frame Rate: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

/*************************************************************************
  \brief
    Render font atlas testing interface
  \details
    Provides controls for testing FontAtlas rendering with static text,
    dynamic FPS counter, and interactive text display.
*************************************************************************/
void DefaultEditorScene::RenderFontTestControls()
{
    ImGui::Text("===[FONT ATLAS TEST]===");
    ImGui::Separator();

    auto& renderManager = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();

    // Test 1: Static "Hello World" text
    ImGui::Text("Test 1: Static Text");
    if (ImGui::Button("Render 'Hello World'"))
    {
        renderManager.SetOverlayTextEnabled(true);
        renderManager.SetOverlayText("Hello World!");
        renderManager.SetOverlayTextPosition(0.0f, 0.0f);
        renderManager.SetOverlayTextScale(2.0f);
        renderManager.SetOverlayTextColor({ 1.0f, 1.0f, 0.0f });


        GGLogger::Info("Static text rendering: ON");
    }
    ImGui::SameLine();
    ImGui::Text("Status: %s", renderManager.IsOverlayTextEnabled() ? "RENDERING" : "OFF");

    ImGui::Separator();

    // Test 2: Dynamic FPS Counter
    ImGui::Text("Test 2: FPS Counter");
    bool showFPS = renderManager.IsOverlayShowFPS();
    if (ImGui::Checkbox("Show FPS Counter", &showFPS))
    {
        renderManager.SetOverlayShowFPS(showFPS);
        GGLogger::Info("FPS counter: " + std::string(showFPS ? "ON" : "OFF"));
    }

    ImGui::Separator();

    // Test 3: Interactive custom text
    ImGui::Text("Test 3: Custom Text");
    static char textBuffer[256] = "Type your text here!";
    ImGui::InputText("Text", textBuffer, sizeof(textBuffer));

    float textScale = renderManager.GetOverlayTextScale();
    glm::vec3 textColor = renderManager.GetOverlayTextColor();
    float posX = renderManager.GetOverlayTextX();
    float posY = renderManager.GetOverlayTextY();

    ImGui::SliderFloat("Scale", &textScale, 0.1f, 5.0f);
    ImGui::ColorEdit3("Color", &textColor.x);
    ImGui::SliderFloat("X Position", &posX, -900.0f, 900.0f);
    ImGui::SliderFloat("Y Position", &posY, -500.0f, 500.0f);

    // Apply updated values
    renderManager.SetOverlayTextScale(textScale);
    renderManager.SetOverlayTextColor(textColor);
    renderManager.SetOverlayTextPosition(posX, posY);

    if (ImGui::Button("Render Custom Text"))
    {
        renderManager.SetOverlayTextEnabled(true);
        renderManager.SetOverlayText(std::string(textBuffer));
        GGLogger::Info("Rendering custom text: " + std::string(textBuffer));
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        renderManager.SetOverlayTextEnabled(false);
        renderManager.SetOverlayShowFPS(false);
        GGLogger::Info("Cleared text rendering");
    }

    ImGui::Separator();

    // Font atlas info
    ImGui::Text("===[FONT ATLAS INFO]===");
    ImGui::Text("Atlas Status: %s", renderManager.GetFontAtlasValid() ? "READY" : "NOT LOADED");

    if (renderManager.GetFontAtlasValid())
    {
        auto dimensions = renderManager.GetFontAtlasDimensions();
        ImGui::Text("Atlas Size: %dx%d", dimensions.x, dimensions.y);
    }
}

/*************************************************************************
  \brief
    Render font test overlays to game screen
  \details
    Handles rendering of test text and FPS counter directly to the
    game viewport, called after ImGui rendering is complete.
*************************************************************************/
void DefaultEditorScene::RenderFontTestOverlays()
{
    //auto& renderManager = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    //auto& fontShader = renderManager.GetDefaultFontShader();

    //// Use the default projection (center-based coordinates)
    //fontShader.Use();
    //fontShader.SetMat4("projection", renderManager.GetDefaultProjection());

    //// Render custom test text if enabled
    //if (renderTestText)
    //{
    //    renderManager.RenderText(fontShader, testText, testTextX, testTextY, testTextScale, testTextColor);
    //}

    //// Render FPS counter dynamically positioned based on window size
    //if (showFPSCounter)
    //{
    //    float fps = ImGui::GetIO().Framerate;
    //    std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));

    //    // Get current window dimensions
    //    int windowWidth = renderManager.GetMainWindow().GetWindowWidth();
    //    int windowHeight = renderManager.GetMainWindow().GetWindowHeight();

    //    // Calculate top-right position in center-based coordinate system
    //    // Your projection uses: X from -width/2 to +width/2, Y from -height/2 to +height/2
    //    float halfWidth = windowWidth / 2.0f;
    //    float halfHeight = windowHeight / 2.0f;

    //    // Position near top-right corner
    //    float textWidth = renderManager.GetPrintSize(fpsText, 1.0f).x;
    //    float xPos = halfWidth - textWidth - 20.0f;  // 20px padding from right edge
    //    float yPos = halfHeight - 60.0f;             // 40px padding from top edge

    //    renderManager.RenderText(fontShader, fpsText, xPos, yPos, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    //}
}

/*************************************************************************
  \brief
    Renders Networking Test Controls to the Screen
  \details
    Handles switching to the Networking Scene and calling ConnectToServer
    when first switched to.
*************************************************************************/
void DefaultEditorScene::RenderNetworkingTestControls()
{
    ImGui::Text("===[NETWORKING TEST]===");
    ImGui::Separator();

    ImGui::TextWrapped("INSTRUCTIONS: You must launch a 2nd instance of this executable or use 2 computers connected to the internet for this"
        " test case. Launch another instance of this executable with a 2nd computer or this same computer and navigate to this tab. Clicking"
        " around anywhere in the scene on one instance should move the Red Player on the other.");
    ImGui::Separator();

    static bool connect = false;

    if (!connect)
    {
        GGNetworking::GetGameClient()->ConnectToServer("onclocal.ddns.net");
        connect = true;
    }
}

/*************************************************************************
  \brief
    Crash log test interface
  \details
    Provide a button to force crash the program.
*************************************************************************/
void DefaultEditorScene::RenderCrashLogging()
{
    ImGui::Text("===[CRASH LOG TEST]===");
    ImGui::Separator();

    // Test: throw runtime_error
    ImGui::Text("Test: Crash Program");
    if (ImGui::Button("Crash Program"))
    {
        throw std::runtime_error("testing exception");
    }
    ImGui::SameLine();
    ImGui::Separator();
}

void DefaultEditorScene::BackgroundSpriteScene()
{
    static std::vector<Entity> spawnedObjects;
    static Entity backgroundEntity = Entity{};
    static bool bgSpawned = false;
    static int32_t numToSpawn = 10;
    static float xScale = 1.f, yScale = 1.f, rot = 0.f;

    auto& world = GGEngine::GetEngine().GetSceneManager().GetActiveScene();

    // Check if tracked entities are still valid
    // If background was spawned but no longer exists in world, reset flag
    if (bgSpawned)
    {
        if (world.GetLivingEntities().find(backgroundEntity) == world.GetLivingEntities().end())
        {
            bgSpawned = false;
            GGLogger::Info("Background entity no longer exists, resetting flag");
        }
    }

    // Remove invalid entities from our tracking vector
    spawnedObjects.erase(
        std::remove_if(spawnedObjects.begin(), spawnedObjects.end(),
            [&world](Entity e) {
                return world.GetLivingEntities().find(e) == world.GetLivingEntities().end();
            }),
        spawnedObjects.end()
    );

    ImGui::Text("===[Render  background Sprite]===");
    ImGui::Separator();

    ImGui::Text("Test:Render background");

    // Spawn Background Button
    if (ImGui::Button("Spawn Background"))
    {
        if (!bgSpawned)
        {
            backgroundEntity = world.CreateEntity();
            world.AddComponent<LocalTransform>(backgroundEntity, LocalTransform());
            world.AddComponent<WorldTransform>(backgroundEntity, WorldTransform());
            world.AddComponent<MeshRenderer>(backgroundEntity, MeshRenderer());
            world.AddComponent<Sprite>(backgroundEntity, Sprite());
            world.AddComponent<Rigidbody2DFloat>(backgroundEntity, Rigidbody2DFloat());

            if (auto* transform = world.GetComponent<LocalTransform>(backgroundEntity))
            {
                transform->position.x = 0.f;
                transform->position.y = 0.f;

                int screenW = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowWidth();
                int screenH = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowHeight();

                transform->scale.x = static_cast<float>(screenW) / 40.f;
                transform->scale.y = static_cast<float>(screenH) / 40.f;
                transform->rotation = 0.f;
            }

            if (auto* sprite = world.GetComponent<Sprite>(backgroundEntity))
            {
                /*
                sprite->textureAssetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(
                    7, "Textures/Brick_Wall.png");
                    */
            }

            if (auto* renderer = world.GetComponent<MeshRenderer>(backgroundEntity))
            {
                renderer->shaderAssetID = Systems::GetSystemsRegistry()
                    .GetSystem<Systems::RenderManager>()
                    .GetDefaultShaderAssetID();
            }

            bgSpawned = true;
            GGLogger::Info("Background spawned to cover the whole screen");
        }
    }

    if (bgSpawned)
    {
        if (auto* transform = world.GetComponent<LocalTransform>(backgroundEntity))
        {
            ImGui::Text("Scale X: %.1f, Scale Y: %.1f", transform->scale.x, transform->scale.y);
        }

        ImGui::SameLine();
        if (ImGui::Button("Destroy Background"))
        {
            world.DestroyEntity(backgroundEntity);
            bgSpawned = false;
            GGLogger::Info("Background entity destroyed");
        }
    }
    ImGui::Separator();

    ImGui::Text("Global Dynamic Transforms:");
    ImGui::Checkbox("Dynamic Scale (All)", &GGMeshRendererSystem::enableDynamicScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::SliderFloat("Amplitude", &GGMeshRendererSystem::dynamicScaleAmplitude, 0.f, 2.f);

    ImGui::Checkbox("Dynamic Rotation (All)", &GGMeshRendererSystem::enableDynamicRotation);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::SliderFloat("Speed", &GGMeshRendererSystem::dynamicRotationSpeed, 0.f, 100.f);

    ImGui::Separator();

    ImGui::Text("Spawn Settings:");
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputFloat("xScale", &xScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputFloat("yScale", &yScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputFloat("rotation", &rot);

    ImGui::SetNextItemWidth(120.f);
    ImGui::SliderInt("Num to Spawn", &numToSpawn, 1, 10);

    ImGui::SameLine();
    if (ImGui::Button("Spawn Objects"))
    {
        int screenW = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowWidth();
        int screenH = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowHeight();

        for (int i = 0; i < numToSpawn; ++i)
        {
            Entity e = world.CreateEntity();
            world.AddComponent<LocalTransform>(e, LocalTransform());
            world.AddComponent<WorldTransform>(e, WorldTransform());
            world.AddComponent<MeshRenderer>(e, MeshRenderer());
            world.AddComponent<Sprite>(e, Sprite());

            auto* t = world.GetComponent<LocalTransform>(e);
            t->scale.x = xScale;
            t->scale.y = yScale;
            t->rotation = rot;
            t->position.x = -screenW / 2.f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / screenW));
            t->position.y = -screenH / 2.f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / screenH));

            auto* renderer = world.GetComponent<MeshRenderer>(e);
            renderer->shaderAssetID = Systems::GetSystemsRegistry()
                .GetSystem<Systems::RenderManager>()
                .GetDefaultShaderAssetID();
            
            spawnedObjects.push_back(e);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Destroy All Objects"))
    {
        for (Entity e : spawnedObjects)
            world.DestroyEntity(e);
        spawnedObjects.clear();
        GGLogger::Info("All spawned objects destroyed");
    }

    if (!spawnedObjects.empty())
    {
        ImGui::SameLine();
        ImGui::Text("| Active: %zu", spawnedObjects.size());
    }

    ImGui::Separator();
}

void DefaultEditorScene::PickingFrameBuffer()
{
    ImGui::Text("===[Picking TEST]===");
    ImGui::Separator();

    auto& testWorld = GGEngine::GetEngine().GetActiveWorld();
    // Test: throw runtime_error
    ImGui::Text("Test: Picking framebuffer");
    if (ImGui::Button("spwan obj"))
    {
       
       
        Entity renderTest = testWorld.CreateEntity();
        testWorld.AddComponent<LocalTransform>(renderTest, LocalTransform());
        testWorld.AddComponent<MeshRenderer>(renderTest, MeshRenderer());
        testWorld.AddComponent<Sprite>(renderTest, Sprite());

        testWorld.GetComponent<LocalTransform>(renderTest)->scale.x = 64;
        testWorld.GetComponent<LocalTransform>(renderTest)->scale.y = 64;
        testWorld.GetComponent<LocalTransform>(renderTest)->rotation = 0;

        testWorld.GetComponent<LocalTransform>(renderTest)->position.x = 0.f;
        testWorld.GetComponent<LocalTransform>(renderTest)->position.y = 0.f;

        testWorld.GetComponent<MeshRenderer>(renderTest)->shaderAssetID = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultShaderAssetID();
        
        testWorld.GetComponent<Sprite>(renderTest)->textureAssetID = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultTextureAssetID();
    
    }
    ImGui::SameLine();
    ImGui::Separator();

}

void DefaultEditorScene::CameraSettings()
{

    ImGui::Text("Camera Settings");
    ImGui::Separator();

    // Get both cameras
    auto& editorNode = GetEditorViewNode();
    auto& gameNode = GetGameViewNode();
    auto* editorCamera = editorNode->GetCamera();
    auto* gameCamera = gameNode->GetCamera();

    // Create tabs for Editor and Game camera
    if (ImGui::BeginTabBar("CameraTabBar"))
    {
       
        if (ImGui::BeginTabItem("Editor Camera"))
        {
            ImGui::Spacing();

            // Position 
            if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen))
            {
                glm::vec2 pos = editorCamera->GetPosition();
                float posX = pos.x;
                float posY = pos.y;

                ImGui::Text("Position:");
                ImGui::PushItemWidth(200);
                if (ImGui::SliderFloat("EditorPosX", &posX, -1000.0f, 1000.0f, "%.1f"))
                {
                    editorCamera->SetPositionImmediate({ posX, pos.y });
                }
                if (ImGui::SliderFloat("EditorPosY", &posY,  -1000.0f, 1000.0f, "%.1f"))
                {
                    editorCamera->SetPositionImmediate({ pos.x, posY });
                }
                ImGui::PopItemWidth();

                // Quick reset button
                if (ImGui::Button("Reset Position Editor"))
                {
                    editorCamera->SetPositionImmediate(glm::vec2(0.0f, 0.0f));
                }
            }

            ImGui::Spacing();

            // === Zoom ===
            if (ImGui::CollapsingHeader("Zoom", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float zoom = editorCamera->GetZoom();
                ImGui::Text("Zoom Level: %.2f", zoom);
                ImGui::PushItemWidth(250);
                if (ImGui::SliderFloat("EditorZoom", &zoom, 1.f, 640.0f, "%.2f"))
                {
                    editorCamera->SetZoomImmediate(zoom);
                }
                ImGui::PopItemWidth();

          
            }

            ImGui::Spacing();

            // Rotation
            if (ImGui::CollapsingHeader("Rotation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float rotation = glm::degrees(editorCamera->GetRotation());
                ImGui::Text("Rotation: %.1f", rotation);
                ImGui::PushItemWidth(250);
                if (ImGui::SliderFloat("editorRotation", &rotation, -180.0f, 180.0f, "%.1f"))
                {
                    editorCamera->SetRotation(glm::radians(rotation));
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();
                if (ImGui::Button("Reset EditorRot"))
                {
                    editorCamera->SetRotation(0.0f);
                }
            }

            ImGui::Spacing();

            //  Pan/Movement Speed 
            if (ImGui::CollapsingHeader("Movement Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float panSpeed = editorCamera->GetPanSpeed();
                float zoomSpeed = editorCamera->GetZoomSpeed();
                ImGui::Text("Use use for this");

                ImGui::Text("Pan Speed:");
                ImGui::PushItemWidth(200);
                if (ImGui::SliderFloat("EditorPanSpeed", &panSpeed, 0.1f, 10.0f, "%.2f"))
                {
                    editorCamera->SetPanSpeed(panSpeed);
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Speed when panning with mouse/keyboard");
                }

                ImGui::Text("Zoom Speed:");
                ImGui::PushItemWidth(200);
                if (ImGui::SliderFloat("EditorZoomSpeed", &zoomSpeed, 0.01f, 1.0f, "%.3f"))
                {
                    editorCamera->SetZoomSpeed(zoomSpeed);
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Speed when zooming with mouse wheel");
                }

                // Smooth movement toggle
                bool smoothMovement = editorCamera->IsSmoothMovementEnabled();
                if (ImGui::Checkbox("Smooth Movement Editor", &smoothMovement))
                {
                    editorCamera->SetSmoothMovement(smoothMovement);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            //Camera Info 
            ImGui::Text("Camera Info:");
            ImGui::BulletText("Position: (%.1f, %.1f)",
                editorCamera->GetPosition().x,
                editorCamera->GetPosition().y);
            ImGui::BulletText("Zoom: %.2fx", editorCamera->GetZoom());
            ImGui::BulletText("Rotation: %.1f", glm::degrees(editorCamera->GetRotation()));

            ImGui::Spacing();

            //Full Reset
            if (ImGui::Button("Reset All Editor", ImVec2(150, 0)))
            {
                editorCamera->Reset();
            }

            ImGui::EndTabItem();
        }

     
        if (ImGui::BeginTabItem("Game Camera"))
        {
            ImGui::Spacing();

            //Position 
            if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen))
            {
                glm::vec2 pos = gameCamera->GetPosition();
                float posX = pos.x;
                float posY = pos.y;

                ImGui::Text("Position:");
                ImGui::PushItemWidth(200);
                if (ImGui::DragFloat("GamePosX", &posX, 1.0f, -10000.0f, 10000.0f, "%.1f"))
                {
                    gameCamera->SetPosition(glm::vec2(posX, pos.y));
                }
                if (ImGui::DragFloat("GamePosY", &posY, 1.0f, -10000.0f, 10000.0f, "%.1f"))
                {
                    gameCamera->SetPosition(glm::vec2(pos.x, posY));
                }
                ImGui::PopItemWidth();

                if (ImGui::Button("Reset Position##Game"))
                {
                    gameCamera->SetPosition(glm::vec2(0.0f, 0.0f));
                }
            }

            ImGui::Spacing();

            // Zoom
            if (ImGui::CollapsingHeader("Zoom", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float zoom = gameCamera->GetZoom();
                ImGui::Text("Zoom Level: %.2f", zoom);
                ImGui::PushItemWidth(250);
                if (ImGui::SliderFloat("GameZoom", &zoom, 1.f, 640.0f, "%.2f"))
                {
                    gameCamera->SetZoom(zoom);
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();
                if (ImGui::Button("1:1Game"))
                {
                    gameCamera->SetZoom(64.0f);
                }

                // Zoom presets
                ImGui::Text("Quick Zoom:");
                ImGui::SameLine();
                if (ImGui::SmallButton("0.5xGame")) gameCamera->SetZoom(32.f);
                ImGui::SameLine();
                if (ImGui::SmallButton("1.0xGame")) gameCamera->SetZoom(64.f);
                ImGui::SameLine();
                if (ImGui::SmallButton("2.0xGame")) gameCamera->SetZoom(128.0f);
                ImGui::SameLine();
                if (ImGui::SmallButton("5.0xGame")) gameCamera->SetZoom(320.0f);
            }

            ImGui::Spacing();

            // Rotation 
            if (ImGui::CollapsingHeader("Rotation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float rotation = glm::degrees(gameCamera->GetRotation());
                ImGui::Text("Rotation: %.1f", rotation);
                ImGui::PushItemWidth(250);
                if (ImGui::SliderFloat("GameRotation", &rotation, -180.0f, 180.0f, "%.1f"))
                {
                    gameCamera->SetRotation(glm::radians(rotation));
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();
                if (ImGui::Button("Reset GameRot"))
                {
                    gameCamera->SetRotation(0.0f);
                }
            }

            ImGui::Spacing();

            

            // === Full Reset ===
            if (ImGui::Button("Reset All Game", ImVec2(150, 0)))
            {
                gameCamera->Reset();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void DefaultEditorScene::GizmoSettings()
{
    ImGui::Text("Gizmo Settings");
    ImGui::Separator();

    bool isOn = GGMeshRendererSystem::showMoveGizmo;

    if (isOn)
    {
        if (ImGui::Button("Gizmo: ON", ImVec2(120, 0)))
            GGMeshRendererSystem::showMoveGizmo = false;
    }
    else
    {
        if (ImGui::Button("Gizmo: OFF", ImVec2(120, 0)))
            GGMeshRendererSystem::showMoveGizmo = true;
    }

    ImGui::Spacing();
    ImGui::Text("Gizmo Mode");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::RadioButton("Translate (G)",
        GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::TRANSLATE))
    {
        GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::TRANSLATE;
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Rotate (R)",
        GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::ROTATE))
    {
        GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::ROTATE;
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Scale (S)",
        GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::SCALE))
    {
        GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::SCALE;
    }

    ImGui::Spacing();
    ImGui::Separator();

   
    if (ImGui::CollapsingHeader("Mode Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        switch (GGMeshRendererSystem::currentGizmoMode)
        {
        case GGMeshRendererSystem::GizmoMode::TRANSLATE:
            ImGui::Text("Translation Gizmo");
            ImGui::PushItemWidth(200);
            ImGui::SliderFloat("Line Length", &GGMeshRendererSystem::gizmoLineLength, 0.1f, 10.0f);
            ImGui::SliderFloat("Handle Size", &GGMeshRendererSystem::gizmoHandleRadius, 0.05f, 0.5f);
            ImGui::PopItemWidth();
            break;

        case GGMeshRendererSystem::GizmoMode::ROTATE:
            ImGui::Text("Rotation Gizmo");
            ImGui::PushItemWidth(200);
            ImGui::SliderFloat("Circle Radius", &GGMeshRendererSystem::rotationGizmoRadius, 0.10f, 1.0f);
            ImGui::PopItemWidth();
            break;

        case GGMeshRendererSystem::GizmoMode::SCALE:
            ImGui::Text("Scale Gizmo");
            ImGui::PushItemWidth(200);
            ImGui::SliderFloat("Handle Size", &GGMeshRendererSystem::scaleHandleSize, 0.05f, 0.5f);
            ImGui::PopItemWidth();
            break;
        }

        ImGui::Spacing();

        if (ImGui::Button("Reset Current Mode", ImVec2(180, 0)))
        {
            if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::TRANSLATE)
            {
                GGMeshRendererSystem::gizmoLineLength = 1.f;
                GGMeshRendererSystem::gizmoHandleRadius = 0.1f;
            }
            if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::ROTATE)
            {
                GGMeshRendererSystem::rotationGizmoRadius = 0.5f;
            }
            if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::SCALE)
            {
                
                GGMeshRendererSystem::scaleHandleSize = 0.15f;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    
    ImGui::Text("Selection Info");
    ImGui::Spacing();

    if (GGMeshRendererSystem::showMoveGizmo && GGMeshRendererSystem::selectedEntity > 0)
    {
        ImGui::Text("Entity ID: %u", GGMeshRendererSystem::selectedEntity);

        auto& world = GGEngine::GetEngine().GetActiveWorld();
        for (const Entity& e : world.GetLivingEntities())
        {
            if (e.id == GGMeshRendererSystem::selectedEntity &&
                world.HasComponent<LocalTransform>(e))
            {
                auto* t = world.GetComponent<LocalTransform>(e);

                ImGui::BulletText("Position: (%.2f, %.2f)", t->position.x, t->position.y);
                ImGui::BulletText("Rotation: %.1f", t->rotation * 180.0f / M_PI);
                ImGui::BulletText("Scale: (%.2f, %.2f)", t->scale.x, t->scale.y);
                break;
            }
        }
    }
    else
    {
        ImGui::TextDisabled("No entity selected");
    }
    
}
