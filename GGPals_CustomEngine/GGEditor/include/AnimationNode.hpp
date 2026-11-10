/*************************************************************************
  \file       AnimationNode.hpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       23 November 2025
  \version    1.2
  \brief
	Declares the AnimationNode editor panel class responsible for
	sprite-sheet slicing, animation state editing, preview rendering,
	and exporting .gganim animation data.

  \details
	The AnimationNode exposes all editor-facing methods used to:
	  - Load textures and animation metadata.
	  - Define and manage animation states (name, frame range, looping).
	  - Configure UV bounds and grid slicing.
	  - Render preview windows and overlays using ImGui.
	  - Validate animation data prior to export.
	  - Save animation information into GGEngines .gganim format.
	  - Integrate with asset-reloading, event buses, and the editors
		docking system.

	This header defines:
	  - TextureData and AnimationState structures.
	  - Export settings configuration.
	  - The AnimationNode class and all editor callbacks.

  \dependencies
    EditorSceneBase.hpp
    GGECSAssetInterface.hpp
    GGAssetID.hpp
    <string>, <vector>, <optional>, <filesystem>, <memory>

  \see
	AnimationNode.cpp

  \history
	1.0: Initial definition of AnimationNode structures and interfaces.
	1.1(24th November 2025): added .gganim loading functions
	1.2(27th January 2026): added multi-page saving and loading

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGAssetID.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <memory>

/*************************************************************************/
/*!
  \class AnimationNode
  \brief
	Provides the full animation editing UI in the GGEngine editor.

  \details
	Handles:
	  - Texture loading and UUID validation.
	  - Grid slicing configuration.
	  - Animation state creation, editing, duplication and removal.
	  - Spritesheet preview rendering and interaction.
	  - Exporting animation data to .gganim files.
	  - Popup and error-handling workflows.
*/
/*************************************************************************/
class AnimationNode : public EditorSceneBase
{
public:
	/*************************************************************************
	// Editor Docking & Registration
	// - AnimationNode::AnimationNode
	// - AnimationNode::Init
	// - AnimationNode::DockBuild
	// - AnimationNode::Update
	*************************************************************************/
	/*************************************************************************
	  \brief
		Constructs the AnimationNode editor panel and binds editor callbacks.

	  \details
		Registers lambda callbacks for Init(), Update(), and DockBuild(),
		enabling the GGEngine editor to invoke this panel automatically
		during initialization, per-frame updates, and dock layout building.
	*************************************************************************/
	AnimationNode();

	/*************************************************************************
	  \brief
		Initializes the AnimationNode panel.

	  \details
		Called once during editor startup. Registers an asset-reload callback
		so that when the underlying texture file changes on disk, the editor
		automatically reloads the texture currently in use.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief
		Registers the AnimationNode windows into the editor docking layout.

	  \details
		Associates Animation and Spritesheet Slicer with the bottom
		docking region managed by EditorManager. These windows are then
		placed automatically in the editor interface.
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief
		Renders the AnimationNode panel each frame.

	  \details
		Performs the following tasks every update:
		  - Creates and draws the Animation control window.
		  - Displays the slicer/preview window.
		  - Processes background tasks such as delayed popups or asset loads.
	*************************************************************************/
	virtual void Update() override;

private:
	/*************************************************************************
	  \struct AnimationEvent
	  \brief
		Represents a named event associated with an animation state.


	  \details
		Contains:
		- Event name identifier.
		- Optional frame binding flag.
		- Frame index (valid only when frame binding is enabled).
		Animation events can be used to trigger gameplay logic, audio cues,
		or visual effects during animation playback.
	*************************************************************************/
	struct AnimationEvent {
		char name[256] = "";
		bool hasFrame = false;
		int frame = 0; // Valid if only hasFrame == true
	};

	/*************************************************************************
	  \struct AnimationState
	  \brief
		Represents a named animation sequence on the spritesheet.

	  \details
		Contains:
		  - State name.
		  - Start and end frame indices.
		  - Looping option.
		  - UI expansion and selection flags.
		  - Helpers for validity checking and frame count.
	*************************************************************************/
	struct AnimationState {
		char stateName[64] = "idle";
		uint32_t startIndex = 0;	// Frame range start
		uint32_t endIndex = 0;		// Frame range end
		bool shouldLoop = true;

		// UI state
		bool isExpanded = true;		// ImGui tree node state
		bool isSelected = false;	// Selection for editinf

		// Valid Range and Name?
		bool isValid() const {
			return startIndex <= endIndex && strlen(stateName) > 0;
		}

		int getFrameCount() const {
			return static_cast<int>(endIndex - startIndex + 1);
		}

		std::vector<AnimationEvent> events;
	};

	/*************************************************************************
	  \struct TextureData
	  \brief
		Holds metadata for the currently loaded spritesheet texture.

	  \details
		Stores:
		  - Path, UUID, OpenGL ID.
		  - Texture dimensions.
		  - Status flags.
		  - Derived values such as max rows/columns and total frames.
	*************************************************************************/
	struct TextureData {
		std::string filePath;	// Asset path
		std::optional<AssetUUID> uuid;	// Asset uuid
		uint32_t textureID = 0;
		int width = 0;		// Texture Dimensions
		int height = 0;
		bool isLoaded = false; // status

		// Configurable params (.gganim)
		int cellWidth = 32;
		int cellHeight = 32;
		int spritesPerRow = 1;	// Derived: width / cellWidth

		// Calculated Grid Properties
		int totalFrames = 1;
		int totalRows = 1;

		// UV bounds ( matching .gganim textureUVBounds)
		// bounds of part of spritesheet
		float uvSMin = 0.0f;	// Left
		float uvTMin = 0.0f;	// Right
		float uvSMax = 1.0f;	// Top
		float uvTMax = 1.0f;	// Bottom

		// Derived(Calculated) Vals
		int maxColumns = 1;		// To see how many sprites are there in one row
		int maxRows = 1;
		int totalPossibleFrames = 1;
	};

	/*************************************************************************
	  \struct GridConfig
	  \brief
		Stores user-configurable slicing and preview settings.

	  \details
		Includes:
		  - Cell size and frame count.
		  - Calculated sprite/row values.
		  - Animation FPS.
		  - UV cropping bounds.
		  - Flags for preview overlays (grid, numbers).
		  - Preview zoom scale.
	*************************************************************************/
	struct GridConfig {
		float animFPS = 12.0f;

		// Visual Options
		bool showGrid = true;	// Show Grid Overlay
		bool showFrameNumbers = true;
		float previewScale = 1.0f;	// UI zoom levels
	};

	/*************************************************************************
	  \struct ExportSettings
	  \brief
		Configures .gganim export parameters.

	  \details
		Includes:
		  - Output filename.
		  - UUID inclusion toggle.
		  - Pre-export validation option.
		  - Output directory.
		  - Last exported path for UI feedback.
	*************************************************************************/
	struct ExportSettings {
		char fileName[256] = "new_animation";
		bool includeUUID = true;	// Export textureAssetUUID
		bool validateOnExport = true;	// Run Validation before saving

		// Export paths
		std::string outputDirectory = "Animations/";
		std::string lastExportPath; // For UI feedback
	};

private:
	/*************************************************************************
	// Main Editor Windows
	// - RenderControlsWindow
	// - RenderMainSlicerWindow
	// - RenderNoImageLoadedPanel
	// - RenderImagePreviewPanel
	*************************************************************************/
	/*************************************************************************
	  \brief
		Renders all control panels for the AnimationNode.

	  \details
		Displays the asset loading panel, grid configuration panel, animation
		state editor panel, export panel, and pending popups. This function
		orchestrates the entire UI layout of the Animation editor window.
	*************************************************************************/
	void RenderControlsWindow();
	/*************************************************************************
	  \brief
		Renders the docked Spritesheet Slicer window.

	  \details
		Depending on whether a texture is loaded, this window either shows
		a no image loaded message or the full preview panel including
		overlays, UV-bound rendering, mouse interaction and grid drawing.
	*************************************************************************/
	void RenderMainSlicerWindow();
	/*************************************************************************
	  \brief
		Displays a warning panel when no texture is currently loaded.

	  \details
		Used as the default visual when the slicer panel is active but no
		asset has been selected or loaded yet.
	*************************************************************************/
	void RenderNoImageLoadedPanel();
	/*************************************************************************
	  \brief
		Displays the full sprite-sheet preview including zoom support, grid,
		frame indices, and animation-state highlights.

	  \details
		- Computes scaled preview size.
		- Places preview inside a scrollable region.
		- Reacts to mouse clicks and wheel zoom.
		- Shows overlays depending on user preferences.
	*************************************************************************/
	void RenderImagePreviewPanel();




	/*************************************************************************
	// Spritesheet Interaction
	// - HandleImageInteraction
	// - OnFrameClicked
	*************************************************************************/
	/*************************************************************************
	  \brief
		Handles mouse interaction with the spritesheet preview.

	  \details
		Supports:
		  - Detecting which frame was clicked based on mouse position.
		  - Passing the selected frame index to the animation state logic.
		  - Adjusting zoom level using the mouse wheel.
		Enables intuitive frame selection inside the preview window.
	*************************************************************************/
	void HandleImageInteraction(ImVec2 imagePos, float imageWidth, float imageHeight);
	/*************************************************************************
	  \brief
		Updates animation state selection based on a clicked frame.

	  \details
		If no state is selected, a new animation state is created starting
		at the clicked frame.
		If a state is selected, this function either resets or extends the
		states frame range depending on whether Shift is held.
	*************************************************************************/
	void OnFrameClicked(int frameIndex);




	/*************************************************************************
	// Overlay Rendering Functions
	// - RenderGridOverlay
	// - RenderFrameNumberOverlay
	// - RenderAnimationStateOverlay
	*************************************************************************/
	/*************************************************************************
	  \brief
		Draws the grid overlay on top of the spritesheet preview.

	  \details
		Computes grid line positions based on cell width/height and renders
		vertical and horizontal lines using the active display scale.
		Also draws a border around the full texture area.
		Helps visually separate spritesheet frames for precise selection.
	*************************************************************************/
	void RenderGridOverlay(ImVec2 imagePos, float imageWidth, float imageHeight) const;
	/*************************************************************************
	  \brief
		Draws frame numbers over each cell of the spritesheet.

	  \details
		Calculates the 0-based index for every frame and displays the value
		with a semi-transparent background for readability.
		Used for debugging and for helping users identify exact frame ranges.
	*************************************************************************/
	void RenderFrameNumberOverlay(ImVec2 imagePos, float imageWidth, float imageHeight) const;
	/*************************************************************************
	  \brief
		Highlights the currently selected animation states frame range.

	  \details
		Renders a semi-transparent fill and border around all frames that
		fall within the states start end range.
		This visual overlay shows users which frames are part of the active
		animation state within the preview window.
	*************************************************************************/
	void RenderAnimationStateOverlay(ImVec2 imagePos, float imageWidth, float imageHeight);
	



	/*************************************************************************
	// Control Panels (Editor UI)
	// - RenderAssetLoadingPanel
	// - RenderPageManagementPanel
	// - RenderGridConfigPanel
	// - RenderAnimationStatesPanel
	// - RenderAnimationStateEditor
	// - RenderSelectedStatePreview
	// - RenderFrameRangeVisualisation
	// - RenderExportPanel
	// - RenderValidationPanel
	*************************************************************************/
	/*************************************************************************
	  \brief
		Renders UI controls for selecting and loading texture assets.

	  \details
		Provides:
		  - Manual text input for asset paths.
		  - A file browser popup.
		  - Load button.
		Also displays validation messages and the currently loaded textures
		metadata when available.
	*************************************************************************/
	void RenderAssetLoadingPanel();
	/*************************************************************************
	  \brief
		Renders the texture page management panel.

	  \details
		Displays the list of loaded texture pages, allows switching between
		pages, removing pages, and adding new empty pages. Also synchronizes
		animation state selection when the active page changes.
	*************************************************************************/
	void RenderPageManagementPanel();
	/*************************************************************************
	  \brief
		Displays and edits the spritesheet grid configuration.

	  \details
		Allows the user to modify:
		  - Cell width and height.
		  - Animation FPS.
		  - UV bounds.
		  - Visual display options (grid, frame numbers).
		  - Preview scale.
		Recalculates grid properties when parameters change.
	*************************************************************************/
	void RenderGridConfigPanel();
	/*************************************************************************
	  \brief
		Displays the list of animation states and provides editing controls.

	  \details
		Supports:
		  - Adding, removing, and duplicating states.
		  - Selecting states via clickable tree nodes.
		  - Inline validation indicators.
		  - Expanding states to edit properties such as name, frame range,
			and looping mode.
	*************************************************************************/
	void RenderAnimationStatesPanel();
	/*************************************************************************
	  \brief
		Draws the editable UI for a single animation state.

	  \details
		Provides inputs for:
		  - State name.
		  - Start and end frame indices.
		  - Looping behaviour.
		Also displays computed metrics such as duration and renders a
		small visual bar showing which frames belong to this state.
	*************************************************************************/
	void RenderAnimationStateEditor(AnimationState& state);
	/*************************************************************************
	  \brief
		Shows a summary of the currently selected animation state.

	  \details
		Displays the states name, start/end frames, total frame count,
		and looping mode. This lightweight panel helps users quickly inspect
		their active selection.
	*************************************************************************/
	void RenderSelectedStatePreview();
	/*************************************************************************
	  \brief
		Renders a compact visualization of the states frame range.

	  \details
		Draws a horizontal bar representing every frame in the spritesheet
		and highlights only the frames belonging to the active animation
		state. This offers a quick overview without needing the full preview.
	*************************************************************************/
	void RenderFrameRangeVisualisation(const AnimationState& state) const;
	/*************************************************************************
	  \brief
		Renders the export settings and .gganim export controls.

	  \details
		Displays file-name input, UUID inclusion toggle, validation options,
		and the main export button. Also shows the last exported file path
		and any export-related error messages. This panel manages all user
		interactions for generating .gganim animation files.
	*************************************************************************/
	void RenderExportPanel();
	/*************************************************************************
	  \brief
		Displays the list of validation errors in the Animation panel.

	  \details
		Prints each error message in red and provides a button to clear
		the list. This panel appears below the primary controls and offers
		persistent inspection of validation issues.
	*************************************************************************/
	void RenderValidationPanel();




	/*************************************************************************
	// Animation State Management
	// - HasSelectedState
	// - AddNewAnimationState
	// - RemoveSelectedState
	// - DuplicateSelectedState
	// - SelectAnimationState
	// - GetSelectedState
	// - UpdateAnimationStateRanges
	*************************************************************************/
	/*************************************************************************
	  \brief
		Checks whether a valid animation state is currently selected.

	  \details
		Ensures that the stored selected-state index is within range of the
		animation state list. Used before performing operations that require
		a valid active animation state.
	*************************************************************************/
	bool HasSelectedState() const;
	/*************************************************************************
	  \brief
		Checks whether a valid animation state is currently selected.

	  \details
		Ensures that the stored selected-state index is within range of the
		animation state list. Used before performing operations that require
		a valid active animation state.
	*************************************************************************/
	void AddNewAnimationState();
	/*************************************************************************
	  \brief
		Removes the currently selected animation state.

	  \details
		Deletes the active state from the list and updates the selected-state
		index to the next closest valid entry. If no states remain, the
		selected index is cleared. Called by the Remove Selected button.
	*************************************************************************/
	void RemoveSelectedState();
	/*************************************************************************
	  \brief
		Duplicates the currently selected animation state.

	  \details
		Copies all properties of the active state, prefixes its name with
		"(Copy)", appends the duplicate to the end of the state list, and
		selects it. Used to quickly create similar animation states.
	*************************************************************************/
	void DuplicateSelectedState();
	/*************************************************************************
	  \brief
		Marks the specified animation state as the active selection.

	  \details
		Clears all existing selection flags and sets the chosen state as the
		selected one. Selection updates UI panels, visual overlays, and
		preview bars to reflect the chosen animation state.
	*************************************************************************/
	void SelectAnimationState(int index);
	/*************************************************************************
	  \brief
		Retrieves a reference to the currently selected animation state.

	  \details
		Validates that a state is selected before returning it. Throws a
		runtime error if no selection exists, ensuring calling code does
		not unintentionally operate on invalid state indices.
	*************************************************************************/
	AnimationState& GetSelectedState();
	/*************************************************************************
	  \brief
		Clamps all animation state ranges to the valid frame boundaries.

	  \details
		Ensures all states have:
		  - Valid names.
		  - Non-inverted start/end indices.
		  - End indices within frame limits.
		Automatically repairs invalid configurations after changes to grid
		size or texture dimensions.
	*************************************************************************/
	void UpdateAnimationStateRanges();




	/*************************************************************************
	// Texture & Grid Logic
	// - ValidateAssetPath
	// - LoadTextureAsset
	// - ReloadCurrentTexture
	// - RecalculateGridProperties
	*************************************************************************/
	/*************************************************************************
	  \brief
		Validates the asset path entered by the user.

	  \details
		Performs checks for:
		  - Empty input.
		  - Supported image extensions.
		  - File existence inside the Assets/ directory.
		  - Valid UUID metadata.
		On success, stores the validated path and associated UUID.
	*************************************************************************/
	void ValidateAssetPath();
	/*************************************************************************
	\brief
		Loads a texture asset and prepares the editor for slicing.

	\param relativePath
		The relative asset path of the texture to load.

	\details
		- Resolves the texture via UUID using the asset system.
		- Registers the texture with the ECS asset manager.
		- Loads the OpenGL texture handle.
		- Queries the texture width and height.
		- Updates cached texture metadata for the active page.
		- Recalculates grid layout based on current cell size.
		- Updates editor UI state and preview data.
		- Displays success or error messages in the editor and console.


	Difference from LoadTextureIntoPage():
		- Represents a user-driven editor action.
		- Updates editor state, validation, and UI.
		- Operates on the currently active page.
		- Intended for normal editor usage (button click / drag & drop).
	*************************************************************************/
	void LoadTextureAsset(const std::string& relativePath);
	/*************************************************************************
	  \brief
		Updates grid-related values after texture or cell-size changes.

	  \details
		Computes:
		  - Sprites per row.
		  - Total rows.
		  - Total frame count.
		Stores these values in both grid configuration and texture metadata.
		This function must be called whenever cell dimensions are modified.
	*************************************************************************/
	void RecalculateGridProperties();
	/*************************************************************************
	  \brief
		Reloads the currently loaded texture from disk.

	  \details
		Called when the asset-reload callback detects a changed file path.
		Reinitializes texture data and repopulates grid and preview settings
		to reflect updated asset content.
	*************************************************************************/
	void ReloadCurrentTexture();




	/*************************************************************************
	// Multi-page Spritesheet Management
	// - AddTexturePage
	// - RemoveTexturePage
	// - SyncSelectionWithActivePage
	// - HasValidPageIndex
	// - HasActivePage
	*************************************************************************/
	/*************************************************************************
	  \brief
		Adds a new empty texture page.

	  \details
		Creates a new TextureData entry, appends it to the page list, and
		automatically selects it as the active page.
	*************************************************************************/
	void AddTexturePage();
	/*************************************************************************
	  \brief
		Removes a texture page by index.

	  \param index
		The index of the texture page to remove.

	  \details
		Deletes the specified page, updates the active page index if needed,
		and recalculates grid and animation state ranges to maintain validity.
	*************************************************************************/
	void RemoveTexturePage(int index);
	/*************************************************************************
	  \brief
		Synchronizes animation state selection with the active texture page.

	  \details
		Ensures that the currently selected animation state overlaps the
		active page. If not, attempts to select another overlapping state,
		or clears the selection if no suitable state exists.
	*************************************************************************/
	void SyncSelectionWithActivePage();
	/*************************************************************************
	  \brief
		Checks whether the active page index is valid.

	  \return
		True if the page index exists within the texture page list, false
		otherwise.
	*************************************************************************/
	bool HasValidPageIndex() const;
	/*************************************************************************
	  \brief
		Checks whether a texture page is active and loaded.

	  \return
		True if a valid page index exists and the texture is loaded, false
		otherwise.
	*************************************************************************/
	bool HasActivePage() const;




	/*************************************************************************
	// Frame Indexing Utilities
	// - GetPageCapacity
	// - GetPageStartOffset
	// - GetTotalGlobalFrames
	// - ResolveGlobalFrameToPageLocal
	*************************************************************************/
	/*************************************************************************
	  \brief
		Returns the total number of frames in a texture page.

	  \param pageIndex
		The index of the texture page.

	  \return
		The number of frames available in the page, or 0 if invalid.
	*************************************************************************/
	uint32_t GetPageCapacity(int pageIndex) const;
	/*************************************************************************
	  \brief
		Computes the global frame offset for a texture page.

	  \param pageIndex
		The index of the texture page.

	  \return
		The starting global frame index for the page.
	*************************************************************************/
	uint32_t GetPageStartOffset(int pageIndex) const;
	/*************************************************************************
	  \brief
		Returns the total number of frames across all texture pages.

	  \return
		The total global frame count.
	*************************************************************************/
	uint32_t GetTotalGlobalFrames() const;
	/*************************************************************************
	  \brief
		Converts a global frame index to a page-local frame index.

	  \param globalFrame
		The global frame index.

	  \param pageIndex
		The target texture page index.

	  \param localFrame
		Output parameter receiving the local frame index.

	  \return
		True if the global frame belongs to the page, false otherwise.
	*************************************************************************/
	bool ResolveGlobalFrameToPageLocal(uint32_t globalFrame, int pageIndex, uint32_t& localFrame) const;




	/*************************************************************************
	// Exporting & Validation
	// - ExportAnimationFile
	// - ValidateAnimationData
	// - CreateAnimationJson
	// - SaveJsonToFileInternal
	// - SaveJSONToFile
	// - TriggerAssetSystemRefresh
	// - GetRelativeAssetPath
	*************************************************************************/
	/*************************************************************************
	  \brief
		Exports the current animation configuration into a .gganim file.

	  \details
		- Optionally performs full validation before exporting.
		- Builds the JSON document using CreateAnimationJson().
		- Checks for file overwrite conditions.
		- Saves the result to the Assets directory.
		Opens confirmation or error popups based on the export outcome.
	*************************************************************************/
	void ExportAnimationFile();
	/*************************************************************************
	  \brief
		Validates all animation data prior to export.

	  \details
		Checks for:
		  - Missing textures.
		  - Missing or invalid animation states.
		  - Invalid grid configuration.
		  - Invalid UV bounds.
		  - Overlapping or out-of-range frame indices.
		Returns a list of human-readable error strings used by validation
		popups and the error panel.
	*************************************************************************/
	std::vector<std::string> ValidateAnimationData();
	/*************************************************************************
	  \brief
		Builds a RapidJSON document representing the .gganim animation file.

	  \details
		Populates:
		  - Texture UUID
		  - Animation FPS
		  - Sprites-per-row
		  - UV bounds
		  - Animation state list (name, range, loop flag)
		Produces the exact data structure expected by GGEngine animation
		runtime and serializer systems.
	*************************************************************************/
	rapidjson::Document CreateAnimationJson();
	/*************************************************************************
	  \brief
		Internal helper for writing .gganim JSON files.

	  \details
		Handles directory creation, file writing, export status updates,
		and post-export system refresh logic. Called by both standard and
		overwrite-confirmation export paths.
	*************************************************************************/
	void SaveJsonToFileInternal(const rapidjson::Document& doc, const std::string& fullPath);
	/*************************************************************************
	  \brief
		Saves a JSON animation document to disk using the configured output
		path and file name.

	  \details
		Writes formatted JSON to the Assets directory, updates export status,
		triggers editor refresh events, and displays a success popup.
		Throws if the file cannot be created or written.
	*************************************************************************/
	void SaveJSONToFile(const rapidjson::Document& doc);
	/*************************************************************************
	  \brief
		Notifies the engine that new or modified animation assets are available.

	  \details
		Informs the AssetManager to rescan and re-register asset UUIDs,
		ensuring that newly exported .gganim files or updated textures
		appear immediately inside the editor. Also emits an editor event
		so UI panels such as the AssetManagerNode refresh their displays.
	*************************************************************************/
	void TriggerAssetSystemRefresh(const std::string& newFilePath);
	/*************************************************************************
	  \brief
		Converts an absolute or mixed-format file path into a clean relative
		Assets/ path.

	  \details
		Attempts multiple strategies to normalize an input path:
		  - Removes the "../../../Assets/" prefix when present.
		  - Strips "Assets/" from paths already relative.
		  - Computes a relative path using std::filesystem when needed.
		Returns the original path if normalization fails. Used for displaying
		asset paths consistently across editor log messages and UI.
	*************************************************************************/
	std::string GetRelativeAssetPath(const std::string& fullPath) const;




	/*************************************************************************
	// .gganim loading functions & Legacy Support
	// - RenderAnimationFileBrowserPopup
	// - LoadAnimationMetadata
	// - FindTexturePathFromUUID
	// - LoadTextureIntoPage
	*************************************************************************/
	/*************************************************************************
	  \brief
		Renders a popup window listing all discovered .gganim files.

	  \details
		Allows the user to select an animation metadata file from the Asset
		directory. Selecting a file triggers metadata loading and updates the
		editor state accordingly.
	*************************************************************************/
	void RenderAnimationFileBrowserPopup();
	/*************************************************************************
	  \brief
		Loads animation metadata from a .gganim file.

	  \param animPath
		Full filesystem path to the .gganim file.

	  \return
		True if metadata was successfully parsed and applied, false otherwise.

	  \details
		Reads animation JSON, loads associated texture via UUID, restores grid
		configuration, animation states, UV bounds, and automatically updates
		the export file name and selected state.
	*************************************************************************/
	bool LoadAnimationMetadata(const std::string& animPath);
	/*************************************************************************
	  \brief
		Finds a texture asset path that corresponds to a given UUID.

	  \param uuid
		The AssetUUID to search for.

	  \return
		The relative asset path (e.g., "Textures/Hero.png") if found, or an
		empty string if no matching texture exists.

	  \details
		Performs a reverse lookup in the discovered texture list using the UUID
		system. Used by .gganim loading to recover correct spritesheet.
	*************************************************************************/
	std::string FindTexturePathFromUUID(const AssetUUID& uuid);
	/*************************************************************************
	\brief
		Loads a texture into a specified texture page.


	\param pageIndex
		The index of the texture page to load the texture into.


	\param path
		The relative asset path of the texture to load.


	\details
		- Resolves the texture using the asset UUID system.
		- Registers the texture with the ECS asset manager.
		- Retrieves the OpenGL texture handle.
		- Queries the texture width and height.
		- Resets and populates the specified texture page.
		- Marks the texture page as loaded.


	Difference from LoadTextureAsset():
		- Loads directly into a specified page slot.
		- Does not modify editor UI state or validation messages.
		- Does not change active page selection.
		- Intended for internal use during .gganim loading or legacy restore.
	*************************************************************************/
	void LoadTextureIntoPage(int pageIndex, const std::string& path);




	/*************************************************************************
	// Popups & Internal Logic
	// - RenderAssetBrowserPopup
	// - RenderExportPopup
	// - RenderValidationPopup
	// - RenderOverwriteConfirmationPopup
	// - ProcessPendingTasks
	*************************************************************************/
	/*************************************************************************
	  \brief
		Renders the asset-browser popup used to select textures.

	  \details
		Lists all discovered texture paths from the AssetManager.
		Selecting a texture immediately loads it into the editor.
		Provides close-button handling for clean popup management.
	*************************************************************************/
	void RenderAssetBrowserPopup();
	/*************************************************************************
	  \brief
		Displays the modal popup shown after a successful export.

	  \details
		Shows the saved file path and an OK button to dismiss the popup.
		Automatically appears when m_showExportSuccessPopup is enabled.
	*************************************************************************/
	void RenderExportPopup();
	/*************************************************************************
	  \brief
		Displays a popup summarizing validation errors detected at export.

	  \details
		Lists each error message in a styled modal window.
		The popup prevents export from proceeding until all issues are
		resolved or acknowledged.
	*************************************************************************/
	void RenderValidationPopup();
	/*************************************************************************
	  \brief
		Renders the confirmation popup shown when exporting over an existing file.

	  \details
		Asks the user whether they want to overwrite an existing .gganim file.
		Overwrite triggers a forced save, while Cancel aborts the export.
		Ensures users do not accidentally overwrite important assets.
	*************************************************************************/
	void RenderOverwriteConfirmationPopup();
	/*************************************************************************
	  \brief
		Executes queued background operations.

	  \details
		Processes all pending tasks queued during UI actions (e.g., delayed
		popups or post-load operations). Ensures UI responsiveness by
		deferring work until safe execution points.
	*************************************************************************/
	void ProcessPendingTasks();

private:
	/*************************************************************************
	// RUNTIME TEXTURE & GRID DATA
	*************************************************************************/
	// Stores information about the currently loaded spritesheet texture
	/*TextureData m_textureData;*/
	std::vector<TextureData> m_texturePages;
	int m_activePageIndex = 0;

	// Stores slicing configuration (cell size, FPS, UV bounds, preview options)
	GridConfig m_gridConfig;

	// All animation states the user has created for this spritesheet
	std::vector<AnimationState> m_animationStates;

	// Export settings for generating .gganim files
	ExportSettings m_exportSettings;




	/*************************************************************************
	// ASSET LOADING STATE
	*************************************************************************/
	// Raw C-string buffer for user typed asset file paths in the UI
	char m_assetPathBuffer[256] = "";	

	// Error message produced when loading or validating an asset path fails
	std::string m_lastLoadError;

	// Callback invoked when an asset reload event is received (hot-reload)
	std::function<void(const std::string&)> m_assetReloadCallback;




	/*************************************************************************
	// EDITOR STATE
	*************************************************************************/
	// Index of the currently selected animation state (-1 = nothing selected)
	int m_selectedStateIndex = -1;

	// Stores error text from the most recent export attempt
	std::string m_lastExportError;

	// Validation error messages gathered when checking animation data
	std::vector<std::string> m_validationErrors;

	// True if a successfully exported popup should be shown
	bool m_showExportSuccessPopup = false;




	/*************************************************************************
	// BACKGROUND TASK SYSTEM
	*************************************************************************/
	// Queue of deferred operations to execute safely at end of frame
	std::vector<std::function<void()>> m_pendingTasks;




	/*************************************************************************
	// UI INTERACTION STATE
	*************************************************************************/
	// Used for advanced multi-frame selection (future feature support)
	int m_selectionAnchorFrame = -1;

	// check for existing file, do a double comfirm for overwriting existing file
	// Flag indicating whether the overwrite confirmation popup is visible
	bool m_showOverwriteConfirmation = false;
	// File path of the animation file awaiting overwrite confirmation
	std::string m_pendingExportPath;
};

/*************************************************************************
  \brief
	Returns a global instance of the AnimationNode editor panel.

  \details
	Implements a static singleton-like accessor for use by editor modules
	that need to interact with the AnimationNode panel.
*************************************************************************/
inline auto& GetAnimationNode()
{
	static std::unique_ptr<AnimationNode> animation = std::make_unique<AnimationNode>();
	return animation;
}