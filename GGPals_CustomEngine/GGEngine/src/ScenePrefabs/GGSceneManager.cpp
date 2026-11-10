/*************************************************************************
  \file       GGSceneManager.cpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.4
  \brief
    Implements the SceneManager and SceneNode classes for managing scene
    hierarchies, loading scenes from JSON, and instantiating entities into
    the ECS world.

  \details
    Provides functionality to:
    - Represent scene hierarchies as a tree of SceneNodes (parent-child).
    - Add and remove child nodes under a parent node.
    - Load scene definitions from JSON files into memory.
    - Instantiate loaded scenes into the ECS world via PrefabManager and ObjectsFactory.
    - Support prefab hierarchy by remapping parent, child, and sibling IDs when deserializing.
    - Update entire scene hierarchies (propagating parent to children).
    - Clear individual scenes or all loaded scenes from the world.
    - Backup and restore scenes with full ECS serialization support.
    - Track and access loaded scenes with scene counts.

  \dependencies
    <iostream>, <utility>, <filesystem>
    GGSceneManager.hpp, GGECSInterface.hpp, GGECSWorld.hpp,
    GGECSComponents.hpp, RapidJSONWrapper.hpp, GGEngine.hpp,
    GGECSGameObjectBridge.hpp, GGEngineEventBus.hpp

  \history
    - 1.0 (30 September 2025): Initial version with scene hierarchy and
      scene loading/instantiation
    - 1.1 (2 October 2025): Added accessors for retrieving loaded scene
      containers and scene counts
    - 1.2 (25 October 2025): Refactored SceneManager scene switching implementation
      to store a map of Worlds instead of clearing/reinstantiating objects
    - 1.3 (28 October 2025): Updated component override application in
      AddNewScene() to use ComponentData::dataString instead of rapidjson::Value.
      Also updated internal JSON copy logic accordingly
    - 1.4 (07 November 2025): Refactored scene deserialization to support
      prefab hierarchy components, correctly remapping parent, firstChild,
      and nextSibling entity IDs in loaded scenes

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <iostream>
#include <utility>
#include "GGSceneManager.hpp"
#include "GGECSInterface.hpp"
#include "GGECSWorld.hpp"
#include "GGECSComponents.hpp"
#include "RapidJSONWrapper.hpp"
#include "GGEngine.hpp"

#include "GGECSGameObjectBridge.hpp"
#include <GGEngineEventBus.hpp>
#include <../../GGEditor/include/AssetManagerNode.hpp>
#include <GGJSONConfig.hpp>

// forward declarations for sync registry
template <typename T>
void RegisterSyncComponent();
void RegisterAllSyncComponents();

// SceneManager functions -----------------------------------------------------------------------------------

/*************************************************************************
  \brief
    Constructs the SceneManager and initializes core systems.
  \details
    Sets the default active scene key, registers all component sync
    handlers for prefab propagation, and creates the internal prefab
    template world.
*************************************************************************/
SceneManager::SceneManager() {
    activeSceneKey = "UnknownScene";

    // register all components for syncing
    RegisterAllSyncComponents();

    prefabWorld = std::make_unique<World>();
}

/*************************************************************************
  \brief 
    Loads a scene JSON file into memory
  \param 
    filename Path to the scene JSON file
  \return 
    True if the file was successfully read and parsed
  \details  
    Initializes LoadedSceneData with the parsed JSON document.
*************************************************************************/
bool SceneManager::LoadNewSceneData(const std::string& filename) {
    std::string str = JSONUtils::ReadFileContents(filename);
    rapidjson::Document doc = JSONUtils::ParseJSON(str);

    if (!doc.IsObject() || !doc.HasMember("entities") || !doc["entities"].IsArray()) {
        std::cerr << "Invalid scene JSON format.\n";

    }

    // get the scene name
    std::string sceneName = "UnknownScene";
    if (doc.HasMember("sceneName") && doc["sceneName"].IsString()) {
        sceneName = doc["sceneName"].GetString();
    }

    // construct a new LoadedSceneData object and store in SceneManager
    loadedSceneData.sceneName = sceneName;
    loadedSceneData.sceneDoc.SetObject(); // make sure sceneDoc is initialized
    loadedSceneData.sceneDoc = std::move(doc);

    return true;
}

/*************************************************************************
  \brief 
    Deserialize a scene JSON into ECS world entities
  \param 
    sceneName Name of the scene being deserialized
  \param 
    doc JSON document containing entities and components
  \param 
    prefabManager Reference to the PrefabManager for component instantiation
  \param 
    newScene Reference to the ECS World to populate
  \return 
    True if deserialization was successful
  \details 
    Handles remapping of prefab hierarchy IDs (parent, firstChild, nextSibling)
    and adds all components to entities.
*************************************************************************/
bool SceneManager::DeserializeSceneFromJSON(const std::string sceneName, const rapidjson::Document& doc, PrefabManager& prefabManager, World& newScene) {

    if (!doc.IsObject() || !doc.HasMember("entities") || !doc["entities"].IsArray()) {
        std::cerr << "[SceneManager] Invalid JSON for scene: " << sceneName << "\n";
        return false;
    }

    const auto& entities = doc["entities"];
    std::unordered_map<uint32_t, Entity> idToEntityMap;

    // first pass: create empty entities
    for (auto& ent : entities.GetArray()) {
        uint32_t jsonID = 0;
        if (ent.HasMember("id") && ent["id"].IsUint()) {
            jsonID = ent["id"].GetUint();
        }
        else {
            // entity doesnt get created if it doesnt have an id
            std::cerr << "[SceneManager] Entity in JSON missing 'id', skipping.\n";
            continue;
        }

        Entity newEntity = newScene.CreateEntityHandle(); // runtime-generated ID
        idToEntityMap[jsonID] = newEntity;
    }

    // second pass: add components to each entity
    for (auto& ent : entities.GetArray()) {
        uint32_t jsonID = ent["id"].GetUint();
        auto entIt = idToEntityMap.find(jsonID);
        if (entIt == idToEntityMap.end()) {
            std::cerr << "[SceneManager] Could not find entity with ID " << jsonID << " in map.\n";
            continue;
        }
        Entity e = entIt->second;


        if (!e.IsValid()) {
            std::cerr << "[SceneManager] Invalid mapped entity for ID " << jsonID << "\n";
            continue; // skip the entity if invalid
        }

        // deserialize all components
        if (ent.HasMember("components") && ent["components"].IsArray()) {
            for (auto& compObj : ent["components"].GetArray()) {
                PrefabManager::ComponentData compData;

                if (!compObj.HasMember("componentID") || !compObj["componentID"].IsInt()) {
                    std::cerr << "[SceneManager] Invalid component ID in scene JSON.\n";
                    continue;
                }

                compData.typeID = static_cast<uint16_t>(compObj["componentID"].GetInt());

                // find component data
                for (auto it = compObj.MemberBegin(); it != compObj.MemberEnd(); ++it) {
                    if (it->name.IsString()) {
                        std::string key = it->name.GetString();
                        if (key != "componentID" && it->value.IsObject()) {
                            compData.typeName = key;
                            compData.dataString = JSONUtils::StringifyJSONValue(it->value);

                            // special handling for HierarchyComponent
                            if (static_cast<Components>(compData.typeID) == Components::HIERARCHY_COMPONENT) {
                                rapidjson::Document hierarchyObj = JSONUtils::ParseJSON(compData.dataString);
                                if (hierarchyObj.HasMember("parent")) {
                                    uint32_t parentJSONID = hierarchyObj["parent"].GetUint();
                                    if (parentJSONID != 0 && idToEntityMap.find(parentJSONID) != idToEntityMap.end()) {
                                        hierarchyObj["parent"].SetUint(idToEntityMap[parentJSONID].id);
                                    }
                                }
                                if (hierarchyObj.HasMember("firstChild")) {
                                    uint32_t childJSONID = hierarchyObj["firstChild"].GetUint();
                                    if (childJSONID != 0 && idToEntityMap.find(childJSONID) != idToEntityMap.end()) {
                                        hierarchyObj["firstChild"].SetUint(idToEntityMap[childJSONID].id);
                                    }
                                }
                                if (hierarchyObj.HasMember("nextSibling")) {
                                    uint32_t sibJSONID = hierarchyObj["nextSibling"].GetUint();
                                    if (sibJSONID != 0 && idToEntityMap.find(sibJSONID) != idToEntityMap.end()) {
                                        hierarchyObj["nextSibling"].SetUint(idToEntityMap[sibJSONID].id);
                                    }
                                }

                                // restringify and store back into dataString
                                compData.dataString = JSONUtils::StringifyJSONValue(hierarchyObj);
                            }

                            prefabManager.AddComponentToEntity(e, compData, newScene);
                            break; // break once component data is found;
                        }
                    }
                }
            }
        }
        else {
            std::cerr << "[SceneManager] Entity " << jsonID << " has no components.\n";
        }
    }


    std::cout << "Scene " << sceneName << " deserialized successfully.\n";
    return true;
}

/*************************************************************************
  \brief 
    Add a new scene by loading from file and deserializing entities
  \param 
    filename Path to scene JSON file
  \param 
    prefabManager Reference to PrefabManager for component handling
  \return 
    True if the scene was loaded and instantiated successfully
*************************************************************************/
bool SceneManager::AddNewScene(const std::string& filename, PrefabManager& prefabManager) {
    // load the JSON file
    if (!LoadNewSceneData(filename)) {
        std::cerr << "[SceneManager] Failed to load scene JSON: " << filename << "\n";
        return false;
    }

    if (!loadedSceneData.sceneDoc.IsObject()) {
        std::cerr << "[SceneManager] Invalid scene document.\n";
        return false;
    }

    // create a new empty world for the scene
    World newScene;

    bool success = DeserializeSceneFromJSON(loadedSceneData.sceneName,
        loadedSceneData.sceneDoc,
        prefabManager, newScene);

    if (!success) {
        std::cerr << "[SceneManager] Failed to deserialize scene: " << loadedSceneData.sceneName << "\n";
        return false;
    }

    latestLoadedScene = loadedSceneData.sceneName;
    sceneMap[loadedSceneData.sceneName] = std::move(newScene);
    std::cout << "[SceneManager] Scene " << loadedSceneData.sceneName << " loaded successfully.\n";
    return true;
}

/*************************************************************************
  \brief 
    Get the name of the latest loaded scene
  \return 
    Reference to the latest loaded scene name
*************************************************************************/
std::string const& SceneManager::GetLatestLoadedSceneName() const
{
    return latestLoadedScene;
}

/*************************************************************************
  \brief 
    Get a reference to a loaded scene by its name
  \param 
    sceneName Name of the scene
  \return 
    Reference to the ECS World representing the scene
  \throws 
    AssetFailedToLoadException if scene does not exist
*************************************************************************/
World& SceneManager::GetLoadedSceneByName(std::string const& sceneName)
{
    std::string resolvedName = sceneName;
    if (sceneName.substr(0, 7) == "Scenes\\")
    {
        resolvedName = sceneName.substr(7);
        std::cout << "Resolved Scene Name to " << resolvedName << std::endl;
    }

    if (sceneName.substr(sceneName.length() - 6) == ".scene")
    {
        resolvedName.erase(resolvedName.end() - 6, resolvedName.end());
        std::cout << "Resolved Scene Name to " << resolvedName << std::endl;
    }

    auto it = sceneMap.find(resolvedName);

    if (it == sceneMap.end())
        throw AssetFailedToLoadException(sceneName);

    return it->second;
}

/*************************************************************************
  \brief 
    Sets the active scene by key
  \param 
    sceneKey Key of the scene to activate
  \return 
    True if the scene exists and was successfully set active
*************************************************************************/
bool SceneManager::SetActiveScene(std::string sceneKey) {
    // Only get fromWorld if we have a valid active scene, to avoid
    // inserting a phantom empty World with key "" into sceneMap
    bool hasFrom = HasActiveScene();

    if (sceneKey == "PrefabEditorWorld") {
        if (hasFrom) {
            GlobalEventBus::onSwitchScenes.Emit(GetActiveScene(), sceneMap["PrefabEditorWorld"]);
        }
        activeSceneKey = "PrefabEditorWorld";
        return true;
    }

    std::string resolvedName = sceneKey;
    if (sceneKey.substr(0, 7) == "Scenes\\")
    {
        resolvedName = sceneKey.substr(7);
        std::cout << "Resolved Scene Name to " << resolvedName << std::endl;
    }

    if (sceneKey.substr(sceneKey.length() - 6) == ".scene")
    {
        resolvedName.erase(resolvedName.end() - 6, resolvedName.end());
        std::cout << "Resolved Scene Name to " << resolvedName << std::endl;
    }

    // check if scene exists in the sceneMap
    if (!sceneMap.contains(resolvedName)) {
        std::cerr << "Scene '" << resolvedName << "' does not exist yet, might not be loaded?\n";

        auto& assetSystem = GGAssetID::GetGGAssetID();

        if (auto uuidOpt = assetSystem->GetUUIDFromPath("Scenes\\" + resolvedName + ".scene"))
        {
            GGLogger::Warning("Scene Scenes\\" + resolvedName + ".scene found but not loaded!");
            GGECSStandardAssetManager<World>::RegisterAsset(*uuidOpt);
        }
    }
  
    CompleteSceneSwitch(resolvedName);

    // start audio fade and schedule scene switch ---
    /*auto* audio = GGEngine::GetEngine().GetAudioSystem();
    if (audio) {
        audio->StartSceneTransition(resolvedName, GGEngineConfig::GGDefaultGameSettings::GetAudioFadeDuration());
    }
    else {
        CompleteSceneSwitch(resolvedName);
    }*/

    return true;
}

/*************************************************************************
  \brief
    Finalizes a deferred scene switch.
  \param[in] resolvedName
    Name of the scene to activate.
  \details
    Completes a scene transition that was previously requested via
    SetActiveScene(). Emits the global scene switch event and updates
    the active scene key.
*************************************************************************/
void SceneManager::CompleteSceneSwitch(std::string& resolvedName) {
    bool hadActive = HasActiveScene();
    if (hadActive) {
        World& fromWorld = GetActiveScene();
        activeSceneKey = resolvedName;
        GlobalEventBus::onSwitchScenes.Emit(fromWorld, sceneMap[resolvedName]);
    }
    else {
        activeSceneKey = resolvedName;
    }
}

/*************************************************************************
  \brief 
    Retrieves the currently active scene
  \return 
    Reference to the active ECS world
*************************************************************************/
World& SceneManager::GetActiveScene() {
    auto it = sceneMap.find(activeSceneKey);
    if (activeSceneKey.empty() || it == sceneMap.end()) {
        std::cerr << "[SceneManager] No active scene set or scene not found! Key: " << activeSceneKey << "\n";
        // Fallback: insert an empty world to avoid undefined behavior,
        // but callers should check HasActiveScene() first
        return sceneMap[activeSceneKey];
    }
    return it->second;
}

/*************************************************************************
  \brief 
    Checks whether there is an active scene
  \return 
    True if an active scene exists
*************************************************************************/
bool SceneManager::HasActiveScene() const {
    return (!activeSceneKey.empty() && sceneMap.find(activeSceneKey) != sceneMap.end());
}

/*************************************************************************
  \brief 
    Returns the key of the active scene
  \return 
    String key of the active scene
*************************************************************************/
const std::string& SceneManager::GetActiveSceneKey() const {
    return activeSceneKey;
}

/*************************************************************************
    \brief 
        Clear a single scene from ECS world
    \param 
        world ECS world reference
    \param 
        objectFactory Object factory reference
*************************************************************************/
void SceneManager::ClearScene(World& world, ObjectsFactory&) {
    // Clear all spawned entities
    world.ResetWorld();

    std::cout << "Scene Cleared Successfully!\n";
}

/*************************************************************************
  \brief 
    Clear all loaded scenes from ECS world
  \param 
    world ECS world reference
  \param 
    objectFactory Object factory reference
*************************************************************************/
void SceneManager::ClearAllLoadedScenes(World& world, ObjectsFactory& objectFactory) {
    // clear current active scene
    ClearScene(world, objectFactory);

    // clear loaded scenes
    loadedScenes.clear();
    for (auto& [key, sceneWorld] : sceneMap)
        sceneWorld.ResetWorld();
    sceneMap.clear();
    activeSceneKey = "UnknownScene";

    std::cout << "All scenes cleared successfully!";
}

/*************************************************************************
  \brief 
    Backup all loaded scenes into serialized JSON
  \param 
    et Entity type registry for serialization
  \return 
    True if all scenes were backed up successfully
*************************************************************************/
bool SceneManager::BackupAllScenes() {
    backupSceneJSON.clear(); // clear old backup

    if (sceneMap.empty()) {
        std::cerr << "[SceneManager] No scenes to back up.\n";
        return false;
    }

    for (auto& [sceneKey, world] : sceneMap) {
        if (sceneKey == "PrefabEditorWorld") {
            continue; // skip backing up prefabWorld
        }
        std::string serialized = EntityJSONInterface::SerializeWorld(world);
        if (serialized.empty()) {
            std::cerr << "[SceneManager] Failed to serialize scene: " << sceneKey << "\n";
            continue; // skip failing scenes
        }

        backupSceneJSON[sceneKey] = serialized;
    }

    backupActiveSceneKey = activeSceneKey; // backup which scene was activee

    std::cout << "[SceneManager] Backed up " << backupSceneJSON.size() << " scenes.\n";
    return true;
}

/*************************************************************************
  \brief 
    Restore all scenes from backup
  \param 
    et Entity type registry for deserialization
  \param 
    prefabManager Reference to PrefabManager for component handling
  \param 
    objectsFactory Reference to ObjectsFactory for object cleanup
  \return 
    True if all scenes were successfully restored
*************************************************************************/
bool SceneManager::RestoreAllScenes(PrefabManager& prefabManager) {
    if (backupSceneJSON.empty()) {
        std::cerr << "[SceneManager] No backup scenes to restore.\n";
        return false;
    }

    std::unordered_map<std::string, World> tempSceneMap;
    bool allRestored = true;

    for (auto& [sceneKey, jsonStr] : backupSceneJSON) {
        if (sceneKey == "PrefabEditorWorld") {
            continue; // skip restoring prefabWorld
        }
        rapidjson::Document doc = JSONUtils::ParseJSON(jsonStr);

        if (!doc.IsObject() || !doc.HasMember("entities") || !doc["entities"].IsArray()) {
            std::cerr << "[SceneManager] Invalid scene JSON for: " << sceneKey << "\n";
            allRestored = false;
            continue;
        }

        World newScene; // fresh world for each scene
        try {
            DeserializeSceneFromJSON(sceneKey, doc, prefabManager, newScene);
            tempSceneMap[sceneKey] = std::move(newScene); // only add if successful
        }
        catch (const std::exception& e) {
            std::cerr << "[SceneManager] Exception restoring scene '" << sceneKey
                << "': " << e.what() << "\n";
            allRestored = false;
            continue;
        }
    }

    if (!tempSceneMap.empty()) {
        // destroy old scenes safely
        for (auto& [sceneKey, scene] : sceneMap) {
            scene.ResetWorld();
        }

        // replace with restored scenes
        sceneMap = std::move(tempSceneMap);

        // restore active scene
        if (!backupActiveSceneKey.empty() && sceneMap.contains(backupActiveSceneKey)) {
            activeSceneKey = backupActiveSceneKey;
        }
        else {
            activeSceneKey = sceneMap.begin()->first; // fallback
        }

        std::cout << "[SceneManager] Successfully restored scenes.\n";

        // Probably don't need this now? Let me know if anything goes wrong
        // SetActiveScene(activeSceneKey);

        // only clear backup if all restored successfully
        if (allRestored) {
            backupSceneJSON.clear();
        }
    }
    else {
        std::cerr << "[SceneManager] Failed to restore any scenes.\n";
    }

    return allRestored;
}

// prefab propagation logic refactored
// debug logic macros
#define LOGP(x) std::cout << "[PrefabSync] " << x << "\n";
#define LOGPW(x) std::cout << "[PrefabSync: WARNING] " << x << "\n";
#define LOGPE(x) std::cout << "[PrefabSync: ERROR] " << x << "\n";

using SyncFunc = void(*)(World&, World&, Entity, Entity);
std::vector<SyncFunc> componentSyncRegistry;

/*************************************************************************
  \brief Registers a synchronization function for a specific component type T
  \details
    Adds a lambda to the global componentSyncRegistry.
    During prefab syncing, this lambda ensures that for component T:
      - If both prefab and scene entity have T - prefab value overwrites scene.
      - If scene has T but prefab does not - component T is removed.
      - If prefab has T but scene does not - component T is added to scene.

    This enables automatic syncing of instantiated prefab entities whenever
    prefab definitions change.
*************************************************************************/
template <typename T>
void RegisterSyncComponent() {
    componentSyncRegistry.push_back(
        [](World& scene, World& prefabWorld, Entity ent, Entity prefabEnt)
        {
            const bool sceneHas = scene.HasComponent<T>(ent);
            const bool prefabHas = prefabWorld.HasComponent<T>(prefabEnt);

            // if both scene and prefab has component, sync component
            if (sceneHas && prefabHas)
            {
                *scene.GetComponent<T>(ent) = *prefabWorld.GetComponent<T>(prefabEnt);
                LOGP("Updated component: " << typeid(T).name());
            }
            // if scene has but prefab doesn't have, remove component
            else if (sceneHas && !prefabHas)
            {
                scene.RemoveComponent<T>(ent);
                LOGP("Removed component: " << typeid(T).name());
            }
            // if prefab has but scene doesn't have, add component
            else if (!sceneHas && prefabHas)
            {
                scene.AddComponent<T>(ent, *prefabWorld.GetComponent<T>(prefabEnt));
                LOGP("Added missing component: " << typeid(T).name());
            }
        }
    );
}

/*************************************************************************
  \brief Registers all engine-supported components for prefab synchronization
  \details
    Calls RegisterSyncComponent<T>() for each supported component type.
    These sync rules define how prefab instances inside scenes get updated
    when prefab definitions are edited in the editor.

    Only components relevant to prefab propagation are registered here.
*************************************************************************/
void RegisterAllSyncComponents() {
    RegisterSyncComponent<AABBFloat>();
    RegisterSyncComponent<MeshRenderer>();
    RegisterSyncComponent<Rigidbody2DFloat>();
    RegisterSyncComponent<Sprite>();
    RegisterSyncComponent<SpriteAnimator>();
    //RegisterSyncComponent<LocalTransform>();
    //RegisterSyncComponent<WorldTransform>();
    RegisterSyncComponent<ScriptComponent>();
    RegisterSyncComponent<ECSGridComponent>();
    RegisterSyncComponent<AudioSource>();
    RegisterSyncComponent<TilemapComponent>();
    RegisterSyncComponent<UIComponent>();
    RegisterSyncComponent<TextComponent>();
    RegisterSyncComponent<WorldTextComponent>();
    RegisterSyncComponent<ObjectEffectComponent>();

}

/*************************************************************************
  \brief Collects all descendant entities under a root entity in hierarchy order
  \param scene Reference to the ECS World containing entities
  \param root Root entity of the hierarchy to traverse
  \param out Vector to append discovered child entities into
  \details
    Performs a recursive depth-first traversal using HierarchyComponent.
    Entities are appended to 'out' in the same order they appear in the
    prefab template hierarchy. This ordering is required so that prefab
    instances can sync children component-by-component in matching index order.
*************************************************************************/
void SceneManager::CollectSceneChildren(World& scene, Entity root, std::vector<Entity>& out) {
    if (!scene.HasComponent<HierarchyComponent>(root)) {
        return;
    }

    HierarchyComponent* hc = scene.GetComponent<HierarchyComponent>(root);
    Entity child = hc->firstChild;

    while (child != NULL_ENTITY) {
        out.push_back(child);

        // recurse into this child -> collects grandchildren, nested prefabs
        CollectSceneChildren(scene, child, out);

        if (scene.HasComponent<HierarchyComponent>(child)) {
            HierarchyComponent* ch = scene.GetComponent<HierarchyComponent>(child);
            child = ch->nextSibling;
        }
        else {
            child = NULL_ENTITY;
        }
    }
}

/*************************************************************************
  \brief Synchronizes a single instantiated prefab entity with its template
  \param scene ECS World containing the prefab instance
  \param prefabManager Reference to PrefabManager used to spawn templates
  \param instanceRoot Root entity of the prefab instance inside the scene
  \param prefabName Name of the prefab being synchronized
  \details
    Ensures that the instance's components match its prefab definition:
      - Root entity components are synced using registered sync functions.
      - All descendants are collected and synced in hierarchy order.
      - Prefab templates are lazily generated and cached for reuse.

    This operation updates components but preserves entity identity and
    hierarchy structure inside the scene.
*************************************************************************/
void SceneManager::SyncSinglePrefabInstance(World& scene, PrefabManager& prefabManager, Entity instanceRoot, const std::string& prefabName) {
    LOGP("Syncing prefab instance of type: " << prefabName);

    auto inst = scene.GetComponent<PrefabInstance>(instanceRoot);
    if (!inst) { return; }

    if (inst->GetPrefabInstanceName() != prefabName) { return; }

    // ensure prefab template exists
    if (!prefabTemplates.count(prefabName)) {
        std::vector<Entity> tmp;
        Entity root = prefabManager.SpawnFromPrefab(prefabName, *prefabWorld, tmp, NULL_ENTITY);

        PrefabTemplate tp;
        tp.root = root;
        tp.allEntities = tmp;
        prefabTemplates[prefabName] = tp;
    }

    PrefabTemplate& tp = prefabTemplates[prefabName];
    Entity prefabRoot = tp.allEntities[0];

    // sync root components
    for (auto& syncFunc : componentSyncRegistry)
        syncFunc(scene, *prefabWorld, instanceRoot, prefabRoot);

    // collect children
    std::vector<Entity> instanceChildren;
    CollectSceneChildren(scene, instanceRoot, instanceChildren);

    size_t prefabChildCount = tp.allEntities.size() - 1;
    size_t instanceChildCount = instanceChildren.size();

    size_t syncCount = std::min(prefabChildCount, instanceChildCount);

    // sync children
    for (size_t i = 0; i < syncCount; ++i) {
        Entity sceneChild = instanceChildren[i];
        Entity prefabChild = tp.allEntities[i + 1];

        for (auto& syncFunc : componentSyncRegistry)
            syncFunc(scene, *prefabWorld, sceneChild, prefabChild);
    }
}

/*************************************************************************
  \brief Synchronizes all instances of a specific prefab across all scenes
  \param prefabName Name of the prefab to synchronize
  \param prefabManager Reference to PrefabManager for template access
  \details
    Searches every loaded scene for entities containing a PrefabInstance
    referencing 'prefabName'.
    Each matching instance is synchronized via SyncSinglePrefabInstance().

    Modified scenes are automatically saved back to disk using SceneSerializer.
*************************************************************************/
void SceneManager::SyncPrefabInstances(const std::string& prefabName, PrefabManager& prefabManager) {
    LOGP("Starting targeted prefab sync for: " << prefabName);

    for (auto& [sceneName, scene] : sceneMap)
    {
        // skip saving PrefabEditorWorld and UnknownScene! (these are meant to stay as empty scenes)
        if (sceneName == "PrefabEditorWorld" || sceneName == "UnknownScene") {
            continue;
        }
            
        bool sceneModified = false;

        for (Entity ent : scene.GetLivingEntities())
        {
            if (!scene.HasComponent<PrefabInstance>(ent)) {
                continue;
            }
            
            auto inst = scene.GetComponent<PrefabInstance>(ent);
            if (!inst || inst->GetPrefabInstanceName() != prefabName) {
                continue; // ONLY sync this prefab type
            }

            SyncSinglePrefabInstance(scene, prefabManager, ent, prefabName);

            sceneModified = true;
        }

        if (sceneModified)
        {
            if (!SceneSerializer::SaveCurrentScene(sceneName, sceneName, *this)) {
                LOGPE("Failed to save scene: " << sceneName);
            }
            else {
                LOGP("Updated scene saved: " << sceneName);
            }
                
        }
    }

    LOGP("Finished targeted prefab sync for: " << prefabName);
}

/*************************************************************************
  \brief Synchronizes all prefab instances of every prefab type
  \param prefabManager Reference to PrefabManager for prefab templates
  \details
    Iterates through all registered prefabs in PrefabManager and calls
    SyncPrefabInstances() for each one.

    Used when global prefab updates occur (e.g., editor save-all).
*************************************************************************/
void SceneManager::SyncAllPrefabInstances(PrefabManager& prefabManager) {
    LOGP("Starting full prefab sync...");

    // iterate all prefab types
    for (auto& [prefabName, prefab] : prefabManager.GetPrefabs())
    {
        SyncPrefabInstances(prefabName, prefabManager);
    }

    LOGP("Finished full prefab sync.");
}

/*************************************************************************
  \brief Clears cached template data for a specific prefab
  \param prefabName Name of the prefab whose template cache should be cleared
  \details
    Removes the prefab's cached PrefabTemplate and reconstructs prefabWorld.
    On the next sync, the prefab template will be regenerated fresh.

    Use when a prefab file changes or is reimported.
*************************************************************************/
void SceneManager::ClearPrefabCache(const std::string& prefabName) {
    // remove cached template
    prefabTemplates.erase(prefabName);

    // reconstruct prefabWorld to create fresh template
    prefabWorld = std::make_unique<World>();

    LOGP("Cleared cached prefab template for: " << prefabName);
}

/*************************************************************************
  \brief Clears all cached prefab templates and resets the prefab world
  \details
    Empties the prefabTemplates map and recreates prefabWorld to ensure all
    templates will be fully regenerated during the next prefab sync.

    Typically used when performing a full reload of all prefab assets.
*************************************************************************/
void SceneManager::ClearAllPrefabCache() {
    // clear all cached templates
    prefabTemplates.clear();

    // reconstruct prefabWorld to create fresh templates
    prefabWorld = std::make_unique<World>();

    LOGP("Cleared ALL cached prefab templates.");
}

/*************************************************************************
  \brief Serializes and saves the specified scene to disk
  \param currentSceneName Name of the scene to serialize
  \param newSceneName Filename (without extension) to save the scene as
  \param sceneManager Reference to SceneManager for accessing scene worlds
  \return True if the scene was serialized and saved successfully
  \details
    Retrieves the World associated with the given scene name, serializes
    its entity/component data into JSON, attaches scene metadata, and
    writes the final .scene file into the Assets/Scenes directory.

    PrefabEditorWorld is intentionally skipped, as it represents an
    isolated template-editing world rather than a regular scene.
*************************************************************************/
bool SceneSerializer::SaveCurrentScene(const std::string& currentSceneName, const std::string& newSceneName, SceneManager& sceneManager) {
    if (currentSceneName == "PrefabEditorWorld") {
        return true; // skip saving prefabWorld
    }

    World* worldPtr = &sceneManager.GetActiveScene();

    // if scene exists, point to the existing scene
    auto& sceneMap = sceneManager.GetSceneMap();
    auto it = sceneMap.find(currentSceneName);
    if (it != sceneMap.end()) {
        worldPtr = &it->second;
    }

    // serialize the ECS world into a JSON string
    std::string sceneStr = EntityJSONInterface::SerializeWorld(*worldPtr);

    // parse the string
    rapidjson::Document doc = JSONUtils::ParseJSON(sceneStr);
    if (!doc.IsObject() || !doc.HasMember("entities")) {
        std::cerr << "Failed to serialize world for scene '" << currentSceneName << "'\n";
        return false;
    }

    // create the full scene with metadeta
    rapidjson::Document fullDoc;
    fullDoc.SetObject();
    auto& allocator = fullDoc.GetAllocator();

    fullDoc.AddMember("sceneName", rapidjson::Value(newSceneName.c_str(), allocator), allocator);
    fullDoc.AddMember("entities", rapidjson::Value(doc["entities"], allocator), allocator);

    // use JSONUtils to write to file
    std::filesystem::path dirName = std::filesystem::path(GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath()) / "Scenes";
    std::filesystem::create_directories(dirName); // ensures a file for "scenes" exists

    // build final save path
    std::filesystem::path filePath = dirName / (newSceneName + ".scene");

    if (!JSONUtils::SerializeJSONToFile(fullDoc, filePath.string())) {
        std::cerr << "Failed to save scene '" << newSceneName << "' to file.\n";
        return false;
    }

    std::cout << "Saving scene to: " << std::filesystem::absolute(filePath) << "\n";
    std::cout << "Scene '" << newSceneName << "' has been saved successfully!\n";

    return true;
}

/*************************************************************************
  \brief Retrieves the internal world used for prefab template generation
  \return Reference to the prefabWorld ECS World
  \details
    prefabWorld is a dedicated world separate from playable scenes.
    It is used to instantiate prefab templates during syncing or spawning.
*************************************************************************/
World& SceneManager::GetPrefabWorld() {
    return *prefabWorld;
}

/*************************************************************************
  \brief
    Returns true if there is no scene backup currently stored.
  \details
    Used by the script system to detect project switches vs hot-reloads.
    On project switch, ClearBackup() is called before onRecompileScripts
    fires, so this returns true � signalling scripts should discard old
    cached paths rather than re-registering them.
*************************************************************************/
bool SceneManager::IsBackupEmpty() const
{
    return backupSceneJSON.empty();
}

/*************************************************************************
  \brief
    Clear backup of SceneJson and ActiveSceneKey
*************************************************************************/
void SceneManager::ClearBackup()
{
    backupSceneJSON.clear();
    backupActiveSceneKey.clear();
}
