/*************************************************************************
  \file		  GGECSGridSystem.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief	  ECS system for managing grids and tilemaps.

  \details
	Provides initialization, update, and asset management for grid
	components and tilemaps in the ECS world. Integrates with GridManager
	and GGTilemapRenderer for rendering.

  \dependencies
	- GGSystemInterface.hpp
	- GGGridManager.hpp
	- GGTilemapData.hpp
	- GGTilemapRenderer.hpp

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGSystemInterface.hpp>
#include <GGGridManager.hpp>

#include <GGTilemapData.hpp>
#include <GGTilemapRenderer.hpp>

class GGECSGridSystem : public SystemInterface
{
public:
	static constexpr size_t MAX_TILEMAP_INSTANCES = 4095;

	/*************************************************************************
	  \brief
		Initialize the grid system.
	  \return
		true if initialization succeeds, false otherwise.
	  \details
		Sets up necessary callbacks and ensures the GridManager singleton is ready
		for use in the ECS world.
	*************************************************************************/
	bool Init() override;

	/*************************************************************************
	  \brief
		Update all grid and tilemap components in the ECS world.
	  \param[in] world
		Reference to the ECS world containing entities and components.
	  \details
		This function is called once per frame. It updates both grid-based
		components and tilemap-related grids using internal update functions.
	*************************************************************************/
	void Update(World& world) override;

	/*************************************************************************
	  \brief
		Retrieve the singleton GridManager instance.
	  \return
		Pointer to the GridManager.
	  \details
		Provides access to the grid manager for querying or modifying
		grid data outside the system.
	*************************************************************************/
	inline Grid::GridManager* GetGridManager() { return gridMgr; }

	/*************************************************************************
	  \brief
		Get a tilemap by its file path.
	  \param[in] filePath
		Path of the tilemap asset file.
	  \return
		Reference to the requested GGTilemap.
	  \details
		Returns the default tilemap if the requested tilemap has not been
		registered or loaded.
	*************************************************************************/
	GGTilemap const& GetTileMap(std::string const& filePath);

	/*************************************************************************
	  \brief
		Retrieve all registered tilemaps.
	  \return
		Reference to the internal unordered_map of file paths to GGTilemap instances.
	  \details
		Allows iteration or queries over all currently registered tilemaps.
	*************************************************************************/
	std::unordered_map<std::string, GGTilemap>&
		GetTileMapList();

	/*************************************************************************
	  \brief
		Register a new tilemap by file path.
	  \param[in] filePath
		Path to the tilemap asset file.
	  \return
		true if registration succeeded, false if already registered.
	  \details
		Loads and stores the tilemap internally if it hasn’t been registered yet.
		Enables subsequent retrieval and rendering of the tilemap.
	*************************************************************************/
	bool RegisterTileMap(std::string const& filePath);

	/*************************************************************************
	  \brief
		Access the tilemap renderer.
	  \return
		Reference to the GGTilemapRenderer.
	  \details
		The renderer handles drawing all registered tilemaps for the ECS world.
	*************************************************************************/

	GGTilemapRenderer& GetTilemapRenderer() { return tilemapRenderer; }

private:
	GGTilemap defaultTilemap; // Default tilemap returned if requested tilemap not found

	Grid::GridManager* gridMgr = Grid::GridManager::GetInstance(); // Singleton GridManager

	Callback<World&, Entity> onEntityDestroyedCallback; // Callback for entity destruction

	std::function<void(Entity,	// Updates ECS grid components
		WorldTransform&, 
		ECSGridComponent&)>
		updateGrids;

	std::function<void(Entity, // Updates ECS grid components associated with tilemaps
		WorldTransform&,
		ECSGridComponent&,
		TilemapComponent&)>
		updateTilemapGrids;

	std::unordered_map<std::string, GGTilemap> // Map of registered tilemaps
		tilemapList;

	Callback<> onReloadAssetsCallback; // Callback triggered when assets are reloaded

	Callback<> onStartPlayModeCallback; // Callback triggered when play mode starts

	GGTilemapRenderer tilemapRenderer; // Renderer for all tilemaps
};