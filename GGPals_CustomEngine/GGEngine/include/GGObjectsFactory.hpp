/*************************************************************************
  \file       GGObjectsFactory.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.1
  \brief
	Declares the ObjectsFactory class for managing ECS entity creation and
	destruction in the engine.

  \details
	Provides functionality to:
	- Instantiate entities from prefabs into the ECS world.
	- Track spawned entities for later access.
	- Retrieve the most recently spawned entity.
	- Destroy individual entities when they die.
	- Clear all currently spawned entities from the world.

  \dependencies
	<string>, <vector>, <memory>, <unordered_map>,
	GGECSInterface.hpp, GGECSWorld.hpp,
	GGPrefabManager.hpp, GGEngineRender.hpp,
	RapidJSONWrapper.hpp

  \history
	- 1.0 (24 October 2025): Initial version with object instantiation and
	  destruction support for ECS entities.
	- 1.1 (27 October 2025): Added tracking for objects spawned by ObjectsFactory.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "GGECSInterface.hpp" // Your ECS interfaces
#include "RapidJSONWrapper.hpp"

class PrefabManager;
class World;

/*************************************************************************
  \class ObjectsFactory
  \brief Manages ECS entity creation, tracking, and removal
  \details Instantiates entities from prefabs, tracks spawned entities,
		   and handles their destruction in the ECS world.
*************************************************************************/
class ObjectsFactory {
public:

	// ADD OBJECTS ---------------------------------------------------------------

	/*************************************************************************
	  \brief Instantiate multiple entities from a prefab
	  \param prefabName Name of the prefab
	  \param prefabManager Reference to PrefabManager
	  \param world ECS world reference
	  \param objCount Number of entities to spawn
	  \param et Entity type registry
	  \return True if all entities were spawned successfully
	*************************************************************************/
	bool InstantiateObjects(const std::string& prefabName, PrefabManager& prefabManager, World& world, int objCount, Entity parentEntity = NULL_ENTITY);

	/*************************************************************************
	  \brief Get the most recently spawned entity
	  \return Entity handle of last spawned object
	*************************************************************************/
	Entity GetLastSpawnedObject() const;

	// DESTROY OBJECTS ------------------------------------------------------------

	/*************************************************************************
	  \brief Destroy entities that are flagged as dead
	  \param world ECS world reference
	*************************************************************************/
	void DestroyOnDeath(World& world);

	/*************************************************************************
	  \brief Destroy all currently spawned entities
	  \param world ECS world reference
	*************************************************************************/
	void DestroyAllSpawnedObjects(World& world);

private:

	// tracks only
	std::vector<Entity> spawned_entities;
};
