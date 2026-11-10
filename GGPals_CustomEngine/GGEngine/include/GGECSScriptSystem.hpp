/*************************************************************************
  \file       GGECSScriptSystem.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.1
  \brief
	Declares the ECS script system responsible for managing Lua-based
	script components.

  \details
	Provides functionality for:
	  - Registering and loading script assets.
	  - Creating, updating, and destroying script instances.
	  - Managing metadata for script types.
	  - Handling script events such as OnClick, OnDrag, and OnDrop.
	  - Integrating script lifecycle callbacks with the ECS world.

  \dependencies
	Requires GGECSInterface, GGSystemInterface, GGEvent, GGScriptAsset,
	and standard containers.

  \history
	- 1.0 (10 October 2025): Initial version.
	- 1.1 (19 Mar 2026): Added ClearScriptCache() to support clean project
	  switching without replaying stale script paths from a previous project.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <GGECSInterface.hpp>
#include <GGSystemInterface.hpp>
#include <GGEvent.hpp>

#include <queue>
#include <array>
#include <string>

#include <GGScriptAsset.hpp>
#include <GGECSComponents.hpp>

class GGECSScriptSystem : public SystemInterface
{
public:
	static constexpr size_t MAX_INSTANCES = 16384;
	static constexpr size_t MAX_SCRIPTS = 4096;

	/*************************************************************************
	 \brief
		Initializes the ECS script system.
	 \return
		True if initialization succeeds, false otherwise.
	*************************************************************************/
	bool Init() override;

	/*************************************************************************
	 \brief
		Updates all active scripts in the world.
	 \param[in] world
		Reference to the game world.
	*************************************************************************/
	void Update(World& world) override;

	/*************************************************************************
	 \brief
		Shuts down the ECS script system and releases resources.
	*************************************************************************/
	void Shutdown();

	/*************************************************************************
	 \brief
		Initializes the pool of inactive script instances.
	*************************************************************************/
	void InitScriptInstances();

	/*************************************************************************
	 \brief
		Registers a Lua script file.
	 \param[in] filePath
		Path to the script file.
	*************************************************************************/
	void RegisterScript(std::string const& filePath);

	/*************************************************************************
	 \brief
		Validates all composition dependencies after all scripts are registered.
		Detects circular references and missing composed types.
	 \return
		True if all dependencies are valid, false if errors were found.
	*************************************************************************/
	bool ValidateCompositionDependencies();

	/*************************************************************************
	 \brief
		Retrieves the script asset for a given file path.
	 \param[in] filePath
		Path to the script file.
	 \return
		Reference to the script asset, or a null asset if not found.
	*************************************************************************/
	GGScriptAsset const& GetScriptAsset(std::string const& filePath);

	/*************************************************************************
	 \brief
		Creates a new script instance for an entity.
	 \param[in] world
		Reference to the game world.
	 \param[in] entity
		Entity to attach the script instance to.
	 \return
		Index of the new script instance.
	*************************************************************************/
	size_t CreateScriptInstance(World&, Entity&);

	/*************************************************************************
	 \brief
		Destroys a script instance attached to an entity.
	 \param[in] world
		Reference to the game world.
	 \param[in] entity
		Entity whose script instance will be destroyed.
	*************************************************************************/
	void DestroyScriptInstance(World& world, Entity& entity);

	/*************************************************************************
	 \brief
		Retrieves a script instance by index.
	 \param[in] index
		Index of the script instance.
	 \return
		Pointer to the script instance, or nullptr if invalid.
	*************************************************************************/
	EntityInstance* GetScriptInstance(size_t index);

	/*************************************************************************
	 \brief
		Sets an integer variable for a script instance.
	 \param[in] instance
		Index of the script instance.
	 \param[in] index
		Index of the integer variable.
	 \param[in] name
		Name of the variable.
	 \param[in] value
		Value to set.
	*************************************************************************/
	void SetIntVar(size_t instance, size_t index, const char* name, int32_t value);

	/*************************************************************************
	 \brief
		Gets an integer variable value from a script instance.
	 \param[in] instance
		Index of the script instance.
	 \param[in] index
		Index of the integer variable.
	 \return
		Value of the integer variable, or 0 if invalid.
	*************************************************************************/
	int32_t GetIntVar(size_t instance, size_t index) const;

	/*************************************************************************
	 \brief
		Binds all script instances on scene switch.
	 \param[in] world
		Reference to the game world.
	*************************************************************************/
	void BindInstanceOnSceneSwitch(World&);

	/*************************************************************************
	 \brief
		Stores new metadata for a script type.
	 \param[in] typeName
		Name of the script type.
	 \param[in] metadata
		Metadata to store.
	*************************************************************************/
	void StoreNewMetadata(std::string const& typeName, GGScriptMetadata metadata);

	/*************************************************************************
	 \brief
		Builds the field layout for a script metadata.
	 \param[in,out] meta
		Metadata whose field layout will be built.
	*************************************************************************/
	void BuildFieldLayout(GGScriptMetadata& meta);

	/*************************************************************************
	 \brief
		Retrieves metadata for a script type.
	 \param[in] typeName
		Name of the script type.
	 \return
		Pointer to the script metadata, or nullptr if not found.
	*************************************************************************/
	GGScriptMetadata* GetMetadata(std::string const& typeName);

	/*************************************************************************
	 \brief
		Executes the OnHoverEnter callback for a script attached to an entity.
	 \param[in] entity
		Entity being hovered.
	 \param[in,out] script
		Script component to execute.
	*************************************************************************/
	void ExecuteOnHoverEnter(Entity entity, ScriptComponent& script);

	/*************************************************************************
	 \brief
		Executes the OnHoverExit callback for a script attached to an entity.
	 \param[in] entity
		Entity no longer being hovered.
	 \param[in,out] script
		Script component to execute.
	*************************************************************************/
	void ExecuteOnHoverExit(Entity entity, ScriptComponent& script);

	/*************************************************************************
	 \brief
		Executes OnClick for a script attached to an entity.
	 \param[in] entity
		Entity with the script.
	 \param[in,out] script
		Script component to execute.
	 \param[in] xPos
		X-coordinate of click.
	 \param[in] yPos
		Y-coordinate of click.
	*************************************************************************/
	void ExecuteScriptClick(Entity entity, ScriptComponent& script, double xPos, double yPos);

	/*************************************************************************
	 \brief
		Executes OnDrag for a script attached to an entity.
	 \param[in] entity
		Entity with the script.
	 \param[in,out] script
		Script component to execute.
	 \param[in] xPos
		X-coordinate of drag.
	 \param[in] yPos
		Y-coordinate of drag.
	*************************************************************************/
	void ExecuteScriptDrag(Entity entity, ScriptComponent& script, double xPos, double yPos);

	/*************************************************************************
	 \brief
		Executes OnDrop for a script attached to an entity.
	 \param[in] entity
		Entity with the script.
	 \param[in,out] script
		Script component to execute.
	 \param[in] xPos
		X-coordinate of drop.
	 \param[in] yPos
		Y-coordinate of drop.
	*************************************************************************/
	void ExecuteScriptDrop(Entity entity, ScriptComponent& script, double xPos, double yPos);

	/*************************************************************************
	 \brief
		Invokes a named Lua function on a script instance with no arguments.
	 \param[in] entity
		Target entity.
	 \param[in] world
		Reference to the game world.
	 \param[in] funcName
		Name of the Lua function to invoke.
	*************************************************************************/
	void InvokeVoidFunction(Entity entity, World& world, std::string const& funcName);

	/*************************************************************************
	 \brief
		Requests a scene change to occur on the next frame.
	 \param[in] sceneName
		Name of the scene to switch to.
	*************************************************************************/
	void SetNextFrameScene(std::string const& sceneName);

	/*************************************************************************
	 \brief
		Marks the script system as entering a scene transition.
	*************************************************************************/
	void BeginSceneTransition();

	/*************************************************************************
	 \brief
		Marks the script system as exiting a scene transition.
	*************************************************************************/
	void EndSceneTransition();

	void ReloadScriptsWithoutRestore();

	/*************************************************************************
	 \brief
		Tears down all script instances, metadata, and caches without
		replaying previously registered script paths.

	 \details
		Use this when switching projects. ReloadScriptsWithoutRestore()
		replays compiledScriptsCache, which re-registers the old project's
		absolute script paths under the new base path. ClearScriptCache()
		discards the cache entirely so stale paths are never replayed.
		New project scripts are picked up lazily on first access.
	*************************************************************************/
	void ClearScriptCache();

private:
	bool isSceneTransitioning = false;

	Entity lastSelectedEntity{};

	std::string nextFrameScene{};

	GGScriptAsset nullAsset{};
	GGScriptMetadata nullMetadata{};

	std::array<std::vector<int32_t>, MAX_INSTANCES> instanceIntVars; // Integer variables per instance
	std::array<std::vector<std::string>, MAX_INSTANCES> instanceVarNames; // Variable names per instance

	std::vector<std::string> compiledScriptsCache; // Cache of compiled scripts
	std::unordered_map<std::string, GGScriptAsset> scriptAssets; // Loaded script assets
	std::unordered_map<std::string, std::unique_ptr<GGScriptMetadata>> scriptMetadata; // Metadata per script

	EntityInstance* instances[MAX_INSTANCES]{ nullptr }; // Active script instances

	std::queue<size_t> inactiveInstances; // Queue of available instance slots

	Callback<World&, Entity> onEntityDestroyedCallback;
	Callback<World&, World&> onScriptInitCallback;
	Callback<World&, World&> onScriptStartCallback;
	Callback<World&, World&> onSwitchSceneCallback;

	Callback<> onPlayModeCallback; // Callback when entering play mode
	Callback<> onStopModeCallback; // Callback when stopping play mode

	Callback<> onScriptRecompileCallback; // Callback when scripts are recompiled

	Callback<World&, World&> resetSelectedEntity;

	std::function
		<void(Entity,
			ScriptComponent&)> executeScriptInit;

	std::function
		<void(Entity,
			ScriptComponent&)> executeScriptStart;

	std::function // Executes script update
		<void(Entity,
			ScriptComponent&)> executeScriptUpdate;

	std::function // Creates script instances
		<void(Entity,
			ScriptComponent&)> createInstances;
};