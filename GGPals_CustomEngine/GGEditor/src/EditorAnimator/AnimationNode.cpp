/*************************************************************************
  \file       AnimationNode.cpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       23 November 2025
  \version    1.2
  \brief
	Implements the AnimationNode editor panel responsible for texture
	loading, sprite-sheet slicing, animation state management, preview
	rendering, validation, and exporting animation data into the
	.gganim file format.

  \details
	This source file provides the complete implementation of the
	AnimationNode editor workflow, including:
	  - Integration with the editor docking system.
	  - Texture loading using GGAssetID and the ECS Asset Interface.
	  - Sprite-sheet preview rendering using ImGui and OpenGL.
	  - Interactive tools for defining animation states and frame ranges.
	  - Grid, UV-bounds, and FPS configuration.
	  - Validation and export of animation data via RapidJSON.
	  - Asset browser popups, error reporting, and overwrite confirmation.
	  - Handling of background tasks and runtime texture reload events.

	The implementation relies heavily on ImGui for UI behaviour and
	GGEngines systems for asset retrieval, event communication, and
	rendering support.

  \dependencies
	AnimationNode.hpp
	GGSystemManagers.hpp
	EditorEventBus.hpp
	<algorithm>, <set>, imgui_internal.h

  \see
	AnimationNode.hpp

  \history
	1.0: Initial implementation of AnimationNode editor functionality.
	1.1(24th November 2025): added .gganim loading functions
	1.2(27th January 2026): added multi-page saving and loading

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <AnimationNode.hpp>
#include <GGSystemManagers.hpp>
#include <EditorEventBus.hpp>

#include <algorithm>
#include <set>

#include <imgui_internal.h>

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
AnimationNode::AnimationNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
	Initializes the AnimationNode panel.

  \details
	Called once during editor startup. Registers an asset-reload callback
	so that when the underlying texture file changes on disk, the editor
	automatically reloads the texture currently  in use.
*************************************************************************/
void AnimationNode::Init()
{
	// Subscribe to asset reload events
	m_assetReloadCallback = [this](const std::string& assetPath) {	// Define a lambda that receives a reloaded asset path

		// Safety Check: If no active page, nothing to reload
		if (!HasActivePage()) {
			return;
		}

		if (m_texturePages[m_activePageIndex].filePath == assetPath) {					// Check if the reloaded file matches the currently loaded spritesheet
			ReloadCurrentTexture();									// If so, reload the texture to update preview + slicing
		}
		};
}

/*************************************************************************
  \brief
	Registers the AnimationNode windows into the editor docking layout.

  \details
	Associates Animation and Spritesheet Slicer with the bottom
	docking region managed by EditorManager. These windows are then
	placed automatically in the editor interface.
*************************************************************************/
void AnimationNode::DockBuild()
{
	// Retrieve the EditorManager system for docking operations
	auto& editor = Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>();

	// Dock the Animation control panel at the bottom area of the editor
	ImGui::DockBuilderDockWindow("Animation", editor.dockBottom);

	// Dock the Spritesheet Slicer preview window in the same region
	ImGui::DockBuilderDockWindow("Spritesheet Slicer", editor.dockBottom);
}

/*************************************************************************
  \brief
	Renders the AnimationNode panel each frame.

  \details
	Performs the following tasks every update:
	  - Creates and draws the Animation control window.
	  - Displays the slicer/preview window.
	  - Processes background tasks such as delayed popups or asset loads.
*************************************************************************/
void AnimationNode::Update()
{
	// Force the Animation window to always open at 800×700 size
	ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

	if (ImGui::Begin("Animation", nullptr, ImGuiWindowFlags_NoResize))
	{
		RenderControlsWindow();		// Slicer Controls (Control Panel)
	}

	ImGui::End();	//End Animation window

	// Docked Windows
	RenderMainSlicerWindow();	// Slicer Preview

	// Execute any delayed operations queued during last frame
	ProcessPendingTasks();
}




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
void AnimationNode::RenderControlsWindow() {
	RenderAssetLoadingPanel();
	RenderPageManagementPanel();

	// Popups
	RenderAssetBrowserPopup();
	RenderExportPopup();
	RenderValidationPopup();
	RenderOverwriteConfirmationPopup();
	RenderAnimationFileBrowserPopup();

	// Only show editor tools if texture loaded / If have page active
	if (HasActivePage()) {
		RenderGridConfigPanel();
		ImGui::Separator();
		RenderAnimationStatesPanel();
		ImGui::Separator();
		RenderExportPanel();
	}

	// Shows Errors
	RenderValidationPanel();
}

/*************************************************************************
  \brief
	Renders the docked Spritesheet Slicer window.

  \details
	Depending on whether a texture is loaded, this window either shows
	a no image loaded message or the full preview panel including
	overlays, UV-bound rendering, mouse interaction and grid drawing.
*************************************************************************/
void AnimationNode::RenderMainSlicerWindow() {
	if (ImGui::Begin("Spritesheet Slicer", nullptr, ImGuiWindowFlags_NoCollapse)) {

		// If no active page, show no texture loaded
		if (!HasActivePage()) {
			RenderNoImageLoadedPanel();
		}

		// If have active page, show spritesheet preview
		else {
			RenderImagePreviewPanel();
		}
	}
	ImGui::End();
}

/*************************************************************************
  \brief
	Displays a warning panel when no texture is currently loaded.

  \details
	Used as the default visual when the slicer panel is active but no
	asset has been selected or loaded yet.
*************************************************************************/
void AnimationNode::RenderNoImageLoadedPanel() {
	ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "NO SPRITESHEET LOADED!");
	ImGui::Text("LOAD A TEXTURE USING THE ANIMATION PANEL (DEFAULT DOCKED BOTTOM)");
}	

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
void AnimationNode::RenderImagePreviewPanel() {

	// If no active page, exit
	if (!HasActivePage()) {
		return;
	}

	// Get active texture
	TextureData& page = m_texturePages[m_activePageIndex];
	// Exit if texture not loaded
	if (!page.isLoaded) {
		return;
	}

	// Calculate display dimensions with zoom
	float displayWidth = page.width * m_gridConfig.previewScale;
	float displayHeight = page.height * m_gridConfig.previewScale;

	// Scrollable area for large textures
	ImGui::BeginChild("ImagePreview", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	// Get position for overlay drawing
	ImVec2 imagePos = ImGui::GetCursorScreenPos();

	// Display the texture
	ImGui::Image((ImTextureID)(uintptr_t)page.textureID,
		ImVec2(displayWidth, displayHeight),
		ImVec2(0.f, 1.f),
		ImVec2(1.f, 0.f));

	// Handle image interaction
	bool imageHovered = ImGui::IsItemHovered();
	if (imageHovered) {		// If hovering
		// Handle click + zoom interactions
		HandleImageInteraction(imagePos, displayWidth, displayHeight);
	}

	// Show Grid
	if (m_gridConfig.showGrid) {
		RenderGridOverlay(imagePos, displayWidth, displayHeight);
	}

	// Show Frame Numbers
	if (m_gridConfig.showFrameNumbers) {
		RenderFrameNumberOverlay(imagePos, displayWidth, displayHeight);
	}

	// Render animation state highlights
	if (HasSelectedState()) {
		RenderAnimationStateOverlay(imagePos, displayWidth, displayHeight);
	}

	// Compute global start and end frame
	uint32_t start = GetPageStartOffset(m_activePageIndex);
	uint32_t end = start + GetPageCapacity(m_activePageIndex) - 1;

	// Display page info and frame range at the bottom of preview image
	ImGui::TextColored( ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Page %d | %s | Frames %u - %u", m_activePageIndex, page.filePath.c_str(), start, end);

	ImGui::EndChild();
}




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
void AnimationNode::HandleImageInteraction(ImVec2 imagePos, float imageWidth, float imageHeight) {
	ImGuiIO& io = ImGui::GetIO();	// Retrieve global ImGui input/output (mouse, keyboard, etc.)

	if (!HasActivePage()) {
		return;
	}

	TextureData& page = m_texturePages[m_activePageIndex];

	// Exit if grid is invalid, must have valid grid values on this page
	if (page.spritesPerRow <= 0 || page.totalRows <= 0) {
		return;
	}

	if (ImGui::IsItemClicked()) {
		// Calculate clicked frame
		// Convert global mouse X/Y into local image-space X/Y
		// localPos represents position inside the drawn image
		ImVec2 localPos = ImVec2(io.MousePos.x - imagePos.x, io.MousePos.y - imagePos.y);

		float cellDisplayWidth = imageWidth / static_cast<float>(page.spritesPerRow);
		float cellDisplayHeight = imageHeight / static_cast<float>(page.totalRows);

		int clickedCol = static_cast<int>(localPos.x / cellDisplayWidth);
		int clickedRow = static_cast<int>(localPos.y / cellDisplayHeight);

		// localFrame within this page
		int localFrame = clickedRow * page.spritesPerRow + clickedCol;

		if (localFrame >= 0 && localFrame < static_cast<int>(page.totalFrames)) {

			// convert local to global frames
			uint32_t globalFrame = GetPageStartOffset(m_activePageIndex) + static_cast<uint32_t>(localFrame);
			OnFrameClicked(static_cast<int>(globalFrame));
		}
	}

	// Mouse wheel zoom
	if (io.MouseWheel != 0.0f) {
		// Compute zoom factor: wheel +1 = zoom in, -1 = zoom out
		float zoomFactor = 1.0f + (io.MouseWheel * 0.1f);
		m_gridConfig.previewScale *= zoomFactor;	// Apply zoom factor to preview scale
		m_gridConfig.previewScale = std::clamp(m_gridConfig.previewScale, 0.1f, 4.0f);	// Minimum 0.1x zoom	// Maximum 4x zoom
	}
}

/*************************************************************************
  \brief
	Updates animation state selection based on a clicked frame.

  \details
	If no state is selected, a new animation state is created starting
	at the clicked frame.
	If a state is selected, this function either resets or extends the
	states frame range depending on whether Shift is held.
*************************************************************************/
void AnimationNode::OnFrameClicked(int frameIndex) {

	// Ignore invalid frame index
	if (frameIndex < 0) {
		return;
	}

	uint32_t globalFrame = static_cast<uint32_t>(frameIndex);
	// Get total frames across all pages
	uint32_t totalGlobal = GetTotalGlobalFrames();

	// Exit if no frames exist
	if (totalGlobal == 0) {
		return;
	}

	// Safety: Clamp to valid range
	if (globalFrame >= totalGlobal) {
		globalFrame = totalGlobal - 1;
	}

	if (!HasSelectedState()) {
		// No selected state create new state starting at this frame
		AddNewAnimationState();

		// Safety check to ensure a state was created
		if (!m_animationStates.empty()) {
			m_animationStates.back().startIndex = globalFrame;		// Set new states start frame to clicked frame
			m_animationStates.back().endIndex = globalFrame;		// And end frame to same value
			SelectAnimationState(static_cast<int>(m_animationStates.size() - 1));		// Select the newly created state
		}
	}
	else {
		// Extend selected state range
		AnimationState& selectedState = GetSelectedState();

		// If SHIFT is held during click shift + click
		if (ImGui::GetIO().KeyShift) {
			// Extend backward based on click position
			if (globalFrame < selectedState.startIndex) {
				selectedState.startIndex = globalFrame;
			}
			// Extend forward based on click position
			else {
				selectedState.endIndex = globalFrame;
			}
		}
		else {
			// Regular click sets new start point (Start and end)
			selectedState.startIndex = globalFrame;
			selectedState.endIndex = globalFrame;
		}

		// Fix inverted frame range
		if (selectedState.endIndex < selectedState.startIndex) {
			selectedState.endIndex = selectedState.startIndex;
		}
	}
}




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
void AnimationNode::RenderGridOverlay(ImVec2 imagePos, float imageWidth, float imageHeight) const {
	// Fetch ImGuis drawing list for the current window (for lines, rects, etc.)
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const TextureData& page = m_texturePages[m_activePageIndex];
	// Calculate grid spacing in display coordinates
	float cellDisplayWidth = imageWidth / page.spritesPerRow;
	float cellDisplayHeight = imageHeight / page.totalRows;

	// Per frame grid
	ImU32 gridColour = IM_COL32(255, 255, 0, 192);  // Semi-transparent yellow grid colour
	float lineThickness = 1.0f;						

	// Draw vertical grid lines
	for (int col = 1; col < page.spritesPerRow; ++col) {
		float x = imagePos.x + col * cellDisplayWidth;

		// Draw vertical line spanning the full preview height (starting Point, ending Point, grid colour, thickness)
		drawList->AddLine(ImVec2(x, imagePos.y), ImVec2(x, imagePos.y + imageHeight), gridColour, lineThickness);
	}

	// Draw horizontal grid lines  
	for (int row = 1; row < page.totalRows; ++row) {
		float y = imagePos.y + row * cellDisplayHeight;

		// Draw horizontal line spanning full preview width 
		drawList->AddLine( ImVec2(imagePos.x, y), ImVec2(imagePos.x + imageWidth, y), gridColour, lineThickness);
	}

	// Draw white border around the entire spritesheet preview (Top Left of Sprite Preview, Bottom Right of Sprite Preview, No rounded corners, No special flags, Border thickness)
	drawList->AddRect( imagePos, ImVec2(imagePos.x + imageWidth, imagePos.y + imageHeight), IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
}

/*************************************************************************
  \brief
	Draws frame numbers over each cell of the spritesheet.

  \details
	Calculates the 0-based index for every frame and displays the value
	with a semi-transparent background for readability.
	Used for debugging and for helping users identify exact frame ranges.
*************************************************************************/
void AnimationNode::RenderFrameNumberOverlay(ImVec2 imagePos, float imageWidth, float imageHeight) const {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const TextureData& page = m_texturePages[m_activePageIndex];
	float cellDisplayWidth = imageWidth / page.spritesPerRow;
	float cellDisplayHeight = imageHeight / page.totalRows;

	ImU32 textColour = IM_COL32(255, 255, 255, 255);	// White Text
	ImU32 bgColour = IM_COL32(0, 0, 0, 128);			// Semi transparent black

	for (int row = 0; row < page.totalRows; ++row) {
		for (int col = 0; col < page.spritesPerRow; ++col) {
			// Convert row/col to linear frame index (0 to n)
			uint32_t localFrame = row * page.spritesPerRow + col;
			uint32_t globalFrame = GetPageStartOffset(m_activePageIndex) + localFrame;

			// X and Y position for frame number text (with padding)
			float x = imagePos.x + col * cellDisplayWidth + 4;
			float y = imagePos.y + row * cellDisplayHeight + 4;

			// Convert frame index to string
			std::string frameText = std::to_string(globalFrame);
			// Measure pixel size of text for background sizing
			ImVec2 textSize = ImGui::CalcTextSize(frameText.c_str());

			// Background rectangle slightly bigger than text
			drawList->AddRectFilled(ImVec2(x - 2, y - 2), ImVec2(x + textSize.x + 2, y + textSize.y + 2), bgColour);

			// Render frame index text at computed location
			drawList->AddText(ImVec2(x, y), textColour, frameText.c_str());
		}
	}
}

/*************************************************************************
  \brief
	Highlights the currently selected animation states frame range.

  \details
	Renders a semi-transparent fill and border around all frames that
	fall within the states start end range.
	This visual overlay shows users which frames are part of the active
	animation state within the preview window.
*************************************************************************/
void AnimationNode::RenderAnimationStateOverlay(ImVec2 imagePos, float imageWidth, float imageHeight) {

	TextureData& page = m_texturePages[m_activePageIndex];

	// If there is no selected state OR grid invalid, do nothing
	if (!HasSelectedState() || page.totalFrames == 0) {
		return;
	}

	// Retrieve the selected animation state
	AnimationState& state = GetSelectedState();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Cell size in display size
	float cellDisplayWidth = imageWidth / page.spritesPerRow;
	float cellDisplayHeight = imageHeight / page.totalRows;

	// Highlight colour (semi transparent blue)
	ImU32 fillColour = IM_COL32(50, 120, 255, 60);
	ImU32 borderColour = IM_COL32(50, 120, 255, 255);

	// Loop through entire selected frame range
	for (uint32_t globalFrame = state.startIndex; globalFrame <= state.endIndex; ++globalFrame) {
		uint32_t localFrame;

		// Skip frames that do not belong to this page
		if (!ResolveGlobalFrameToPageLocal(globalFrame, m_activePageIndex, localFrame)) {
			continue;
		}

		// Convert frameIndex to (row, col)
		uint32_t col = localFrame % page.spritesPerRow;
		uint32_t row = localFrame / page.spritesPerRow;

		// Compute pixel positions
		float x0 = imagePos.x + col * cellDisplayWidth;		// Compute top left X of highlight
		float y0 = imagePos.y + row * cellDisplayHeight;	// Compute top left Y of highlight
		float x1 = x0 + cellDisplayWidth;					// Compute bottom right X
		float y1 = y0 + cellDisplayHeight;					// Compute bottom right Y

		// Filled Highlight
		// Draw semi blue transparent filled rectangle
		drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fillColour);

		// Border
		// Draw solid border around the frame highlight
		drawList->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), borderColour, 0.0f, 0, 2.0f);
	}
}




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
void AnimationNode::RenderAssetLoadingPanel() {
	ImGui::SeparatorText("Asset Loading");

	// Asset Path input with auto-completion
	// Asset path textbox with placeholder (For users to input the type of texture that they want)
	if (ImGui::InputTextWithHint("Asset Path", "Textures/spritesheet.png (or drag & drop from AssetBrowser Texture)", m_assetPathBuffer, sizeof(m_assetPathBuffer))) {
		ValidateAssetPath();	// Validate users typed path
	}

	// drag & drop texture from assetbrowser into input text
	if (ImGui::BeginDragDropTarget())
	{
		// Check if a texture asset is being dragged over this UI and accept its payload data
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE"))
		{
			// Read assetID from payload
			uint32_t textureAssetID = *(const uint32_t*)payload->Data;

			// Convert ID to UUID
			auto uuidOpt = GGECSStandardAssetManager<GLuint>::GetUUIDFromID(textureAssetID);

			// Check if uuid exists
			if (uuidOpt.has_value())
			{
				// get the file path from UUID
				auto& assetSystem = GGAssetID::GetGGAssetID();
				std::string texturePath = assetSystem->GetFilePath(uuidOpt.value());

				// copy path to buffer
				strncpy(m_assetPathBuffer, texturePath.c_str(), sizeof(m_assetPathBuffer));
				m_assetPathBuffer[sizeof(m_assetPathBuffer) - 1] = '\0';

				// load the texture
				LoadTextureAsset(texturePath);

				std::cout << "[AnimationNode] Loaded texture via drag & drop: " << texturePath << std::endl;
			}
			else
			{
				std::cout << "[AnimationNode] Failed to get UUID from asset ID: " << textureAssetID << std::endl;
			}
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::SameLine();

	auto* assetManager = GGEngine::GetEngine().GetAssetManager();

	if (ImGui::Button("Browse...")) {
		assetManager->ScanAssetDirectories();		// Refresh Asset list
		assetManager->RegisterAssetsWithUUIDs();	// Ensure UUID exists
		ImGui::OpenPopup("Asset Browser");			// Open the popup see RenderAssetBrowserPopup()
	}

	ImGui::SameLine();

	// Load button pressed and textbox not empty
	if (ImGui::Button("Load") && strlen(m_assetPathBuffer) > 0) {
		LoadTextureAsset(m_assetPathBuffer);	// Load the texture by path
	}

	ImGui::SameLine();
	if (ImGui::Button("Load .gganim")) {			
		ImGui::OpenPopup("Animation File Browser");	// In RenderAnimationFileBrowserPopup()
	}

	// Asset Status Display 
	// If a texture is loaded, display green Loaded: ...
	if (HasActivePage()) {
		TextureData& page = m_texturePages[m_activePageIndex];
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Loaded: %s (%d%d)", page.filePath.c_str(), page.width, page.height);
	}
	// Otherwise show error message in red
	else if (!m_lastLoadError.empty()) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", m_lastLoadError.c_str());
	}
}

/*************************************************************************
  \brief
    Renders the texture page management panel.

  \details
    Displays the list of loaded texture pages, allows switching between
    pages, removing pages, and adding new empty pages. Also synchronizes
    animation state selection when the active page changes.
*************************************************************************/
void AnimationNode::RenderPageManagementPanel() {
	ImGui::SeparatorText("Texture Pages");

	// Page List (Loop through all pages)
	for (size_t i = 0; i < m_texturePages.size(); ++i) {
		ImGui::PushID(static_cast<int>(i)); // Push unique ImGui ID

		// Page Select Button
		std::string label;
		if (m_texturePages[i].filePath.empty()) {
			label = "Page " + std::to_string(i);	// Use generic page label (Page + i)
		}
		else {
			label = m_texturePages[i].filePath;		// Use texture path as label
		}

		ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_AllowOverlap;

		if (ImGui::Selectable(label.c_str(), static_cast<int>(i) == m_activePageIndex, selectableFlags)) {
			m_activePageIndex = static_cast<int>(i);
			SyncSelectionWithActivePage();
		}

		ImGui::SameLine();

		// Remove page Button
		if (ImGui::Button("x")) {
			RemoveTexturePage(static_cast<int>(i));	// Remove selected page
			ImGui::PopID();
			break;
		}

		ImGui::PopID();
	}

	// Add Page
	if (ImGui::Button("+ Add Page")) {
		AddTexturePage();
	}
}

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
void AnimationNode::RenderGridConfigPanel() {
	ImGui::SeparatorText("Grid Configuration");

	// Tracks if user modified grid parameters
	bool gridChanged = false;

	TextureData& page = m_texturePages[m_activePageIndex];

	// Input for cell width
	if (ImGui::InputInt("Cell Width", &page.cellWidth, 1, 10)) {
		// Clamp to valid range
		page.cellWidth = std::clamp(page.cellWidth, 1, page.width);
		// Mark as changed
		gridChanged = true;
	}

	if (ImGui::InputInt("Cell Height", &page.cellHeight, 1, 10)) {
		page.cellHeight = std::clamp(page.cellHeight, 1, page.height);
		gridChanged = true;
	}

	// Real time grid calculation display
	if (gridChanged) {					// If cell size changed
		RecalculateGridProperties();	// Update grid layout
		UpdateAnimationStateRanges();	// Clamps existing states to new grid
	}

	// Textbox editor
	// Calculate grid information (read only)
	ImGui::BeginDisabled();
	ImGui::InputInt("Sprites Per Row", &page.spritesPerRow);
	ImGui::InputInt("Total Rows", &page.totalRows);
	ImGui::InputInt("Total Frames", &page.totalFrames);
	ImGui::EndDisabled();

	// Animation FPS (direct .gganim mapping)
	ImGui::SliderFloat("Animation FPS", &m_gridConfig.animFPS, 1.0f, 60.0f, "%.1f");

	// UV bounds configuration (.gganim textureUVBounds mapping)
	if (ImGui::TreeNode("UV Bounds")) {
		ImGui::InputFloat("UV S Min", &page.uvSMin, 0.0f, 1.0f, "%.3f");
		ImGui::InputFloat("UV T Min", &page.uvTMin, 0.0f, 1.0f, "%.3f");
		ImGui::InputFloat("UV S Max", &page.uvSMax, 0.0f, 1.0f, "%.3f");
		ImGui::InputFloat("UV T Max", &page.uvTMax, 0.0f, 1.0f, "%.3f");

		if (ImGui::Button("Reset UV Bounds")) {
			page.uvSMin = 0.0f;
			page.uvTMin = 0.0f;
			page.uvSMax = 1.0f;
			page.uvTMax = 1.0f;
		}
		ImGui::TreePop();
	}

	// Visual options
	ImGui::Checkbox("Show Grid Overlay", &m_gridConfig.showGrid);			// Checkbox to toggle grid overlay
	ImGui::SameLine();
	ImGui::Checkbox("Show Frame Numbers", &m_gridConfig.showFrameNumbers);	// Toggle frame index overlay

	ImGui::SliderFloat("Preview Scale", &m_gridConfig.previewScale, 0.1f, 4.0f, "%.1fx");	// Zoom slider
}

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
void AnimationNode::RenderAnimationStatesPanel() {
	ImGui::SeparatorText("Animation States");

	// Add / Remove state controls
	if (ImGui::Button("Add State")) {
		AddNewAnimationState();
	}

	ImGui::SameLine();
	if (ImGui::Button("Remove Selected") && HasSelectedState()) {
		RemoveSelectedState();
	}

	ImGui::SameLine();
	if (ImGui::Button("Duplicate Selected") && HasSelectedState()) {
		DuplicateSelectedState();
	}

	// States list wih selection
	// Scrollable list region for states
	ImGui::BeginChild("States List", ImVec2(0, 200), true);

	// Loop through all states
	for (size_t i = 0; i < m_animationStates.size(); ++i) {
		AnimationState& state = m_animationStates[i];

		// Unique ID for widgets of this state
		ImGui::PushID(static_cast<int>(i));

		// State selection and expansion
		// Display set up to configure ImGui tree node appearance and interaction:
		// - ImGuiTreeNodeFlags_OpenOnArrow: expand/collapse only when the small arrow is clicked
		//   (clicking the label instead selects the entity, like in Unitys Hierarchy) (Dropdowm Menu)
		// - ImGuiTreeNodeFlags_SpanAvailWidth: make the selectable area span the full row width
		//   for better UX and consistent highlight behavior
		// - ImGuiTreeNodeFlags_Selected: visually highlight the node if this entity is currently selected.
		//  |  (bitwise OR) merges bits from multiple flags into one variable
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		if (state.isSelected) {
			//  |=  (bitwise OR assignment) preserves existing flags and adds a new one
			flags |= ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen;
		}

		// Render this entity as a tree node in the Hierarchy window.
		// - (void*)(intptr_t)entity.id: unique ImGui ID (cast from entitys numeric ID)
		// - intptr_t: pointer-sized integer (avoids size loss on 32/64-bit systems)
		// - void*: ImGui expects IDs as void pointers, not raw integers.
		// - flags: visual/behavior settings (e.g., selected, open-on-arrow)
		// Returns true if the node is expanded, false if collapsed.
		bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", state.stateName);

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
			SelectAnimationState(static_cast<int>(i));	// If user clicks on state mark as selected
		}

		// State validation indicator
		ImGui::SameLine();
		if (state.isValid()) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Vaild");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid");
		}

		// Expand state editing
		// If node expanded
		if (nodeOpen) {
			RenderAnimationStateEditor(state);	// Draw state editor for this state

			ImGui::SeparatorText("Events");

			// Loop through state events
			for (size_t e = 0; e < state.events.size(); ++e) {
				AnimationEvent& event = state.events[e];	
				ImGui::PushID(static_cast<int>(e));		// Push unique ImGui ID for event

				const char* label = "NewEvent";

				if (event.name[0] != '\0') {
					label = event.name;
				}

				// Render this entity as a tree node in the Hierarchy window.
				// - (void*)(intptr_t)entity.id: unique ImGui ID (cast from entitys numeric ID)
				// - intptr_t: pointer-sized integer (avoids size loss on 32/64-bit systems)
				// - void*: ImGui expects IDs as void pointers, not raw integers.
				// - flags: visual/behavior settings (e.g., selected, open-on-arrow)
				// Returns true if the node is expanded, false if collapsed.
				bool open = ImGui::TreeNodeEx((void*)(intptr_t)e, ImGuiTreeNodeFlags_OpenOnArrow, "%s", label);

				ImGui::SameLine();

				bool removeEvent = false;
				if (ImGui::SmallButton("X")) {
					removeEvent = true;
				}

				if (open) {
					// Edit event name textbox and Has Frame checkbox 
					ImGui::InputText("Event Name", event.name, sizeof(event.name));
					ImGui::Checkbox("Has Frame", &event.hasFrame);

					// If has frame checked
					if (event.hasFrame) {
						// Find out how many frame selected
						int maxSize = static_cast<int>(state.endIndex - state.startIndex);
						ImGui::SliderInt("Frame", &event.frame, 0, maxSize);
					}

					ImGui::TreePop();
				}

				ImGui::PopID();

				if (removeEvent) {
					state.events.erase(state.events.begin() + e);
					break;
				}
			}

			if (ImGui::Button("+ Add Event")) {
				// Create default event with "NewEvent" as default name
				AnimationEvent ev{};
				strcpy(ev.name, "NewEvent");
				ev.hasFrame = false;		// Disable frame binding
				ev.frame = 0;				// Default frame index
				state.events.push_back(ev);	// Add event to state
			}

			ImGui::TreePop(); // Collapse node
		}

		ImGui::PopID();
	}

	ImGui::EndChild();

	// Selected state preview
	if (HasSelectedState()) {
		RenderSelectedStatePreview();
	}
}

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
void AnimationNode::RenderAnimationStateEditor(AnimationState& state) {
	// State name (maps to .gganim stateName)
	ImGui::InputText("State Name", state.stateName, sizeof(state.stateName));

	uint32_t totalGlobal = GetTotalGlobalFrames();		// Get total frames across all pages
	int maxFrame = 0;
	if (totalGlobal > 0) {
		maxFrame = static_cast<int>(totalGlobal - 1);	// Max valid frame index
	}

	// Frame range config (maps to .gganim startIndex/endIndex)
	int startIdx = static_cast<int>(state.startIndex);
	int endIdx = static_cast<int>(state.endIndex);


	if (ImGui::SliderInt("Start Frame", &startIdx, 0, maxFrame)) {
		state.startIndex = static_cast<uint32_t>(startIdx);
		if (state.endIndex < state.startIndex) {
			state.endIndex = state.startIndex;
		}
	}

	if (ImGui::SliderInt("End Frame", &endIdx, 0, maxFrame))
	{
		state.endIndex = static_cast<uint32_t>(endIdx);
		if (state.endIndex < state.startIndex) {
			state.endIndex = state.startIndex;
		}
	}

	// Loop configuration(maps to .gganim shouldLoop)
	ImGui::Checkbox("Should Loop", &state.shouldLoop);

	// State Information Display
	int frameCount = state.getFrameCount();
	// Compute duration in seconds
	float duration = frameCount / static_cast<float>(m_gridConfig.animFPS);

	ImGui::Text("Frame Count: %d", frameCount);
	ImGui::Text("Duration: %.2fs @ %.1f FPS", duration, m_gridConfig.animFPS);

	// Frame Range Visualisation (Visual bar of selected frame range)
	RenderFrameRangeVisualisation(state);
}

/*************************************************************************
  \brief
	Shows a summary of the currently selected animation state.

  \details
	Displays the states name, start/end frames, total frame count,
	and looping mode. This lightweight panel helps users quickly inspect
	their active selection.
*************************************************************************/
void AnimationNode::RenderSelectedStatePreview() {
	if (!HasSelectedState()) {
		return;
	}

	AnimationState& state = m_animationStates[m_selectedStateIndex];

	ImGui::SeparatorText("Selected State");

	ImGui::Text("Name: %s", state.stateName);
	ImGui::Text("Start Frame: %u", state.startIndex);
	ImGui::Text("End Frame: %u", state.endIndex);

	ImGui::Text("Total Frames: %d", state.getFrameCount());
	ImGui::Text("Loop: %s", state.shouldLoop ? "Yes" : "No");
}

/*************************************************************************
  \brief
	Renders a compact visualization of the states frame range.

  \details
	Draws a horizontal bar representing every frame in the spritesheet
	and highlights only the frames belonging to the active animation
	state. This offers a quick overview without needing the full preview.
*************************************************************************/
void AnimationNode::RenderFrameRangeVisualisation(const AnimationState& state) const {
	// Visual Representation of frame range within grid
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();		// Available width of UI container
	canvasSize.y = 30.0f;	// Fixed height for visualisation

	// Reserve UI space without drawing anything
	ImGui::InvisibleButton("FrameRange", canvasSize);

	// Draw Frame Range Bar
	const TextureData& page = m_texturePages[m_activePageIndex];
	// Width of one frame in the visualization bar
	float frameWidth = canvasSize.x / page.totalFrames;

	for (uint32_t frame = 0; frame < static_cast<uint32_t>(page.totalFrames); ++frame) {
		float x = canvasPos.x + frame * frameWidth;					// Compute the bar segment X position
		ImU32 colour;

		if (frame >= state.startIndex && frame <= state.endIndex) {	// If frame belongs to selected state
			colour = IM_COL32(0, 255, 0, 128);						// Green for included frames
		}
		else {
			colour = IM_COL32(128, 128, 128, 64);					// Grey for excluded frames
		}

		drawList->AddRectFilled(ImVec2(x, canvasPos.y), ImVec2(x + frameWidth - 1, canvasPos.y + canvasSize.y), colour);
	}

	// Draw frame indices at key points (Number above frame timeline)
	char frameText[16];
	sprintf(frameText, "%u", state.startIndex);
	// Draw text 15 pixels above the bar
	drawList->AddText(ImVec2(canvasPos.x + state.startIndex * frameWidth, canvasPos.y - 15), IM_COL32(255, 255, 255, 255), frameText);

	sprintf(frameText, "%u", state.endIndex);
	drawList->AddText(ImVec2(canvasPos.x + state.endIndex * frameWidth, canvasPos.y - 15), IM_COL32(255, 255, 255, 255), frameText);
}

/*************************************************************************
  \brief
	Renders the export settings and .gganim export controls.

  \details
	Displays file-name input, UUID inclusion toggle, validation options,
	and the main export button. Also shows the last exported file path
	and any export-related error messages. This panel manages all user
	interactions for generating .gganim animation files.
*************************************************************************/
void AnimationNode::RenderExportPanel() {
	ImGui::SeparatorText("Export Animation");

	// File name configuration
	ImGui::InputText("File Name", m_exportSettings.fileName, sizeof(m_exportSettings.fileName));

	// Export options
	ImGui::Checkbox("Include UUID", &m_exportSettings.includeUUID);
	ImGui::SameLine();

	ImGui::Checkbox("Validate Before Export", &m_exportSettings.validateOnExport);

	// Determine if export button should be active: Must have a texture loaded, Must have at least one animation state and File name must not be empty
	bool canExport = HasActivePage() && !m_animationStates.empty() && strlen(m_exportSettings.fileName) > 0;

	// Disable button if export conditions fail
	ImGui::BeginDisabled(!canExport);


	if (ImGui::Button("export .gganim file", ImVec2(-1, 0))) {
		ExportAnimationFile();
	}

	// End conditional disabling
	ImGui::EndDisabled();

	// Export Status
	if (!m_exportSettings.lastExportPath.empty()) {
		// Show green success text
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Last exported: %s", m_exportSettings.lastExportPath.c_str());
	}

	if (!m_lastExportError.empty()) {
		// Show red error message
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Export error: %s", m_lastExportError.c_str());
	}
}

/*************************************************************************
  \brief
	Displays the list of validation errors in the Animation panel.

  \details
	Prints each error message in red and provides a button to clear
	the list. This panel appears below the primary controls and offers
	persistent inspection of validation issues.
*************************************************************************/
void AnimationNode::RenderValidationPanel() {
	if (!m_validationErrors.empty()) {
		ImGui::SeparatorText("Validation Errors");
	}

	for (const auto& error : m_validationErrors) {
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", error.c_str());
	}

	if (ImGui::Button("Clear Errors")) {
		m_validationErrors.clear();
	}
}




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
bool AnimationNode::HasSelectedState() const {
	return m_selectedStateIndex >= 0 && m_selectedStateIndex < static_cast<int>(m_animationStates.size());
}

/*************************************************************************
  \brief
	Creates a new animation state with default parameters.

  \details
	Generates a state name ("StateX"), initializes a single-frame range,
	enables looping by default, and immediately selects the newly created
	state. Called when the user presses the Add State button or when
	clicking on the spritesheet with no state selected.
*************************************************************************/
void AnimationNode::AddNewAnimationState() {
	AnimationState newState;

	// Give default name
	snprintf(newState.stateName, sizeof(newState.stateName), "State%d", static_cast<int>(m_animationStates.size()) + 1);

	// Default Frame Range
	newState.startIndex = 0;
	newState.endIndex = 0;
	newState.shouldLoop = true;
	newState.isExpanded = true;

	m_animationStates.push_back(newState);

	// Auto select the new animation state
	SelectAnimationState(static_cast<int>(m_animationStates.size()) - 1);
}

/*************************************************************************
  \brief
	Removes the currently selected animation state.

  \details
	Deletes the active state from the list and updates the selected-state
	index to the next closest valid entry. If no states remain, the
	selected index is cleared. Called by the Remove Selected button.
*************************************************************************/
void AnimationNode::RemoveSelectedState() {
	// If nothing is selected, do nothing
	if (!HasSelectedState()) {
		return;
	}

	// Store index of the state to remove
	int index = m_selectedStateIndex;

	// Remove the state
	m_animationStates.erase(m_animationStates.begin() + index);

	// If list is now empty, No more states
	if (m_animationStates.empty()) {
		m_selectedStateIndex = -1;
	}

	// If removed last state, Select last remaining state and Mark as selected
	else if (index >= static_cast<int>(m_animationStates.size())) {
		m_selectedStateIndex = static_cast<int>(m_animationStates.size()) - 1;
		m_animationStates[m_selectedStateIndex].isSelected = true;
	}
	else {
		m_selectedStateIndex = index;	 // Select the next state in list
		m_animationStates[index].isSelected = true;
	}
}

/*************************************************************************
  \brief
	Duplicates the currently selected animation state.

  \details
	Copies all properties of the active state, prefixes its name with
	"(Copy)", appends the duplicate to the end of the state list, and
	selects it. Used to quickly create similar animation states.
*************************************************************************/
void AnimationNode::DuplicateSelectedState() {
	if (!HasSelectedState()) {
		return;
	}

	AnimationState copy = m_animationStates[m_selectedStateIndex];	// Copy selected state data

	// Rename duplicated string
	snprintf(copy.stateName, sizeof(copy.stateName), "(Copy) %s", m_animationStates[m_selectedStateIndex].stateName);

	// Add duplicated state to list
	m_animationStates.push_back(copy);

	// Select the new duplicate
	SelectAnimationState(static_cast<int>(m_animationStates.size()) - 1);
}

/*************************************************************************
  \brief
	Marks the specified animation state as the active selection.

  \details
	Clears all existing selection flags and sets the chosen state as the
	selected one. Selection updates UI panels, visual overlays, and
	preview bars to reflect the chosen animation state.
*************************************************************************/
void AnimationNode::SelectAnimationState(int index) {

	// Bounds Check
	if (index < 0 || index >= static_cast<int>(m_animationStates.size())) {
		return;
	}

	// Loop through all states, Clear selection flags
	for (auto& state : m_animationStates) {
		state.isSelected = false;
	}

	// Select the requested state
	m_animationStates[index].isSelected = true;

	// Track selection index
	m_selectedStateIndex = index;
}

/*************************************************************************
  \brief
	Retrieves a reference to the currently selected animation state.

  \details
	Validates that a state is selected before returning it. Throws a
	runtime error if no selection exists, ensuring calling code does
	not unintentionally operate on invalid state indices.
*************************************************************************/
AnimationNode::AnimationState& AnimationNode::GetSelectedState() {
	if (!HasSelectedState()) {
		throw std::runtime_error("GetSelectedState() called but no state is selected!");
	}
	return m_animationStates[m_selectedStateIndex];
}

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
void AnimationNode::UpdateAnimationStateRanges() {
	if (!HasActivePage()) {
		return;
	}
	// If no texture loaded or grid not computed, return
	TextureData& page = m_texturePages[m_activePageIndex];
	if (!page.isLoaded || page.totalFrames <= 0) {
		return;
	}

	const int maxFrame = page.totalFrames - 1;

	// Loop through all animation states, If name empty, Auto repair to default name
	for (auto& state : m_animationStates) {
		if (strlen(state.stateName) == 0) {
			strcpy(state.stateName, "Unamed");
		}

		// Clamp startIndex
		if (state.startIndex < 0) {
			state.startIndex = 0;
		}
		// If start index exceeds max, Auto clamp end index
		else if (state.startIndex > static_cast<uint32_t>(maxFrame)) {
			state.endIndex = static_cast<uint32_t>(maxFrame);
		}

		// Clamp endIndex
		if (state.endIndex < state.startIndex) {
			state.endIndex = state.startIndex;	// fix inverted range
		}

		// Clamp end index
		if (state.endIndex > static_cast<uint32_t>(maxFrame)) {
			state.endIndex = static_cast<uint32_t>(maxFrame);
		}

		// If state still invalid
		if (!state.isValid()) {
			// Clamp start index
			state.startIndex = std::min(state.startIndex, static_cast<uint32_t>(maxFrame));
			state.endIndex = state.startIndex;
		}
	}
}




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
void AnimationNode::ValidateAssetPath() {
	m_lastLoadError.clear();

	// Convert input char buffer to std::string
	std::string path = m_assetPathBuffer;

	// If user typed nothing, Record error
	if (path.empty()) {
		m_lastLoadError = "ASSET PATH CANNOT BE EMPTY";
		return;
	}

	// Validate images only
	const std::vector<std::string> imagesOnly = { ".png", ".jpg", ".jpeg", ".tga" };
	bool valid = false;

	// Loop through allowed extensions, Ensure path is long enough, Compare last characters to extension
	for (const auto& extension : imagesOnly) {
		if (path.size() >= extension.size() && path.compare(path.size() - extension.size(), extension.size(), extension) == 0) {
			valid = true;
		}
	}

	if (!valid) {
		m_lastLoadError = "UNSUPPORTED FILE TYPE, USE: .png, .jpg, .jpeg, .tga";
		return;
	}

	// Check file existence
	std::filesystem::path full = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
	full /= path;

	if (!std::filesystem::exists(full)) {
		m_lastLoadError = "FILE NOT FOUND IN Assets/: " + full.string();
		return;
	}

	// Check UUID metadata required for .gganim export
	// Access global UUID system
	auto& uuidSystem = *GGAssetID::GetGGAssetID();

	// Load or create .meta UUID file
	uuidSystem.LoadOrGenerateMeta(path);

	// Retrieve UUID mapped to this asset
	std::optional<AssetUUID> uuid = uuidSystem.GetUUIDFromPath(path);

	if (!uuid.has_value()) {
		m_lastLoadError = "META FILE MISSING OR INVALID FOR ASSET: " + path;
		return;
	}

	// Success: Store uuid and filepath into m_texturePages[m_activePageIndex]
	if (!HasValidPageIndex()) {
		return;
	}
	TextureData& page = m_texturePages[m_activePageIndex];
	page.filePath = path;
	page.uuid = uuid;
}

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
void AnimationNode::LoadTextureAsset(const std::string& relativePath) {
	if (!HasValidPageIndex()) {
		return;
	}

	try {
		// Clear previous state
		TextureData& page = m_texturePages[m_activePageIndex];
		page = TextureData{};
		m_lastLoadError.clear();

		// Access global UUID subsystem
		auto& assetSystem = GGAssetID::GetGGAssetID();

		// Get UUID for given asset path
		const AssetUUID& uuid = assetSystem->GetUUID(relativePath);

		// Register with AssetManager (Register OpenGL texture if needed)
		uint32_t textureID = GGECSStandardAssetManager<GLuint>::RegisterAsset(uuid);

		// Fetch GL texture handle by ID
		GLuint glTexture = GGECSStandardAssetManager<GLuint>::GetAsset(textureID);

		// Get texture dimensions with OpenGL
		glBindTexture(GL_TEXTURE_2D, glTexture);
		GLint width, height;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);		// Get texture width
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);		// Get texture height
		glBindTexture(GL_TEXTURE_2D, 0);											// Unbind texture

		// Populate texture data
		page.filePath = relativePath;
		page.uuid = uuid;
		page.textureID = glTexture;
		page.width = width;
		page.height = height;
		page.isLoaded = true;

		// Update grid calculations
		RecalculateGridProperties();

		std::cout << "SUCCESSFULLY LOADED TEXTURE: " << relativePath << std::endl;
	}
	catch (const std::exception& e) {
		m_lastLoadError = e.what();		// Catch any load errors and Store error message
		std::cerr << "TEXTURE LOAD FAILED: " << e.what() << std::endl;
	}
}

/*************************************************************************
  \brief
	Reloads the currently loaded texture from disk.

  \details
	Called when the asset-reload callback detects a changed file path.
	Reinitializes texture data and repopulates grid and preview settings
	to reflect updated asset content.
*************************************************************************/
void AnimationNode::ReloadCurrentTexture() {
	TextureData& page = m_texturePages[m_activePageIndex];
	if (!page.filePath.empty()) {					// Ensure file path stored
		std::string currentPath = page.filePath;	// Store current texture path
		LoadTextureAsset(currentPath);						// Reload current texture
	}
}

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
void AnimationNode::RecalculateGridProperties() {
	if (!HasActivePage()) {
		return;
	}

	TextureData& page = m_texturePages[m_activePageIndex];
	if (!page.isLoaded) {
		return;
	}

	// Calculate grid layout (matches .gganim logic)
	// number sprites horizontally based on cell width
	page.spritesPerRow = page.width / page.cellWidth;
	// number sprites vertically based on cell height
	page.totalRows = page.height / page.cellHeight;

	// Update texture data cache
	page.maxColumns = page.spritesPerRow;
	page.maxRows = page.totalRows;

	/*m_gridConfig.totalFrames = m_gridConfig.spritesPerRow * m_gridConfig.totalRows;*/
	page.totalFrames = page.maxColumns * page.maxRows;
	page.totalPossibleFrames = page.totalFrames;

	page.uvSMax = 1.0f / page.spritesPerRow;
	page.uvTMax = 1.0f / page.totalRows;

	std::cout << "GRID RECALCULATED: " << page.spritesPerRow << "x" << page.totalRows << "=" << page.totalFrames << "FRAMES" << std::endl;
}




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
void AnimationNode::AddTexturePage() {
	TextureData newPage{};
	m_texturePages.push_back(newPage);

	// Auto-Select the new page
	m_activePageIndex = static_cast<int>(m_texturePages.size() - 1);
}

/*************************************************************************
  \brief
	Removes a texture page by index.

  \param index
	The index of the texture page to remove.

  \details
	Deletes the specified page, updates the active page index if needed,
	and recalculates grid and animation state ranges to maintain validity.
*************************************************************************/
void AnimationNode::RemoveTexturePage(int index) {
	if (index < 0 || index >= static_cast<int>(m_texturePages.size())) {
		return;
	}

	m_texturePages.erase(m_texturePages.begin() + index);

	if (m_texturePages.empty()) {		// If no pages remain
		m_activePageIndex = -1;			// Clear active page index
	}
	else if (m_activePageIndex >= static_cast<int>(m_texturePages.size())) {	// If active index is now out of range
		m_activePageIndex = static_cast<int>(m_texturePages.size() - 1);		// Clamp active index to last page
	}

	RecalculateGridProperties();		// Recompute grid for new page layout
	UpdateAnimationStateRanges();		// Clamp states to new frame ranges
}

/*************************************************************************
  \brief
	Synchronizes animation state selection with the active texture page.

  \details
	Ensures that the currently selected animation state overlaps the
	active page. If not, attempts to select another overlapping state,
	or clears the selection if no suitable state exists.
*************************************************************************/
void AnimationNode::SyncSelectionWithActivePage() {
	uint32_t pageStart = GetPageStartOffset(m_activePageIndex);				// Get global start frame of active page
	uint32_t pageEnd = pageStart + GetPageCapacity(m_activePageIndex) - 1;	// Get global end frame of active page

	// If current selection overlaps new page keep it
	if (HasSelectedState()) {	// If an animation state is selected
		const auto& state = m_animationStates[m_selectedStateIndex];		// Get selected state
		if (state.endIndex >= pageStart && state.startIndex <= pageEnd) {	// Check overlap with page
			return;				// Keep current selection
		}
	}

	// Otherwise try to find state that overlaps with the page
	for (size_t i = 0; i < m_animationStates.size(); ++i) {					// Search for overlapping state
		const auto& state = m_animationStates[i];
		if (state.endIndex >= pageStart && state.startIndex <= pageEnd) {	// Check overlap with page
			SelectAnimationState(static_cast<int>(i));						// Select matching state
			return;
		}
	}

	// Otherwise clear selection
	m_selectedStateIndex = -1;

	// Loop through all states and clear selection flags
	for (auto& state : m_animationStates) {
		state.isSelected = false;
	}
}

/*************************************************************************
  \brief
	Checks whether the active page index is valid.

  \return
	True if the page index exists within the texture page list, false 
	otherwise.
*************************************************************************/
bool AnimationNode::HasValidPageIndex() const {
	return !m_texturePages.empty() && m_activePageIndex >= 0 && m_activePageIndex < static_cast<int>(m_texturePages.size());
}

/*************************************************************************
  \brief
	Checks whether a texture page is active and loaded.

  \return
	True if a valid page index exists and the texture is loaded, false 
	otherwise.
*************************************************************************/
bool AnimationNode::HasActivePage() const {
	return HasValidPageIndex() && m_texturePages[m_activePageIndex].isLoaded;
}




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
uint32_t AnimationNode::GetPageCapacity(int pageIndex) const {
	if (pageIndex < 0 || pageIndex >= static_cast<int>(m_texturePages.size())) {
		return 0;
	}

	const TextureData& page = m_texturePages[pageIndex];
	if (!page.isLoaded) {
		return 0;
	}

	// Capacity = column * Rows (Return total frames on this page)
	return static_cast<uint32_t>(page.maxColumns * page.maxRows);
}

/*************************************************************************
  \brief
	Computes the global frame offset for a texture page.

  \param pageIndex
	The index of the texture page.

  \return
	The starting global frame index for the page.
*************************************************************************/
uint32_t AnimationNode::GetPageStartOffset(int pageIndex) const {
	uint32_t offset = 0;

	// Loop through earlier pages and accumulate frame counts
	for (int i = 0; i < pageIndex; ++i) {
		offset += GetPageCapacity(i);
	}

	return offset;	// Return global start index
}

/*************************************************************************
  \brief
	Returns the total number of frames across all texture pages.

  \return
	The total global frame count.
*************************************************************************/
uint32_t AnimationNode::GetTotalGlobalFrames() const {
	uint32_t total = 0;

	for (int i = 0; i < static_cast<int>(m_texturePages.size()); ++i) {
		total += GetPageCapacity(i);		// Sum frames from all pages
	}

	return total;
}

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
bool AnimationNode::ResolveGlobalFrameToPageLocal(uint32_t globalFrame, int pageIndex, uint32_t& localFrame) const {
	uint32_t start = GetPageStartOffset(pageIndex);		// Get page start index
	uint32_t capacity = GetPageCapacity(pageIndex);		// Get number of frames in page

	if (capacity == 0) {
		return false;	// Fail if page has no frames
	}

	if (globalFrame < start || globalFrame >= start + capacity) {
		return false;
	}

	localFrame = globalFrame - start;	// Convert to page local index
	return true;
}




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
void AnimationNode::ExportAnimationFile() {
	try {
		// If validation is enabled
		if (m_exportSettings.validateOnExport) {
			std::vector<std::string> validationErrors = ValidateAnimationData();	// Run validator
			if (!validationErrors.empty()) {										// If problems found
				m_validationErrors = validationErrors;								// Save errors for UI
				ImGui::OpenPopup("Validation Errors");								// Open popup window and cancel popup
				return;
			}
		}

		std::string baseFileName = m_exportSettings.fileName;						// Read export file base name
		std::string finalfileName = baseFileName + ".gganim";						// Append file extension

		// Construct full export path
		std::string fullPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + m_exportSettings.outputDirectory + finalfileName;

		// check if file existed in folder
		if (std::filesystem::exists(fullPath)) {
			m_pendingExportPath = fullPath;					// Store path for overwrite popup
			m_showOverwriteConfirmation = true;				// Enable confirmation popup
			ImGui::OpenPopup("Existing animation files!");	// Notify user
			return;
		}

		// Write to file and Write JSON to disk
		rapidjson::Document doc = CreateAnimationJson();
		SaveJsonToFileInternal(doc, fullPath);
	}
	catch (const std::exception& e) {
		m_lastExportError = e.what();
		std::cerr << "Export failed: " << e.what() << std::endl;
	}
}

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
std::vector<std::string> AnimationNode::ValidateAnimationData() {
	std::vector<std::string> errors;

	// Basic data validation
	TextureData& page = m_texturePages[m_activePageIndex];
	if (!page.isLoaded) {
		errors.push_back("No texture loaded");
		return errors;
	}

	if (m_animationStates.empty()) {
		errors.push_back("No animation states defined");
	}

	// Grid configuration validation  
	if (page.totalFrames <= 0) {
		errors.push_back("Invalid grid configuration: no frames available");
	}

	if (m_gridConfig.animFPS <= 0.0) {
		errors.push_back("Animation FPS must be greater than 0");
	}

	// UV bounds validation
	if (page.uvSMin >= page.uvSMax || page.uvTMin >= page.uvTMax) {
		errors.push_back("Invalid UV bounds: min values must be less than max values");
	}

	// Animation states validation
	std::set<std::string> stateNames;
	for (size_t i = 0; i < m_animationStates.size(); ++i) {
		const auto& state = m_animationStates[i];
		std::string statePrefix = "State " + std::to_string(i) + " (" + state.stateName + "): ";	// Build error prefix

		// Name validation
		if (strlen(state.stateName) == 0) {
			errors.push_back(statePrefix + "name cannot be empty");
		}

		// Duplicate name check
		if (stateNames.count(state.stateName) > 0) {
			errors.push_back(statePrefix + "duplicate state name");
		}
		else {
			stateNames.insert(state.stateName);
		}

		// Frame range validation
		if (state.startIndex > state.endIndex) {
			errors.push_back(statePrefix + "start index greater than end index");
		}

		// Frame count validation
		if (state.getFrameCount() <= 0) {
			errors.push_back(statePrefix + "no frames in range");
		}

		// Global frames validation
		uint32_t totalGlobalFrames = GetTotalGlobalFrames();
		if (totalGlobalFrames == 0) {
			errors.push_back("No frames available across texture pages");
		}
		else if (state.endIndex >= totalGlobalFrames) {
			errors.push_back(statePrefix + "end index exceeds available frames");
		}
	}

	return errors;
}

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
rapidjson::Document AnimationNode::CreateAnimationJson()
{
	// Create JSON matching .gganim format exactly
	rapidjson::Document doc(rapidjson::kObjectType);

	// JSON allocator for inserting values
	auto& allocator = doc.GetAllocator();

	// Root "anims" array (matches existing .gganim structure)
	rapidjson::Value animsArray(rapidjson::kArrayType);

	// Create One Anim object per texture page
	for (size_t pageIndex = 0; pageIndex < m_texturePages.size(); ++pageIndex) {
		const TextureData& page = m_texturePages[pageIndex];

		// Single animation object (One spritesheet)
		rapidjson::Value animObject(rapidjson::kObjectType);

		// Texture asset references
		// If user wants UUID added
		if (m_exportSettings.includeUUID && page.uuid.has_value()) {
			// Convert UUID to string
			std::string uuidStr = uuids::to_string(page.uuid.value());
			// Add UUID field
			animObject.AddMember("textureAssetUUID", rapidjson::Value(uuidStr.c_str(), allocator), allocator);
		}

		// Animation properties/params (direct mapping to .gganim format)
		animObject.AddMember("animFPS", m_gridConfig.animFPS, allocator);
		animObject.AddMember("spritesPerRow", page.spritesPerRow, allocator);
		animObject.AddMember("numRows", page.maxRows, allocator);

		// UV bounds object (direct mapping to textureUVBounds)
		rapidjson::Value uvBounds(rapidjson::kObjectType);
		uvBounds.AddMember("sMin", static_cast<double>(page.uvSMin), allocator);
		uvBounds.AddMember("tMin", static_cast<double>(page.uvTMin), allocator);
		uvBounds.AddMember("sMax", static_cast<double>(page.uvSMax), allocator);
		uvBounds.AddMember("tMax", static_cast<double>(page.uvTMax), allocator);
		animObject.AddMember("textureUVBounds", uvBounds, allocator);

		// Animation states array (direct mapping to animStates)
		// Begin animStates array (Only first page has states)
		rapidjson::Value animStatesArray(rapidjson::kArrayType);

		// Only first page stores states
		if (pageIndex == 0) {
			for (const auto& state : m_animationStates) {

				if (!state.isValid()) {
					continue; // Skip invalid states
				}

				// Create state object and add members
				rapidjson::Value stateObject(rapidjson::kObjectType);
				stateObject.AddMember("stateName", rapidjson::Value(state.stateName, allocator), allocator);
				stateObject.AddMember("startIndex", state.startIndex, allocator);
				stateObject.AddMember("endIndex", state.endIndex, allocator);
				stateObject.AddMember("shouldLoop", state.shouldLoop, allocator);

				// Add events only if present
				if (!state.events.empty()) {
					rapidjson::Value eventsArray(rapidjson::kArrayType);

					for (const auto& event : state.events) {
						if (strlen(event.name) == 0) {
							continue;
						}

						rapidjson::Value eventObject(rapidjson::kObjectType);
						eventObject.AddMember("name", rapidjson::Value(event.name, allocator), allocator);

						if (event.hasFrame) {
							eventObject.AddMember("frame", event.frame, allocator);
						}

						eventsArray.PushBack(eventObject, allocator);
					}

					// Add events array if not empty
					if (!eventsArray.Empty()) {
						stateObject.AddMember("events", eventsArray, allocator);
					}
				}

				// Add to array
				animStatesArray.PushBack(stateObject, allocator);
			}
		}

		// Append animStates into anim
		animObject.AddMember("animStates", animStatesArray, allocator);


		// Add anim to root
		animsArray.PushBack(animObject, allocator);
	}

	// Add root "anims"
	doc.AddMember("anims", animsArray, allocator);

	return doc;
}

/*************************************************************************
  \brief
	Internal helper for writing .gganim JSON files.

  \details
	Handles directory creation, file writing, export status updates,
	and post-export system refresh logic. Called by both standard and
	overwrite-confirmation export paths.
*************************************************************************/
void AnimationNode::SaveJsonToFileInternal(const rapidjson::Document& doc, const std::string& fullPath)
{
	// ensure directory exists
	std::filesystem::create_directories(std::filesystem::path(fullPath).parent_path());

	// convert to formatted JSON string
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	// write to file
	std::ofstream file(fullPath);
	if (!file.is_open())
	{
		throw std::runtime_error("Could not open file for writing: " + fullPath);
	}

	file << buffer.GetString();
	file.close();

	// Update export status
	m_exportSettings.lastExportPath = fullPath;
	m_lastExportError.clear();

	std::cout << "Successfully exported animation: " << fullPath << std::endl;

	m_showExportSuccessPopup = true;

	TriggerAssetSystemRefresh(fullPath);	// Notify asset system
}

/*************************************************************************
  \brief
	Saves a JSON animation document to disk using the configured output
	path and file name.

  \details
	Writes formatted JSON to the Assets directory, updates export status,
	triggers editor refresh events, and displays a success popup.
	Throws if the file cannot be created or written.
*************************************************************************/
void AnimationNode::SaveJSONToFile(const rapidjson::Document& doc) {
	std::string baseFileName = m_exportSettings.fileName;

	// creates new animetion file with the type .gganim
	std::string finalFileName = baseFileName + ".gganim";

	// Generate full file path
	std::string fullPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + m_exportSettings.outputDirectory + finalFileName;

	// Ensure directory exists
	std::filesystem::create_directories(std::filesystem::path(fullPath).parent_path());

	// Convert to formatted JSON string
	rapidjson::StringBuffer buffer;
	// Writer to format JSON
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	// Convert document to string
	doc.Accept(writer);

	// Write to file (Open output file)
	std::ofstream file(fullPath);
	if (!file.is_open()) {
		throw std::runtime_error("Could not open file for writing: " + fullPath);
	}

	file << buffer.GetString();		// Write JSON to file
	file.close();					// Close stream

	// Update export status
	m_exportSettings.lastExportPath = fullPath;
	m_lastExportError.clear();

	std::cout << "Successfully exported animation: " << fullPath << std::endl;

	RenderExportPopup();			// Trigger success popup

	// Trigger asset system update to recognize new file
	TriggerAssetSystemRefresh(fullPath);
}

/*************************************************************************
  \brief
	Notifies the engine that new or modified animation assets are available.

  \details
	Informs the AssetManager to rescan and re-register asset UUIDs,
	ensuring that newly exported .gganim files or updated textures
	appear immediately inside the editor. Also emits an editor event
	so UI panels such as the AssetManagerNode refresh their displays.
*************************************************************************/
void AnimationNode::TriggerAssetSystemRefresh(const std::string& newFilePath) {
	// Notify asset manager of new file
	auto* assetManager = GGEngine::GetEngine().GetAssetManager();
	assetManager->ScanAssetDirectories();  // Rediscover assets
	assetManager->RegisterAssetsWithUUIDs();  // Assign UUIDs to new files

	std::cout << "[AnimationNode] Animation exported: " << GetRelativeAssetPath(newFilePath) << std::endl;

	// Emit event to request AssetManagerNode refresh
	EditorEventBus::onAssetSystemRefresh.Emit();
}

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
std::string AnimationNode::GetRelativeAssetPath(const std::string& fullPath) const {
	const std::string prefix = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();

	// If fullPath already contains prefix remove it
	if (fullPath.find(prefix) != std::string::npos) {	// Check if path starts with "../../../Assets/"
		return fullPath.substr(prefix.size());			// Strip the prefix and return the relative path portion
	}

	// If user passes a raw relative path (e.g. "Textures/Hero.png" just return as it is
	if (fullPath.find("Assets/") != std::string::npos) {
		// remove "Assets/"(7 chars) prefix
		return fullPath.substr(fullPath.find("Assets/") + 7);
	}

	// Last fallback try to extract from full filePath system
	std::filesystem::path p(fullPath);				// Convert provided string into a filesystem path object

	// Compute absolute path to the project assets root
	std::filesystem::path assetsRoot = std::filesystem::absolute(GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath());

	// Error code to safely capture failures (no exceptions)
	std::error_code ec;
	// Attempt to compute fullPath relative to the asset root
	std::filesystem::path rel = std::filesystem::relative(p, assetsRoot, ec);

	// If no error occurred (relative path computation succeeded)
	if (!ec) {
		// Return the relative filesystem path as a string
		return rel.string();
	}

	// If everything fails, return original
	return fullPath;
}




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
void AnimationNode::RenderAnimationFileBrowserPopup() {
	if (ImGui::BeginPopup("Animation File Browser")) {

		auto* assetManager = GGEngine::GetEngine().GetAssetManager();

		// Get list of discovered .gganim files from the asset system
		const auto& animFiles = assetManager->GetDiscoveredAnimationPaths();

		ImGui::Text("Available Animations:");
		ImGui::Separator();

		// Loop through all discovered .gganim files
		for (const auto& path : animFiles) {
			if (ImGui::Selectable(path.c_str())) {
				// Load metadata from selected .gganim file (prepend full path)
				LoadAnimationMetadata(GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + path);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Close")) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

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
bool AnimationNode::LoadAnimationMetadata(const std::string& animPath) {
	// Ensure the requested file exists on disk
	if (!std::filesystem::exists(animPath)) {
		return false;
	}

	rapidjson::Document doc;		// JSON container
	std::ifstream file(animPath);	// Open file stream

	// Check if file was successfully opened
	if (!file.is_open()) {
		return false;
	}

	// Read entire .gganim file into a string
	std::string jsonText((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	// Parse JSON content
	doc.Parse(jsonText.c_str());

	// Ensure JSON has the "anims" array expected by the .gganim format
	if (!doc.IsObject() || !doc.HasMember("anims") || !doc["anims"].IsArray()) {
		return false;
	}

	const auto& anims = doc["anims"].GetArray();	// Get anims array
	if (anims.Size() == 0) {						// Ensure at least one anim exists
		return false;
	}

	// Export filename
	std::filesystem::path p(animPath);
	std::string baseName = p.stem().string();

	// Copy filename into export field
	strncpy(m_exportSettings.fileName, baseName.c_str(), sizeof(m_exportSettings.fileName));
	m_exportSettings.fileName[sizeof(m_exportSettings.fileName) - 1] = '\0';

	// Reset editor state
	m_texturePages.clear();
	m_animationStates.clear();
	m_selectedStateIndex = -1;
	m_activePageIndex = 0;

	// Detect Legacy format
	const bool isLegacySinglePage = (anims.Size() == 1);

	// Load animFPS from first page
	if (anims[0].HasMember("animFPS") && anims[0]["animFPS"].IsNumber()) {
		m_gridConfig.animFPS = static_cast<float>(anims[0]["animFPS"].GetDouble());
	}

	// Load all pages first
	for (rapidjson::SizeType i = 0; i < anims.Size(); ++i) {
		const auto& anim = anims[i];

		// Create page slot first
		m_texturePages.emplace_back();
		TextureData& page = m_texturePages.back();

		// If .gganim includes a texture UUID, attempt automatic texture loading
		if (anim.HasMember("textureAssetUUID") && anim["textureAssetUUID"].IsString())
		{
			std::string uuidStr = anim["textureAssetUUID"].GetString();

			// Convert UUID string into uuid object
			auto parsed = uuids::uuid::from_string(uuidStr);
			if (parsed.has_value()) {
				// Valid UUID
				page.uuid = parsed.value();

				// Find actual texture path by UUID
				std::string texturePath = FindTexturePathFromUUID(page.uuid.value());

				// If texture path found, load PNG texture immediately
				if (!texturePath.empty()) {
					LoadTextureIntoPage((int)m_texturePages.size() - 1, texturePath);
				}
			}
		}

		if (anim.HasMember("spritesPerRow")) {
			page.spritesPerRow = anim["spritesPerRow"].GetInt();
		}

		if (anim.HasMember("numRows")) {
			page.totalRows = anim["numRows"].GetInt();
		}

		if (page.isLoaded && page.spritesPerRow > 0 && page.totalRows > 0) {
			// Compute per-cell pixel dimensions
			page.cellWidth = page.width / page.spritesPerRow;
			page.cellHeight = page.height / page.totalRows;

			page.maxColumns = page.spritesPerRow;
			page.maxRows = page.totalRows;

			// Compute total frames used
			page.totalFrames = page.maxColumns * page.maxRows;
			page.totalPossibleFrames = page.totalFrames;
		}

		if (anim.HasMember("textureUVBounds")) {
			// Load UV bounds (raw values from .gganim)
			const auto& uv = anim["textureUVBounds"];
			if (uv.HasMember("sMin")) {
				page.uvSMin = uv["sMin"].GetFloat();
			}
			if (uv.HasMember("tMin")) {
				page.uvTMin = uv["tMin"].GetFloat();
			}
			if (uv.HasMember("sMax")) {
				page.uvSMax = uv["sMax"].GetFloat();
			}
			if (uv.HasMember("tMax")) {
				page.uvTMax = uv["tMax"].GetFloat();
			}
		}
	}

	// Load Global States from anims[0]
	const auto& first = anims[0];

	if (first.HasMember("animStates") && first["animStates"].IsArray()) {
		for (const auto& s : first["animStates"].GetArray()) {	// Loop through animation states
			if (!s.HasMember("stateName") || !s["stateName"].IsString()) {
				continue;
			}
			if (!s.HasMember("startIndex") || !s["startIndex"].IsUint()) {
				continue;
			}
			if (!s.HasMember("endIndex") || !s["endIndex"].IsUint()) {
				continue;
			}
			if (!s.HasMember("shouldLoop") || !s["shouldLoop"].IsBool()) {
				continue;
			}

			AnimationState state;

			// Create new animation state
			strncpy(state.stateName, s["stateName"].GetString(), sizeof(state.stateName));
			state.stateName[sizeof(state.stateName) - 1] = '\0';

			uint32_t start = s["startIndex"].GetUint();
			uint32_t end = s["endIndex"].GetUint();

			// Clamp for legacy format 
			// Check if frame exists in range if not, move it back to the last frame that does exist
			if (isLegacySinglePage) {
				uint32_t cap = GetPageCapacity(0);

				uint32_t maxFrame = 0;
				if (cap > 0) {
					maxFrame = cap - 1;
				}

				if (start > maxFrame) {
					start = maxFrame;
				}

				if (end > maxFrame) {
					end = maxFrame;
				}
			}

			state.startIndex = start;
			state.endIndex = end;
			state.shouldLoop = s["shouldLoop"].GetBool();

			if (s.HasMember("events") && s["events"].IsArray()) {
				for (const auto& event : s["events"].GetArray()) {
					if (!event.HasMember("name") || !event["name"].IsString()) {
						continue;
					}

					AnimationEvent ev{};
					strncpy(ev.name, event["name"].GetString(), sizeof(ev.name));
					ev.name[sizeof(ev.name) - 1] = '\0';

					if (event.HasMember("frame") && event["frame"].IsInt()) {
						ev.hasFrame = true;
						ev.frame = event["frame"].GetInt();
					}
					else {
						ev.hasFrame = false; // No frame, omit in JSON
					}

					state.events.push_back(ev);
				}
			}

			m_animationStates.push_back(state);
		}
	}

	// Select first state
	if (!m_animationStates.empty()) {
		SelectAnimationState(0);
	}

	// Select first loaded page
	for (int i = 0; i < static_cast<int>(m_texturePages.size()); ++i) {
		if (m_texturePages[i].isLoaded) {
			m_activePageIndex = i;
			break;
		}
	}

	return true;
}

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
std::string AnimationNode::FindTexturePathFromUUID(const AssetUUID& uuid) {
	auto* assetManager = GGEngine::GetEngine().GetAssetManager();
	auto& uuidSystem = GGAssetID::GetGGAssetID();

	// Query all discovered texture paths inside Assets/
	const auto& textures = assetManager->GetDiscoveredTexturePaths();

	// Loop all textures and find matching UUID
	for (const auto& path : textures) {
		// Fetch UUID assigned to this texture path
		std::optional<AssetUUID> texUUID = uuidSystem->GetUUIDFromPath(path);

		// If a valid UUID exists AND it matches the requested one
		if (texUUID.has_value() && texUUID.value() == uuid) {
			return path; // Match Found (return relative texture path)
		}
	}

	return {}; // No match (return empty string)
}

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
void AnimationNode::LoadTextureIntoPage(int pageIndex, const std::string& path) {
	if (pageIndex < 0 || pageIndex >= (int)m_texturePages.size()) {
		return;
	}

	TextureData& page = m_texturePages[pageIndex];
	page = TextureData{};

	auto& assetSystem = GGAssetID::GetGGAssetID();
	const AssetUUID& uuid = assetSystem->GetUUID(path);

	uint32_t texID = GGECSStandardAssetManager<GLuint>::RegisterAsset(uuid);
	page.textureID = GGECSStandardAssetManager<GLuint>::GetAsset(texID);

	glBindTexture(GL_TEXTURE_2D, page.textureID);		// Bind texture
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &page.width);		// Query width
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &page.height);	// Query height
	glBindTexture(GL_TEXTURE_2D, 0);	// Unbind texture

	page.filePath = path;
	page.uuid = uuid;
	page.isLoaded = true;
}




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
void AnimationNode::RenderAssetBrowserPopup() {
	if (ImGui::BeginPopup("Asset Browser")) {
		ImGui::Text("Available Textures:");
		ImGui::Separator();

		auto* assetManager = GGEngine::GetEngine().GetAssetManager();

		// Ask for discovered texture paths
		const auto& textures = assetManager->GetDiscoveredTexturePaths();

		// Loop through all discovered textures
		for (const auto& texPath : textures) {
			// If user clicked a texture
			if (ImGui::Selectable(texPath.c_str())) {
				// Copy path into input field
				strncpy(m_assetPathBuffer, texPath.c_str(), sizeof(m_assetPathBuffer));

				// Immediately load the texture
				LoadTextureAsset(texPath);

				// Close the popup window
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Close")) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

/*************************************************************************
  \brief
	Displays the modal popup shown after a successful export.

  \details
	Shows the saved file path and an OK button to dismiss the popup.
	Automatically appears when m_showExportSuccessPopup is enabled.
*************************************************************************/
void AnimationNode::RenderExportPopup() {
	// Center this popup in the viewport
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	// If export success flag is set, open export-success popup
	if (m_showExportSuccessPopup) {
		ImGui::OpenPopup("Export Successful");
	}

	if (ImGui::BeginPopupModal("Export Successful", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Animation successfully exported!");
		ImGui::Text("Saved to:");
		// Show file path header
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", m_exportSettings.lastExportPath.c_str());

		if (ImGui::Button("OK", ImVec2(100, 0))) {
			m_showExportSuccessPopup = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

/*************************************************************************
  \brief
	Displays a popup summarizing validation errors detected at export.

  \details
	Lists each error message in a styled modal window.
	The popup prevents export from proceeding until all issues are
	resolved or acknowledged.
*************************************************************************/
void AnimationNode::RenderValidationPopup() {
	if (ImGui::BeginPopup("Validation Errors")) {
		ImGui::SeparatorText("Animation Validation Failed");

		for (const auto& err : m_validationErrors) {
			ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", err.c_str());
		}

		ImGui::Separator();
		if (ImGui::Button("OK")) {
			m_validationErrors.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

/*************************************************************************
  \brief
	Renders the confirmation popup shown when exporting over an existing file.

  \details
	Asks the user whether they want to overwrite an existing .gganim file.
	Overwrite triggers a forced save, while Cancel aborts the export.
	Ensures users do not accidentally overwrite important assets.
*************************************************************************/
void AnimationNode::RenderOverwriteConfirmationPopup()
{
	// Center popup on screen
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	// Begin modal popup
	if (ImGui::BeginPopupModal("Existing animation files!", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		std::filesystem::path filePath(m_pendingExportPath);	// Convert pending path to filesystem path
		std::string fileName = filePath.filename().string();	// Extract filename for display

		ImGui::Text("Warning: File Already Exists!");
		ImGui::Separator();

		ImGui::Text("The file already existed in Assets folder:");
		ImGui::BulletText("%s", fileName.c_str());

		ImGui::Spacing();
		ImGui::Text("Do you want to overwrite it?");
		ImGui::Spacing();

		ImGui::Separator();

		if (ImGui::Button("Overwrite", ImVec2(120, 0)))
		{
			try
			{
				rapidjson::Document doc = CreateAnimationJson();	// Build JSON document again
				SaveJsonToFileInternal(doc, m_pendingExportPath);	// Overwrite file
				m_showOverwriteConfirmation = false;				// Reset flag
			}
			catch (const std::exception& e)
			{
				m_lastExportError = e.what();
				std::cerr << "Export failed: " << e.what() << std::endl;
			}
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0)))				// User cancels overwrite
		{
			m_showOverwriteConfirmation = false;
			m_pendingExportPath.clear();							// Discard pending path
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();

	}
}

/*************************************************************************
  \brief
	Executes queued background operations.

  \details
	Processes all pending tasks queued during UI actions (e.g., delayed
	popups or post-load operations). Ensures UI responsiveness by
	deferring work until safe execution points.
*************************************************************************/
void AnimationNode::ProcessPendingTasks() {
	if (m_pendingTasks.empty()) {
		return;
	}

	// Execute and clear all queued tasks
	for (auto& task : m_pendingTasks) {
		task();
	}

	m_pendingTasks.clear();
}