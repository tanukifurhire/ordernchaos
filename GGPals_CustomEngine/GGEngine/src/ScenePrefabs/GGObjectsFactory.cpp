/*************************************************************************
  \file       GGObjectsFactory.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.2
  \brief
	Declares the ObjectsFactory class for managing ECS entity creation and
	destruction in the engine.

  \details
	Provides functionality to:
	- Instantiate entities from prefabs into the ECS world recursively (for children entities).
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
	- 1.2 (07 November 2025): Refactored instantiation logic for hierarchy
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <string>
#include "GGObjectsFactory.hpp"
#include "GGJSONUtils.hpp"
#include "GGPrefabManager.hpp"
#include "GGECSInterface.hpp"
#include "GGECSWorld.hpp"
#include "GGEngineRender.hpp"
#include "GGECSComponents.hpp"
#include <iostream>

// ADD OBJECTS ---------------------------------------------------------------

/*************************************************************************
	\brief Instantiate multiple entities from a prefab, 
	including all child entities recursively
	\param prefabName Name of the prefab
	\param prefabManager Reference to PrefabManager
	\param world ECS world reference
	\param objCount Number of entities to spawn
	\param parentEntity Optional parent entity to attach the spawned objects to
	\return True if all entities (including children) were spawned successfully
*************************************************************************/
bool ObjectsFactory::InstantiateObjects(const std::string& prefabName, PrefabManager& prefabManager, World& world, int objCount, Entity parentEntity) {
	std::cout << "Spawning object of name " << prefabName << std::endl;
	
	if (objCount <= 0) {
		std::cerr << "[ObjectsFactory] Object count must be at least 1\n";
		return false;
	}

	for (int i = 0; i < objCount; ++i) {
		std::vector<Entity> newEntities;
		prefabManager.SpawnFromPrefab(prefabName, world, newEntities, parentEntity);

		if (newEntities.empty()) {
			std::cerr << "[ObjectsFactory] Failed to spawn entities for prefab #" << i << "\n";
			continue;
		}

		// add all spawned entities (root + children) to tracking
		spawned_entities.insert(spawned_entities.end(), newEntities.begin(), newEntities.end());
		
	}

	return true;
}

/*************************************************************************
	\brief Get the most recently spawned entity
	\return Entity handle of last spawned object
*************************************************************************/
Entity ObjectsFactory::GetLastSpawnedObject() const {
	if (spawned_entities.empty()) {
		return Entity{}; // invalid
	}
	return spawned_entities.back();
}

// DESTROY OBJECTS ------------------------------------------------------------

/*************************************************************************
	\brief Destroy entities that are flagged as dead
	\param world ECS world reference
*************************************************************************/
void ObjectsFactory::DestroyOnDeath(World& world) {
	std::vector<Entity> toRemove;
	for (Entity entity : spawned_entities) {
		Health* health = world.GetComponent<Health>(entity);
		if (health && health->currentHp <= 0) {
			world.DestroyEntity(entity);
			toRemove.push_back(entity);
		}
	}
	for (Entity e : toRemove) {
		auto it = std::find(spawned_entities.begin(), spawned_entities.end(), e);
		if (it != spawned_entities.end()) { 
			spawned_entities.erase(it);
		}
	}
}

/*************************************************************************
	\brief Destroy all currently spawned entities
	\param world ECS world reference
*************************************************************************/
void ObjectsFactory::DestroyAllSpawnedObjects(World& world) {
	for (Entity entity : spawned_entities) {
		world.DestroyEntityHandle(entity);
	}

	spawned_entities.clear();
}