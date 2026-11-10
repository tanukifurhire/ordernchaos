/*************************************************************************
  \file		  GGECSGridSystem.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief	  Implementation of the GGECSGridSystem for ECS-based grid management.

  \details
	Handles grid and tilemap components for ECS entities, including
	initialization, updates, and asset reloading. Integrates with GridManager
	and GGTilemapSerializer for rendering and tilemap management.

  \dependencies
	- GGECSGridSystem.hpp
	- GGTilemapSerializer.hpp
	- GGEngine.hpp
	- GGECSAssetInterface.hpp

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGECSGridSystem.hpp>
#include <GGTilemapSerializer.hpp>

#include <GGEngine.hpp>

#include <GGECSAssetInterface.hpp>

/*************************************************************************
  \brief
	Initialize the grid system.
  \return
	true if initialization succeeds, false otherwise.
  \details
	Sets up necessary callbacks and ensures the GridManager singleton is ready
	for use in the ECS world.
*************************************************************************/
bool GGECSGridSystem::Init()
{
	if (gridMgr)
	{
		gridMgr->Initialize();
		std::cout << "[Grid] Grid system initialized!" << std::endl;

		Grid::GridRenderConfig& gridConfig = gridMgr->GetRenderConfig();
		gridConfig.gridOffsetX = -0.5f;
		gridConfig.gridOffsetY = -0.5f;
		gridConfig.tileSpacing = 0.f;
		gridConfig.tileSize = 0.8f;

		std::cout << "[Grid] Grids centered at origin (0, 0)" << std::endl;
	}
	else
	{
		std::cout << "[Grid] WARNING: Failed to get Gridmanager instance" << std::endl;
	}

	onEntityDestroyedCallback = [&](World& world, Entity entity)
		{
			if (world.HasComponent<ECSGridComponent>(entity))
			{
				ECSGridComponent* grid = world.GetComponent<ECSGridComponent>(entity);

				if (grid->gridID > 0) gridMgr->DestroyGrid(grid->gridID);
			}
		};

	onEntityDestroyed += onEntityDestroyedCallback;

	updateGrids = [&](Entity, WorldTransform& worldTransform, ECSGridComponent& grid)
		{
			Grid::GridComponent* gridComponent = gridMgr->GetGrid(grid.gridID);

			if (gridComponent)
			{
				gridComponent->SetPosition(worldTransform.matrix.getTranslation());
			}
		};

	updateTilemapGrids =
		[&](Entity, [[maybe_unused]] WorldTransform& worldTransform,
			ECSGridComponent& grid, TilemapComponent& tilemapComponent)
		{
			if (tilemapComponent.tilemapAssetID == 0) return;

			Grid::GridComponent* gridComponent = gridMgr->GetGrid(grid.gridID);

			if (gridComponent == nullptr) return;

			GGTilemap const& tilemap =
				GGECSStandardAssetManager<GGTilemap>::GetAsset(tilemapComponent.tilemapAssetID);

			if (gridComponent->GetTileMap().mapName != tilemap.mapName)
			{
				gridComponent->SetTileMap(tilemap);
			}

			GGMath::Matrix3x3 transform;
			transform.identity();

			gridMgr->RenderGrid(grid.gridID, tilemapComponent.perspectiveFactor);

			//tilemapRenderer.RenderTilemap(tilemap, transform, pos, tilemapComponent.perspectiveFactor);
		};

	onReloadAssetsCallback =
		[&]()
		{
			std::vector<std::string> paths;
			paths.reserve(tilemapList.size());

			for (const auto& [path, tilemap] : tilemapList)
			{
				paths.push_back(path);
			}

			tilemapList.clear();

			for (const auto& path : paths)
			{
				RegisterTileMap(path);
			}
		};

	onStartPlayModeCallback = [&]()
		{
			for (auto& [str, scene] : GetEngineInstance().GetSceneManager().GetSceneMap())
			{
				Update(scene);
			}
		};

	GlobalEventBus::onReloadAssets += onReloadAssetsCallback;
	GlobalEventBus::onPlayModeStart += onStartPlayModeCallback;

	return true;
}

/*************************************************************************
  \brief
	Update all grid and tilemap components in the ECS world.
  \param[in] world
	Reference to the ECS world containing entities and components.
  \details
	This function is called once per frame. It updates both grid-based
	components and tilemap-related grids using internal update functions.
*************************************************************************/
void GGECSGridSystem::Update(World& world)
{
	world.ForEach<WorldTransform, ECSGridComponent>(updateGrids);
	world.ForEach<WorldTransform, ECSGridComponent, TilemapComponent>(updateTilemapGrids);
}

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
bool GGECSGridSystem::RegisterTileMap(std::string const& filePath)
{
	GGTilemap tilemap;

	std::string resolved = GetEngineInstance().GetAssetManager()->ResolvePath(filePath);

	bool success = GGTilemapSerializer::LoadFromFile(resolved, tilemap);

	if (success)
	{
		tilemapList[filePath] = tilemap;
		return true;
	}

	return false;
}

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
GGTilemap const& GGECSGridSystem::GetTileMap(std::string const& filePath)
{
	if (!tilemapList.contains(filePath))
	{
		return defaultTilemap;
	}

	return tilemapList[filePath];
}

/*************************************************************************
  \brief
	Retrieve all registered tilemaps.
  \return
	Reference to the internal unordered_map of file paths to GGTilemap instances.
  \details
	Allows iteration or queries over all currently registered tilemaps.
*************************************************************************/
std::unordered_map<std::string, GGTilemap>&
GGECSGridSystem::GetTileMapList()
{
	return tilemapList;
}