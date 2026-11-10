/*************************************************************************
  \file       EditorViewNode.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
	Header for EditorViewNode class for GGEngine editor.
  \details
	Provides an ImGui-based editor viewport, handling rendering, camera control,
	object selection, dragging, gizmos, grid display, and keyboard shortcuts.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <TransformCommand.hpp>
#include <memory>
#include <imgui_internal.h>

#include <GGECSInterface.hpp>
#include <GGEditorCamera2D.hpp>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/*************************************************************************
  \class EditorViewNode
  \brief Editor viewport node for GGEngine.
  \details
	Manages the editor camera, viewport rendering, input handling, object selection,
	dragging, and visualization helpers such as grid and gizmos.
*************************************************************************/

class EditorViewNode : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief Constructor for EditorViewNode.
	  \details
		Initializes callback handles for Init, Update, and DockBuild functions.
	*************************************************************************/
	EditorViewNode();
	virtual ~EditorViewNode() = default;

	/*************************************************************************
	  \brief Initializes the editor view node.
	  \details
		- Creates the editor camera and sets the viewport size.
		- Registers callbacks for entity selection events.
		- Registers the editor camera with the EditorManager system.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief Builds docking layout for the editor view window.
	  \details
		Uses ImGui DockBuilder to attach the "Editor View" window to the main dock.
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief Updates the editor view node every frame.
	  \details
		- Handles delta time calculation.
		- Updates camera controls and keyboard shortcuts.
		- Performs object picking and dragging.
		- Renders the editor viewport using ImGui.
		- Updates the renderer's editor camera reference.
	*************************************************************************/
	virtual void Update() override;

	ImVec2 GetMousePosLocal() { return mousePosLocal; }

	GGEditorCamera2D* GetCamera() {
		return editorCamera.get();
	}

	/*************************************************************************
	  \brief Selects an object by its ID.
	  \param objectID ID of the object to select.
	  \details
		- Updates selected entity in the EditorViewNode.
		- Shows the move gizmo in GGMeshRendererSystem.
	*************************************************************************/
	void SelectObject(uint32_t objectID);

	/*************************************************************************
	  \brief Moves the camera to focus on the selected object.
	  \details
		- Calls FrameSelection() on the editor camera if an entity is selected.
	*************************************************************************/
	void FocusOnSelectedObject();
	//void FrameAllObjects();
	//void HandleMouseClick(glm::vec2 mousePos);

	// Visual helpers
	void SetGridSize(float size) { gridSize = size; }
	bool IsGridVisible() const { return showGrid; }
	float GetGridSize() const { return gridSize; }

	// Selection queries

	ImVec2 GetMousePosLocal() const { return mousePosLocal; }
	glm::vec2 GetViewportSize() const { return viewportSize; }
private:

	Entity selectedEntity = NULL_ENTITY;
	Callback<Entity> entitySelectedCallback;

	Entity GetSelectedEntity() const { return selectedEntity; }
	void SetSelectedEntity(Entity entity) { selectedEntity = entity; }

	static float NormalizeAngle(float angle) {
		constexpr float PI = static_cast<float>(M_PI);
		constexpr float TWO_PI = 2.0f * PI;

		if (angle > -PI && angle <= PI) {
			return angle;
		}

		angle = std::fmodf(angle + PI, TWO_PI);

		if (angle < 0.0f) {
			angle += TWO_PI;
		}

		return angle - PI;
	}


	//camera
	std::unique_ptr<GGEditorCamera2D> editorCamera;

	// Viewport
	glm::vec2 viewportSize{ 1600.0f, 900.0f };
	ImVec2 mousePosLocal{ 0, 0 };
	ImVec2 imagePos{ 0, 0 }; // Position of the rendered image in ImGui space
	// Visual settings
	bool showGrid = true;
	bool showGizmos = true;
	float gridSize = 32.0f;

	// Dragging state
	bool shouldDrag = false;
	bool isDragging = false;
	glm::vec2 dragStartMouseWorld{ 0.0f, 0.0f };
	glm::vec2 dragStartEntityPos{ 0.0f, 0.0f };
	TransformSnapshot dragBeforeState;

	// Camera control state
	double lastMouseX = 0.0;
	double lastMouseY = 0.0;
	bool firstMouse = true;
	float lastFrameTime = 0.0f;


	// Rendering passes
	/*************************************************************************
	  \brief Renders the editor viewport and updates ImGui elements.
	  \details
		- Centers the viewport in available space.
		- Updates the camera matrix in InputManager.
		- Renders the framebuffer as an ImGui Image.
		- Tracks mouse position for picking.
		- Shows a tooltip with mouse position and selected entity info.
	*************************************************************************/
	void RenderEditorView();

	/*************************************************************************
	  \brief Checks if the mouse is interacting with a gizmo.
	  \param entity The entity to check.
	  \param mouseWorld Mouse position in world coordinates.
	  \details
		- Determines if the user is clicking near the X or Y axis gizmo lines.
		- Sets the dragging axis in GGMeshRendererSystem accordingly.
	*************************************************************************/
	void CheckGizmoInteraction(const Entity& entity, glm::vec2 mouseWorld);
	//void RenderPickingPass();
	//void RenderGrid();
	//void RenderGizmos();
	//void RenderSelectionOutline();

	// Input handling
	/*************************************************************************
	  \brief Handles object picking using right-click.
	  \param mousePos Mouse position in local viewport coordinates.
	  \details
		- Converts screen position to world position.
		- Picks entity under the mouse using InputManager.
		- Initiates dragging if an entity is selected.
	*************************************************************************/
	void HandleObjectPicking(glm::vec2 mousePos);

	/*************************************************************************
	  \brief Handles camera controls using mouse input and scroll.
	  \param deltaTime Time elapsed since last frame.
	  \details
		- Updates editor camera position and zoom.
		- Processes middle mouse button panning.
		- Processes mouse wheel zooming.
	*************************************************************************/
	void HandleCameraControls(float deltaTime);

	/*************************************************************************
	  \brief Handles object dragging with right mouse button.
	  \details
		- Calculates world-space delta based on mouse movement.
		- Updates entity position according to axis constraints.
		- Ends dragging when the right mouse button is released.
	*************************************************************************/
	void HandleObjectDragging();

	/*************************************************************************
	  \brief Handles keyboard shortcuts for camera and editor actions.
	  \details
		- F: Frame selection
		- R: Reset camera
		- C: Clear camera target
		- T: Target camera to selected entity
		- G: Toggle grid visibility
		- H: Toggle gizmo visibility
	*************************************************************************/
	void HandleKeyboardShortcuts();

	//helper

	/*************************************************************************
	  \brief Clears the current selection.
	  \details
		- Stops dragging.
		- Resets selected entity and related gizmo states.
		- Emits entity selection event with NULL_ENTITY.
	*************************************************************************/
	void ClearSelection();

	/*************************************************************************
	  \brief Updates the editor viewport size.
	  \details
		- Maintains a 16:9 aspect ratio.
		- Updates the InputManager and editor camera with new viewport dimensions.
	*************************************************************************/
	void UpdateViewportSize();

	/*************************************************************************
	  \brief Converts screen coordinates to world coordinates.
	  \param screenPos Screen-space 2D position.
	  \return glm::vec2 World-space position.
	  \details
		Uses the InputManager's ScreenToWorld2D function for conversion.
	*************************************************************************/
	glm::vec2 ScreenToWorld(glm::vec2 screenPos);

	/*************************************************************************
	  \brief Resets camera control state.
	  \details
		- Resets mouse tracking and scroll delta to prevent stale input.
	*************************************************************************/
	void ResetCameraControlState();

	/*************************************************************************
	  \brief Deletes an entity in the active scene.
	  \param world Reference to the active World.
	  \param entity Entity to delete.
	  \details
		- Saves a backup of the entity for undo.
		- Pushes a DeleteEntityCommand to the UndoStack.
		- Removes entity from the ECS world and clears selection if needed.
	*************************************************************************/
	void DeleteEntityInView(World& world, Entity entity);

	glm::vec2  dragStartMouseAbsolute{ 0.0f,0.0f };
};

/*************************************************************************
  \brief Singleton accessor for EditorViewNode.
  \return std::unique_ptr<EditorViewNode>& Reference to static editor view instance.
*************************************************************************/
inline auto& GetEditorViewNode()
{
	static std::unique_ptr<EditorViewNode> editorView = std::make_unique<EditorViewNode>();
	return editorView;
}