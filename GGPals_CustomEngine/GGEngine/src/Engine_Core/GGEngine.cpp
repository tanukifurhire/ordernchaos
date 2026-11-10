/*************************************************************************
  \file       GGEngine.cpp
  \author
  \par        Email:
  \date       02 October 2025
  \version    1.0
  \brief
	Implements the GGEngine core class that manages initialization,
	updating, and shutdown of the game engine.

  \details
	Provides the central control logic for the engine lifecycle:
	- Initializes subsystems (rendering, ECS, timers, networking, audio, editor).
	- Loads entity types, prefabs, and scenes from JSON configuration files.
	- Registers core assets (textures, shaders, animations).
	- Spawns test entities and boundary walls for the physics test world.
	- Runs the main game loop:
		* Processes input events.
		* Updates ECS systems (transform, physics, sprite animation, render).
		* Updates core managers (simulation, timer, editor, input).
		* Runs audio system updates.
		* Manages networking (server or client mode).
		* Handles ImGui editor rendering and performance profiling.
	- Provides a frame limiter using busy-wait for microsecond-accurate
	  timing, ensuring deterministic 60 FPS for multiplayer synchronization.
	- Shuts down subsystems and cleans up resources (GLFW, ImGui, audio, assets).

  \dependencies
	GGEngine.hpp, GGEngineRender.hpp, GGSystemManagers.hpp,
	GGECSPhysics.hpp, GGECSMeshRenderer.hpp, GGECSComponents.hpp,
	GGECSAssetInterface.hpp, GGECSGameObjectBridge.hpp, GGECSWorld.hpp,
	GGJSONConfig.hpp, GGAssetManager.hpp, GGAudioManager.hpp, GGAudioAsset.hpp,
	GGFontAtlas.hpp, GGPerformanceProfiler.hpp, ImGui (with GLFW + OpenGL3),
	SOIL2 image loader, FMOD audio wrapper, OpenGL, GLFW

  \see
	GGEngine.hpp, GGAssetManager.hpp, GGECSWorld.hpp, GGPerformanceProfiler.hpp

  \history
	- 1.0 (02 October 2025): Initial version with full engine lifecycle
	  (startup, initialization, test scenes, main loop, frame limiter,
	  subsystem shutdown).

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

// System
#include <GGEngine.hpp>
#include <GGAccessibilityShortcuts.hpp>
#include <GGEngineRender.hpp>
#include <GGSystemManagers.hpp>

// ECS
#include <GGECSPhysics.hpp>
#include <GGECSMeshRenderer.hpp>
#include <GGECSComponents.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGECSGameObjectBridge.hpp>

// Libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SOIL2/SOIL2.h>
#include <GGECSWorld.hpp>
#include <GGJSONConfig.hpp>

// Asset Manager
#include <filesystem>
#include <GGAudioManager.hpp>
#include <GGAssetManager.hpp>
#include <GGAudioAsset.hpp>
#include <GGFontAtlas.hpp>
#include <GGAssetID.hpp>

// Networking
#include <PlatformWindows.hpp>
#include <GameClient.hpp>
#include <GameServer.hpp>
#include <SteamPlatform.hpp>

// imgui stuff
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

// Performance profiler
#include "GGPerformanceProfiler.hpp"
#include <GGRender2d.hpp>

// Lua scripting integration
#include "GGLuaVM.hpp"
#include "GGLuaBindings.hpp"
#include "GGLuaExtensions.hpp"
#include "GGLuaInput.hpp"
#include "GGLuaDebug.hpp"
#include "GGEventBus.hpp"
#include "GGLuaVars.hpp"
#include "GGLuaGridBindings.hpp"
#include "GGLuaDefineType.hpp"
#include "GGLuaRPCBindings.hpp"

// Grid
#include <GGGridPathfinding.hpp>

// Interrupt Handling
#include "GGInterruptHandler.hpp"

GGEngine GGEngine::core;
float GGEngine::deltaTime = 0.0f;

GGEngine::GGEngine() {
	assetManager = std::make_unique<GGAssetManager>();
	audioSystem = std::make_unique<GGFMODWrapper>();
	luaVM = nullptr; // Will be initialized in Init()
	eventBus = nullptr; // Will be initialized in Init()
}

GGEngine::~GGEngine() {
	GGInterruptHandler::DestroyInstance();
	// Clear event subscriptions BEFORE destroying Lua VM
	// (subscriptions hold Lua registry references that must be released)
	if (eventBus && luaVM)
	{
		eventBus->ClearAllSubscriptions(luaVM->GetState());
	}

	// Cleanup Lua VM first (before worlds/ECS are destroyed)
	if (luaVM)
	{
		luaVM.reset();
	}

	if (eventBus)
	{
		eventBus.reset();
	}

	if (assetManager)
	{
		assetManager->Shutdown();
	}

	if (audioSystem)
	{
		audioSystem->Shutdown();
	}

	// After the networking sessions are gone, so no transport is still holding
	// a Steam connection handle when the API context disappears.
	GGNetworking::SteamPlatform::Shutdown();

	GGNetworking::NetworkUtils::ShutdownWindowsNetworking();

	Grid::GridManager::Destroy();

	delete g_pathfinding;
}

/*************************************************************************
  \brief
	Entry point for starting the engine. Initializes subsystems,
	loads configuration files, and begins the main game loop.

  \return
	True if the engine started successfully, false otherwise.

  \throws
	AssetFailedToLoadException if core assets fail to initialize.
*************************************************************************/
bool GGEngine::StartEngine() {
	if (!core.Load()) return false;
	if (!core.Init()) return false;

	core.Update();

	GGLogger::Info("[GGEngine] End of Update()");

	return true;
}

/*************************************************************************
  \brief
	Loads engine subsystems and prepares core resources prior to initialization.

  \return
	True if all subsystems loaded successfully, false otherwise.
*************************************************************************/
bool GGEngine::Load() {
	if (!Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().Load())
	{
		GGLogger::Info("[GGEngine] Failed to load RenderManager!");
		return false;
	}

	return true;
}

/*************************************************************************
  \brief
	Initializes all core engine systems including rendering, ECS world,
	audio, asset management, editor systems, and input.

  \details
	- Sets up the window and OpenGL context.
	- Initializes ImGui and debug systems.
	- Loads entity types, prefabs, textures, and shaders.
	- Registers ECS component types in the world.
	- Prepares built-in editor tools and managers.

  \return
	True if initialization succeeded, false otherwise.
*************************************************************************/
bool GGEngine::Init() {
	if (!Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to init RenderManager!");
		return false;
	}

	// Initialize interrupt handler right after window creation
	// This enables ALT-TAB detection immediately
	{
		auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
		GLFWwindow* window = renderMgr.GetMainWindow().GetGLFWwindow();

		if (window) {
			GGInterruptHandler::GetInstance()->SetWindow(window);
			if (!GGInterruptHandler::GetInstance()->Init()) {
				std::cerr << "[GGEngine] WARNING: Interrupt handler initialization failed" << std::endl;
				GGLogger::Info("[GGEngine] WARNING: Interrupt handler initialization failed");
				// Non-fatal - continue without interrupt handling
			}
			else {
				GGLogger::Info("[GGEngine] Interrupt handler initialized (ALT-TAB detection active)");
				std::cout << "[GGEngine] Interrupt handler initialized (ALT-TAB detection active)" << std::endl;
			}

			// Stop Windows from reading gameplay input as an accessibility
			// shortcut - five Shift taps for Sticky Keys, a long right-Shift
			// hold for Filter Keys, a long Num Lock hold for Toggle Keys. All
			// three throw a modal dialog over a fullscreen game, and Sticky
			// Keys can leave modifiers latched afterwards. Required for the
			// Steam submission because the lobby name field takes Shift.
			//
			// Save() before Disable() captures what the player actually has
			// configured; features they have deliberately switched on are left
			// working. From here the state follows window focus, via
			// GGInterruptHandler::OnWindowFocusCallback, and is put back in the
			// shutdown path at the end of Update().
			GGAccessibilityShortcuts::Save();
			GGAccessibilityShortcuts::Disable();
		}
	}

	// Shutdown events - differentiating between editor and release builds
	shutdownEventHandler = [&]()
		{
#ifndef GG_EDITOR_BUILD
			auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
			GLFWwindow* window = renderMgr.GetMainWindow().GetGLFWwindow();
			glfwSetWindowShouldClose(window, true);
#endif
		};

	GlobalEventBus::shutdownEvent += shutdownEventHandler;

	if (!Systems::GetSystemsRegistry().GetSystem<Systems::TimerManager>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to init TimerManager!");
		return false;
	}

	if (!GGNetworking::NetworkUtils::InitWindowsNetworking())
	{
		GGLogger::Info("[GGEngine] Failed to init Windows Networking!");
		return false;
	}

	// Steam, at startup rather than when the player first picks multiplayer.
	// Three things depend on it happening this early:
	//
	//   - The overlay only hooks a process that holds a live Steam API context.
	//     Initialising lazily means Shift+Tab does nothing until someone opens
	//     the multiplayer menu, which reads as "the overlay is broken".
	//   - InitRelayNetworkAccess needs several seconds of lead time. Starting
	//     it now hides that latency behind the main menu instead of paying it
	//     at connect time.
	//   - GameLobbyJoinRequested_t is delivered shortly after launch when a
	//     player accepts a friend invite. No API context at that moment means
	//     the invite is silently dropped - the exact failure the integration
	//     guide flags as hard to diagnose.
	//
	// Deliberately NOT fatal. A false return is the ordinary "no Steam client,
	// or not logged in, or a build without the SDK" case, and the game still
	// plays over the Winsock transport.
	if (!GGNetworking::SteamPlatform::Init())
	{
		GGLogger::Info("[GGEngine] Steam unavailable; multiplayer will use the Winsock transport");
	}

	// Initialize asset manager
	if (!assetManager->Initialize())
	{
		GGLogger::Info("[GGEngine] Failed to init AssetsManager!");
		return false;
	}

	// Initialize FMOD audio system
	if (audioSystem && !audioSystem->Initialize())
	{
		GGLogger::Info("[GGEngine] Failed to init Audio System!");
		return false;
	}

	//assetManager->RunAssetTests(); // Run asset tests and test cached audio playback

	/*if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSAudio>().Init())
	{
		std::cout << "ERROR: Failed to initialize audio ecs system" << std::endl;
		return false;
	}*/

	// Note from Zaf:
/* LuaJIT (JIT enabled):
 - Performance: ~10-50x faster than interpreter (CPU-intensive scripts)
 - Determinism: NON-DETERMINISTIC - different machines produce different results
 - Reason: JIT compiler makes optimization decisions based on runtime profiling,
		   CPU architecture, and memory layout (varies per machine/run)
 - Use case: Single-player games, non-networked gameplay

 LuaJIT (JIT disabled - CURRENT CHOICE):
 - Performance: ~2-3x faster than vanilla Lua 5.4
 - Determinism: DETERMINISTIC - bit-exact execution across all clients
 - Reason: Pure bytecode interpreter, no runtime optimization variability
 - Trade-off: Slower than JIT, but required for lockstep multiplayer
 - Bonus: Better performance than vanilla Lua due to optimized interpreter

 Vanilla Lua 5.4:
 - Performance: Baseline (slowest of the three)
 - Determinism: DETERMINISTIC - pure interpreter
 - Drawbacks: Slower interpreter, breaking C API changes from 5.1, less ecosystem support

 Decision: LuaJIT with JIT disabled gives us deterministic execution for
 multiplayer networking while maintaining better performance than vanilla Lua.
 Can enable JIT later for single-player/non-networked scenarios if needed.*/

#pragma region Lua System Initialization
 // IMPORTANT: Lua VM must be initialized BEFORE EditorManager.Init()
 // because the editor's AssetManagerNode calls PreloadAllAssets which
 // may attempt to load Lua scripts via GetLuaScript()

 // Initialize Event Bus for Lua event system
	eventBus = std::make_unique<GGEventBus>();
	std::cout << "[Lua] Event bus initialized" << std::endl;

	// Initialize Lua VM (JIT disabled for deterministic networking)
	luaVM = std::make_unique<GGLuaVM>(false); // false = JIT off
	std::cout << "[Lua] VM initialized (JIT disabled for determinism)" << std::endl;
#pragma endregion

#pragma region Lua Bindings Registration
	// Register all Lua bindings
	RegisterAllLuaBindings(*luaVM, GetActiveWorld(), *eventBus);
	GGLuaDebug::RegisterDebugBindings(luaVM->GetState());
#pragma endregion

#ifdef GG_EDITOR_BUILD
	if (!Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().Init())
	{
		std::cout << "ERROR: Failed to initialize Editor System" << std::endl;
		return false;
	}
#endif

#pragma region Grid System Initialization

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to Init ECSGrid!");
		return false;
	}

#pragma endregion

#pragma region Lua Script Loading and Initialization
	// Set custom error callback
	luaVM->SetErrorCallback([](const std::string& msg, const std::string& path)
		{
			std::cerr << "[Lua Error] " << path << ": " << msg << std::endl;
		});

	// Execute test script with success feedback
	std::cout << "[Lua] === Loading Lua Script ===" << std::endl;
#ifndef GG_EDITOR_BUILD
	if (luaVM->ExecuteFile("../../../Assets/Scripts/test_event_driven.lua"))
	{
		//std::cout << "[Lua] SUCCESS: Script loaded successfully!" << std::endl;

		// Call Start() function once for initialization
		CallLuaFunction("Start");
	}
	else
	{
		std::cout << "[Lua] FAILED: Script execution failed (see error above)" << std::endl;
	}
#endif
#pragma endregion

#ifndef GG_EDITOR_BUILD
	// Build asset base path from config
	const std::string projectName = GGEngineConfig::GGDefaultGameSettings::GetProjectName();
	const std::string assetBasePath = "../../../GGProjects/" + projectName + "/Assets/";
	const std::string prefabsDir = assetBasePath + "Prefabs/";

	assetManager->SetAssetBasePath(assetBasePath);
	// scan assets first
	assetManager->ScanAssetDirectories();

	// assign UUIDs without overwriting existing functions
	assetManager->RegisterAssetsWithUUIDs();

	// DO NOT uncomment this. Init() is already called below after PreloadAllAssets().
	// Having it here too means all the GlobalEventBus callbacks get registered twice,
	// so things like play mode and asset reload fire their handlers twice per event.
	// Confirmed that "[ECSSpriteAnimator] Init() called" printed twice on startup.
	
	//// Ref to ECSSpriteAnimator
	//auto& animSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();

	//// Initialise
	//if (!animSystem.Init()) {
	//	std::cout << "ERROR: Failed to initialize Animation System" << std::endl;
	//	return false;
	//}

	// Preload all textures into cache
	assetManager->PreloadAllAssets();
#endif

#ifndef GG_EDITOR_BUILD
	auto& prefabMgr = GetPrefabManager();

	//std::string prefabsDirectory = "../../../GGProjects/Order&Chaos/Assets/Prefabs/";
	int loadedPrefabCount = 0;
	int failedPrefabCount = 0;


	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(prefabsDir))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".prefab")
			{
				std::string prefabPath = entry.path().string();
				if (prefabMgr.LoadPrefabsFromFile(prefabPath))
				{
					loadedPrefabCount++;
				}
				else
				{
					std::cerr << "WARNING: Failed to load prefab: " << prefabPath << std::endl;
					failedPrefabCount++;
				}
			}
		}

		std::cout << "[GGEngine] Loaded " << loadedPrefabCount << " prefab files from "
			<< prefabsDir << " (Total prefabs: " << prefabMgr.PrefabCount() << ")" << std::endl;

		if (failedPrefabCount > 0)
		{
			std::cerr << "WARNING: " << failedPrefabCount << " prefab files failed to load!" << std::endl;
		}

		if (loadedPrefabCount == 0)
		{
			std::cerr << "WARNING: No .prefab files found in " << prefabsDir << std::endl;
			std::cerr << "Prefab spawning from Lua (Prefab.spawn) will not work!" << std::endl;
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cerr << "ERROR: Failed to scan prefabs directory: " << e.what() << std::endl;
		std::cerr << "Prefab spawning from Lua (Prefab.spawn) will not work!" << std::endl;
	}

	auto& assetSystem = GGAssetID::GetGGAssetID();
	for (const auto& path : assetManager->GetDiscoveredAudioPaths()) {
		if (auto uuidOpt = assetSystem->GetUUIDFromPath(path)) {
			GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuidOpt);
		}
	}

	TestFontAtlas();
#endif

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSAudio>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to init ECSAudio!");
		return false;
	}

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSTransform>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to Init TransformSystem!");
		return false;
	}

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to Init ECSHierarchy!");
		return false;
	}

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSPhysics>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to Init PhysicsSystem!");
		return false;
	}

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to Init AnimationSystem!");
		return false;
	}

	if (!Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().Init())
	{
		GGLogger::Info("[GGEngine] Failed to Init ScriptingSystem!");
		return false;
	}

	return true;

}

/*************************************************************************
  \brief
	Updates the main game loop, handling ECS systems, rendering, input,
	audio, networking, and frame limiting.

  \details
	- Updates interrupt handler to handle ALT-TAB and focus loss.
	- Updates ECS systems in priority order (Transform, Physics, Script, Audio, Render).
	- Handles networking for server and client.
	- Executes precise 60 FPS frame limiting using busy-wait for deterministic timing.
	- Manages ImGui editor rendering in editor builds.
	- Performs profiling for performance measurement.
*************************************************************************/
void GGEngine::Update() {
	// Main game loop - runs until window close requested
		
	auto& Mgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
	Mgr.CreateGameViewFramebuffer(Mgr.GetMainWindow().GetWindowWidth(),
		Mgr.GetMainWindow().GetWindowHeight());
	Mgr.InitializeGameCamera(
		static_cast<float>(Mgr.GetMainWindow().GetWindowWidth()),
		static_cast<float>(Mgr.GetMainWindow().GetWindowHeight())
	);

#ifndef GG_EDITOR_BUILD
	Mgr.currentMode = Systems::RenderManager::EditorMode::gameView;
	const std::string projectName = GGEngineConfig::GGDefaultGameSettings::GetProjectName();
	const std::string assetBasePath = "../../../GGProjects/" + projectName + "/Assets/";
	const std::string prefabsDir = assetBasePath + "Prefabs/";
	const std::string scenesDir = assetBasePath + "Scenes/";
	// Register all Lua scripts with the ECS asset system.
	// In editor builds, AssetManagerNode::RefreshAssetLists() does this;
	// in ReleaseGame we must do it explicitly so that script metadata
	// (including composition fields like Contains = { ... }) is available
	// before CreateScriptInstance runs.
	{
		auto& assetSystem = GGAssetID::GetGGAssetID();
		for (const auto& path : assetManager->GetDiscoveredLuaPaths())
		{
			if (auto uuidOpt = assetSystem->GetUUIDFromPath(path))
			{
				GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuidOpt);
			}
		}
		std::cout << "[GAME MODE] Registered " << assetManager->GetDiscoveredLuaPaths().size() << " Lua scripts with ECS" << std::endl;
	}

	// ReleaseGame build - automatically load game scene
	std::cout << "[GAME MODE] Loading default game scene..." << std::endl;

	// Use proper file path - make sure this matches your actual file location
	//std::string defaultScenePath = "../../../Assets/Scenes/M3Game.scene";
	std::string defaultSceneName = GGEngineConfig::GGDefaultGameSettings::GetDefaultGameScene();
	std::string defaultScenePath = scenesDir + defaultSceneName + ".scene";

	for (auto const& scene : GGEngineConfig::configDoc["gameSettings"]["requiredScenes"].GetArray())
	{
		if (scene.IsObject() && scene.HasMember("name") && scene["name"].IsString())
		{
			sceneManager.AddNewScene(scenesDir + std::string(scene["name"].GetString()) + ".scene", prefabManager);
		}
	}

	// Check if file exists before loading
	if (!std::filesystem::exists(defaultScenePath)) {
		std::cerr << "[GAME MODE] ERROR: Scene file not found at: " << defaultScenePath << std::endl;
		std::cerr << "[GAME MODE] Current working directory: " << std::filesystem::current_path() << std::endl;
	}
	else if (!sceneManager.AddNewScene(defaultScenePath, prefabManager))
	{
		std::cerr << "[GAME MODE] ERROR: Failed to load scene from " << defaultScenePath << std::endl;
	}
	else
	{
		std::cout << "[GAME MODE] Scene loaded, setting as active..." << std::endl;

		// Set it as the active scene (use just the name without path/extension)
		if (!sceneManager.SetActiveScene(defaultSceneName))
		{
			std::cerr << "[GAME MODE] ERROR: Failed to set DefaultGameScene as active" << std::endl;
		}
		else
		{
			std::cout << "[GAME MODE] DefaultGameScene is now active!" << std::endl;
		}
	}

	// Backup all loaded scenes so RestoreAllScenes() works on the first scene switch
	//sceneManager.BackupAllScenes();

	GlobalEventBus::onPlayModeStart.Emit();
	sceneManager.BackupAllScenes();
#endif


	// Get window reference once for efficiency
	GLFWwindow* window = Mgr.GetMainWindow().GetGLFWwindow();

	while (!glfwWindowShouldClose(Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetMainWindow().GetGLFWwindow())) {
		// ========================================================================
		// PRIORITY 1: Update interrupt handler FIRST (before any game logic)
		// This checks for ALT-TAB, minimize, and focus loss events
		// ========================================================================
		GGInterruptHandler::GetInstance()->Update();

		// ========================================================================
		// PRIORITY 2: Check if system is paused due to OS interrupt
		// If paused, skip ALL game logic to maintain state consistency
		// ========================================================================
		if (GGInterruptHandler::GetInstance()->IsSystemPaused()) {
			// System interrupted (ALT-TAB, minimized, etc.)
			// Only do essential window updates
			glfwPollEvents();
			glfwSwapBuffers(window);

			// Optional: You could render a pause overlay here
			// Mgr.RenderPauseOverlay("PAUSED - ALT-TAB DETECTED");

			// We have to handle CRITICAL systems even during OS interrupts
			// This means that our heartbeat packets must still send periodically
			// Even when OS is interrupting us

			// Steam's callback pump. Every asynchronous Steamworks result -
			// lobby created, lobby list, connection state changed - is
			// delivered from inside this call, so it has to run even while
			// alt-tabbed or a peer's disconnect goes unnoticed until focus
			// returns. Main thread only, once a frame; a no-op when Steam is
			// unavailable.
			GGNetworking::SteamPlatform::RunCallbacks();

			// Server and Client event management
			if (GGNetworking::GetGameServer()->IsRunning())
			{
				GGNetworking::GetGameServer()->Update();
			}
			if (GGNetworking::GetGameClient()->IsRunning())
			{
				GGNetworking::GetGameClient()->Update();
			}
		#ifdef GG_EDITOR_BUILD
			if (ImGui::GetCurrentContext() && ImGui::GetFrameCount() > 0)
				ImGui::EndFrame();
		#endif
			// Skip the rest of this frame
			continue;
		}

		// ========================================================================
		// NORMAL GAME UPDATE - Only runs if not interrupted
		// ========================================================================

		// Mark start of frame for timing measurements
		auto frameStartTime = std::chrono::high_resolution_clock::now();

		// Update delta time and begin performance profiling
		Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().Update();
		Systems::PerformanceProfiler::GetInstance().BeginFrame();

		// Process input events (keyboard, mouse, window events)
		glfwPollEvents();

		auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();

		// Only update ECS systems and render scene content when a scene is loaded
		if (sceneManager.HasActiveScene())
		{
			// Update core game systems in priority order
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().Update(GetActiveWorld());
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSTransform>().Update(GetActiveWorld());
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>().Update(GetActiveWorld());
			Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>().Update();  // Fixed timestep physics
			Systems::GetSystemsRegistry().GetSystem<Systems::TimerManager>().Update();        // Timer callbacks
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSAudio>().Update(GetActiveWorld());

			// Steam's callback pump - see the note in the interrupt path above.
			// Runs whether or not a session exists, because lobby callbacks
			// have to arrive before any transport is created.
			GGNetworking::SteamPlatform::RunCallbacks();

			// Server and Client event management
			if (GGNetworking::GetGameServer()->IsRunning())
			{
				GGNetworking::GetGameServer()->Update();
			}

			if (GGNetworking::GetGameClient()->IsRunning())
			{
				GGNetworking::GetGameClient()->Update();
			}

			// Flush any entities queued for destruction during networking updates
			// (e.g. from RPC handlers) before rendering
			GetActiveWorld().ProcessDeferredDestruction();

			renderMgr.pickingFramebuffer.EnableWriting();
			renderMgr.pickingFramebuffer.Clear();

			render2d::SetPickingMode(true);
			// Render all entities with their ID colors
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSRender>().RenderPicking(GetActiveWorld());
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSRender>().RenderUIPicking(GetActiveWorld());

			render2d::SetPickingMode(false);
			renderMgr.pickingFramebuffer.DisableWriting();

			// Update audio system if available
			if (audioSystem)
			{
				audioSystem->Update();
			}
		}

#ifdef GG_EDITOR_BUILD
		renderMgr.BindGameViewFramebuffer();
#else
		//no binding of framebuffer for imgui in ReleaseGame
#endif

		renderMgr.Update(); // Clear and setup

		[[maybe_unused]] int fbWidth = static_cast<int>(renderMgr.viewBuffer.getWidth());
		[[maybe_unused]] int fbHeight = static_cast<int>(renderMgr.viewBuffer.getHeight());

		if (sceneManager.HasActiveScene())
		{
			// Grid System Render
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().Update(GetActiveWorld());
			// Sprite Animator Update
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>().Update(GetActiveWorld());
			// ECS Render call
			Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSRender>().Update(GetActiveWorld());

			static bool hasRun = false;
			static double startTime = glfwGetTime();
			// for testing performance for text
			/*if (!hasRun && glfwGetTime() - startTime >= 10.0)
			{
				Systems::GetECSSystemsRegistry()
					.GetSystem<Systems::ECSRender>()
					.SampleTextTest(GetActiveWorld());

				hasRun = true;
			}*/

			renderMgr.RenderFontOverlays();
		}

#ifdef GG_EDITOR_BUILD
		renderMgr.UnbindGameViewFramebuffer();
		Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().Update();
#else
		// No editor in ReleaseGame build, we render the framebuffer directly
#endif

		Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().Update();

		// Present rendered frame to screen
		glfwSwapBuffers(Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetMainWindow().GetGLFWwindow());

		// Profile frame limiting
		auto limitStart = std::chrono::high_resolution_clock::now();

		// Frame limiting - busy-wait for precise 60 FPS
		auto& timeManager = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
		double targetFrameTime = 1.0 / static_cast<double>(timeManager.GetTargetFPS());

		auto now = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration<double>(now - frameStartTime).count();

		// If frame completed early, busy-wait until target frame time reached
		// Uses 100% of one CPU core but provides microsecond-precision timing
		// Btw, this is critical for deterministic multiplayer network synchronization
		if (elapsed < targetFrameTime)
		{
			while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - frameStartTime).count() < targetFrameTime)
			{
				// Empty by design: Busy-wait loop for precise frame timing
				//
				// Why this loop is empty:
				// The loop body does NO work intentionally. It just repeatedly checks the current time
				// thousands of times per millisecond until exactly 16.67ms has passed since frameStartTime.
				//
				// Each iteration:
				// 1. Gets current time with microsecond precision
				// 2. Subtracts frameStartTime to get elapsed time
				// 3. Compares: "Have we reached 16.67ms yet?"
				// 4. If NO: loop again immediately (takes ~0.0001ms per check)
				// 5. If YES: exit and start next frame
				//
				// Why not use std::this_thread::sleep_for()?
				// Sleep would save CPU (thread yields to OS), but on Windows it has poor precision:
				// - sleep_for(8ms) often sleeps 1ms, 7ms, or 15ms - very unpredictable
				// - This causes FPS to vary wildly: 45, 52, 68, 61, 50 FPS instead of stable 60
				// - Even with timeBeginPeriod(1), sleep accuracy is only plus/minus 1-2ms
				//
				// This empty busy-wait burns CPU but achieves plus/minus 0.01ms accuracy.
				// For multiplayer networking requiring deterministic timing, precision > CPU efficiency.
			}
		}

		auto limitEnd = std::chrono::high_resolution_clock::now();
		Systems::PerformanceProfiler::GetInstance().RecordSystemTime(
			"FrameLimiter",
			std::chrono::duration<double>(limitEnd - limitStart).count()
		);

		// Complete profiling measurements for this frame - call AFTER frame limiter
		Systems::PerformanceProfiler::GetInstance().EndFrame();
	}

#ifdef GG_EDITOR_BUILD
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
#endif
	// Hand the player back their Sticky Keys / Filter Keys / Toggle Keys
	// settings exactly as we found them. These are process-wide OS settings:
	// skipping this leaves the accessibility shortcuts suppressed system-wide
	// after the game has closed.
	GGAccessibilityShortcuts::Restore();

	// Cleanup GLFW resources before exit
	glfwTerminate();

	Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().Shutdown();
}

void GGEngine::ResetLuaVM()
{
	luaVM.reset();
}

void GGEngine::InitLuaVM([[maybe_unused]] bool jitEnabled)
{
	
	luaVM = std::make_unique<GGLuaVM>(false);
	RegisterAllLuaBindings(*luaVM, GetActiveWorld(), *eventBus);
	GGLuaDebug::RegisterDebugBindings(luaVM->GetState());
}

/*************************************************************************
  \brief
	Returns a reference to the global GGEngine instance.

  \return
	Reference to the core GGEngine singleton instance.
*************************************************************************/
GGEngine& GetEngineInstance()
{
	return GGEngine::GetEngine();
}

#pragma region Lua Public Helper Methods
/*************************************************************************
  \brief
	Calls a Lua function by name with optional string arguments.

  \param functionName
	Name of the Lua function to call.

  \param args
	Vector of string arguments to pass to the Lua function.

  \details
	- Checks if the Lua VM is initialized.
	- Pushes each argument onto the Lua stack.
	- Executes the function and handles any runtime errors.
*************************************************************************/
void GGEngine::CallLuaFunction(const std::string& functionName, const std::vector<std::string>& args)
{
	if (!luaVM) return;

	lua_State* L = luaVM->GetState();
	lua_getglobal(L, functionName.c_str());

	if (!lua_isfunction(L, -1))
	{
		lua_pop(L, 1);
		return; // Function doesn't exist (this is OK for optional callbacks)
	}

	// Push arguments
	for (const auto& arg : args)
	{
		lua_pushstring(L, arg.c_str());
	}

	// Call function
	if (lua_pcall(L, static_cast<int>(args.size()), 0, 0) != 0)
	{
		std::cerr << "[Lua Error] Failed to call " << functionName << ": "
			<< lua_tostring(L, -1) << std::endl;
		lua_pop(L, 1);
	}
}

#pragma endregion