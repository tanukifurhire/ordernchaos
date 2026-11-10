/*************************************************************************
  \file       GGECSScriptSystem.cpp
  \author     Tai Xiao Xuan, Zafir Haqqym
  \par        Email: xiaoxuan.tai@digipen.edu, zafir.h@digipen.edu
  \date       10 October 2025
  \version    1.2
  \brief
    Implements the ECS Script System for managing Lua-driven scripts.

  \details
    Contains the runtime logic for:
      - Script registration, loading, and validation.
      - Script instance creation, initialization, and destruction.
      - Binding metadata layouts and default values.
      - Executing script lifecycle functions (Start, Update).
      - Handling input-driven Lua callbacks (OnClick, OnDrag, OnDrop).
      - Managing script recompilation and scene-switch behavior.

  \dependencies
    Requires GGECSScriptSystem.hpp, ECS/Engine managers, Lua VM bindings,
    and related script asset/metadata utilities.

  \history
    - 1.0 (10 October 2025): Initial version.
    - 1.1 (25 January 2025): Integrated GGMemoryManager for script allocations.
                             Replaced new/delete/malloc/free with memory manager.
    - 1.2 (19 Mar 2026): Added ClearScriptCache() for clean project switching.
                         ReloadScriptsWithoutRestore() replays compiledScriptsCache
                         which re-registers stale absolute paths from the old project.
                         ClearScriptCache() discards the cache entirely.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <GGECSScriptSystem.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>
#include <GGLuaBindings.hpp>
#include <GGLuaExtensions.hpp>
#include <GGEngine.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGLogger.hpp>
#include <GGLuaDefineType.hpp>
#include <GGEventBus.hpp>
#include <GGSystemManagers.hpp>

#include <GGMemoryManager.hpp>
#include <GGScriptAsset.hpp>
#include <GGLayerManager.hpp>

/*************************************************************************
 \brief
    Initialize the ECS Script System, setting up callbacks and script execution mechanisms.

 \return
    true if the system initialized successfully

 \details
    This function performs several key initialization steps for the Script System:
        1. Initializes the pool of script instances.
        2. Sets up callbacks for:
            - Entity destruction
            - Scene switching
            - Play mode start
            - Play mode stop
            - Script recompilation
        3. Sets up execution functions for Start and Update of scripts, which will be
           called for each entity with a ScriptComponent during the game loop.
        4. Registers all callbacks to the global event bus.

 \note
    This must be called before any script execution to ensure proper script lifecycle
    management. The function sets up internal lambda callbacks that handle script instance
    creation, Lua VM binding, and error handling during script execution.
 *************************************************************************/
bool GGECSScriptSystem::Init()
{
    InitScriptInstances();

    onEntityDestroyedCallback =
        [&](World& world, Entity entity)
        {
            DestroyScriptInstance(world, entity);
        };

    onSwitchSceneCallback =
        [&](World&, World& world)
        {
            GetEngineInstance().GetAudioSystem()->StopAllSounds();

            SetWorldBinding(*GetEngineInstance().GetLuaVM(), world);

            lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
            GetEngineInstance().GetEventBus()->ClearAllSubscriptions(L);

            Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().SetTimeScale(1.f);

            GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

            if (camera) camera->Reset();
        };

    resetSelectedEntity =
        [&](World&, World&)
        {
            lastSelectedEntity = NULL_ENTITY;
        };

    onPlayModeCallback =
        [&]()
        {
            auto& world = GetEngineInstance().GetSceneManager().GetActiveScene();
            SetWorldBinding(*GetEngineInstance().GetLuaVM(), world);

            GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

            if (camera) camera->Reset();

            // Ensure script instances exist before calling Init/Start.
            // In editor builds, instances are created by the InspectorNode;
            // in ReleaseGame builds, this is the only place they get created.
            world.ForEach<ScriptComponent>(std::function<void(Entity entity, ScriptComponent& script)>(
                [&](Entity entity, ScriptComponent& script) {
                if (script.scriptInstance == 0 || instances[script.scriptInstance] == nullptr) {
                    script.scriptInstance = CreateScriptInstance(world, entity);
                }
            }));

            world.ForEach(executeScriptInit);
            world.ForEach(executeScriptStart);

            GlobalEventBus::onSwitchScenes += onScriptInitCallback;
            GlobalEventBus::onSwitchScenes += onScriptStartCallback;
            GlobalEventBus::onSwitchScenes += resetSelectedEntity;
        };

    // Cleans up all subscription information
    onStopModeCallback =
        [&]()
        {
            GetEngineInstance().ResetLuaVM();

            GetEngineInstance().InitLuaVM(false);

            lua_State* L = GetEngineInstance().GetLuaVM()->GetState();

            // Clear persistent data so stale state (e.g. connection flags)
            // does not leak into the next play session
            lua_pushnil(L);
            lua_setglobal(L, "__persistent_data");

            GetEngineInstance().GetEventBus()->ClearAllSubscriptions(L);

            GetEngineInstance().GetAudioSystem()->StopAllSounds();

            shouldSimulate = false;

            lastSelectedEntity = NULL_ENTITY;

            Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().SetTimeScale(1.f);

            for (auto& it : GetEngineInstance().GetSceneManager().GetSceneMap())
            {
                it.second.ResetWorld();
            }

            for (size_t i{}; i < MAX_INSTANCES; i++)
            {
                if (instances[i]) GGMemoryManager::GetInstance().Free(instances[i]);
                instances[i] = nullptr;
            }

            while (!inactiveInstances.empty()) inactiveInstances.pop();
            InitScriptInstances();

            scriptMetadata.clear();

            std::vector<std::string> scripts = std::move(compiledScriptsCache);

            scriptAssets.clear();

            for (std::string const& script : scripts)
            {
                RegisterScript(script);
            }

            ValidateCompositionDependencies();

            GlobalEventBus::onSwitchScenes -= onScriptInitCallback;
            GlobalEventBus::onSwitchScenes -= onScriptStartCallback;
            GlobalEventBus::onSwitchScenes -= resetSelectedEntity;
        };

    // Create new instances for all scripts in the Engine.
    // Usually called when we reload scenes, hot reload code, etc.
    createInstances =
        [&](Entity entity, ScriptComponent& script)
        {
            auto& world = GetEngineInstance().GetSceneManager().GetActiveScene();
            script.scriptInstance = CreateScriptInstance(world, entity);
        };

    onScriptRecompileCallback =
        [&]()
        {
            auto& prefabManager = GetEngineInstance().GetPrefabManager();
            //auto& sceneManager = GetEngineInstance().GetSceneManager();

            GetEngineInstance().GetSceneManager().BackupAllScenes();

            for (auto& it : GetEngineInstance().GetSceneManager().GetSceneMap())
            {
                it.second.ResetWorld();
            }

            for (size_t i{}; i < MAX_INSTANCES; i++)
            {
                if (instances[i]) GGMemoryManager::GetInstance().Free(instances[i]);
                instances[i] = nullptr;
            }
            while (!inactiveInstances.empty()) inactiveInstances.pop();
            InitScriptInstances();

            scriptMetadata.clear();

            std::vector<std::string> scripts = std::move(compiledScriptsCache);

            scriptAssets.clear();

            for (std::string const& script : scripts)
            {
                if (shouldSimulate && script == "..\\..\\Assets\\Scripts\\PersistentData.lua")
                {
                    continue;
                }
                RegisterScript(script);
            }

            ValidateCompositionDependencies();

            GetEngineInstance().GetSceneManager()
                .RestoreAllScenes(prefabManager);
        };

    // Register scene switch callbacks
    GlobalEventBus::onSwitchScenes += onSwitchSceneCallback;

    // Register Start playing callbacks
    GlobalEventBus::onPlayModeStart += onStartSimulate;
    GlobalEventBus::onPlayModeStart += onPlayModeCallback;

    // Register Stop playing callbacks
    GlobalEventBus::onStopPlayMode += onStopSimulate;
    GlobalEventBus::onStopPlayMode += onStopModeCallback;

    // Regster Pause/Resume callbacks
    GlobalEventBus::onPause += onStopSimulate;
    GlobalEventBus::onResume += onStartSimulate;

    // Register recompile callbacks
    GlobalEventBus::onRecompileScripts += onScriptRecompileCallback;

    // Register Entity Destruction callback
    onEntityDestroyed += onEntityDestroyedCallback;

    executeScriptInit = [&](Entity entity, ScriptComponent& script)
        {

#ifdef GG_EDITOR_BUILD
            auto& layerManager = GGLayerManager::Get();
            if (layerManager.IsEntityDisabled(entity)) {
                return;  // Skip disabled entity
            }
#endif
            if (script.scriptAssetID == 0) return;

            lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
            if (!L)
            {
                GGLogger::Error("CreateScriptInstance: Lua state is null");
                GlobalEventBus::onPause.Emit();
                return;
            }

            // RAII stack guard - ensures stack cleanup on any error
            LuaStackGuard stackGuard(L);

            try
            {
                GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

                int moduleRef = scriptAsset.moduleRef;

                if (moduleRef == LUA_NOREF)
                {
                    return;
                }

                lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

                // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
                lua_pushstring(L, "Init");
                lua_rawget(L, -2);  // Get table[key] without calling metamethods

                if (lua_isfunction(L, -1))
                {
                    // Pass script instance "self"
                    InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

                    if (instances[script.scriptInstance] != nullptr)
                    {
                        userData->instance = instances[script.scriptInstance];

                        lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                        lua_setmetatable(L, -2);

                        uint64_t packed = PackEntity(entity);

                        lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
                    }
                    else
                    {
                        GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                        GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                        lua_pop(L, 1); // Pop function
                        lua_pop(L, 1); // Pop module table
                        return;
                    }

                    // Use protected call
                    int result = lua_pcall(L, 2, 0, 0);

                    if (result != 0)
                    {
                        // Handle Lua error
                        const char* errorMsg = lua_tostring(L, -1);
                        GGLogger::Error("[ScriptSystem] Init error for entity " +
                            std::to_string(entity.id) +
                            ": " +
                            (errorMsg ? errorMsg : "Unknown error")
                        );
                        GlobalEventBus::onPause.Emit();
                        std::cerr << "[ScriptSystem] Init error for entity " << entity.id
                            << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                        lua_pop(L, 1); // Pop error message
                        lua_pop(L, 1); // Pop module table
                        return;
                    }

                    lua_pop(L, 1); // Pop module table
                }
                else
                {
                    // Start not found or not a function
                    GGLogger::Warning("[ScriptSystem] Init not found for entity " +
                        std::to_string(entity.id)
                    );
                    lua_pop(L, 1); // Pop non-function value
                    lua_pop(L, 1); // Pop module table
                }
            }
            catch (const std::exception& e)
            {
                GGLogger::Error("Init: Exception occurred - "
                    + std::string(e.what()));
                GlobalEventBus::onPause.Emit();
                return;
            }
            catch (...)
            {
                GGLogger::Error("Init: Unknown exception occurred");
                GlobalEventBus::onPause.Emit();
                return;
            }
        };

    executeScriptStart = [&](Entity entity, ScriptComponent& script)
        {
#ifdef GG_EDITOR_BUILD
            auto& layerManager = GGLayerManager::Get();
            if (layerManager.IsEntityDisabled(entity)) {
                return;  // Skip disabled entity
            }
#endif
            if (script.scriptAssetID == 0) return;

            lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
            if (!L)
            {
                GGLogger::Error("CreateScriptInstance: Lua state is null");
                GlobalEventBus::onPause.Emit();
                return;
            }

            // RAII stack guard - ensures stack cleanup on any error
            LuaStackGuard stackGuard(L);

            try
            {
                GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

                int moduleRef = scriptAsset.moduleRef;

                if (moduleRef == LUA_NOREF)
                {
                    return;
                }

                lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

                // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
                lua_pushstring(L, "Start");
                lua_rawget(L, -2);  // Get table[key] without calling metamethods

                if (lua_isfunction(L, -1))
                {
                    // Pass script instance "self"
                    InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

                    if (instances[script.scriptInstance] != nullptr)
                    {
                        userData->instance = instances[script.scriptInstance];

                        lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                        lua_setmetatable(L, -2);

                        uint64_t packed = PackEntity(entity);

                        lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
                    }
                    else
                    {
                        GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                        GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                        lua_pop(L, 1); // Pop function
                        lua_pop(L, 1); // Pop module table
                        return;
                    }

                    // Use protected call
                    int result = lua_pcall(L, 2, 0, 0);

                    if (result != 0)
                    {
                        // Handle Lua error
                        const char* errorMsg = lua_tostring(L, -1);
                        GGLogger::Error("[ScriptSystem] Start error for entity " +
                            std::to_string(entity.id) +
                            ": " +
                            (errorMsg ? errorMsg : "Unknown error")
                        );
                        GlobalEventBus::onPause.Emit();
                        std::cerr << "[ScriptSystem] Start error for entity " << entity.id
                            << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                        lua_pop(L, 1); // Pop error message
                        lua_pop(L, 1); // Pop module table
                        return;
                    }

                    lua_pop(L, 1); // Pop module table
                }
                else
                {
                    // Start not found or not a function
                    GGLogger::Warning("[ScriptSystem] Start not found for entity " +
                        std::to_string(entity.id)
                    );
                    lua_pop(L, 1); // Pop non-function value
                    lua_pop(L, 1); // Pop module table
                }
            }
            catch (const std::exception& e)
            {
                GGLogger::Error("Start: Exception occurred - "
                    + std::string(e.what()));
                GlobalEventBus::onPause.Emit();
                return;
            }
            catch (...)
            {
                GGLogger::Error("Start: Unknown exception occurred");
                GlobalEventBus::onPause.Emit();
                return;
            }
        };

    executeScriptUpdate = [&](Entity entity, ScriptComponent& script)
        {
#ifdef GG_EDITOR_BUILD
            auto& layerManager = GGLayerManager::Get();
            if (layerManager.IsEntityDisabled(entity)) {
                return;  // Skip disabled entity
            }
#endif
            if (script.scriptAssetID == 0) return;

            lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
            if (!L)
            {
                GGLogger::Error("CreateScriptInstance: Lua state is null");
                GlobalEventBus::onPause.Emit();
                return;
            }

            // RAII stack guard - ensures stack cleanup on any error
            LuaStackGuard stackGuard(L);

            try
            {
                GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

                int moduleRef = scriptAsset.moduleRef;

                if (moduleRef == LUA_NOREF)
                {
                    return;
                }

                lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

                // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
                lua_pushstring(L, "Update");
                lua_rawget(L, -2);  // Get table[key] without calling metamethods

                if (lua_isfunction(L, -1))
                {
                    // Pass script instance "self"
                    InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

                    if (instances[script.scriptInstance] != nullptr)
                    {
                        userData->instance = instances[script.scriptInstance];

                        lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                        lua_setmetatable(L, -2);

                        uint64_t packed = PackEntity(entity);

                        lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
                    }
                    else
                    {
                        GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                        GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                        lua_pop(L, 1); // Pop function
                        lua_pop(L, 1); // Pop module table
                        return;
                    }

                    // Use protected call
                    int result = lua_pcall(L, 2, 0, 0);

                    if (result != 0)
                    {
                        // Handle Lua error
                        const char* errorMsg = lua_tostring(L, -1);
                        GGLogger::Error("[ScriptSystem] Update error for entity " +
                            std::to_string(entity.id) +
                            ": " +
                            (errorMsg ? errorMsg : "Unknown error")
                        );
                        GlobalEventBus::onPause.Emit();
                        std::cerr << "[ScriptSystem] Update error for entity " << entity.id
                            << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                        lua_pop(L, 1); // Pop error message
                        lua_pop(L, 1); // Pop module table
                        return;
                    }

                    lua_pop(L, 1); // Pop module table
                }
                else
                {
                    // Update not found or not a function
                    lua_pop(L, 1); // Pop non-function value
                    lua_pop(L, 1); // Pop module table
                }
            }
            catch (const std::exception& e)
            {
                GGLogger::Error("Update: Exception occurred - "
                    + std::string(e.what()));
                GlobalEventBus::onPause.Emit();
                return;
            }
            catch (...)
            {
                GGLogger::Error("Update: Unknown exception occurred");
                GlobalEventBus::onPause.Emit();
                return;
            }
        };

    onScriptStartCallback = [&](World&, World& world)
        {
            if (!shouldSimulate) return;

            Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().Update(world);
            world.ForEach(executeScriptStart);
        };

    onScriptInitCallback = [&](World&, World& world)
        {
            if (!shouldSimulate) return;
            Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().Update(world);

            world.ForEach<ScriptComponent>(std::function<void(Entity entity, ScriptComponent& script)>(
                [&](Entity entity, ScriptComponent& script) {
                    if (script.scriptInstance == 0 || instances[script.scriptInstance] == nullptr) {
                        script.scriptInstance = CreateScriptInstance(world, entity);
                    }
                }));

            world.ForEach(executeScriptInit);
        };

    return true;
}

/*************************************************************************
 \brief
    Updates all active scripts in the world and handles input events.
 \param[in] world
    Reference to the current ECS world.
 \details
    Executes script update functions for each entity with a ScriptComponent.
    Handles keyboard and mouse input events, forwarding them to Lua functions.
 *************************************************************************/
void GGECSScriptSystem::Update(World& world)
{
    /*if (isSceneTransitioning) {
        return;
    }*/

    if (shouldSimulate)
    {
        auto& inputManager = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
        [[maybe_unused]] float scale = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().WORLD_TO_SCREEN_SCALE;

        double x = 0, y = 0;
        inputManager.GetMousePosition(x, y);

        // Check for keyboard events using the InputManager's actual methods
        if (inputManager.GGInputCheckTriggered(GGKey_SPACE))
        {
            GetEngineInstance().CallLuaFunction("OnKeyPressed", { "SPACE" });
        }
        if (inputManager.GGInputCheckTriggered(GGKey_ENTER))
        {
            GetEngineInstance().CallLuaFunction("OnKeyPressed", { "ENTER" });
        }
        if (inputManager.GGInputCheckTriggered(GGKey_UP))
        {
            GetEngineInstance().CallLuaFunction("OnKeyPressed", { "UP" });
        }
        if (inputManager.GGInputCheckTriggered(GGKey_DOWN))
        {
            GetEngineInstance().CallLuaFunction("OnKeyPressed", { "DOWN" });
        }
        if (inputManager.GGInputCheckTriggered(GGKey_LEFT))
        {
            GetEngineInstance().CallLuaFunction("OnKeyPressed", { "LEFT" });
        }
        if (inputManager.GGInputCheckTriggered(GGKey_RIGHT))
        {
            GetEngineInstance().CallLuaFunction("OnKeyPressed", { "RIGHT" });
        }

        auto result = inputManager.PickObjectAtScreenPosition({ x, y });

        if (result.IsValid())
        {
            Entity entity = world.GetEntityFromID(result.EntityID);

            if (lastSelectedEntity != entity)
            {
                if (world.IsEntityAlive(lastSelectedEntity))
                {
                    if (world.HasComponent<ScriptComponent>(lastSelectedEntity))
                    {
                        ScriptComponent* fromScript = world.GetComponent<ScriptComponent>(lastSelectedEntity);

                        if (fromScript)
                        {
                            ExecuteOnHoverExit(lastSelectedEntity, *fromScript);
                        }
                    }
                }
            }

            if (entity != NULL_ENTITY)
            {
                if (world.HasComponent<ScriptComponent>(entity))
                {
                    ScriptComponent* script = world.GetComponent<ScriptComponent>(entity);

                    if (script)
                    {
                        if (lastSelectedEntity != entity)
                        {
                            ExecuteOnHoverEnter(entity, *script);
                        }

                        if (inputManager.GGInputCheckTriggered(GGMouse_LEFT))
                        {
                            ExecuteScriptClick(entity, *script, x, y);
                        }

                        if (inputManager.GGInputCheckCurr(GGMouse_LEFT))
                        {
                            ExecuteScriptDrag(entity, *script, x, y);
                        }

                        if (inputManager.GGInputCheckReleased(GGMouse_LEFT))
                        {
                            ExecuteScriptDrop(entity, *script, x, y);
                        }
                    }
                }

                lastSelectedEntity = entity;
            }
        }

        world.ProcessDeferredDestruction();

        world.ForEach(executeScriptUpdate);

        // Resolve scene changes!
        if (!nextFrameScene.empty())
        {
            GetEngineInstance().GetSceneManager().RestoreAllScenes(GetEngineInstance().GetPrefabManager());
            onScriptRecompileCallback();
            GetEngineInstance().GetSceneManager().BackupAllScenes();
            GetEngineInstance().GetSceneManager().SetActiveScene(nextFrameScene);
            nextFrameScene.clear();
        }
    }
}

/*************************************************************************
 \brief
    Shuts down the ECS script system and cleans up all script instances.
 \details
    Iterates over all script instances, deletes allocated memory, and properly destructs string fields.
 *************************************************************************/
void GGECSScriptSystem::Shutdown()
{
    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("[ScriptSystem] Lua state is null!");
        std::cerr << "[ScriptSystem] ERROR: Lua state is null!" << std::endl;
        return;
    }

    for (size_t i{}; i < MAX_INSTANCES; i++)
    {
        if (instances[i] == nullptr) continue;

        if (instances[i]->data)
        {
            char* dataPtr = static_cast<char*>(instances[i]->data);

            for (const auto& field : instances[i]->type->fieldDescriptors)
            {
                std::cout << "Deleting field " << field.name << std::endl;
                if (field.type == FieldDescriptor::TYPE_STRING)
                {
                    void* fieldPtr = dataPtr + field.offset;
                    static_cast<std::string*>(fieldPtr)->~basic_string();
                }
            }

            GGMemoryManager::GetInstance().Free(instances[i]->data);
        }

        // Release Lua fields table reference
        if (instances[i]->luaFieldsRef != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, instances[i]->luaFieldsRef);
        }

        // Call destructor manually
        instances[i]->~EntityInstance();

        // Free the instance memory
        GGMemoryManager::GetInstance().Free(instances[i]);
    }

    // Log memory stats before shutdown completes
    GGMemoryManager::GetInstance().LogStats();
}

/*************************************************************************
 \brief
    Initializes the pool of inactive script instances.
 \details
    Populates the inactiveInstances queue to allow allocation of new script instances.
 *************************************************************************/
void GGECSScriptSystem::InitScriptInstances()
{
    for (size_t i{ 1 }; i < MAX_INSTANCES; i++)
    {
        inactiveInstances.push(i);
    }
}

/*************************************************************************
 \brief
    Registers a Lua script file as a script asset.
 \param[in] filePath
    Path to the Lua script file.
 \details
    Loads and compiles the script, ensures it returns a table, stores module and function references.
    Handles error checking for file loading, compilation, and execution.
 *************************************************************************/
void GGECSScriptSystem::RegisterScript(std::string const& filePath)
{
    // Initialize inactive instances if needed
    if (inactiveInstances.empty() && scriptAssets.empty())
    {
        for (size_t i{ 1 }; i < MAX_INSTANCES; i++)
        {
            inactiveInstances.push(i);
        }
        //std::cout << "[ScriptSystem] Initialized " << (MAX_INSTANCES - 1) << " inactive instances" << std::endl;
    }

    //std::cout << "[ScriptSystem] RegisterScript called for: " << filePath << std::endl;

    // Validate file path
    if (filePath.empty())
    {
        GGLogger::Error("[ScriptSystem] Empty file path provided!");
        std::cerr << "[ScriptSystem] ERROR: Empty file path provided!" << std::endl;
        return;
    }

    // Check if script is already registered
    if (scriptAssets.find(filePath) != scriptAssets.end())
    {
        GGLogger::Warning("[ScriptSystem] Script " + filePath + " already registered, skipping.");
        std::cout << "[ScriptSystem] WARNING: Script " << filePath << " already registered, skipping." << std::endl;
        return;
    }

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("[ScriptSystem] Lua state is null!");
        std::cerr << "[ScriptSystem] ERROR: Lua state is null!" << std::endl;
        return;
    }

    // Save initial stack position for cleanup
    int initialStackTop = lua_gettop(L);

    try
    {
        // Method 1: Use luaL_loadfile + lua_pcall instead of dofile for better error handling
        //std::cout << "[ScriptSystem] Loading script file: " << filePath << std::endl;

        int loadResult = luaL_loadfile(L, filePath.c_str());

        if (loadResult != 0)
        {
            const char* errorMsg = lua_tostring(L, -1);
            GGLogger::Error("[ScriptSystem] COMPILE ERROR in " + filePath + ": "
                + (errorMsg ? errorMsg : "Unknown compile error"));
            std::cerr << "[ScriptSystem] COMPILE ERROR in " << filePath << ":" << std::endl;
            std::cerr << "  " << (errorMsg ? errorMsg : "Unknown compile error") << std::endl;
            lua_settop(L, initialStackTop); // Restore stack
            return;
        }

        //std::cout << "[ScriptSystem] Script compiled successfully, executing..." << std::endl;

        // Execute the script
        int execResult = lua_pcall(L, 0, 1, 0);

        if (execResult != 0)
        {
            const char* errorMsg = lua_tostring(L, -1);
            GGLogger::Error("[ScriptSystem] RUNTIME ERROR executing " + filePath + ": "
                + (errorMsg ? errorMsg : "Unknown runtime error"));
            std::cerr << "[ScriptSystem] RUNTIME ERROR executing " << filePath << ":" << std::endl;
            std::cerr << "  " << (errorMsg ? errorMsg : "Unknown runtime error") << std::endl;
            lua_settop(L, initialStackTop); // Restore stack
            return;
        }

        // Validate that script returned a table
        if (!lua_istable(L, -1))
        {
            int luaType = lua_type(L, -1);
            GGLogger::Error("[ScriptSystem] Script " + filePath + " did not return a table! Returned type: "
                + lua_typename(L, luaType) + ". Scripts must end with 'return ScriptTable'");
            std::cerr << "[ScriptSystem] ERROR: Script " << filePath << " did not return a table!" << std::endl;
            std::cerr << "  Returned type: " << lua_typename(L, luaType) << std::endl;
            std::cerr << "  Scripts must end with 'return ScriptTable'" << std::endl;
            // Do NOT add to compiledScriptsCache -- no scriptAsset was stored,
            // re-registering this path on recompile is pointless and spams errors.
            lua_settop(L, initialStackTop);
            return;
        }

        //std::cout << "[ScriptSystem] Script returned valid table, creating registry reference..." << std::endl;

        // Create module reference - table is consumed by luaL_ref
        int moduleRef = luaL_ref(L, LUA_REGISTRYINDEX);

        if (moduleRef == LUA_NOREF || moduleRef == LUA_REFNIL)
        {
            GGLogger::Error("[ScriptSystem] Failed to create module registry reference for " + filePath);
            std::cerr << "[ScriptSystem] ERROR: Failed to create module registry reference!" << std::endl;
            lua_settop(L, initialStackTop); // Restore stack
            return;
        }

        //std::cout << "[ScriptSystem] Created module reference: " << moduleRef << std::endl;

        // Create and store script asset
        GGScriptAsset scriptAsset;
        scriptAsset.moduleRef = moduleRef;

        // Safely extract function references
        int startRef = LUA_NOREF;
        int updateRef = LUA_NOREF;

        // Get Start function reference
        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef); // Push module table

        if (!lua_istable(L, -1))
        {
            GGLogger::Error("[ScriptSystem] Module reference corrupted for " + filePath);
            std::cerr << "[ScriptSystem] ERROR: Module reference corrupted!" << std::endl;
            luaL_unref(L, LUA_REGISTRYINDEX, moduleRef); // Cleanup
            lua_settop(L, initialStackTop); // Restore stack
            return;
        }

        // Use raw access to avoid metamethod issues
        lua_pushstring(L, "Start");
        lua_rawget(L, -2);

        if (lua_isfunction(L, -1))
        {
            startRef = luaL_ref(L, LUA_REGISTRYINDEX);
            //std::cout << "[ScriptSystem] Stored Start function reference: " << startRef << std::endl;
        }
        else
        {
            //std::cout << "[ScriptSystem] INFO: Start function not found or not a function" << std::endl;
            lua_pop(L, 1); // Pop non-function value
        }

        // Get Update function reference (module table still on stack)
        lua_pushstring(L, "Update");
        lua_rawget(L, -2);

        if (lua_isfunction(L, -1))
        {
            updateRef = luaL_ref(L, LUA_REGISTRYINDEX);
            //std::cout << "[ScriptSystem] Stored Update function reference: " << updateRef << std::endl;
        }
        else
        {
            //std::cout << "[ScriptSystem] INFO: Update function not found or not a function" << std::endl;
            lua_pop(L, 1); // Pop non-function value
        }

        lua_pop(L, 1); // Pop module table

        scriptAsset.startFunctionRef = startRef;
        scriptAsset.updateFunctionRef = updateRef; // You might need to add this field

        scriptAssets[filePath] = scriptAsset;
        compiledScriptsCache.push_back(filePath);

        //std::cout << "[ScriptSystem] SUCCESS: Registered script asset for " << filePath << std::endl;
        //std::cout << "  Module ref: " << scriptAssets[filePath].moduleRef << std::endl;
        //std::cout << "  Start ref: " << scriptAssets[filePath].startFunctionRef << std::endl;
        //std::cout << "  Update ref: " << scriptAssets[filePath].updateFunctionRef << std::endl;

        // Restore stack to initial state
        lua_settop(L, initialStackTop);

    }
    catch (const std::exception& e)
    {
        GGLogger::Error("[ScriptSystem] EXCEPTION in RegisterScript(" + filePath + "): " + e.what());
        std::cerr << "[ScriptSystem] EXCEPTION in RegisterScript: " << e.what() << std::endl;
        lua_settop(L, initialStackTop); // Restore stack
        return;
    }
    catch (...)
    {
        GGLogger::Error("[ScriptSystem] Unknown exception in RegisterScript(" + filePath + ")");
        std::cerr << "[ScriptSystem] UNKNOWN EXCEPTION in RegisterScript!" << std::endl;
        lua_settop(L, initialStackTop); // Restore stack
        return;
    }
}

/*************************************************************************
 \brief
    Validates all composition dependencies after all scripts are registered.
    Detects circular references and missing composed types.
 \return
    True if all dependencies are valid, false if errors were found.
 *************************************************************************/
bool GGECSScriptSystem::ValidateCompositionDependencies()
{
    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L) return false;

    bool valid = true;

    // Tracks types that have been fully walked with no cycle found.
    // Shared across outer iterations so we skip redundant re-traversal when
    // the same type is reachable from multiple roots (diamond dependency).
    std::unordered_set<std::string> fullyVisited;

    for (const auto& [metaTypeName, metaPtr] : scriptMetadata)
    {
        if (metaPtr->compositionFields.empty()) continue;

        // Tracks types on the *current* DFS path only.
        // Cleared via erase on backtrack so diamond deps don't false-positive.
        // A hit here means a real back edge -- genuine cycle.
        std::unordered_set<std::string> inProgress;

        std::function<bool(const std::string&)> detectCycle =
            [&](const std::string& typeName) -> bool
            {
                // Already fully checked through a previous path -- safe, skip.
                if (fullyVisited.count(typeName))
                    return false;

                // On the current DFS stack -- genuine back edge.
                if (inProgress.count(typeName))
                {
                    GGLogger::Error("[ScriptSystem] Circular composition detected! Type '"
                        + typeName + "' forms a cycle starting from '"
                        + metaTypeName + "'.");
                    std::cerr << "[ScriptSystem] ERROR: Circular composition detected! Type '"
                        << typeName << "' forms a cycle starting from '"
                        << metaTypeName << "'." << std::endl;
                    return true;
                }

                inProgress.insert(typeName);

                GGScriptMetadata* checkMeta = GetMetadata(typeName);
                if (!checkMeta)
                {
                    // Unknown type -- treat as terminal, no children to recurse into.
                    inProgress.erase(typeName);
                    fullyVisited.insert(typeName);
                    return false;
                }

                for (const auto& compField : checkMeta->compositionFields)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, checkMeta->defaultValueRef);

                    if (!lua_istable(L, -1))
                    {
                        lua_pop(L, 1);
                        continue;
                    }

                    lua_getfield(L, -1, compField.c_str());

                    // Copy before any further stack ops or recursion.
                    // lua_tostring returns a pointer into Lua memory -- unsafe to hold
                    // across calls that touch the stack.
                    std::string composedType;
                    const char* raw = lua_tostring(L, -1);
                    if (raw) composedType = raw;

                    lua_pop(L, 2); // field value + default table

                    if (composedType.empty()) continue;

                    if (!GetMetadata(composedType))
                    {
                        GGLogger::Warning("[ScriptSystem] Composition field '"
                            + compField + "' in type '" + typeName
                            + "' references unknown type '"
                            + composedType + "'");
                        std::cerr << "[ScriptSystem] WARNING: Composition field '"
                            << compField << "' in type '" << typeName
                            << "' references unknown type '"
                            << composedType << "'" << std::endl;
                        valid = false;
                    }
                    else if (detectCycle(composedType))
                    {
                        // Propagate cycle upward without finishing this node.
                        inProgress.erase(typeName);
                        return true;
                    }
                }

                // Backtrack: remove from current path, mark as fully checked.
                inProgress.erase(typeName);
                fullyVisited.insert(typeName);
                return false;
            };

        if (detectCycle(metaTypeName))
            valid = false;
    }

    return valid;
}

/*************************************************************************
 \brief
    Retrieves the script asset associated with a file path.
 \param[in] filePath
    Path to the Lua script file.
 \return
    Reference to the corresponding GGScriptAsset.
 \details
    If the asset is not already registered, it will be registered automatically.
 *************************************************************************/
GGScriptAsset const& GGECSScriptSystem::GetScriptAsset(std::string const& filePath)
{
    std::string fullPath = GetEngineInstance().GetAssetManager()->ResolvePath(filePath);

    auto it = scriptAssets.find(fullPath);
    if (it != scriptAssets.end())
    {
        return it->second;
    }

    //std::cout << "Register Asset called" << std::endl;

    RegisterScript(fullPath);

    auto it2 = scriptAssets.find(fullPath);
    if (it2 != scriptAssets.end())
    {
        return it2->second;
    }

    return nullAsset;
}

/*************************************************************************
 \brief
    Creates a new script instance for a given entity.
 \param[in] world
    Reference to the ECS world.
 \param[in] entity
    The entity to attach the script instance to.
 \return
    Index of the newly created script instance, or 0 if creation fails.
 \details
    Allocates memory for the instance, initializes fields from defaults,
    and stores the instance for later execution.
 *************************************************************************/
size_t GGECSScriptSystem::CreateScriptInstance(World& world, Entity& entity)
{
    // Early validation
    if (inactiveInstances.empty())
    {
        GGLogger::Error("CreateScriptInstance: No inactive instances available");
        return 0;
    }

    if (!world.HasComponent<ScriptComponent>(entity))
    {
        GGLogger::Error("CreateScriptInstance: Entity does not have ScriptComponent");
        return 0;
    }

    ScriptComponent* script = world.GetComponent<ScriptComponent>(entity);
    if (!script || !script->scriptAssetID)
    {
        GGLogger::Error("CreateScriptInstance: Invalid script or scriptAssetID");
        return 0;
    }

    // Get instance ID upfront (before any allocations)
    size_t instanceID = inactiveInstances.front();

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        return 0;
    }

    // Destroy existing instance if present
    if (script->scriptInstance > 0)
    {
        DestroyScriptInstance(world, entity);
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        // ====================================================================
        // STEP 1: Load script asset and validate
        // ====================================================================

        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(
            script->scriptAssetID
        );

        int moduleRef = scriptAsset.moduleRef;
        if (moduleRef == LUA_NOREF || moduleRef == LUA_REFNIL)
        {
            GGLogger::Error("CreateScriptInstance: Invalid module reference for asset "
                + std::to_string(script->scriptAssetID));
            return 0;
        }

        GGLogger::Info("CreateScriptInstance: moduleRef = "
            + std::to_string(moduleRef)
            + ", assetID = "
            + std::to_string(script->scriptAssetID));

        // ====================================================================
        // STEP 2: Get type name from module
        // ====================================================================

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);
        if (!lua_istable(L, -1))
        {
            GGLogger::Error("CreateScriptInstance: Module reference is not a table");
            lua_pop(L, 1);
            return 0;
        }

        lua_pushstring(L, "__type_name");
        lua_rawget(L, -2);

        if (!lua_isstring(L, -1))
        {
            GGLogger::Error("CreateScriptInstance: __type_name not found or not a string");
            lua_pop(L, 2);
            return 0;
        }

        const char* typeName = lua_tostring(L, -1);
        GGLogger::Info("CreateScriptInstance: Loaded type " + std::string(typeName));

        lua_pop(L, 1);  // Pop __type_name

        // ====================================================================
        // STEP 3: Get and validate metadata
        // ====================================================================

        GGScriptMetadata* meta = GetMetadata(typeName);
        if (!meta)
        {
            GGLogger::Error("CreateScriptInstance: Metadata not found for type "
                + std::string(typeName));
            lua_pop(L, 1);  // Pop module table
            return 0;
        }

        if (meta->typeName.empty())
        {
            GGLogger::Error("CreateScriptInstance: Empty typeName in metadata for "
                + std::string(typeName));
            lua_pop(L, 1);
            return 0;
        }

        BuildFieldLayout(*meta);

        // ====================================================================
        // STEP 4: Allocate instance with RAII guards
        // ====================================================================

        EntityInstance* instance = static_cast<EntityInstance*>(
            GGMemoryManager::GetInstance().Allocate(sizeof(EntityInstance), "EntityInstance"));

        if (!instance)
        {
            GGLogger::Error("CreateScriptInstance: Failed to allocate EntityInstance");
            lua_pop(L, 1);
            return 0;
        }

        // Call constructor using placement new
        new(instance) EntityInstance();

        EntityInstanceGuard instanceGuard(instance, L);  // Will cleanup on exception

        instance->entity = entity;
        instance->type = meta;
        instance->data = GGMemoryManager::GetInstance().Allocate(meta->dataSize, "ScriptData");

        if (!instance->data)
        {
            GGLogger::Error("CreateScriptInstance: Failed to allocate "
                + std::to_string(meta->dataSize)
                + " bytes for instance data");
            lua_pop(L, 1);
            return 0;
        }

        memset(instance->data, 0, meta->dataSize);

        // ====================================================================
        // STEP 4.5: Construct std::string fields with placement-new
        // ====================================================================

        char* dataPtr = static_cast<char*>(instance->data);
        for (const auto& field : meta->fieldDescriptors)
        {
            if (field.type == FieldDescriptor::TYPE_STRING)
            {
                void* fieldPtr = dataPtr + field.offset;
                new (fieldPtr) std::string();  // Always construct strings, even if empty
            }
        }

        // ====================================================================
        // STEP 5: Initialize fields from default values
        // ====================================================================

        if (meta->defaultValueRef == LUA_NOREF || meta->defaultValueRef == LUA_REFNIL)
        {
            GGLogger::Warning("CreateScriptInstance: No default values for type "
                + std::string(typeName));
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, meta->defaultValueRef);

            if (!lua_istable(L, -1))
            {
                GGLogger::Warning("CreateScriptInstance: Default value reference is not a table");
                lua_pop(L, 1);
            }
            else
            {
                for (const auto& field : meta->fieldDescriptors)
                {
                    lua_getfield(L, -1, field.name.c_str());

                    /*GGLogger::Info("CreateScriptInstance: Initializing field "
                        + std::string(field.name));*/

                    char* dataPointer = static_cast<char*>(instance->data);
                    void* fieldPtr = dataPointer + field.offset;

                    // Initialize based on field type
                    switch (field.type)
                    {
                    case FieldDescriptor::TYPE_INT:
                        if (lua_isnumber(L, -1))
                        {
                            *static_cast<int*>(fieldPtr) = static_cast<int>(
                                lua_tointeger(L, -1)
                                );
                        }
                        else
                        {
                            *static_cast<int*>(fieldPtr) = 0;
                            GGLogger::Warning("Field "
                                + std::string(field.name)
                                + " is not a number, using 0");
                        }
                        break;

                    case FieldDescriptor::TYPE_FLOAT:
                        if (lua_isnumber(L, -1))
                        {
                            *static_cast<float*>(fieldPtr) = static_cast<float>(
                                lua_tonumber(L, -1)
                                );
                        }
                        else
                        {
                            *static_cast<float*>(fieldPtr) = 0.0f;
                            GGLogger::Warning("Field "
                                + std::string(field.name)
                                + " is not a number, using 0.0");
                        }
                        break;

                    case FieldDescriptor::TYPE_BOOL:
                        *static_cast<bool*>(fieldPtr) = lua_toboolean(L, -1);
                        break;

                    case FieldDescriptor::TYPE_STRING:
                        if (lua_isstring(L, -1))
                        {
                            // Assign to already-constructed string
                            *static_cast<std::string*>(fieldPtr) = lua_tostring(L, -1);
                        }
                        else
                        {
                            GGLogger::Warning("Field "
                                + std::string(field.name)
                                + " is not a string, using empty string");
                        }
                        break;

                    default:
                        GGLogger::Warning("Unknown field type for "
                            + std::string(field.name));
                        break;
                    }

                    lua_pop(L, 1);  // Pop field value
                }

                lua_pop(L, 1);  // Pop default values table
            }
        }

        // ====================================================================
        // STEP 5.5: Create Lua table for non-serializable fields
        // ====================================================================

        // Create a new table to store non-serializable fields
        lua_newtable(L);
        int luaFieldsTableIndex = lua_gettop(L);

        // Copy non-serializable fields from defaultValueRef to the new table
        if (meta->defaultValueRef != LUA_NOREF && meta->defaultValueRef != LUA_REFNIL)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, meta->defaultValueRef);

            if (lua_istable(L, -1))
            {
                // Iterate through all fields that are NOT in fieldDescriptors
                for (const auto& fieldName : meta->fields)
                {
                    // Skip fields that have C++ storage
                    if (meta->fieldNameToIndex.count(fieldName)) continue;

                    lua_getfield(L, -1, fieldName.c_str());
                    lua_setfield(L, luaFieldsTableIndex, fieldName.c_str());

                   /* GGLogger::Info("CreateScriptInstance: Initialized Lua field "
                        + std::string(fieldName));*/
                }

                // Recursive helper to create composition sub-instances
                std::function<void(GGScriptMetadata*, EntityInstance*, int)> createCompositionSubInstances;
                createCompositionSubInstances = [&](GGScriptMetadata* parentMeta, EntityInstance* parentInstance, int parentLuaTableIdx)
                    {
                        for (const auto& fieldName : parentMeta->compositionFields)
                        {
                            auto compFieldIt = parentMeta->fieldNameToIndex.find(fieldName);
                            if (compFieldIt == parentMeta->fieldNameToIndex.end()) continue;

                            const FieldDescriptor& compField = parentMeta->fieldDescriptors[compFieldIt->second];
                            char* parentDataPtr = static_cast<char*>(parentInstance->data);
                            std::string* composedTypeName = static_cast<std::string*>(
                                static_cast<void*>(parentDataPtr + compField.offset));

                            if (!composedTypeName || composedTypeName->empty()) continue;

                            GGScriptMetadata* composedMeta = GetMetadata(*composedTypeName);
                            if (!composedMeta)
                            {
                                // Script for this composed type hasn't been registered yet.
                                // Register all known script assets through the asset interface, then retry.
                                for (const auto& scriptPath : GGECSStandardAssetManager<GGScriptAsset>::GetAllRegisteredPaths())
                                    GetScriptAsset(scriptPath);
                                composedMeta = GetMetadata(*composedTypeName);
                                if (!composedMeta) continue;
                            }

                            BuildFieldLayout(*composedMeta);

                            // Allocate sub-instance using memory manager
                            EntityInstance* subInstance = static_cast<EntityInstance*>(
                                GGMemoryManager::GetInstance().Allocate(sizeof(EntityInstance), "EntityInstance"));
                            if (!subInstance) continue;
                            new(subInstance) EntityInstance();
                            subInstance->entity = entity;
                            subInstance->type = composedMeta;

                            if (composedMeta->dataSize > 0)
                            {
                                subInstance->data = GGMemoryManager::GetInstance().Allocate(composedMeta->dataSize, "ScriptData");
                                if (subInstance->data)
                                {
                                    memset(subInstance->data, 0, composedMeta->dataSize);

                                    char* subDataPtr = static_cast<char*>(subInstance->data);
                                    for (const auto& f : composedMeta->fieldDescriptors)
                                    {
                                        if (f.type == FieldDescriptor::TYPE_STRING)
                                            new (subDataPtr + f.offset) std::string();
                                    }

                                    if (composedMeta->defaultValueRef != LUA_NOREF &&
                                        composedMeta->defaultValueRef != LUA_REFNIL)
                                    {
                                        lua_rawgeti(L, LUA_REGISTRYINDEX, composedMeta->defaultValueRef);
                                        if (lua_istable(L, -1))
                                        {
                                            for (const auto& f : composedMeta->fieldDescriptors)
                                            {
                                                lua_getfield(L, -1, f.name.c_str());
                                                void* fPtr = subDataPtr + f.offset;
                                                switch (f.type)
                                                {
                                                case FieldDescriptor::TYPE_INT:
                                                    if (lua_isnumber(L, -1))
                                                        *static_cast<int*>(fPtr) = static_cast<int>(lua_tointeger(L, -1));
                                                    break;
                                                case FieldDescriptor::TYPE_FLOAT:
                                                    if (lua_isnumber(L, -1))
                                                        *static_cast<float*>(fPtr) = static_cast<float>(lua_tonumber(L, -1));
                                                    break;
                                                case FieldDescriptor::TYPE_STRING:
                                                    if (lua_isstring(L, -1))
                                                        *static_cast<std::string*>(fPtr) = lua_tostring(L, -1);
                                                    break;
                                                case FieldDescriptor::TYPE_BOOL:
                                                    *static_cast<bool*>(fPtr) = lua_toboolean(L, -1);
                                                    break;
                                                }
                                                lua_pop(L, 1);
                                            }
                                        }
                                        lua_pop(L, 1);
                                    }
                                }
                            }

                            // Create Lua table for sub-instance's non-serializable fields
                            lua_newtable(L);
                            int subLuaTableIdx = lua_gettop(L);
                            if (composedMeta->defaultValueRef != LUA_NOREF &&
                                composedMeta->defaultValueRef != LUA_REFNIL)
                            {
                                lua_rawgeti(L, LUA_REGISTRYINDEX, composedMeta->defaultValueRef);
                                if (lua_istable(L, -1))
                                {
                                    for (const auto& f : composedMeta->fields)
                                    {
                                        if (composedMeta->fieldNameToIndex.count(f)) continue;
                                        lua_getfield(L, -1, f.c_str());
                                        lua_setfield(L, subLuaTableIdx, f.c_str());
                                    }
                                }
                                lua_pop(L, 1);
                            }

                            // Recurse into this sub-instance's own composition fields
                            if (!composedMeta->compositionFields.empty())
                            {
                                createCompositionSubInstances(composedMeta, subInstance, subLuaTableIdx);
                            }

                            subInstance->luaFieldsRef = luaL_ref(L, LUA_REGISTRYINDEX);

                            // Create userdata wrapper with composed type's metatable
                            InstanceUserdata* subUD = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));
                            subUD->instance = subInstance;

                            if (composedMeta->metatableRef != LUA_NOREF)
                            {
                                lua_rawgeti(L, LUA_REGISTRYINDEX, composedMeta->metatableRef);
                                lua_setmetatable(L, -2);
                            }

                            // Store userdata in parent's Lua fields table
                            lua_setfield(L, parentLuaTableIdx, fieldName.c_str());

                            GGLogger::Info("CreateScriptInstance: Created sub-instance for composition field "
                                + fieldName + " of type " + *composedTypeName);
                        }
                    };

                // Initialize composition fields as full userdata sub-instances (recursive)
                createCompositionSubInstances(meta, instance, luaFieldsTableIndex);
            }

            lua_pop(L, 1);  // Pop default values table
        }

        // Store the Lua fields table in the registry
        instance->luaFieldsRef = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_pop(L, 1);  // Pop module table

        // ====================================================================
        // STEP 6: Store instance and update tracking
        // ====================================================================

        // Log registered functions
        /*for (const auto& pair : meta->functions)
        {
            GGLogger::Info("CreateScriptInstance: Function " + std::string(pair.first));
        }*/

        // Success! Release guards and store instance
        instances[instanceID] = instanceGuard.Release();
        inactiveInstances.pop();

        GGLogger::Info("CreateScriptInstance: Successfully created instance "
            + std::to_string(instanceID)
            + " for type "
            + meta->typeName);

        return instanceID;
    }
    catch (const std::bad_alloc& e)
    {
        GGLogger::Error("CreateScriptInstance: Memory allocation failed - "
            + std::string(e.what()));
        return 0;
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("CreateScriptInstance: Exception occurred - "
            + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        GGLogger::Error("CreateScriptInstance: Unknown exception occurred");
        return 0;
    }

    // Should never reach here due to returns in try block and catch blocks
    // But compiler may complain without this
    return 0;
}

/*************************************************************************
 \brief
    Destroys a script instance attached to a specific entity.
 \param[in] world
    Reference to the ECS world.
 \param[in] entity
    The entity whose script instance should be destroyed.
 \details
    Properly destructs string fields, frees allocated memory, and marks the instance as inactive.
 *************************************************************************/
void GGECSScriptSystem::DestroyScriptInstance(World& world, Entity& entity)
{
    if (world.HasComponent<ScriptComponent>(entity))
    {
        ScriptComponent* script = world.GetComponent<ScriptComponent>(entity);

        if (script && script->scriptInstance > 0)
        {
            if (!instances[script->scriptInstance]) return;

            std::cout << "Deleting script instance " << script->scriptInstance << std::endl;

            // Call OnDestroy lifecycle hook BEFORE cleanup
            // Only call if we're still simulating (VM is valid during gameplay destruction)
            // When play mode stops, onStopModeCallback resets the VM and clears subscriptions,
            // so OnDestroy is unnecessary and would crash with stale references
            if (shouldSimulate)
            {
                lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
                if (L && script->scriptAssetID != 0 && instances[script->scriptInstance]->type)
                {
                    GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script->scriptAssetID);
                    int moduleRef = scriptAsset.moduleRef;

                    if (moduleRef != LUA_NOREF)
                    {
                        LuaStackGuard stackGuard(L);

                        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);
                        
                        // Verify we actually got a table from the registry
                        if (lua_istable(L, -1))
                        {
                            lua_pushstring(L, "OnDestroy");
                            lua_rawget(L, -2);

                            if (lua_isfunction(L, -1))
                            {
                                // Remove the module table, keep only the function
                                lua_remove(L, -2);

                                InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));
                                userData->instance = instances[script->scriptInstance];
                                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);
                                lua_setmetatable(L, -2);

                                uint64_t packed = PackEntity(entity);
                                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));

                                if (lua_pcall(L, 2, 0, 0) != 0)
                                {
                                    const char* errorMsg = lua_tostring(L, -1);
                                    GGLogger::Warning("[ScriptSystem] OnDestroy error: " +
                                        std::string(errorMsg ? errorMsg : "Unknown error"));
                                }
                            }
                        }
                        // LuaStackGuard will clean up the stack
                    }
                }
            }

            if (instances[script->scriptInstance]->data)
            {
                char* dataPtr = static_cast<char*>(instances[script->scriptInstance]->data);

                for (const auto& field : instances[script->scriptInstance]->type->fieldDescriptors)
                {
                    std::cout << "Deleting field " << field.name << std::endl;
                    if (field.type == FieldDescriptor::TYPE_STRING)
                    {
                        void* fieldPtr = dataPtr + field.offset;
                        static_cast<std::string*>(fieldPtr)->~basic_string();
                    }
                }

                GGMemoryManager::GetInstance().Free(instances[script->scriptInstance]->data);
            }

            // Release Lua fields table reference
            if (instances[script->scriptInstance]->luaFieldsRef != LUA_NOREF)
            {
                lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
                if (L)
                {
                    // Recursive cleanup of composition sub-instances
                    std::function<void(EntityInstance*)> destroySubInstances;
                    destroySubInstances = [&](EntityInstance* inst)
                        {
                            if (!inst || !inst->type) return;
                            GGScriptMetadata* instMeta = inst->type;
                            if (instMeta->compositionFields.empty()) return;
                            if (inst->luaFieldsRef == LUA_NOREF) return;

                            lua_rawgeti(L, LUA_REGISTRYINDEX, inst->luaFieldsRef);
                            if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }

                            for (const auto& compField : instMeta->compositionFields)
                            {
                                lua_getfield(L, -1, compField.c_str());
                                if (lua_isuserdata(L, -1))
                                {
                                    InstanceUserdata* subUD = (InstanceUserdata*)lua_touserdata(L, -1);
                                    if (subUD && subUD->instance)
                                    {
                                        // Recurse into nested composition first
                                        destroySubInstances(subUD->instance);

                                        EntityInstance* subInst = subUD->instance;
                                        if (subInst->data && subInst->type)
                                        {
                                            char* subDataPtr = static_cast<char*>(subInst->data);
                                            for (const auto& f : subInst->type->fieldDescriptors)
                                            {
                                                if (f.type == FieldDescriptor::TYPE_STRING)
                                                {
                                                    static_cast<std::string*>(
                                                        static_cast<void*>(subDataPtr + f.offset))->~basic_string();
                                                }
                                            }
                                            GGMemoryManager::GetInstance().Free(subInst->data);
                                        }
                                        if (subInst->luaFieldsRef != LUA_NOREF)
                                            luaL_unref(L, LUA_REGISTRYINDEX, subInst->luaFieldsRef);
                                        subInst->~EntityInstance();
                                        GGMemoryManager::GetInstance().Free(subInst);
                                        subUD->instance = nullptr;
                                    }
                                }
                                lua_pop(L, 1); // Pop sub-instance userdata or nil
                            }
                            lua_pop(L, 1); // Pop Lua fields table
                        };

                    destroySubInstances(instances[script->scriptInstance]);

                    luaL_unref(L, LUA_REGISTRYINDEX, instances[script->scriptInstance]->luaFieldsRef);
                }
            }

            // Call destructor manually
            instances[script->scriptInstance]->~EntityInstance();

            // Free the instance memory
            GGMemoryManager::GetInstance().Free(instances[script->scriptInstance]);
            instances[script->scriptInstance] = nullptr;
            inactiveInstances.push(script->scriptInstance);
            script->scriptInstance = 0;
        }
    }
}

/*************************************************************************
 \brief
    Sets an integer variable for a script instance.
 \param[in] instance
    Index of the script instance.
 \param[in] index
    Index of the integer variable.
 \param[in] name
    Optional variable name (unused in current implementation).
 \param[in] value
    Value to set.
 *************************************************************************/
void GGECSScriptSystem::SetIntVar(size_t instance, size_t index, [[maybe_unused]] const char* name, int32_t value)
{
    if (instance >= MAX_INSTANCES)
    {
        GGLogger::Error("[SetIntVar] Instance ID " + std::to_string(instance)
            + " is larger than " + std::to_string(MAX_INSTANCES) + "!");
        std::cerr << "[SetIntVar]: Instance ID " << instance << " is larger than "
            << MAX_INSTANCES << "!" << std::endl;

        return;
    }

    if (instanceIntVars[instance].size() <= index)
    {
        // Resize buffer if too small
        instanceIntVars[instance].resize(index + 1);
    }

    // Note, we don't need to worry about null entity references here,
    // ECS should manage the lifetime of entities. 
    instanceIntVars[instance][index] = value;
}

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
int32_t GGECSScriptSystem::GetIntVar(size_t instance, size_t index) const
{
    if (instance >= MAX_INSTANCES)
    {
        GGLogger::Error("[GetIntVar] Instance ID " + std::to_string(instance)
            + " is larger than " + std::to_string(MAX_INSTANCES) + "!");
        std::cerr << "[GetIntVar]: Instance ID " << instance << " is larger than "
            << MAX_INSTANCES << "!" << std::endl;

        return 0;
    }

    if (instanceIntVars[instance].size() <= index)
    {
        // Trying to read an index of larger size than the number of 
        // vars registered!
        GGLogger::Error("[GetIntVar] Index ID " + std::to_string(index)
            + " is larger than registered var container size!");
        std::cerr << "[GetIntVar]: Index ID " << index << " is larger than registered var container size!" << std::endl;
        return 0;
    }

    return instanceIntVars[instance][index];
}

/*************************************************************************
 \brief
    Binds all script instances in a world when switching scenes.
 \param[in] world
    Reference to the ECS world.
 *************************************************************************/
void GGECSScriptSystem::BindInstanceOnSceneSwitch(World& world)
{
    world.ForEach<ScriptComponent>(createInstances);
}

/*************************************************************************
 \brief
    Builds memory layout for fields of a script metadata object.
 \param[in,out] meta
    Reference to the GGScriptMetadata to process.
 \details
    Computes offsets, sizes, and types for Serializable, TextureAsset, and Composition fields.
    Regular fields are stored in Lua and do not require C++ memory allocation.
 *************************************************************************/
void GGECSScriptSystem::BuildFieldLayout(GGScriptMetadata& meta)
{
    std::cout << "Attempting to build field layout" << std::endl;
    meta.fieldDescriptors.clear();
    meta.fieldNameToIndex.clear();
    meta.dataSize = 0;
    std::cout << "Building field layout for Serializable/TextureAsset/AudioAsset fields only" << std::endl;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();

    lua_rawgeti(L, LUA_REGISTRYINDEX, meta.defaultValueRef);

    size_t offset{};
    size_t fieldIdx{};

    auto processField = [&](std::string const& fieldName)
        {
            lua_getfield(L, -1, fieldName.c_str());

            FieldDescriptor descriptor;
            descriptor.name = fieldName;

            int luaType = lua_type(L, -1);
            switch (luaType)
            {
            case LUA_TNUMBER:
            {
                lua_Number num = lua_tonumber(L, -1);
                if (num == floor(num) && num >= INT_MIN && num <= INT_MAX)
                {
                    descriptor.type = FieldDescriptor::TYPE_INT;
                    descriptor.size = sizeof(int);
                }
                else
                {
                    descriptor.type = FieldDescriptor::TYPE_FLOAT;
                    descriptor.size = sizeof(float);
                }
                break;
            }
            case LUA_TSTRING:
                descriptor.type = FieldDescriptor::TYPE_STRING;
                descriptor.size = sizeof(std::string);
                break;
            case LUA_TBOOLEAN:
                descriptor.type = FieldDescriptor::TYPE_BOOL;
                descriptor.size = sizeof(bool);
                break;
            default:
                lua_pop(L, 1);
                return;
            }

            size_t alignment = alignof(std::max_align_t);
            if (offset % alignment != 0)
            {
                offset = (offset + alignment - 1) & ~(alignment - 1);
            }

            descriptor.offset = offset;
            offset += descriptor.size;

            meta.fieldDescriptors.push_back(descriptor);
            meta.fieldNameToIndex[fieldName] = fieldIdx++;

            lua_pop(L, 1);
        };

    // Only process Serializable and TextureAsset fields for C++ storage
    for (const auto& fieldName : meta.serializedFields)
    {
        processField(fieldName);
    }

    for (const auto& fieldName : meta.textureAssetFields)
    {
        if (meta.fieldNameToIndex.count(fieldName)) continue;
        processField(fieldName);
    }

    // Process Composition fields for C++ storage (stores the composed type name as string)
    for (const auto& fieldName : meta.compositionFields)
    {
        if (meta.fieldNameToIndex.count(fieldName)) continue;
        processField(fieldName);
    }

    for (const auto& fieldName : meta.audioAssetFields)
    {
        if (meta.fieldNameToIndex.count(fieldName)) continue;
        processField(fieldName);
    }

    // Regular fields (meta.fields) are NOT processed here - they live in Lua

    meta.dataSize = (offset + 7) & ~7;
    std::cout << "C++ data size (Serializable/TextureAsset/Composition): " << meta.dataSize << std::endl;

    lua_pop(L, 1);
}

/*************************************************************************
 \brief
    Retrieves metadata for a given script type name.
 \param[in] typeName
    Name of the script type.
 \return
    Pointer to GGScriptMetadata or nullptr if not found.
 *************************************************************************/
GGScriptMetadata* GGECSScriptSystem::GetMetadata(std::string const& typeName)
{
    auto it = scriptMetadata.find(typeName);
    if (it == scriptMetadata.end()) {
        GGLogger::Error("[ScriptSystem] No metadata available for typeName: " + typeName);
        std::cerr << "Error: No metadata available for typeName: " << typeName << std::endl;
        return nullptr;
    }
    return it->second.get();
}

/*************************************************************************
 \brief
    Stores a new script metadata object.
 \param[in] typeName
    Name of the script type.
 \param[in] metadata
    Metadata to store.
 *************************************************************************/
void GGECSScriptSystem::StoreNewMetadata(std::string const& typeName, GGScriptMetadata metadata)
{
    scriptMetadata[typeName] = std::make_unique<GGScriptMetadata>(std::move(metadata));
}

/*************************************************************************
 \brief
    Retrieves a script instance by its index.
 \param[in] index
    Index of the script instance.
 \return
    Pointer to EntityInstance or nullptr if invalid.
 *************************************************************************/
EntityInstance* GGECSScriptSystem::GetScriptInstance(size_t index)
{
    if (index < 0 || index >= MAX_INSTANCES)
    {
        GGLogger::Error("[ScriptSystem] Trying to access beyond instance array bounds! Index: "
            + std::to_string(index));
        std::cerr << "Error: Trying to access beyond instance array bounds! Index: " << index << std::endl;
        return nullptr;
    }

    return instances[index];
}

/*************************************************************************
 \brief
    Executes the OnClick function of a script attached to an entity.
 \param[in] entity
    Target entity.
 \param[in,out] script
    ScriptComponent to execute.
 \param[in] xPos
    X position of the click.
 \param[in] yPos
    Y position of the click.
 *************************************************************************/
void GGECSScriptSystem::ExecuteScriptClick(Entity entity, ScriptComponent& script, double xPos, double yPos)
{
    if (script.scriptAssetID == 0) return;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        GlobalEventBus::onPause.Emit();
        return;
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

        int moduleRef = scriptAsset.moduleRef;

        if (moduleRef == LUA_NOREF)
        {
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

        // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
        lua_pushstring(L, "OnClick");
        lua_rawget(L, -2);  // Get table[key] without calling metamethods

        if (lua_isfunction(L, -1))
        {
            // Pass script instance "self"
            InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

            if (instances[script.scriptInstance] != nullptr)
            {
                userData->instance = instances[script.scriptInstance];

                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                lua_setmetatable(L, -2);

                uint64_t packed = PackEntity(entity);

                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
            else
            {
                GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                lua_pop(L, 1); // Pop function
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pushnumber(L, xPos);
            lua_pushnumber(L, yPos);

            // Use protected call
            int result = lua_pcall(L, 4, 0, 0);

            if (result != 0)
            {
                // Handle Lua error
                const char* errorMsg = lua_tostring(L, -1);
                GGLogger::Error("[ScriptSystem] OnClick error for entity " +
                    std::to_string(entity.id) +
                    ": " +
                    (errorMsg ? errorMsg : "Unknown error")
                );
                GlobalEventBus::onPause.Emit();
                std::cerr << "[ScriptSystem] OnClick error for entity " << entity.id
                    << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                lua_pop(L, 1); // Pop error message
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pop(L, 1); // Pop module table
        }
        else
        {
            // Update not found or not a function
            lua_pop(L, 1); // Pop non-function value
            lua_pop(L, 1); // Pop module table
        }
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("OnClick: Exception occurred - "
            + std::string(e.what()));
        GlobalEventBus::onPause.Emit();
        return;
    }
    catch (...)
    {
        GGLogger::Error("OnClick: Unknown exception occurred");
        GlobalEventBus::onPause.Emit();
        return;
    }
}

/*************************************************************************
 \brief
    Executes the OnDrag function of a script attached to an entity.
 \param[in] entity
    Target entity.
 \param[in,out] script
    ScriptComponent to execute.
 \param[in] xPos
    Current X position of the drag.
 \param[in] yPos
    Current Y position of the drag.
 *************************************************************************/
void GGECSScriptSystem::ExecuteScriptDrag(Entity entity, ScriptComponent& script, double xPos, double yPos)
{
    if (script.scriptAssetID == 0) return;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        GlobalEventBus::onPause.Emit();
        return;
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

        int moduleRef = scriptAsset.moduleRef;

        if (moduleRef == LUA_NOREF)
        {
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

        // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
        lua_pushstring(L, "OnDrag");
        lua_rawget(L, -2);  // Get table[key] without calling metamethods

        if (lua_isfunction(L, -1))
        {
            // Pass script instance "self"
            InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

            if (instances[script.scriptInstance] != nullptr)
            {
                userData->instance = instances[script.scriptInstance];

                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                lua_setmetatable(L, -2);

                uint64_t packed = PackEntity(entity);

                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
            else
            {
                GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                lua_pop(L, 1); // Pop function
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pushnumber(L, xPos);
            lua_pushnumber(L, yPos);

            // Use protected call
            int result = lua_pcall(L, 4, 0, 0);

            if (result != 0)
            {
                // Handle Lua error
                const char* errorMsg = lua_tostring(L, -1);
                GGLogger::Error("[ScriptSystem] OnDrag error for entity " +
                    std::to_string(entity.id) +
                    ": " +
                    (errorMsg ? errorMsg : "Unknown error")
                );
                GlobalEventBus::onPause.Emit();
                std::cerr << "[ScriptSystem] OnDrag error for entity " << entity.id
                    << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                lua_pop(L, 1); // Pop error message
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pop(L, 1); // Pop module table
        }
        else
        {
            // OnDrag not found or not a function
            lua_pop(L, 1); // Pop non-function value
            lua_pop(L, 1); // Pop module table
        }
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("OnDrag: Exception occurred - "
            + std::string(e.what()));
        GlobalEventBus::onPause.Emit();
        return;
    }
    catch (...)
    {
        GGLogger::Error("OnDrag: Unknown exception occurred");
        GlobalEventBus::onPause.Emit();
        return;
    }
}

/*************************************************************************
 \brief
    Executes the OnDrop function of a script attached to an entity.
 \param[in] entity
    Target entity.
 \param[in,out] script
    ScriptComponent to execute.
 \param[in] xPos
    X position where the drop occurs.
 \param[in] yPos
    Y position where the drop occurs.
 *************************************************************************/
void GGECSScriptSystem::ExecuteScriptDrop(Entity entity, ScriptComponent& script, double xPos, double yPos)
{
    if (script.scriptAssetID == 0) return;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        GlobalEventBus::onPause.Emit();
        return;
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

        int moduleRef = scriptAsset.moduleRef;

        if (moduleRef == LUA_NOREF)
        {
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

        // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
        lua_pushstring(L, "OnDrop");
        lua_rawget(L, -2);  // Get table[key] without calling metamethods

        if (lua_isfunction(L, -1))
        {
            // Pass script instance "self"
            InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

            if (instances[script.scriptInstance] != nullptr)
            {
                userData->instance = instances[script.scriptInstance];

                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                lua_setmetatable(L, -2);

                uint64_t packed = PackEntity(entity);

                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
            else
            {
                GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                lua_pop(L, 1); // Pop function
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pushnumber(L, xPos);
            lua_pushnumber(L, yPos);

            // Use protected call
            int result = lua_pcall(L, 4, 0, 0);

            if (result != 0)
            {
                // Handle Lua error
                const char* errorMsg = lua_tostring(L, -1);
                GGLogger::Error("[ScriptSystem] OnDrop error for entity " +
                    std::to_string(entity.id) +
                    ": " +
                    (errorMsg ? errorMsg : "Unknown error")
                );
                GlobalEventBus::onPause.Emit();
                std::cerr << "[ScriptSystem] OnDrop error for entity " << entity.id
                    << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                lua_pop(L, 1); // Pop error message
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pop(L, 1); // Pop module table
        }
        else
        {
            // Update not found or not a function
            lua_pop(L, 1); // Pop non-function value
            lua_pop(L, 1); // Pop module table
        }
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("OnDrop: Exception occurred - "
            + std::string(e.what()));
        GlobalEventBus::onPause.Emit();
        return;
    }
    catch (...)
    {
        GGLogger::Error("OnDrop: Unknown exception occurred");
        GlobalEventBus::onPause.Emit();
        return;
    }
}

/*************************************************************************
 \brief
    Executes the OnHoverEnter function of a script attached to an entity.
 \param[in] entity
    Target entity being hovered.
 \param[in,out] script
    ScriptComponent to execute.
 *************************************************************************/
void GGECSScriptSystem::ExecuteOnHoverEnter(Entity entity, ScriptComponent& script)
{
    if (script.scriptAssetID == 0) return;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        GlobalEventBus::onPause.Emit();
        return;
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

        int moduleRef = scriptAsset.moduleRef;

        if (moduleRef == LUA_NOREF)
        {
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

        // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
        lua_pushstring(L, "OnHoverEnter");
        lua_rawget(L, -2);  // Get table[key] without calling metamethods

        if (lua_isfunction(L, -1))
        {
            // Pass script instance "self"
            InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

            if (instances[script.scriptInstance] != nullptr)
            {
                userData->instance = instances[script.scriptInstance];

                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                lua_setmetatable(L, -2);

                uint64_t packed = PackEntity(entity);

                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
            else
            {
                GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                lua_pop(L, 1); // Pop function
                lua_pop(L, 1); // Pop module table
                return;
            }

            // Use protected call
            int result = lua_pcall(L, 2, 0, 0);

            if (result != 0)
            {
                // Handle Lua error
                const char* errorMsg = lua_tostring(L, -1);
                GGLogger::Error("[ScriptSystem] OnHoverEnter error for entity " +
                    std::to_string(entity.id) +
                    ": " +
                    (errorMsg ? errorMsg : "Unknown error")
                );
                GlobalEventBus::onPause.Emit();
                std::cerr << "[ScriptSystem] OnHoverEnter error for entity " << entity.id
                    << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                lua_pop(L, 1); // Pop error message
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pop(L, 1); // Pop module table
        }
        else
        {
            // Update not found or not a function
            lua_pop(L, 1); // Pop non-function value
            lua_pop(L, 1); // Pop module table
        }
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("OnHoverEnter: Exception occurred - "
            + std::string(e.what()));
        GlobalEventBus::onPause.Emit();
        return;
    }
    catch (...)
    {
        GGLogger::Error("OnHoverEnter: Unknown exception occurred");
        GlobalEventBus::onPause.Emit();
        return;
    }
}

/*************************************************************************
 \brief
    Executes the OnHoverExit function of a script attached to an entity.
 \param[in] entity
    Target entity no longer being hovered.
 \param[in,out] script
    ScriptComponent to execute.
 *************************************************************************/
void GGECSScriptSystem::ExecuteOnHoverExit(Entity entity, ScriptComponent& script)
{
    if (script.scriptAssetID == 0) return;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        GlobalEventBus::onPause.Emit();
        return;
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script.scriptAssetID);

        int moduleRef = scriptAsset.moduleRef;

        if (moduleRef == LUA_NOREF)
        {
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

        // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
        lua_pushstring(L, "OnHoverExit");
        lua_rawget(L, -2);  // Get table[key] without calling metamethods

        if (lua_isfunction(L, -1))
        {
            // Pass script instance "self"
            InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

            if (instances[script.scriptInstance] != nullptr)
            {
                userData->instance = instances[script.scriptInstance];

                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                lua_setmetatable(L, -2);

                uint64_t packed = PackEntity(entity);

                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
            else
            {
                GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                lua_pop(L, 1); // Pop function
                lua_pop(L, 1); // Pop module table
                return;
            }

            // Use protected call
            int result = lua_pcall(L, 2, 0, 0);

            if (result != 0)
            {
                // Handle Lua error
                const char* errorMsg = lua_tostring(L, -1);
                GGLogger::Error("[ScriptSystem] OnHoverExit error for entity " +
                    std::to_string(entity.id) +
                    ": " +
                    (errorMsg ? errorMsg : "Unknown error")
                );
                GlobalEventBus::onPause.Emit();
                std::cerr << "[ScriptSystem] OnHoverExit error for entity " << entity.id
                    << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                lua_pop(L, 1); // Pop error message
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pop(L, 1); // Pop module table
        }
        else
        {
            // Update not found or not a function
            lua_pop(L, 1); // Pop non-function value
            lua_pop(L, 1); // Pop module table
        }
    }
    catch (const std::exception& e)
    {
        GGLogger::Error("OnHoverExit: Exception occurred - "
            + std::string(e.what()));
        GlobalEventBus::onPause.Emit();
        return;
    }
    catch (...)
    {
        GGLogger::Error("OnHoverExit: Unknown exception occurred");
        GlobalEventBus::onPause.Emit();
        return;
    }
}

/*************************************************************************
 \brief
    Invokes a named Lua function on a script instance with no arguments.
 \param[in] entity
    Target entity.
 \param[in] world
    Reference to the ECS world.
 \param[in] funcName
    Name of the Lua function to invoke.
 *************************************************************************/
void GGECSScriptSystem::InvokeVoidFunction(Entity entity, World& world, std::string const& funcName)
{
    if (!world.IsEntityAlive(entity)) return;

    if (!world.HasComponent<ScriptComponent>(entity)) return;

    ScriptComponent* script = world.GetComponent<ScriptComponent>(entity);

    if (!script) return;

    if (script->scriptAssetID == 0) return;

    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    if (!L)
    {
        GGLogger::Error("CreateScriptInstance: Lua state is null");
        GlobalEventBus::onPause.Emit();
        return;
    }

    // RAII stack guard - ensures stack cleanup on any error
    LuaStackGuard stackGuard(L);

    try
    {
        GGScriptAsset scriptAsset = GGECSStandardAssetManager<GGScriptAsset>::GetAsset(script->scriptAssetID);

        int moduleRef = scriptAsset.moduleRef;

        if (moduleRef == LUA_NOREF)
        {
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef);

        // Use lua_pushstring + lua_rawget instead of lua_getfield to avoid metamethods
        lua_pushstring(L, funcName.c_str());
        lua_rawget(L, -2);  // Get table[key] without calling metamethods

        if (lua_isfunction(L, -1))
        {
            // Pass script instance "self"
            InstanceUserdata* userData = (InstanceUserdata*)lua_newuserdata(L, sizeof(InstanceUserdata));

            if (instances[script->scriptInstance] != nullptr)
            {
                userData->instance = instances[script->scriptInstance];

                lua_rawgeti(L, LUA_REGISTRYINDEX, userData->instance->type->metatableRef);

                lua_setmetatable(L, -2);

                uint64_t packed = PackEntity(entity);

                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
            else
            {
                GGLogger::Error("[ScriptSystem] No script instance attached to this script!");
                std::cerr << "No script instance attached to this script! " << std::endl;
                lua_pop(L, 1); // Pop function
                lua_pop(L, 1); // Pop module table
                return;
            }

            // Use protected call
            int result = lua_pcall(L, 2, 0, 0);

            if (result != 0)
            {
                // Handle Lua error
                const char* errorMsg = lua_tostring(L, -1);
                GGLogger::Error("[ScriptSystem] " + funcName + " error for entity " +
                    std::to_string(entity.id) +
                    ": " +
                    (errorMsg ? errorMsg : "Unknown error")
                );
                GlobalEventBus::onPause.Emit();
                std::cerr << "[ScriptSystem] " + funcName + " error for entity " << entity.id
                    << ": " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
                lua_pop(L, 1); // Pop error message
                lua_pop(L, 1); // Pop module table
                return;
            }

            lua_pop(L, 1); // Pop module table
        }
        else
        {
            // Update not found or not a function
            lua_pop(L, 1); // Pop non-function value
            lua_pop(L, 1); // Pop module table
        }
    }
    catch (const std::exception& e)
    {
        GGLogger::Error(funcName + ": Exception occurred - "
            + std::string(e.what()));
        GlobalEventBus::onPause.Emit();
        return;
    }
    catch (...)
    {
        GGLogger::Error(funcName + ": Unknown exception occurred");
        GlobalEventBus::onPause.Emit();
        return;
    }
}

/*************************************************************************
 \brief
    Requests a scene change to occur on the next frame.
 \param[in] sceneName
    Name of the scene to switch to.
 *************************************************************************/
void GGECSScriptSystem::SetNextFrameScene(std::string const& sceneName)
{
    if (nextFrameScene.empty() && sceneName != GetEngineInstance().GetSceneManager().GetActiveSceneKey())
        nextFrameScene = sceneName;
    else
        GGLogger::Warning("Scene " + sceneName + " transition ignored as there was another scene change requested earlier this frame!");
}

/*************************************************************************
 \brief
    Marks the script system as entering a scene transition.
 *************************************************************************/
void GGECSScriptSystem::BeginSceneTransition()
{
    isSceneTransitioning = true;
}

/*************************************************************************
 \brief
    Marks the script system as exiting a scene transition.
 *************************************************************************/
void GGECSScriptSystem::EndSceneTransition()
{
    isSceneTransitioning = false;
}

void GGECSScriptSystem::ReloadScriptsWithoutRestore()
{
    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();
    for (size_t i{ 1 }; i < MAX_INSTANCES; i++)
    {
        if (!instances[i]) continue;
        if (L && instances[i]->luaFieldsRef != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, instances[i]->luaFieldsRef);
        if (instances[i]->data)
            GGMemoryManager::GetInstance().Free(instances[i]->data);
        instances[i]->~EntityInstance();
        GGMemoryManager::GetInstance().Free(instances[i]);
        instances[i] = nullptr;
    }
    while (!inactiveInstances.empty()) inactiveInstances.pop();
    InitScriptInstances();

    scriptMetadata.clear();

    std::vector<std::string> scripts = std::move(compiledScriptsCache);
    scriptAssets.clear();

    for (std::string const& script : scripts)
        RegisterScript(script);

    ValidateCompositionDependencies();
}

/*************************************************************************
 \brief
    Tears down all script instances, metadata, and caches without
    replaying previously registered script paths.
 \details
    ReloadScriptsWithoutRestore() moves compiledScriptsCache and replays
    every path through RegisterScript. When switching projects this is
    wrong -- the cache still holds the old project's absolute paths, so
    they get re-registered under the new base path.

    ClearScriptCache() discards compiledScriptsCache entirely. New project
    scripts are then registered lazily on first access via GetScriptAsset.
    Call this from LoadProject before SetAssetBasePath.
 *************************************************************************/
void GGECSScriptSystem::ClearScriptCache()
{
    lua_State* L = GetEngineInstance().GetLuaVM()->GetState();

    // Free all active instances -- identical teardown to ReloadScriptsWithoutRestore
    for (size_t i{ 1 }; i < MAX_INSTANCES; i++)
    {
        if (!instances[i]) continue;
        if (L && instances[i]->luaFieldsRef != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, instances[i]->luaFieldsRef);
        if (instances[i]->data)
            GGMemoryManager::GetInstance().Free(instances[i]->data);
        instances[i]->~EntityInstance();
        GGMemoryManager::GetInstance().Free(instances[i]);
        instances[i] = nullptr;
    }

    while (!inactiveInstances.empty()) inactiveInstances.pop();
    InitScriptInstances();

    scriptMetadata.clear();
    scriptAssets.clear();

    // Key difference from ReloadScriptsWithoutRestore: discard the cache
    // entirely instead of replaying it. Stale absolute paths from the old
    // project must not be re-registered under the new base path.
    compiledScriptsCache.clear();
}