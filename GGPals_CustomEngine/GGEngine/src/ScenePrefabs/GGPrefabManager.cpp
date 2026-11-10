/*************************************************************************
  \file       GGPrefabManager.cpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.2
  \brief
    Defines the PrefabManager class functions for loading, storing, and instantiating
    prefabs into the ECS world.

  \details
    Provides functionality to:
    - Load prefab definitions from JSON files.
    - Store prefabs with their associated component data.
    - Spawn entities from prefabs into the ECS world.
    - Dynamically add components to entities at runtime.
    - Track runtime component data for spawned entities.
    - Query loaded prefabs and prefab counts.

  \dependencies
    <iostream>
    GGJSONUtils.hpp, GGPrefabManager.hpp, GGECSInterface.hpp, GGECSWorld.hpp,
    GGECSComponents.hpp, GGEngineRender.hpp, GGECSMeshRenderer.hpp,
    GGSystemManagers.hpp, GGEngine.hpp,

  \history
    - 1.0 (16 September 2025): Initial version with prefab loading and spawning
    - 1.1 (2 October 2025): Add accessors to get PrefabCount() and return Prefab Container
    - 1.2 (28 October 2025): Updated PrefabManager to store ComponentData as JSON strings
      and parse them at runtime when adding components to entities (better practice)

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <iostream>
#include "GGJSONUtils.hpp"
#include "GGPrefabManager.hpp"
#include "GGECSInterface.hpp"
#include "GGECSWorld.hpp"
#include "GGECSComponents.hpp"
#include "GGEngineRender.hpp"
#include "GGECSMeshRenderer.hpp"
#include "GGSystemManagers.hpp"
#include "GGEngine.hpp"

#include <GGECSGameObjectBridge.hpp>

/*************************************************************************
  \brief
    Loads prefabs from a JSON file.
  \param filename
    Path to the JSON file containing prefab definitions.
  \return
    True if prefabs were loaded successfully, false otherwise.
*************************************************************************/
bool PrefabManager::LoadPrefabsFromFile(const std::string& filename) {
    // read file contents
    std::string prefabStr = JSONUtils::ReadFileContents(filename);
    rapidjson::Document doc = JSONUtils::ParseJSON(prefabStr);

    if (!doc.IsObject()) {
        std::cerr << "[PrefabManager] Invalid JSON format in " << filename << "\n";
        return false;
    }

    // New entity-based format: { "entities": [...] }
    if (doc.HasMember("entities") && doc["entities"].IsArray()) {
        const auto& entities = doc["entities"];
        if (entities.Empty()) {
            std::cerr << "[PrefabManager] Empty entities array in " << filename << "\n";
            return false;
        }

        // Root entity is the first one in the array
        std::string rootName = "Unknown Prefab";
        const auto& rootEnt = entities[0];
        if (rootEnt.HasMember("name") && rootEnt["name"].IsString()) {
            rootName = rootEnt["name"].GetString();
        }

        // Register in lightweight prefab registry
        Prefab prefab;
        prefab.prefabName = rootName;
        prefab.prefabParentName = "";
        m_prefabs[rootName] = std::move(prefab);

        // Cache the raw JSON for two-pass spawning
        m_prefabJsonCache[rootName] = prefabStr;

        return true;
    }

    // Legacy format: { "prefabs": [...] } - convert to entity format on load
    if (doc.HasMember("prefabs") && doc["prefabs"].IsArray()) {
        const auto& prefabArray = doc["prefabs"].GetArray();
        if (prefabArray.Empty()) {
            std::cerr << "[PrefabManager] Empty prefabs array in " << filename << "\n";
            return false;
        }

        // Find root prefab (the one with empty prefabParentName)
        std::string rootName = "Unknown Prefab";
        for (const auto& prefabObj : prefabArray) {
            if (!prefabObj.IsObject()) continue;
            if (prefabObj.HasMember("prefabParentName") && prefabObj["prefabParentName"].IsString()) {
                std::string parent = prefabObj["prefabParentName"].GetString();
                if (parent.empty() && prefabObj.HasMember("prefabName") && prefabObj["prefabName"].IsString()) {
                    rootName = prefabObj["prefabName"].GetString();
                    break;
                }
            }
        }

        // Register in lightweight prefab registry and cache
        // Legacy files still work but are loaded with old component-based spawning
        for (const auto& prefabObj : prefabArray) {
            if (!prefabObj.IsObject()) continue;
            if (!prefabObj.HasMember("prefabName") || !prefabObj["prefabName"].IsString()) continue;
            if (!prefabObj.HasMember("prefabParentName") || !prefabObj["prefabParentName"].IsString()) continue;

            Prefab prefab;
            prefab.prefabName = prefabObj["prefabName"].GetString();
            prefab.prefabParentName = prefabObj["prefabParentName"].GetString();

            if (prefabObj.HasMember("components") && prefabObj["components"].IsArray()) {
                for (const auto& compObj : prefabObj["components"].GetArray()) {
                    if (!compObj.IsObject()) continue;
                    if (!compObj.HasMember("componentID") || !compObj["componentID"].IsInt()) continue;

                    ComponentData comp;
                    comp.typeID = static_cast<uint16_t>(compObj["componentID"].GetInt());

                    for (auto it = compObj.MemberBegin(); it != compObj.MemberEnd(); ++it) {
                        std::string key = it->name.GetString();
                        if (key != "componentID" && it->value.IsObject()) {
                            comp.typeName = key;
                            comp.dataString = JSONUtils::StringifyJSONValue(it->value);
                            break;
                        }
                    }

                    prefab.components.push_back(std::move(comp));
                }
            }

            m_prefabs[prefab.prefabName] = std::move(prefab);
        }

        // Cache JSON for root prefab name (legacy files use component-based spawning fallback)
        m_prefabJsonCache[rootName] = prefabStr;

        return true;
    }

    std::cerr << "[PrefabManager] No 'entities' or 'prefabs' array in " << filename << "\n";
    return false;
}

bool PrefabManager::LoadAllPrefabsFromDirectory(const std::string& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".prefab") {
            LoadPrefabsFromFile(entry.path().string());
        }
    }
    return true;
}

/*************************************************************************
  \brief
    Recursively spawns an entity and all its child entities from a prefab.
  \param prefabName
    Name of the prefab to spawn.
  \param world
    Reference to the ECS world.
  \param spawnedEntities
    Vector to store all spawned entities.
  \param parentEntity
    Optional parent entity to attach spawned entities to.
  \return
    The root entity of the spawned prefab.
*************************************************************************/
Entity PrefabManager::SpawnFromPrefab(const std::string& prefabName, World& world, std::vector<Entity>& spawnedEntities, Entity parentEntity) {
    // Look up cached JSON for this prefab
    auto cacheIt = m_prefabJsonCache.find(prefabName);
    if (cacheIt == m_prefabJsonCache.end()) {
        std::cerr << "[PrefabManager] Prefab not found: " << prefabName << ".\n";
        return NULL_ENTITY;
    }

    rapidjson::Document doc = JSONUtils::ParseJSON(cacheIt->second);

    // New entity-based format: two-pass spawning with ID remapping
    if (doc.HasMember("entities") && doc["entities"].IsArray()) {
        const auto& entities = doc["entities"];
        std::unordered_map<uint32_t, Entity> idMap;
        Entity root = NULL_ENTITY;

        // First pass: create all entities and build old->new ID map
        for (const auto& ent : entities.GetArray()) {
            if (!ent.HasMember("id") || !ent["id"].IsUint()) {
                std::cerr << "[PrefabManager] Entity missing 'id' in prefab " << prefabName << "\n";
                continue;
            }
            uint32_t jsonID = ent["id"].GetUint();

            Entity newEntity = world.CreateEntityHandle();
            idMap[jsonID] = newEntity;
            spawnedEntities.push_back(newEntity);

            if (root == NULL_ENTITY) {
                root = newEntity; // first entity is root
            }
        }

        // Second pass: add components with remapped hierarchy IDs
        for (const auto& ent : entities.GetArray()) {
            uint32_t jsonID = ent["id"].GetUint();
            auto entIt = idMap.find(jsonID);
            if (entIt == idMap.end()) continue;
            Entity e = entIt->second;

            if (!ent.HasMember("components") || !ent["components"].IsArray()) continue;

            for (const auto& compObj : ent["components"].GetArray()) {
                if (!compObj.HasMember("componentID") || !compObj["componentID"].IsInt()) continue;

                ComponentData compData;
                compData.typeID = static_cast<uint16_t>(compObj["componentID"].GetInt());

                for (auto it = compObj.MemberBegin(); it != compObj.MemberEnd(); ++it) {
                    if (!it->name.IsString()) continue;
                    std::string key = it->name.GetString();
                    if (key != "componentID" && it->value.IsObject()) {
                        compData.typeName = key;
                        compData.dataString = JSONUtils::StringifyJSONValue(it->value);

                        // Remap hierarchy component entity IDs from old to new
                        if (static_cast<Components>(compData.typeID) == Components::HIERARCHY_COMPONENT) {
                            rapidjson::Document hierarchyDoc = JSONUtils::ParseJSON(compData.dataString);
                            auto remapField = [&](const char* fieldName) {
                                if (hierarchyDoc.HasMember(fieldName) && hierarchyDoc[fieldName].IsUint()) {
                                    uint32_t oldID = hierarchyDoc[fieldName].GetUint();
                                    if (oldID != 0) {
                                        auto mapIt = idMap.find(oldID);
                                        if (mapIt != idMap.end()) {
                                            hierarchyDoc[fieldName].SetUint(mapIt->second.id);
                                        } else {
                                            hierarchyDoc[fieldName].SetUint(0);
                                        }
                                    }
                                }
                            };
                            remapField("parent");
                            remapField("firstChild");
                            remapField("nextSibling");
                            compData.dataString = JSONUtils::StringifyJSONValue(hierarchyDoc);
                        }

                        AddComponentToEntity(e, compData, world);
                        break;
                    }
                }
            }
        }

        // Attach root to external parent if provided
        if (parentEntity != NULL_ENTITY && root != NULL_ENTITY) {
            auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();
            hierarchySystem.SetParent(world, root, parentEntity);
        }

        return root;
    }

    // Legacy format fallback: use old component-based spawning
    auto prefabIt = m_prefabs.find(prefabName);
    if (prefabIt == m_prefabs.end()) {
        std::cerr << "[PrefabManager] Prefab not found in registry: " << prefabName << ".\n";
        return NULL_ENTITY;
    }

    // Build parent-child map from legacy prefabs
    std::unordered_map<std::string, std::vector<std::string>> legacyChildren;
    for (auto& [name, prefab] : m_prefabs) {
        if (!prefab.prefabParentName.empty()) {
            legacyChildren[prefab.prefabParentName].push_back(prefab.prefabName);
        }
    }

    // Recursive lambda for legacy spawning
    std::function<Entity(const std::string&, Entity)> spawnLegacy;
    spawnLegacy = [&](const std::string& name, Entity parent) -> Entity {
        auto it = m_prefabs.find(name);
        if (it == m_prefabs.end()) return NULL_ENTITY;

        Entity entity = world.CreateEntityHandle();
        spawnedEntities.push_back(entity);

        for (auto compData : it->second.components) {
            AddComponentToEntity(entity, compData, world);
        }

        if (parent != NULL_ENTITY) {
            auto& hierarchySystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>();
            hierarchySystem.SetParent(world, entity, parent);
        }

        auto childIt = legacyChildren.find(name);
        if (childIt != legacyChildren.end()) {
            for (const auto& childName : childIt->second) {
                spawnLegacy(childName, entity);
            }
        }

        return entity;
    };

    return spawnLegacy(prefabName, parentEntity);
}

/*************************************************************************
  \brief
    Adds a component to an existing entity at runtime using stored JSON data.
  \param entity
    The target entity.
  \param compData
    Component data (type and serialized JSON string).
  \param world
    Reference to the ECS world.
  \return
    True if the component was added successfully, false otherwise.
*************************************************************************/
bool PrefabManager::AddComponentToEntity(Entity entity, ComponentData& compData, World& world) {

    // parse back into a document so that its values can be extracted
    rapidjson::Document data = JSONUtils::ParseJSON(compData.dataString);

    // if more component types to be added in the future, update here:
    switch (static_cast<Components>(compData.typeID)) {
    case Components::ENTITY_NAME: {
        if (!world.HasComponent<EntityName>(entity)) {
            world.AddComponent<EntityName>(entity, ComponentSerializer<EntityName>::FromJSON(data));
        }
        break;
    }

    case Components::AABB_FLOAT: {
        if (!world.HasComponent<AABBFloat>(entity)) {
            world.AddComponent<AABBFloat>(entity, ComponentSerializer<AABBFloat>::FromJSON(data));
        }
        break;
    }

    case Components::MESH_RENDERER: {
        if (!world.HasComponent<MeshRenderer>(entity)) {
            world.AddComponent<MeshRenderer>(entity, ComponentSerializer<MeshRenderer>::FromJSON(data));
        }
        break;
    }

    case Components::UI_COMPONENT: { // MeshRenderer
        if (!world.HasComponent<UIComponent>(entity)) {
            world.AddComponent<UIComponent>(entity, ComponentSerializer<UIComponent>::FromJSON(data));
        }
        break;
    }

    case Components::RIGIDBODY2D_FLOAT: { // Rigidbody2DFloat
        if (!world.HasComponent<Rigidbody2DFloat>(entity)) {
            world.AddComponent<Rigidbody2DFloat>(entity, ComponentSerializer<Rigidbody2DFloat>::FromJSON(data));
        }
        break;
    }

    case Components::MOVEMENT_INPUT: {
        if (!world.HasComponent<MovementInput>(entity)) {
            world.AddComponent<MovementInput>(entity, ComponentSerializer<MovementInput>::FromJSON(data));
        }
        break;
    }

    case Components::SCREEN_BOUNDS: {
        if (!world.HasComponent<ScreenBounds>(entity)) {
            world.AddComponent<ScreenBounds>(entity, ComponentSerializer<ScreenBounds>::FromJSON(data));
        }

        break;
    }

    case Components::PREFAB_INSTANCE: {
        if (!world.HasComponent<PrefabInstance>(entity)) {
            world.AddComponent<PrefabInstance>(entity, ComponentSerializer<PrefabInstance>::FromJSON(data));
        }

        break;
    }

    case Components::SPRITE: {
        if (!world.HasComponent<Sprite>(entity)) {
            world.AddComponent<Sprite>(entity, ComponentSerializer<Sprite>::FromJSON(data));
        }
        break;
    }

    case Components::SPRITE_ANIMATOR: {
        if (!world.HasComponent<SpriteAnimator>(entity)) {
            world.AddComponent<SpriteAnimator>(entity, ComponentSerializer<SpriteAnimator>::FromJSON(data));
        }
        break;
    }

    case Components::LOCAL_TRANSFORM: {
        if (!world.HasComponent<LocalTransform>(entity)) {
            world.AddComponent<LocalTransform>(entity, ComponentSerializer<LocalTransform>::FromJSON(data));
        }
        break;
    }

    case Components::WORLD_TRANSFORM: {
        if (!world.HasComponent<WorldTransform>(entity)) {
            world.AddComponent<WorldTransform>(entity, ComponentSerializer<WorldTransform>::FromJSON(data));
        }
        break;
    }

    case Components::NAME_COMPONENT: {
        if (!world.HasComponent<NameComponent>(entity)) {
            world.AddComponent<NameComponent>(entity, ComponentSerializer<NameComponent>::FromJSON(data));
        }
        break;
    }

    case Components::HIERARCHY_COMPONENT: {
        if (!world.HasComponent<HierarchyComponent>(entity)) {
            world.AddComponent<HierarchyComponent>(entity, ComponentSerializer<HierarchyComponent>::FromJSON(data));
        }
        break;
    }

    case Components::GRID_COMPONENT: {
        if (!world.HasComponent<ECSGridComponent>(entity)) {
            world.AddComponent<ECSGridComponent>(entity, ComponentSerializer<ECSGridComponent>::FromJSON(data));
        }
        break;
    }

    case Components::TILEMAP_COMPONENT:
        if (!world.HasComponent<TilemapComponent>(entity))
        {
            world.AddComponent<TilemapComponent>(entity, ComponentSerializer<TilemapComponent>::FromJSON(data));
        }
        break;

    case Components::SCRIPT_COMPONENT: {
        if (!world.HasComponent<ScriptComponent>(entity)) {
            world.AddComponent<ScriptComponent>(entity, ComponentSerializer<ScriptComponent>::FromJSON(data));
            ScriptComponent* script = world.GetComponent<ScriptComponent>(entity);

            if (script)
            {
                script->scriptInstance = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().CreateScriptInstance(world, entity);
                EntityInstance* instance = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().GetScriptInstance(script->scriptInstance);

                if (!instance) break;

                GGScriptMetadata* meta = instance->type;

                if (!meta) break;

                for (auto const& field : meta->fieldDescriptors)
                {
                    if (!meta->serializedFields.count(field.name)) continue;
                    std::cout << "FromJSON field: " << field.name << std::endl;

                    if (!data.HasMember(field.name.c_str())) continue;

                    char* dataPtr = static_cast<char*>(instance->data);
                    void* fieldPtr = dataPtr + field.offset;

                    switch (field.type)
                    {
                    case FieldDescriptor::TYPE_INT:
                        if (!data[field.name.c_str()].IsInt()) break;
                        *static_cast<int*>(fieldPtr) =
                            data[field.name.c_str()].GetInt();
                        std::cout << "Field " << field.name << " set successfully" << std::endl;
                        break;
                    case FieldDescriptor::TYPE_FLOAT:
                        if (!data[field.name.c_str()].IsDouble()) break;
                        *static_cast<float*>(fieldPtr) =
                            static_cast<float>(
                                data[field.name.c_str()].GetDouble()
                                );
                        break;
                    case FieldDescriptor::TYPE_STRING:
                        if (!data[field.name.c_str()].IsString()) break;
                        *static_cast<std::string*>(fieldPtr) =
                            data[field.name.c_str()].GetString();
                        break;
                    case FieldDescriptor::TYPE_BOOL:
                        if (!data[field.name.c_str()].IsBool()) break;
                        *static_cast<bool*>(fieldPtr) =
                            data[field.name.c_str()].GetBool();
                        break;
                    }
                }

                for (auto const& field : meta->fieldDescriptors)
                {
                    if (!meta->textureAssetFields.count(field.name)) continue;
                    std::cout << "FromJSON field: " << field.name << std::endl;

                    if (!data.HasMember(field.name.c_str())) continue;

                    char* dataPtr = static_cast<char*>(instance->data);
                    void* fieldPtr = dataPtr + field.offset;

                    switch (field.type)
                    {
                    case FieldDescriptor::TYPE_INT:
                    {
                        if (!data[field.name.c_str()].IsString()) break;

                        std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
                            data[field.name.c_str()].GetString()
                        );

                        if (uuid.has_value())
                        {
                            auto& assetSystem = GGAssetID::GetGGAssetID();

                            if (assetSystem->DoesUUIDExist(*uuid))
                            {
                                std::string const& filePath = assetSystem->GetFilePath(*uuid);

                                if (filePath == "") continue;

                                uint32_t assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuid);

                                *static_cast<int*>(fieldPtr) = assetID;

                                std::cout << "Field " << field.name << " set successfully" << std::endl;
                                break;
                            }
                            else
                            {
                                *static_cast<int*>(fieldPtr) = 0;
                            }
                        }
                        else
                        {
                            *static_cast<int*>(fieldPtr) = 0;
                        }

                        break;
                    }
                    }
                }

                // Recursive deserialization of composition fields from nested JSON objects
                lua_State* compDeserL = GetEngineInstance().GetLuaVM()->GetState();
                auto& compDeserScriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
                std::function<void(GGScriptMetadata*, EntityInstance*, const rapidjson::Value&)> deserializeComposition;
                deserializeComposition = [&](GGScriptMetadata* parentMeta, EntityInstance* parentInst, const rapidjson::Value& jsonObj)
                {
                    for (const auto& compFieldName : parentMeta->compositionFields)
                    {
                        if (!jsonObj.HasMember(compFieldName.c_str())) continue;
                        if (!jsonObj[compFieldName.c_str()].IsObject()) continue;

                        const rapidjson::Value& compObj = jsonObj[compFieldName.c_str()];
                        if (!compObj.HasMember("__composedType") || !compObj["__composedType"].IsString()) continue;

                        GGScriptMetadata* composedMeta = compDeserScriptSystem.GetMetadata(compObj["__composedType"].GetString());
                        if (!composedMeta) continue;

                        if (parentInst->luaFieldsRef == LUA_NOREF || parentInst->luaFieldsRef == LUA_REFNIL) continue;

                        lua_rawgeti(compDeserL, LUA_REGISTRYINDEX, parentInst->luaFieldsRef);
                        if (!lua_istable(compDeserL, -1)) { lua_pop(compDeserL, 1); continue; }

                        lua_getfield(compDeserL, -1, compFieldName.c_str());
                        if (lua_isuserdata(compDeserL, -1))
                        {
                            InstanceUserdata* subUD = (InstanceUserdata*)lua_touserdata(compDeserL, -1);
                            if (subUD && subUD->instance && subUD->instance->data)
                            {
                                char* subDataPtr = static_cast<char*>(subUD->instance->data);

                                // Deserialize serialized fields
                                for (const auto& innerField : composedMeta->fieldDescriptors)
                                {
                                    if (!composedMeta->serializedFields.count(innerField.name)) continue;
                                    if (!compObj.HasMember(innerField.name.c_str())) continue;

                                    void* innerPtr = subDataPtr + innerField.offset;
                                    switch (innerField.type)
                                    {
                                    case FieldDescriptor::TYPE_INT:
                                        if (compObj[innerField.name.c_str()].IsInt())
                                            *static_cast<int*>(innerPtr) = compObj[innerField.name.c_str()].GetInt();
                                        break;
                                    case FieldDescriptor::TYPE_FLOAT:
                                        if (compObj[innerField.name.c_str()].IsDouble())
                                            *static_cast<float*>(innerPtr) = static_cast<float>(compObj[innerField.name.c_str()].GetDouble());
                                        break;
                                    case FieldDescriptor::TYPE_STRING:
                                        if (compObj[innerField.name.c_str()].IsString())
                                            *static_cast<std::string*>(innerPtr) = compObj[innerField.name.c_str()].GetString();
                                        break;
                                    case FieldDescriptor::TYPE_BOOL:
                                        if (compObj[innerField.name.c_str()].IsBool())
                                            *static_cast<bool*>(innerPtr) = compObj[innerField.name.c_str()].GetBool();
                                        break;
                                    }
                                }

                                // Deserialize textureAsset fields
                                for (const auto& innerField : composedMeta->fieldDescriptors)
                                {
                                    if (!composedMeta->textureAssetFields.count(innerField.name)) continue;
                                    if (!compObj.HasMember(innerField.name.c_str())) continue;

                                    void* innerPtr = subDataPtr + innerField.offset;
                                    switch (innerField.type)
                                    {
                                    case FieldDescriptor::TYPE_INT:
                                    {
                                        if (!compObj[innerField.name.c_str()].IsString()) break;
                                        std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
                                            compObj[innerField.name.c_str()].GetString());
                                        if (uuid.has_value())
                                        {
                                            auto& assetSystem = GGAssetID::GetGGAssetID();
                                            if (assetSystem->DoesUUIDExist(*uuid))
                                            {
                                                std::string const& filePath = assetSystem->GetFilePath(*uuid);
                                                if (!filePath.empty())
                                                {
                                                    uint32_t assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuid);
                                                    *static_cast<int*>(innerPtr) = assetID;
                                                }
                                                else *static_cast<int*>(innerPtr) = 0;
                                            }
                                            else *static_cast<int*>(innerPtr) = 0;
                                        }
                                        else *static_cast<int*>(innerPtr) = 0;
                                        break;
                                    }
                                    }
                                }

                                // Recurse into nested composition
                                if (!composedMeta->compositionFields.empty())
                                    deserializeComposition(composedMeta, subUD->instance, compObj);
                            }
                        }
                        lua_pop(compDeserL, 1); // Pop sub-instance userdata or nil
                        lua_pop(compDeserL, 1); // Pop luaFields table

                        GGLogger::Info("Composition field " + compFieldName + "deserialized successfully");
                    }
                };

                deserializeComposition(meta, instance, data);

                // deserialize audio assets and their configs
                for (auto const& field : meta->fieldDescriptors)
                {
                    if (!meta->audioAssetFields.count(field.name)) continue;

                    if (!data.HasMember(field.name.c_str())) continue;

                    char* dataPtr = static_cast<char*>(instance->data);
                    void* fieldPtr = dataPtr + field.offset;

                    switch (field.type)
                    {
                    case FieldDescriptor::TYPE_INT:
                    {
                        if (!data[field.name.c_str()].IsString()) break;

                        std::optional<uuids::uuid> uuid =
                            uuids::uuid::from_string(data[field.name.c_str()].GetString());

                        if (uuid.has_value())
                        {
                            auto& assetSystem = GGAssetID::GetGGAssetID();

                            if (assetSystem->DoesUUIDExist(*uuid))
                            {
                                uint32_t assetID =
                                    GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuid);

                                *static_cast<int*>(fieldPtr) = assetID;
                            }
                            else
                            {
                                *static_cast<int*>(fieldPtr) = 0;
                            }
                        }
                        else
                        {
                            *static_cast<int*>(fieldPtr) = 0;
                        }

                        break;
                    }
                    }
                }

                for (auto const& field : meta->fieldDescriptors)
                {
                    if (!meta->audioAssetFields.count(field.name)) continue;

                    std::string cfgKey = field.name + "_config";
                    if (!data.HasMember(cfgKey.c_str())) continue;
                    if (!data[cfgKey.c_str()].IsObject()) continue;

                    const rapidjson::Value& cfgObj = data[cfgKey.c_str()];
                    AudioAssetConfig cfg{};

                    if (cfgObj.HasMember("volume") && cfgObj["volume"].IsDouble())
                    {
                        cfg.volume = static_cast<float>(cfgObj["volume"].GetDouble());
                    }
                    if (cfgObj.HasMember("pitch") && cfgObj["pitch"].IsDouble())
                    {
                        cfg.pitchVariance = static_cast<float>(cfgObj["pitch"].GetDouble());
                    }
                    if (cfgObj.HasMember("loop") && cfgObj["loop"].IsBool())
                    {
                        cfg.loop = cfgObj["loop"].GetBool();
                    }
                    if (cfgObj.HasMember("oneShot") && cfgObj["oneShot"].IsBool())
                    {
                        cfg.oneShot = cfgObj["oneShot"].GetBool();
                    }

                    meta->audioAssetConfigs[field.name] = cfg;
                }
            }
        }
        break;
    }

    case Components::AUDIO_SOURCE: {
        if (!world.HasComponent<AudioSource>(entity)) {
            world.AddComponent<AudioSource>(entity, ComponentSerializer<AudioSource>::FromJSON(data));
        }
        break;
    }
    // added text component
    case Components::TEXT_COMPONENT:{
        if (!world.HasComponent<TextComponent>(entity)) {
            world.AddComponent<TextComponent>(entity, ComponentSerializer<TextComponent>::FromJSON(data));
        }
        break;
    }
    case Components::WORLD_TEXT_COMPONENT: {
        if (!world.HasComponent<WorldTextComponent>(entity)) {
            world.AddComponent<WorldTextComponent>(entity, ComponentSerializer<WorldTextComponent>::FromJSON(data));
        }
        break;
    }
    case Components::OBJECT_EFFECT_COMPONENT: {
        if (!world.HasComponent<ObjectEffectComponent>(entity)) {
            world.AddComponent<ObjectEffectComponent>(entity, ComponentSerializer<ObjectEffectComponent>::FromJSON(data));
        }
        break;
    }

    default:
        std::cerr << "Unknown componentID: " << compData.typeID << ".\n";
        return false;
    }

    return true;
}

/*************************************************************************
  \brief
    Saves an entity and all its descendants into a backup JSON document.
  \param entity
    The entity to save.
  \param world
    Reference to the ECS world.
  \return
    A rapidjson::Document representing the serialized entity tree.
*************************************************************************/
rapidjson::Document PrefabManager::SaveEntityBackup(Entity entity, World& world)
{
    rapidjson::Document document;
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();
    writer.Key("entities");
    writer.StartArray();

    if (entity != NULL_ENTITY && world.IsEntityValidHandle(entity))
    {
        SaveEntityRecursive(entity, world, writer);
    }

    writer.EndArray();
    writer.EndObject();

    document.Parse(buffer.GetString());

    if (document.HasParseError()) {
        std::cerr << "[SaveEntityBackup] JSON parse error: "
            << document.GetParseError() << "\n";
    }

    return document;
}

/*************************************************************************
  \brief
    Recursively serializes an entity and all its children.
  \param entity
    Entity to serialize.
  \param world
    Reference to the ECS world.
  \param writer
    JSON writer to append the entity data.
*************************************************************************/
void PrefabManager::SaveEntityRecursive(Entity entity, World& world, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
{
    SaveEntity(entity, world, writer);

    if (world.HasComponent<HierarchyComponent>(entity))
    {
        HierarchyComponent* hierarchy = world.GetComponent<HierarchyComponent>(entity);
        if (hierarchy)
        {
            Entity child = hierarchy->firstChild;
            while (child != NULL_ENTITY)
            {
                // RECURSIVE CALL - saves child and all its descendants
                SaveEntityRecursive(child, world, writer);

                if (world.HasComponent<HierarchyComponent>(child))
                {
                    HierarchyComponent* childHierarchy = world.GetComponent<HierarchyComponent>(child);
                    child = childHierarchy ? childHierarchy->nextSibling : NULL_ENTITY;
                }
                else {
                    break;
                }
            }
        }
    }
}

/*************************************************************************
  \brief
    Serializes a single entity's data into JSON format.
  \param entity
    Entity to serialize.
  \param world
    Reference to the ECS world.
  \param writer
    JSON writer to append the entity's component data.
*************************************************************************/
void PrefabManager::SaveEntity(Entity entity, World& world, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
{
    writer.StartObject();
    std::string entityName = "Unknown";

    if (world.HasComponent<PrefabInstance>(entity))
    {
        entityName = world.GetComponent<PrefabInstance>(entity)->GetPrefabInstanceName();
    }

    writer.Key("id");
    writer.Uint(entity.id);

    writer.Key("generation");
    writer.Uint(entity.generation);

    writer.Key("name");
    writer.String(entityName.c_str());

    writer.Key("components");
    writer.StartArray();

    for (auto const& [typeID, componentArray] : world.GetComponentArrays())
    {
        writer.StartObject();
        writer.Key("componentID"); writer.Int(typeID);
        componentArray->SerializeToJSON(entity, writer);
        writer.EndObject();
    }

    writer.EndArray();
    writer.EndObject();
}

/*************************************************************************
  \brief
    Deserializes entities from a JSON document and reconstructs them in the ECS world.
  \param doc
    JSON document containing entity data.
  \param world
    Reference to the ECS world.
  \param originalEntity
    Optional original entity to match root mapping.
  \param parent
    Optional parent entity for hierarchical setup.
  \return
    The root entity of the deserialized hierarchy.
*************************************************************************/
Entity PrefabManager::DeserializeEntityFromJSON(rapidjson::Document& doc, World& world, Entity originalEntity)
{
    if (!doc.IsObject() || !doc.HasMember("entities") || !doc["entities"].IsArray()) {
        std::cerr << "[DeserializeEntityFromJSON] Error: invalid JSON" << "\n";
        return NULL_ENTITY;
    }

    const auto& entities = doc["entities"];
    std::unordered_map<uint32_t, Entity> idToEntityMap;

    Entity root = NULL_ENTITY;

    // first pass: create empty entities
    for (auto& ent : entities.GetArray()) {
        uint32_t jsonID = 0;
        uint8_t jsonGen = 0;
        if (ent.HasMember("id") && ent["id"].IsUint()) {
            jsonID = ent["id"].GetUint();
        }
        else {
            // entity doesnt get created if it doesnt have an id
            std::cerr << "[DeserializeEntityFromJSON] Entity in JSON missing 'id', skipping.\n";
            continue;
        }

        if (ent.HasMember("generation") && ent["generation"].IsUint()) {
            jsonGen = static_cast<uint8_t>(ent["generation"].GetUint());
        }
        else {
            // entity doesnt get created if it doesnt have an generation
            std::cerr << "[DeserializeEntityFromJSON] Entity in JSON missing 'generation', skipping.\n";
            continue;
        }

        Entity original(jsonID, jsonGen);

        Entity newEntity = world.CreateEntityHandle(); // runtime-generated ID
        idToEntityMap[jsonID] = newEntity;

        if (originalEntity == original)
        {
            root = newEntity;
        }
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
                                    else hierarchyObj["parent"].SetUint(0);
                                }
                                if (hierarchyObj.HasMember("firstChild")) {
                                    uint32_t childJSONID = hierarchyObj["firstChild"].GetUint();
                                    if (childJSONID != 0 && idToEntityMap.find(childJSONID) != idToEntityMap.end()) {
                                        hierarchyObj["firstChild"].SetUint(idToEntityMap[childJSONID].id);
                                    }
                                    else hierarchyObj["firstChild"].SetUint(0);
                                }
                                if (hierarchyObj.HasMember("nextSibling")) {
                                    uint32_t sibJSONID = hierarchyObj["nextSibling"].GetUint();
                                    if (sibJSONID != 0 && idToEntityMap.find(sibJSONID) != idToEntityMap.end()) {
                                        hierarchyObj["nextSibling"].SetUint(idToEntityMap[sibJSONID].id);
                                    }
                                    else hierarchyObj["nextSibling"].SetUint(0);
                                }

                                // restringify and store back into dataString
                                compData.dataString = JSONUtils::StringifyJSONValue(hierarchyObj);
                            }

                            AddComponentToEntity(e, compData, world);
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

    return root;
}

/*************************************************************************
  \brief
    Recursively saves an entity and its children as a prefab.
  \param entity
    Entity to save.
  \param parentName
    Name of the parent prefab (empty for root).
  \param prefabName
    Name of the prefab being saved.
  \param prefabArray
    JSON array to store prefab entries.
  \param allocator
    JSON allocator for rapidjson objects.
  \param world
    Reference to the ECS world.
  \param isNewPrefab
    Whether this is a new prefab or an update of an existing one.
*************************************************************************/
void PrefabManager::SaveEntityRecursive(
    Entity entity,
    const std::string& parentName,
    std::string& prefabName,
    rapidjson::Value& prefabArray,
    rapidjson::Document::AllocatorType& allocator,
    World& world,
    bool isNewPrefab)
{
    // generate a prefabName for every entity
    std::string entityName;
    // if is parent prefab
    if (parentName.empty()) {
        entityName = prefabName;
    }
    // else if is child prefab
    else {
        if (world.HasComponent<PrefabInstance>(entity)) {
            prefabName = world.GetComponent<PrefabInstance>(entity)->GetPrefabInstanceName();
            // if is an existing prefab, retain existing prefabName and nameComponent, else inherit from the nameComponent
            if (!prefabName.empty() && !isNewPrefab) {
                entityName = prefabName;
            }
            else {
                //entityName = parentName + "_Child_" + std::to_string(prefabArray.Size());
                entityName = world.GetComponent<NameComponent>(entity)->GetName();
            }
        }
        else {
            //entityName = parentName + "_Child_" + std::to_string(prefabArray.Size());
            entityName = world.GetComponent<NameComponent>(entity)->GetName();
        }
    }

    if (!world.HasComponent<PrefabInstance>(entity)) {
        world.AddComponent<PrefabInstance>(entity, PrefabInstance());
    }
    world.GetComponent<PrefabInstance>(entity)->SetPrefabInstanceName(entityName);

    if (!world.HasComponent<NameComponent>(entity)) {
        world.AddComponent<NameComponent>(entity, NameComponent());
    }
    world.GetComponent<NameComponent>(entity)->SetName(entityName);

    // serialize components
    rapidjson::Value componentsArray(rapidjson::kArrayType);
    for (auto const& [typeID, compArray] : world.GetComponentArrays()) {

        // skip components the entity does not have!
        if (!compArray->HasEntity(entity)) {
            continue;
        }

        // skip hierarchy component
        if (typeID == static_cast<uint16_t>(Components::HIERARCHY_COMPONENT)) {
            continue;
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("componentID");
        writer.Int(typeID);

        // only serialize if entity has this component
        compArray->SerializeToJSON(entity, writer);
        writer.EndObject();

        // create a temporary Document using the same allocator as prefabArray
        rapidjson::Document tempDoc(&allocator);
        tempDoc.Parse(buffer.GetString());

        // Only push if parsing succeeded and it�s an object
        if (!tempDoc.HasParseError() && tempDoc.IsObject()) {
            componentsArray.PushBack(tempDoc.Move(), allocator);
        }
    }

    // create prefab entry
    rapidjson::Value prefabObj(rapidjson::kObjectType);
    prefabObj.AddMember("prefabName", rapidjson::Value(entityName.c_str(), allocator), allocator);
    prefabObj.AddMember("prefabParentName", rapidjson::Value(parentName.c_str(), allocator), allocator);

    // update runtime cache BEFORE moving componentsArray
    Prefab newPrefab;
    newPrefab.prefabName = entityName;
    newPrefab.prefabParentName = parentName;

    if (!componentsArray.IsArray()) {
        std::cerr << "componentsArray is not an Array!\n";
    }
    for (rapidjson::SizeType i{ 0 }; i < componentsArray.Size(); ++i) {
        const rapidjson::Value& compVal = componentsArray[i];
        ComponentData compData;
        compData.typeID = static_cast<uint16_t>(compVal["componentID"].GetInt());
        compData.typeName = WorldHelpers::GetComponentTypeName(compData.typeID);
        compData.dataString = JSONUtils::StringifyJSONValue(compVal);
        newPrefab.components.push_back(std::move(compData));
    }

    // NOW move it into the prefab object
    prefabObj.AddMember("components", componentsArray, allocator);
    prefabArray.PushBack(prefabObj, allocator);

    // recursively save children
    if (world.HasComponent<HierarchyComponent>(entity)) {
        auto* hierarchy = world.GetComponent<HierarchyComponent>(entity);
        Entity child = hierarchy->firstChild;

        while (child != NULL_ENTITY) {
            SaveEntityRecursive(child, entityName, prefabName, prefabArray, allocator, world, isNewPrefab);

            if (world.HasComponent<HierarchyComponent>(child)) {
                child = (world.GetComponent<HierarchyComponent>(child))->nextSibling;
            }
            else {
                break;
            }
        }
    }
}

/*************************************************************************
  \brief
    Saves an entity as a prefab file and updates internal prefab caches.
  \param root
    Root entity of the prefab hierarchy.
  \param prefabName
    Name of the prefab to save.
  \param world
    Reference to the ECS world.
  \return
    True if the prefab was saved successfully, false otherwise.
*************************************************************************/
bool PrefabManager::SavePrefab(Entity root, const std::string& prefabName, World& world) {
    // check validity of root entity
    if (!root.IsValid()) {
        std::cerr << "[PrefabManager] Invalid entity for prefab. \n";
        return false;
    }

    std::string fileNameStr{ prefabName };
    if (fileNameStr.find(".prefab") == std::string::npos) {
        fileNameStr += ".prefab";
    }

    std::string prefabFilePath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Prefabs/" + fileNameStr;
    std::cout << "Saving prefab to: " << prefabFilePath << "\n";

    // Ensure root entity has PrefabInstance and NameComponent
    if (!world.HasComponent<PrefabInstance>(root)) {
        world.AddComponent<PrefabInstance>(root, PrefabInstance());
    }
    world.GetComponent<PrefabInstance>(root)->SetPrefabInstanceName(prefabName);

    if (!world.HasComponent<NameComponent>(root)) {
        world.AddComponent<NameComponent>(root, NameComponent());
    }
    world.GetComponent<NameComponent>(root)->SetName(prefabName);

    // Serialize using entity-based format (same as SaveEntityBackup)
    // This preserves entity IDs, hierarchy, and supports duplicates
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();
    writer.Key("entities");
    writer.StartArray();

    SaveEntityRecursive(root, world, writer);

    writer.EndArray();
    writer.EndObject();

    // Parse to document for file serialization
    rapidjson::Document saveDoc;
    saveDoc.Parse(buffer.GetString());

    if (saveDoc.HasParseError()) {
        std::cerr << "[PrefabManager] JSON parse error during prefab save.\n";
        return false;
    }

    // Save to file
    if (!JSONUtils::SerializeJSONToFile(saveDoc, prefabFilePath)) {
        std::cerr << "[PrefabManager] Failed to save prefab to file.\n";
        return false;
    }

    std::cout << "[PrefabManager] Prefab '" << prefabName << "' saved successfully.\n";

    // Update runtime caches immediately (no restart needed)
    // Cache the JSON string we just wrote
    m_prefabJsonCache[prefabName] = buffer.GetString();

    // Update lightweight registry
    Prefab registryEntry;
    registryEntry.prefabName = prefabName;
    registryEntry.prefabParentName = "";
    m_prefabs[prefabName] = std::move(registryEntry);

    // Refresh SceneManager prefab cache
    auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
    sceneManager.ClearAllPrefabCache();

    // Reload all prefabs from disk to pick up any other changes
    m_prefabs.clear();
    m_prefabJsonCache.clear();
    LoadAllPrefabsFromDirectory(GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Prefabs/");

    return true;
}

/*************************************************************************
  \brief
    Retrieves a prefab by name from the manager's internal storage.
  \param name
    Name of the prefab.
  \return
    Reference to the stored Prefab object.
  \throws
    AssetFailedToLoadException if the prefab does not exist.
*************************************************************************/
PrefabManager::Prefab& PrefabManager::GetPrefabByName(std::string const& name)
{
    std::string const& resolvedName = ResolvePrefabName(name);

    if (!m_prefabs.contains(resolvedName))
    {
        throw AssetFailedToLoadException(name);
    }

    return m_prefabs[resolvedName];
}

/*************************************************************************
  \brief
    Resolves a prefab name from a file path by removing prefixes and extensions.
  \param path
    File path to the prefab.
  \return
    Resolved prefab name.
*************************************************************************/
std::string PrefabManager::ResolvePrefabName(std::string const& path)
{
    std::string resolvedName = path;
    if (path.substr(0, 8) == "Prefabs\\")
    {
        resolvedName = path.substr(8);
        std::cout << "Resolved Prefab Name to " << resolvedName << std::endl;
    }

    if (path.substr(path.length() - 7) == ".prefab")
    {
        resolvedName.erase(resolvedName.end() - 7, resolvedName.end());
        std::cout << "Resolved Prefab Name to " << resolvedName << std::endl;
    }

    return resolvedName;
}

/*************************************************************************
  \brief
    Retrieves the currently edited prefab name in the editor.
  \return
    The name of the currently edited prefab.
*************************************************************************/
std::string const& PrefabManager::GetCurrentlyEditedPrefabName() const
{
    return currentlyEditedPrefab;
}

/*************************************************************************
  \brief
    Sets the currently edited prefab name in the editor.
  \param currentName
    Name of the prefab to mark as currently edited.
*************************************************************************/
void PrefabManager::SetCurrentlyEditedPrefabName(std::string const& currentName)
{
    currentlyEditedPrefab = currentName;
}