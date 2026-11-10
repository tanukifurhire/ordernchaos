/*************************************************************************
  \file       GGPrefabManager.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.3
  \brief
    Declares the PrefabManager class for loading, storing, and instantiating
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
    <string>, <vector>, <memory>, <unordered_map>,
    GGECSInterface.hpp, GGEngineRender.hpp,
    RapidJSONWrapper.hpp

  \history
    - 1.0 (16 September 2025): Initial version with prefab loading and spawning
    - 1.1 (2 October 2025): Add accessors to get PrefabCount() and return Prefab Container
    - 1.2 (28 October 2025): Updated ComponentData to store the component data as a string
      instead of rapidJSON::Value
    - 1.3 (07 November 2025): Refactored prefab spawning logic to support prefab hierarchy

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <GGECSInterface.hpp>
class World;

#include "RapidJSONWrapper.hpp"

/*************************************************************************
  \brief
    Prefab manager class for ECS entity creation and runtime component handling
  \par Usage
  \code
    PrefabManager mgr;
    mgr.LoadPrefabsFromFile("prefabs.json");

    // Spawn entity from prefab
    Entity e = mgr.SpawnFromPrefab("PlayerCharacter", world, entityTypes);

    // Add component dynamically
    PrefabManager::ComponentData healthComp;
    healthComp.type = "Health";
    healthComp.data = ...; // RapidJSON serialized data
    mgr.AddComponentToEntity(e, healthComp, world);

    // Query loaded prefabs
    size_t count = mgr.PrefabCount();
    const auto& prefabs = mgr.GetPrefabs();
  \endcode
*************************************************************************/
class PrefabManager {
public:

    /*************************************************************************
      \struct 
        PrefabManager::ComponentData
      \brief 
        Represents data for a single ECS component
      \details
        Stores type information and serialized data for prefab or runtime use.
      \members
        - uint32_t typeID : Unique numeric ID for component type
        - std::string type : Component type name (e.g., "transform", "health")
        - rapidjson::Value data : Serialized component data
    *************************************************************************/
    struct ComponentData {
        uint16_t typeID = 0;
        std::string typeName;   // e.g. "transform"
        std::string dataString; // serialized component JSON data
    };

    /*************************************************************************
      \struct PrefabManager::Prefab
      \brief Represents a prefab consisting of multiple components
      \details
        Used to define entity templates for ECS instantiation.
      \members
        - std::string name : Prefab name (e.g., "PlayerCharacter")
        - std::vector<ComponentData> components : List of components
    *************************************************************************/
    struct Prefab {
        std::string prefabName;         // e.g. CharacterBody
        std::string prefabParentName;
        std::vector<ComponentData> components;
    };

    /*************************************************************************
      \brief 
        Load prefabs from a JSON file
      \param 
        filename Path to JSON file
      \return 
        True if successfully loaded
    *************************************************************************/
	bool LoadPrefabsFromFile(const std::string& filename);

    bool LoadAllPrefabsFromDirectory(const std::string& directory);

    /*************************************************************************
      \brief 
        Spawn an entity from a prefab
      \param 
        prefabName Name of the prefab
      \param 
        world ECS world reference
      \param 
        et Entity type registry
      \return 
        Spawned entity
    *************************************************************************/
	Entity SpawnFromPrefab(const std::string& prefabName, World& world, std::vector<Entity>& spawnedEntities, Entity parentEntity = NULL_ENTITY);

    /*************************************************************************
      \brief 
        Add a component to an existing entity at runtime (Helper)
      \param 
        entity Target entity
      \param 
        compData Component data to add
      \param 
        world ECS world reference
      \return 
        True if successful
    *************************************************************************/
    bool AddComponentToEntity(Entity entity, ComponentData& compData, World& world);

    /*************************************************************************
      \brief 
        Get all loaded prefabs
      \return 
        Constant reference to prefab container
    *************************************************************************/
    const std::unordered_map<std::string, Prefab>& GetPrefabs() const { return m_prefabs; }

    Prefab& GetPrefabByName(std::string const& name);
    
    std::string ResolvePrefabName(std::string const& path);

    /*************************************************************************
      \brief 
        Get number of loaded prefabs
      \return 
        Prefab count
    *************************************************************************/
    const size_t PrefabCount() const { return m_prefabs.size(); }

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
    void SaveEntityRecursive(
        Entity entity,
        const std::string& parentName,
        std::string& prefabName,
        rapidjson::Value& prefabArray,
        rapidjson::Document::AllocatorType& allocator,
        World& world,
        bool isNewPrefab);

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
    rapidjson::Document SaveEntityBackup(Entity entity, World& world);

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
    void SaveEntityRecursive(Entity entity, World& world, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer);

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
    void SaveEntity(Entity entity, World& world, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer);

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
    bool SavePrefab(Entity root, const std::string& prefabName, World& world);

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
    Entity DeserializeEntityFromJSON(rapidjson::Document& document, World& world, Entity originalEntity);

    /*************************************************************************
      \brief
        Retrieves the currently edited prefab name in the editor.
      \return
        The name of the currently edited prefab.
    *************************************************************************/
    std::string const& GetCurrentlyEditedPrefabName() const;

    /*************************************************************************
      \brief
        Sets the currently edited prefab name in the editor.
      \param currentName
        Name of the prefab to mark as currently edited.
    *************************************************************************/
    void SetCurrentlyEditedPrefabName(std::string const& currentName);
    
    // check if the map contains a Prefab type
    bool HasPrefab(std::string entityName) const {
        return m_prefabs.contains(entityName);
    }

private:
    std::string currentlyEditedPrefab;

	std::unordered_map<std::string, Prefab> m_prefabs; // stores all loaded prefabs (lightweight registry)

    std::unordered_map<std::string, std::string> m_prefabJsonCache; // prefab name -> raw JSON string for two-pass spawning

}; // end of class PrefabManager

struct Health {
    static constexpr uint16_t COMPONENT_ID = 25;
    static constexpr size_t SERIALIZED_SIZE = 8;

    int maxHp = 100;
    int currentHp = 100;
};