/*************************************************************************
  \file       AssetManagerNode.hpp
  \author     Zafir Haqqym, Tok Duan Jie Nelson
  \par        Email: zafir.h@digipen.edu, t.duanjienelson@digipen.edu
  \date       04 November 2025
  \version    5.1
  \contribute Zafir(20% AssetBrowser setup), Nelson(80%)
  \brief
	Implementation of asset manager UI panel.
  \details
	Declares asset browser with collapsible sections for each asset type,
	refresh functionality, selection tracking, and load-on-demand behavior.
	Integrates with GGAssetManager for discovery and loading operations.
	Provides visual feedback for asset states and comprehensive statistics.
  \dependencies
	AssetManagerNode.hpp, GGSystemManagers.hpp, GGECSAssetInterface.hpp,
	EditorEventBus.hpp, EditorSceneBase.hpp, GGLogger.hpp
  \see
	AssetManagerNode.hpp, GGAssetManager.hpp
  \history
	- 1.0 (04 November 2025): Initial implementation with asset discovery,
	  click-to-load lua scripts, asset statistics display, and UUID generation
	  for discovered assets.
	- 2.0 (05 November 2025): Added Prefabs, Scripts and Scenes implementation
	- 2.1 (06 November 2025): Creating new scene file with right click logic
	- 3.0 (13 November 2025): Popup handling of duplicated/unsupported files
	- 4.0 (15 Jan 2026): New asset browser with thumbnail
	- 5.0 (18 Mar 2026): Added "Assets Used" tab for per-scene asset reporting
	- 5.1 (05 Apr 2026): Assets Used tab now uses checkboxes for multi-scene
	  selection with Select All / Deselect All; combined report deduplicates
	  textures by filename and always writes to combinedScenes.txt
  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include "EditorSceneBase.hpp"
#include <memory>
#include <GGLogger.hpp>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>

#include <GL/glew.h>

class AssetManagerNode : public EditorSceneBase
{
public:
	/*************************************************************************
	 \brief
		Constructor for AssetManagerNode

	 \details
		Initializes callback handles for Init, Update, and DockBuild
		functions. These callbacks are used by the editor system to
		manage the asset manager's lifecycle and rendering.
	 *************************************************************************/
	AssetManagerNode();

	/*************************************************************************
	 \brief
		Initialize the asset manager node

	 \details
		Currently empty but reserved for future initialization logic
		such as loading persistent settings or setting up initial state.
	 *************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	 \brief
		Configure ImGui docking layout for the asset manager window

	 \details
		Splits the left dock vertically to create space for both the
		hierarchy (top) and asset manager (bottom). The asset manager
		window is docked to the bottom portion, taking 50% of the height.
	 *************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	 \brief
		Update and render the asset manager window each frame

	 \details
		Handles the asset refresh flag and renders the main asset
		manager ImGui window with a fixed size. The window contains
		the resource browser UI.
	 *************************************************************************/
	virtual void Update() override;

	std::vector<std::string> GetScenePaths() {
		return discoveredScenePaths;
	}
	/*************************************************************************
	 \brief
		Refresh all asset lists from disk and memory

	 \details
		Performs a complete refresh of the asset management system:
			1. Scans asset directories for new files on disk
			2. Preloads textures, audio, shaders, and lua scripts into memory
			3. Updates cached lists of loaded assets
			4. Updates discovered lists of on-disk assets
			5. Ensures all discovered assets have UUID meta files

	 \note
		This operation can be expensive as it scans directories and
		generates UUIDs for new assets. Should not be called every frame.
	 *************************************************************************/
	void RefreshAssetLists();

	/*************************************************************************
	 \brief
		Load a project by switching the asset system to a new
		Asset folder. Called by StopPlayToolbar when the user
		clicks New Project or Open Project.

	 \param[in] projectAssetsPath
		Absolute path to the project's "Assets" folder.

	 \details
			1. Calls GGAssetManager::SetAssetBasePath() - clearrs old cache.
			2. Re-creates all folderNode roots for the new location.
			3. Resets current Folder pointers to the new roots
			4. sets needsAssetRefresh = true so the browser rescans next frame.
	 *************************************************************************/
	void LoadProject(const std::string& projectAssetsPath);

private:
	// Cached asset paths (loaded into memory)
	std::vector<std::string> cachedTexturePaths;
	std::vector<std::string> cachedAudioPaths;
	std::vector<std::string> cachedShaderPaths;
	std::vector<std::string> cachedPrefabPaths;
	std::vector<std::string> cachedLuaPaths;
	std::vector<std::string> cachedScenePaths;
	std::vector<std::string> cachedAnimationPaths;
	std::vector<std::string> cachedTilemapPaths;
	std::vector<std::string> cachedFontPaths;

	// Discovered asset paths (found on disk)
	std::vector<std::string> discoveredTexturePaths;
	std::vector<std::string> discoveredShaderPaths;
	std::vector<std::string> discoveredAudioPaths;
	std::vector<std::string> discoveredPrefabPaths;
	std::vector<std::string> discoveredLuaPaths;
	std::vector<std::string> discoveredScenePaths;
	std::vector<std::string> discoveredAnimationPaths;
	std::vector<std::string> discoveredTilemapPaths;
	std::vector<std::string> discoveredFontPaths;

	// State flags
	bool needsAssetRefresh = false;
	bool projectLoaded = false;

	// state flags when game engine is on play mode
	bool isPlaying = false;
	Callback<> onPlayModeCallback;
	Callback<> onStopPlayModeCallback;

	// Selection indices (-1 = no selection)
	int selectedTextureIndex	= -1;
	int selectedAudioIndex		= -1;
	int selectedShaderIndex		= -1;
	int selectedPrefabIndex		= -1;
	int selectedLuaIndex		= -1;
	int selectedSceneIndex		= -1;
	int selectedAnimationIndex	= -1;
	int selectedTilemapIndex	= -1;
	int selectedFontIndex       = -1;

	// Selected asset paths
	std::string selectedAudioPath	= "";
	std::string selectedLuaPath		= "";

	// Thumbnail cache
	std::unordered_map<std::string, GLuint> thumbnailCache;
	GLuint defaultTextureThumbnail = 0;
	GLuint defaultAudioThumbnail = 0;
	GLuint defaultShaderThumbnail = 0;
	GLuint defaultPrefabThumbnail = 0;
	GLuint defaultScriptThumbnail = 0;
	GLuint defaultSceneThumbnail = 0;
	GLuint defaultAnimationThumbnail = 0;
	GLuint defaultTilemapThumbnail = 0;
	GLuint defaultFontThumbnail = 0;

	// Folder management structures
	struct FolderNode {
		std::string name;
		std::string fullPath;
		std::vector<std::string> assets;
		std::vector<std::shared_ptr<FolderNode>> subfolders;
		std::weak_ptr<FolderNode> parent;
		//bool isExpanded = false;

		FolderNode(const std::string& n, const std::string& p)
			: name(n), fullPath(p) {}
	};

	// Current folder being displayed per asset type
	std::shared_ptr<FolderNode> currentTextureFolder;
	std::shared_ptr<FolderNode> currentAudioFolder;
	std::shared_ptr<FolderNode> currentShaderFolder;
	std::shared_ptr<FolderNode> currentScriptFolder;
	std::shared_ptr<FolderNode> currentAnimationFolder;
	std::shared_ptr<FolderNode> currentPrefabFolder;
	std::shared_ptr<FolderNode> currentSceneFolder;
	std::shared_ptr<FolderNode> currentTilemapFolder;
	std::shared_ptr<FolderNode> currentFontFolder;

	// Root folders for each asset type
	std::shared_ptr<FolderNode> textureRootFolder;
	std::shared_ptr<FolderNode> audioRootFolder;
	std::shared_ptr<FolderNode> shaderRootFolder;
	std::shared_ptr<FolderNode> scriptRootFolder;
	std::shared_ptr<FolderNode> animationRootFolder;
	std::shared_ptr<FolderNode> prefabRootFolder;
	std::shared_ptr<FolderNode> sceneRootFolder;
	std::shared_ptr<FolderNode> tilemapRootFolder;
	std::shared_ptr<FolderNode> fontRootFolder;

	// UI state
	std::string newFolderName = "NewFolder";
	std::string defaultFolderName = newFolderName;
	bool showCreateScenePopup = false;
	bool showCreateScriptPopup = false;
	bool showCreateFolderPopup = false;
	bool showRenameFolderPopup = false;
	std::shared_ptr<FolderNode> contextMenuFolder = nullptr;
	std::string createFolderAssetType = "";

	// member variables for renaming of asset
	bool showRenameAssetPopup = false;
	std::string assetToRename;
	std::string renameAssetPath;
	std::string renameAssetType;
	std::string renameAssetNewName;
	char renameAssetBuffer[128] = {};
	char renameFolderBuffer[128] = {};

	// member variables for deletion of asset
	bool showDeleteConfirmationPopup = false;
	std::string assetToDelete = "";
	std::string assetTypeToDelete = "";

	// Drag and drop state
	std::string draggedAssetPath = "";
	std::shared_ptr<FolderNode> dragTargetFolder = nullptr;
	// Track last set drop targets to avoid redundant calls
	std::unordered_map<std::string, std::string> lastSetDropTargets;

	// Helper fucntion
	void BuildFolderHierarchy(const std::string& assetType);

	void RenderFolderTree(std::shared_ptr<FolderNode> folder,
						  const std::string& assetType,
						  GLuint defaultThumbnail,
						  const char* dragDropPayload);

	//std::shared_ptr<FolderNode> FindExpandedFolder(std::shared_ptr<FolderNode> root);

	void CreateNewFolder(std::shared_ptr<FolderNode> parentFolder,
						 const std::string& folderName);

	void RenameFolder(std::shared_ptr<FolderNode> folder,
					  const std::string& newName);

	void MoveAssetToFolder(uint32_t assetID, std::shared_ptr<FolderNode> targetFolder, const std::string& assetType);

	std::shared_ptr<FolderNode> FindFolderByPath(std::shared_ptr<FolderNode> root, const std::string& path);

	//void RenderAssetGridWithFolders(const char* label,
	//								std::shared_ptr<FolderNode> rootFolder,
	//								GLuint defaultThumbnail,
	//								int& selectedIndex,
	//								const char* dragDropPayload);

	/*************************************************************************
	 \brief
		Render the main resource browser UI

	 \details
		Displays an ImGui interface showing all asset categories
		(textures, audio, shaders, prefabs, scripts, scenes) in
		collapsible sections. Each asset is displayed as a selectable
		item with drag-and-drop support and shows its load status and
		ECS asset ID. Includes a refresh button and asset statistics.

	 \note
		Assets can be selected to emit events and support drag-and-drop
		operations to other editor windows for easy asset assignment.
	 *************************************************************************/
	void RenderResourceBrowser();

	void RenderThumbnailBrowser();

	void RenderAssetGrid(const char* label,
		const std::vector<std::string>& assets,
		GLuint defaultThumbnail,
		int& selectedIndex,
		const char* dragDropPayload);

	struct FileInfo {
		std::string filename;
		std::string fullPath;
	};

	std::vector<FileInfo> duplicatedFiles;
	std::vector<FileInfo> unsupportedFiles;

	std::vector<char> selectedFilesForReplacement;

	/*************************************************************************
	  \brief
		Renders the popup for duplicated files.
	  \details
		Displays a modal listing all duplicated files and allows the user
		to replace all, skip all, or select files individually for replacement.
	*************************************************************************/
	void RenderDuplicatedFilePopUp();

	/*************************************************************************
	  \brief
		Renders the popup for unsupported files.
	  \details
		Displays a modal listing all unsupported files and the supported
		file types. Provides an "Okay" button to close the popup and
		clear the unsupported file flags.
	*************************************************************************/
	void RenderUnsupportedFilePopUp();

	/*************************************************************************
	  \brief
		fucntion to rename asset with right click
	  \param[in]
		assetPath Relative path to the asset to rename
	 \param[in]
		newName New name for the asset to be updated with
	  \param[in]
		assetType Type of asset (for UI feedback)
	  \details
		Marks the asset for renaming and shows a renaming popup.
	*************************************************************************/
	void RenameAsset(const std::string& assetPath,
		const std::string& newName, const std::string& assetType);

	/*************************************************************************
	 \brief
		Delete the selected asset with confirmation

	 \param[in]
		assetPath Relative path to the asset to delete
	 \param[in]
		assetType Type of asset (for UI feedback)

	 \details
		Marks the asset for deletion and shows a confirmation popup.
		The actual deletion happens when user confirms in the modal.
	 *************************************************************************/
	void DeleteAssetWithConfirmation(const std::string& assetPath, const std::string& assetType);

	/*************************************************************************
	 \brief
		Render the delete confirmation modal popup

	 \details
		Shows a modal dialog asking user to confirm asset deletion.
		If confirmed, performs the deletion and refreshes the asset browser.
	 *************************************************************************/
	void RenderDeleteConfirmationPopup();

	bool openDuplicatePopupNextFrame	= false;
	bool openUnsupportedPopUpNextFrame	= false;
	bool openIndividualSelectionPopup	= false;

	// Helper functions
	GLuint GetThumbnailForAsset(const std::string& assetPath, GLuint defaultThumbnail);
	std::string GetAssetDisplayName(const std::string& path);
	GLuint CreateColoredThumbnail(float r, float g, float b);
	void InitializeDefaultThumbnails();

	/*************************************************************************
	 \brief
		Render the "Assets Used" tab contents

	 \details
		Shows a list of all discovered scene files. The user picks one and
		presses "Generate Report". The function scans the scene's JSON for
		UUID references, resolves them via GGAssetID, buckets them by asset
		type and subfolder, then writes a human-readable .txt report into
		the Assets/ root folder. Feedback is shown inline in the tab.
	 *************************************************************************/
	void RenderAssetsUsedTab();

	/*************************************************************************
	 \brief
		Parse a scene JSON file and collect every asset path referenced in it

	 \param[in]
		sceneFilePath  Absolute (or relative-to-cwd) path to the .scene file
	 \param[out]
		outAssetsByTypeAndFolder
			Map keyed by asset-type string ("Textures", "Audio", etc.),
			whose value is a map keyed by subfolder (e.g. "Textures/UI")
			containing the sorted list of relative asset paths in that folder.
	 \return
		true if the file was parsed successfully, false on read/parse error

	 \details
		Walks every string value in the JSON document, tests whether it
		is a valid UUID, resolves it through GGAssetID::GetFilePath(), and
		classifies the resulting path by extension into one of the six
		tracked asset types. Paths that cannot be resolved or whose extension
		is not tracked are silently skipped.
	 *************************************************************************/
	bool CollectAssetsFromScene(
		const std::string& sceneFilePath,
		std::map<std::string, std::map<std::string, std::vector<std::string>>>& outAssetsByTypeAndFolder);

	/*************************************************************************
	 \brief
		Write the collected asset report to a .txt file in Assets/

	 \param[in]
		sceneName   Base name of the scene (used as the filename stem)
	 \param[in]
		assetsByTypeAndFolder
			Fully populated map from CollectAssetsFromScene()
	 \return
		true if the file was written successfully, false otherwise

	 \details
		Writes a human-readable report with one section per asset type,
		each subdivided by subfolder. Each path is listed as a bullet.
		The output file is always written to "../../../Assets/<sceneName>.txt".
	 *************************************************************************/
	bool WriteAssetsUsedReport(
		const std::string& sceneName,
		const std::map<std::string, std::map<std::string, std::vector<std::string>>>& assetsByTypeAndFolder);

	// State for the Assets Used tab
	std::vector<bool> assetsUsedCheckedScenes;
	std::string assetsUsedStatusMessage;  // feedback line shown under the button
	bool   assetsUsedStatusIsError = false;

};


/*************************************************************************
 \brief 
	Get the singleton instance of AssetManagerNode

 \return 
	Reference to the unique pointer holding the AssetManagerNode singleton

 \details 
	Creates and returns a static singleton instance of AssetManagerNode
	using a unique pointer. The instance is initialized on first
	access and persists for the lifetime of the application.

 \note 
	This is thread-safe in C++11 and later due to static local
	variable initialization guarantees. All asset manager operations
	should go through this singleton accessor.
 *************************************************************************/
inline auto& GetAssetManagerNode()
{
	static std::unique_ptr<AssetManagerNode> assetManager = std::make_unique<AssetManagerNode>();
	return assetManager;
}