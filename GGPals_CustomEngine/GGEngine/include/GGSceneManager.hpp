/*************************************************************************
  \file       GGSceneManager.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.2
  \brief
	Declares the SceneManager and SceneNode classes for managing scene
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
	<memory>, <string>, <vector>, <unordered_map>, <filesystem>
	GGECSInterface.hpp, GGECSWorld.hpp,
	GGObjectsFactory.hpp, RapidJSONWrapper.hpp

  \history
	- 1.0 (30 September 2025): Initial version with scene hierarchy and
	  scene loading/instantiation
	- 1.1 (2 October 2025): Added accessors for retrieving loaded scene
	  containers and scene counts
	- 1.2 (07 November 2025): Added prefab hierarchy support, backup/restore
	  functionality, and detailed Doxygen documentation

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef GGSCENEMANAGER_HPP
#define GGSCENEMANAGER_HPP

#include "GGECSInterface.hpp"

#include <memory> // for smart pointers
#include <string>
#include <vector>
#include <filesystem>

class World;
class PrefabManager;
class ObjectsFactory;

/*************************************************************************
  \struct LoadedScene
  \brief
	Container for a loaded scene definition.
  \details
	- Stores the scene name for identification.
	- Holds the parsed RapidJSON document representing the scene data.
  \par Usage
  \code
	LoadedScene scene;
	scene.sceneName = "level01";
	// scene.sceneDoc already holds parsed JSON after LoadSceneFromFile()
  \endcode
*************************************************************************/
struct LoadedSceneData {
	std::string sceneName;
	rapidjson::Document sceneDoc;
};

/*************************************************************************
  \class SceneManager
  \brief
	Scene management system for loading, instantiating, and maintaining
	scene hierarchies.
  \details
	- Loads scene definitions from JSON files into memory.
	- Instantiates entities into the ECS world via PrefabManager and
	  ObjectsFactory.
	- Tracks active and loaded scenes.
	- Supports clearing and updating entire scenes.
  \par Usage
  \code
	SceneManager sceneMgr;
	sceneMgr.LoadSceneFromFile("Scenes/level01.json");
	sceneMgr.InstantiateScene(world, prefabMgr, objFactory, entityTypes, 0);
	sceneMgr.UpdateScene();
  \endcode
*************************************************************************/
class SceneManager {
public:

	/*************************************************************************
	  \brief
		Constructs a new SceneManager instance.
	  \details
		Initializes internal containers and state used for managing loaded
		scenes, active scene tracking, and prefab synchronization.
	*************************************************************************/
	SceneManager();

	/*************************************************************************
	  \brief 
		Load scene data from a JSON file into a temporary LoadedSceneData
	  \param 
		filename Path to the JSON scene file
	  \return 
		true if the file was successfully read and parsed; false otherwise
	*************************************************************************/
	bool LoadNewSceneData(const std::string& filename);

	/*************************************************************************
	  \brief 
		Deserialize a JSON document into ECS entities in a new world
	  \param 
		sceneName Name of the scene being deserialized
	  \param 
		doc JSON document containing entities and components
	  \param 
		prefabManager Reference to PrefabManager to instantiate components
	  \param 
		newScene Reference to the ECS world to populate
	  \return 
		true if deserialization succeeded
	  \details 
		Handles remapping of hierarchy components (parent, firstChild, nextSibling).
	*************************************************************************/
	bool DeserializeSceneFromJSON(const std::string sceneName, const rapidjson::Document& doc, PrefabManager& prefabManager, World& newScene);

	/*************************************************************************
	  \brief 
		Add a new scene from a JSON file and instantiate it in ECS world
	  \param 
		filename Path to the JSON scene file
	  \param 
		prefabManager Reference to PrefabManager for component creation
	  \return 
		true if scene was successfully loaded and instantiated
	*************************************************************************/
	bool AddNewScene(const std::string& filename, PrefabManager& prefabManager);

	/*************************************************************************
	  \brief 
		Get the name of the most recently loaded scene
	  \return 
		Reference to the scene name string
	*************************************************************************/
	std::string const& GetLatestLoadedSceneName() const;

	/*************************************************************************
	  \brief 
		Get a loaded scene by name
	  \param 
		sceneName Name of the scene to retrieve
	  \return 
		Reference to the ECS World representing the scene
	  \throws 
		AssetFailedToLoadException if the scene does not exist
	*************************************************************************/
	World& GetLoadedSceneByName(std::string const& sceneName);

	/*************************************************************************
	  \brief 
		Set the active scene for runtime operations
	  \param 
		sceneKey Name of the scene to set as active
	  \return 
		true if the scene exists and was set; false otherwise
	*************************************************************************/
	bool SetActiveScene(std::string sceneKey);
	
	/*************************************************************************
	  \brief
		Completes a deferred scene switch operation.
	  \param[in] resolvedName
		Name of the scene to finalize as active.
	  \details
		Finalizes a scene change that was previously requested via
		SetActiveScene(). This function is typically called after any
		transition effects (e.g. audio fade-out) have completed.
	*************************************************************************/
	void CompleteSceneSwitch(std::string& resolvedName);

	/*************************************************************************
	  \brief 
		Get the currently active scene's ECS world
	  \return 
		Reference to the World object of the active scene
	  \pre 
		HasActiveScene() must return true
	*************************************************************************/
	World& GetActiveScene();

	/*************************************************************************
	  \brief 
		Check whether an active scene has been set
	  \return 
		true if there is an active scene; false otherwise
	*************************************************************************/
	bool HasActiveScene() const;

	/*************************************************************************
	  \brief 
		Get the key (name) of the active scene
	  \return 
		Reference to the string storing the active scene key
	  \details
		- Returns an empty string if no active scene is set.
	*************************************************************************/
	const std::string& GetActiveSceneKey() const;

	/*************************************************************************
	  \brief 
		Clear a single scene from ECS world
	  \param 
		world ECS world reference
	  \param 
		objectFactory Object factory reference
	*************************************************************************/
	void ClearScene(World& world, ObjectsFactory& objectFactory);

	/*************************************************************************
	  \brief 
		Clear all loaded scenes from ECS world
	  \param 
		world ECS world reference
	  \param 
		objectFactory Object factory reference
	*************************************************************************/
	void ClearAllLoadedScenes(World& world, ObjectsFactory& objectFactory);

	/*************************************************************************
	  \brief 
		Get the internal map of loaded scenes
	  \return 
		Reference to the scene map
	*************************************************************************/
	std::unordered_map<std::string, World>& GetSceneMap() {
		return sceneMap;
	}

	/*************************************************************************
	  \brief 
		Get the total number of loaded scenes
	  \return 
		Number of loaded scenes
	*************************************************************************/
	const size_t GetSceneCount() const {
		return sceneMap.size();
	}

	/*************************************************************************
	  \brief 
		Backup all currently loaded scenes into serialized JSON
	  \param 
		et Entity type registry for serialization
	  \return 
		true if backup succeeded
	*************************************************************************/
	bool BackupAllScenes();

	/*************************************************************************
	  \brief 
		Restore all scenes from backup JSON
	  \param 
		et Entity type registry for deserialization
	  \param 
		prefabManager PrefabManager reference to instantiate components
	  \param 
		objectsFactory ObjectsFactory reference to clear existing entities
	  \return 
		true if all scenes were successfully restored
	*************************************************************************/
	bool RestoreAllScenes(PrefabManager& prefabManager);

	/*************************************************************************
	  \brief
		Collects all child entities recursively starting from a root entity
	  \param scene
		Reference to the ECS world containing the entities
	  \param root
		Root entity from which to start collection
	  \param out
		Vector to store collected child entities
	*************************************************************************/
	void CollectSceneChildren(World& scene, Entity root, std::vector<Entity>& out);

	/*************************************************************************
	  \brief
		Sync a single prefab instance with its template
	  \param scene
		Reference to the ECS world containing the prefab instance
	  \param prefabManager
		Reference to PrefabManager for prefab handling
	  \param instanceRoot
		Root entity of the prefab instance in the scene
	  \param prefabName
		Name of the prefab template to sync from
	*************************************************************************/
	void SyncSinglePrefabInstance(World& scene, PrefabManager& prefabManager, Entity instanceRoot, const std::string& prefabName);

	/*************************************************************************
	  \brief
		Sync all instances of a specific prefab type across all loaded scenes
	  \param prefabName
		Name of the prefab type to sync
	  \param prefabManager
		Reference to PrefabManager for prefab handling
	*************************************************************************/
	void SyncPrefabInstances(const std::string& prefabName, PrefabManager& prefabManager);

	/*************************************************************************
	  \brief
		Sync all prefab instances across all scenes for all prefab types
	  \param prefabManager
		Reference to PrefabManager for prefab handling
	*************************************************************************/
	void SyncAllPrefabInstances(PrefabManager& prefabManager);

	/*************************************************************************
	  \brief Clears the cached template for a specific prefab
	  \param prefabName Name of the prefab whose cache is cleared
	  \details
		Removes the prefab's stored PrefabTemplate and resets the internal
		prefabWorld.
		Ensures that the next synchronization regenerates the template fresh.

		Typically used when a prefab asset is reimported or significantly modified.
	*************************************************************************/
	void ClearPrefabCache(const std::string& prefabName);

	/*************************************************************************
	  \brief Clears all cached prefab templates
	  \details
		Removes all prefab templates stored in the prefabTemplates map and
		reconstructs prefabWorld from scratch.

		Forces all prefab templates to be regenerated during next sync or spawn.
	*************************************************************************/
	void ClearAllPrefabCache();

	/*************************************************************************
	  \brief Access the internal prefab template world
	  \return Reference to the dedicated World used for generating prefab templates
	  \details
		prefabWorld is separate from scene worlds and is used solely to hold
		prefab template entities during synchronization and spawning.

		This function exposes the world for debugging or advanced editor features.
	*************************************************************************/
	World& GetPrefabWorld();

	/*************************************************************************
	  \brief
		Clear backup of SceneJson and ActiveSceneKey
	*************************************************************************/
	void ClearBackup();

	/*************************************************************************
	 \brief
		Returns true if there is no scene backup currently stored.
	 \details
		Used by the script system to detect project switches vs hot-reloads.
		On project switch, ClearBackup() is called before onRecompileScripts
		fires, so this returns true — signalling scripts should discard old
		cached paths rather than re-registering them.
*************************************************************************/
	bool IsBackupEmpty() const;

private:
	// for scene audio transition --------
	std::string pendingSceneKey;
	bool isSceneSwitchPending = false;
	// -----------------------------------

	// vector of stored loaded scenes
	std::vector<LoadedSceneData> loadedScenes;

	// map of loaded scenes (worlds) -> accessible by key
	std::unordered_map<std::string, World> sceneMap;

	std::string latestLoadedScene;

	// backup for all worlds
	std::unordered_map<std::string, std::string> backupSceneJSON;

	std::string backupActiveSceneKey;

	LoadedSceneData loadedSceneData;

	std::string activeSceneKey;

	// for prefab -> scene propagation
	struct PrefabTemplate {
		Entity root;					 // prefab root entity in prefabWorld
		std::vector<Entity> allEntities; // root + children

		std::unordered_map<std::string, Entity> nameToPrefabEntity;
	};

	std::unique_ptr<World> prefabWorld; // ONE prefab world for all prefab entities (templates)
	World* prefabEditorWorld = nullptr;
	std::unordered_map<std::string, PrefabTemplate> prefabTemplates;
};

class SceneSerializer {
public:
	/*************************************************************************
	  \brief Save the specified scene to a .scene file
	  \param currentSceneName Name of the scene to serialize
	  \param newSceneName Filename (without extension) to save the scene as
	  \param sceneManager Reference to SceneManager for world access
	  \return true if the scene was successfully serialized and saved
	*************************************************************************/
	static bool SaveCurrentScene(const std::string& currentSceneName, const std::string& newSceneName, SceneManager& sceneManager);
};

#endif // for header guards