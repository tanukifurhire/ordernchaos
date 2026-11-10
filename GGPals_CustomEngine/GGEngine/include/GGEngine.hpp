/*************************************************************************
  \file       GGEngine.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       22 September 2025
  \version    2.1
  \brief
	Core game engine class with event system integration
  \details
	Central orchestrator for all engine subsystems including ECS, physics,
	rendering, audio, scripting (Lua), and event-driven gameplay. Manages
	the main game loop and coordinates system updates.
  \dependencies
	GGShader.hpp, GGAssetManager.hpp, GGECSPhysics.hpp,
	GGECSMeshRenderer.hpp, GGPrefabManager.hpp, GGECSWorld.hpp,
	GGEngineRender.hpp, GGEventBus.hpp, GGLuaDebug.hpp
  \see
	SystemInterface, SystemManagerInterface, SystemContainer,
	SystemPriorityRegister, World, GGEventBus, GGLuaVM
  \history
	- 1.0 (Initial): Basic engine structure with manual system management
	- 2.0 (22 September 2025): Advanced template metaprogramming system registry
	- 2.1 (1 November 2025): Added event system and enhanced Lua integration
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGShader.hpp>
#include <GGAssetManager.hpp>
#include <GGTime.hpp>
#include <GGPrefabManager.hpp>
#include <GGECSWorld.hpp>
#include <GGObjectsfactory.hpp>
#include <GGSystemDebug.hpp>
#include <GGAudioManager.hpp>
#include <GGInputManager.hpp>
#include <GGEditorInterface.hpp>
#include <GGSceneManager.hpp>
#include "GGLuaVM.hpp"
#include "GGLuaBindings.hpp"

// Forward declarations for event system
class GGEventBus;

/*************************************************************************
  \brief
	Central game engine orchestrator with advanced system management
  \details
	Manages initialization and updating of all engine subsystems. Provides
	centralized access to major components like asset management, audio,
	physics, and scripting. Now includes event-driven architecture for
	turn-based gameplay and network synchronization.

  \par Usage Pattern
  \code
	// Engine initialization
	GGEngine::StartEngine();

	// Access event system
	GGEngine::GetEngine().GetEventBus()->EmitLocal("GameStart", params);
  \endcode
*************************************************************************/
class GGEngine {
public:

	GGEngine();

	~GGEngine();

	/*************************************************************************
	  \brief
		Entry point for starting the engine. Initializes subsystems,
		loads configuration files, and begins the main game loop.

	  \return
		True if the engine started successfully, false otherwise.

	  \throws
		AssetFailedToLoadException if core assets fail to initialize.
	*************************************************************************/
	static bool StartEngine();

	static float deltaTime;

	GGAssetManager* GetAssetManager() const
	{
		return assetManager.get();
	}

	GGFMODWrapper* GetAudioSystem() const
	{
		return audioSystem.get();
	}

	static GGEngine& GetEngine()
	{
		return core;
	}

	/*************************************************************************
	  \brief
		Gets the event bus for event-driven gameplay.
	  \return
		Pointer to the global event bus.
	  \details
		Provides access to the event system for emitting and subscribing
		to game events. Used for turn-based logic and network synchronization.
	*************************************************************************/
	GGEventBus* GetEventBus() const
	{
		return eventBus.get();
	}

	/*************************************************************************
	  \brief
		Gets the Lua VM for scripting.
	  \return
		Pointer to the Lua virtual machine.
	  \details
		Provides access to the Lua scripting system for executing scripts
		and managing game logic written in Lua.
	*************************************************************************/
	GGLuaVM* GetLuaVM() const
	{
		return luaVM.get();
	}

	/*************************************************************************
	  \brief
		Resets the active Lua VM
	  \details
		Cleans up Lua VM and clears resources
	*************************************************************************/
	void ResetLuaVM();

	/*************************************************************************
	  \brief
		Initializes the Lua VM
	  \details
		Initializes Lua VM with the option to enable jit.
	*************************************************************************/
	void InitLuaVM(bool jitEnabled);

#pragma region Lua Helper Methods
	/*************************************************************************
	  \brief
		Call a Lua function with string arguments
	  \param 
		functionName Name of the Lua function to call
	  \param 
		args Optional vector of string arguments
	*************************************************************************/
	void CallLuaFunction(const std::string& functionName, const std::vector<std::string>& args = {});

	/*************************************************************************
	  \brief
		Call a Lua function with variardic template arguments
	  \param
		functionName Name of the Lua function to call
	  \param
		args Optional variardic arguments
	*************************************************************************/
	template<typename... T>
	void CallLuaFunctionAdvanced(const std::string& functionName, T... args)
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
		int argCount = 0;
		([&] {
			PushLuaValue(L, args);
			++argCount;
		}(), ...);

		// Call function
		if (lua_pcall(L, argCount, 0, 0) != 0)
		{
			std::cerr << "[Lua Error] Failed to call " << functionName << ": "
				<< lua_tostring(L, -1) << std::endl;
			lua_pop(L, 1);
		}
	}

	/*************************************************************************
	  \brief
		Template function to push arguments to Lua
	  \param
		L lua_State state of lua
	  \param
		T&& perfect fowarded value (assuming primitive)
	*************************************************************************/
	template<typename T>
	void PushLuaValue(lua_State* L, T&& value)
	{
		using DecayedT = std::decay_t<T>;

		if constexpr (std::is_same_v<DecayedT, const char*> ||
			std::is_same_v<DecayedT, std::string>)
		{
			if constexpr (std::is_same_v<DecayedT, std::string>)
				lua_pushstring(L, value.c_str());
			else
				lua_pushstring(L, value);
		}
		else if constexpr (std::is_integral_v<DecayedT>)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(value));
		}
		else if constexpr (std::is_floating_point_v<DecayedT>)
		{
			lua_pushnumber(L, static_cast<lua_Number>(value));
		}
		else if constexpr (std::is_same_v<DecayedT, bool>)
		{
			lua_pushboolean(L, value);
		}
		else
		{
			static_assert(sizeof(T) == 0, "Unsupported type for Lua argument");
		}
	}

	// This allows you to call Lua functions from anywhere in the engine:
	// Example: GGEngine::GetEngine().CallLuaFunction("OnGameWon", {"Player1"});
#pragma endregion

	/*************************************************************************
	\brief
		Gets the Active World.
	\details
		NOTE: THIS IS A TEMPORARY IMPLEMENTATION UNTIL AN ACTUAL SCENE MANAGER
		WITH WORLD MANAGEMENT IS READY.
	*************************************************************************/
	World& GetActiveWorld()
	{
		return sceneManager.GetActiveScene();
	}

	PrefabManager& GetPrefabManager() {
		return prefabManager;
	}

	ObjectsFactory& GetObjectsFactory() {
		return objectsFactory;
	}

	SceneManager& GetSceneManager() {
		return sceneManager;
	}

protected:
	static GGEngine core;

	std::unique_ptr<GGAssetManager> assetManager;

	std::unique_ptr<GGFMODWrapper> audioSystem;

	std::unique_ptr<GGLuaVM> luaVM;

	std::unique_ptr<GGEventBus> eventBus;  // Added for event system

	PrefabManager prefabManager;

	ObjectsFactory objectsFactory;

	SceneManager sceneManager;



	Callback<> shutdownEventHandler;

	bool Load();

	bool Init();

	void Update();
};

GGEngine& GetEngineInstance();