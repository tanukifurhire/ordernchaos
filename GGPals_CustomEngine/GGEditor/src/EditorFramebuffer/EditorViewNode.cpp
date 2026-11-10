/*************************************************************************
  \file       EditorViewNode.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    2.0
  \brief
    Implementation of EditorViewNode for GGEngine editor viewport.
  \details
    This module handles:
      - Viewport rendering using ImGui
      - Editor camera creation, updates, and input handling
      - Entity picking, selection, and dragging
      - Gizmo interaction (translate, rotate, scale)
      - Grid rendering and viewport resizing
      - Popup UI for switching gizmo modes
*************************************************************************/
#include <EditorViewNode.hpp>
#include <GGSystemManagers.hpp>
#include <EditorEventBus.hpp>
#include <EditorUndoStack.hpp>
#include <GGLogger.hpp>
#include <imgui_internal.h>
#include <EntityCreationCommand.hpp>
#include <GGInputManager.hpp>
#include <GGEngine.hpp>
#include <GGECSWorld.hpp>      
#include <GGECSComponents.hpp> 
#include <GGTime.hpp>



// Camera control state
static double lastMouseX = 0.0;
static double lastMouseY = 0.0;
static bool firstMouse = true;

/*************************************************************************
  \brief Constructor for EditorViewNode.
  \details
    Initializes callback handles for Init, Update, and DockBuild functions.
*************************************************************************/
EditorViewNode::EditorViewNode()
{
    initCallbackHandle = [this]() -> void { this->Init(); };
    updateCallbackHandle = [this]() -> void { this->Update(); };
    dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief Initializes the editor view node.
  \details
    - Creates the editor camera and sets the viewport size.
    - Registers callbacks for entity selection events.
    - Registers the editor camera with the EditorManager system.
*************************************************************************/
void EditorViewNode::Init()
{
  
    editorCamera = std::make_unique<GGEditorCamera2D>();
    editorCamera->SetViewportSize(viewportSize);

    entitySelectedCallback =
        [&](Entity entity)
        {
            GGMeshRendererSystem::selectedEntity = entity.id;
            SetSelectedEntity(entity);
        };

    // to give acces to other system
    auto& editorInterface = Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>();
    editorInterface.RegisterEditorCamera(GetCamera());
    EditorEventBus::onEntitySelected += entitySelectedCallback;
}

/*************************************************************************
  \brief Builds docking layout for the editor view window.
  \details
    Uses ImGui DockBuilder to attach the "Editor View" window to the main dock.
*************************************************************************/
void EditorViewNode::DockBuild()
{
    ImGui::DockBuilderDockWindow("Editor View", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockMain);
}

/*************************************************************************
  \brief Updates the editor view node every frame.
  \details
    - Handles delta time calculation.
    - Updates camera controls and keyboard shortcuts.
    - Performs object picking and dragging.
    - Renders the editor viewport using ImGui.
    - Updates the renderer's editor camera reference.
*************************************************************************/
void EditorViewNode::Update()
{
 
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

if (ImGui::Begin("Editor View", nullptr, ImGuiWindowFlags_NoResize))
{

    // Button to open the popup
    if (ImGui::Button("Gizmo Mode"))
        ImGui::OpenPopup("GizmoModePopup");

    // Begin the popup
    if (ImGui::BeginPopup("GizmoModePopup"))
    {
        ImGui::Text("Select Gizmo Mode:");
        ImGui::Separator();

        // Radio buttons for modes
        if (ImGui::Selectable("Translate (G)"))
            GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::TRANSLATE;
        if (ImGui::Selectable("Rotate (R)"))
            GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::ROTATE;
        if (ImGui::Selectable("Scale (S)"))
            GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::SCALE;

        ImGui::EndPopup();
    }
    // Calculate delta time
    float currentTime = static_cast<float>(glfwGetTime());
    auto& timeMgr = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
    float deltaTime = static_cast<float>(timeMgr.GetDeltaTime());
    lastFrameTime = currentTime;
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    if (ImGui::IsWindowFocused())
    {
        // The current window is focused
        renderMgr.currentMode = Systems::RenderManager::EditorMode::editorView;
    }
    // Update viewport size
    UpdateViewportSize();

    // Render the editor view
    RenderEditorView();

    if (ImGui::IsItemHovered()) {
        
        // Handle camera controls
        HandleCameraControls(deltaTime);

        // Handle keyboard shortcuts
        HandleKeyboardShortcuts();
    }
    else {
        ResetCameraControlState();  
    }

    // Handle object picking and dragging
    HandleObjectPicking({ mousePosLocal.x,mousePosLocal.y });
    HandleObjectDragging();

    // Update renderer camera
   
    renderMgr.SetEditorCamera(editorCamera.get());

    // accept prefabs dropped into scene --------------------------------------------------
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {

            auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
            auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
            auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
            auto& world = sceneManager.GetActiveScene();

            std::string prefabName = prefabManager.GetCurrentlyEditedPrefabName();
            objectsFactory.InstantiateObjects(prefabName, prefabManager, world, 1);
        }
        ImGui::EndDragDropTarget();
    }
    // ------------------------------------------------------------------------------------

}

ImGui::End();
ImGui::PopStyleVar();
}

/*************************************************************************
  \brief Updates the editor viewport size.
  \details
    - Updates the InputManager and editor camera with new viewport dimensions.
*************************************************************************/
void EditorViewNode::UpdateViewportSize()
{
    //make target ratio matche the ratio of the framebuffer height
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    float targetAspect = static_cast<float>(renderMgr.viewBuffer.getWidth()) /
        static_cast<float>(renderMgr.viewBuffer.getHeight());

    ImVec2 available = ImGui::GetContentRegionAvail();

    ImVec2 viewportSizeT = available;
    float currentAspect = available.x / available.y;

    if (currentAspect > targetAspect) {
        viewportSizeT.x = available.y * targetAspect;
    }
    else {
        viewportSizeT.y = available.x / targetAspect;
    }

    viewportSize = glm::vec2(viewportSizeT.x, viewportSizeT.y);



    // Update input manager
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    inputMgr.UpdateViewportSize(viewportSize);
    ///set viewport sizze
    editorCamera->SetViewportSize(glm::vec2(renderMgr.viewBuffer.getWidth(), renderMgr.viewBuffer.getHeight()));
}

/*************************************************************************
  \brief Handles camera controls using mouse input and scroll.
  \param deltaTime Time elapsed since last frame.
  \details
    - Updates editor camera position and zoom.
    - Processes middle mouse button panning.
    - Processes mouse wheel zooming.
*************************************************************************/
void EditorViewNode::HandleCameraControls(float deltaTime)
{
    // Update camera
   
    
    editorCamera->Update(deltaTime);

   auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

    // Get mouse position
    double mouseX, mouseY;
    inputMgr.GetMousePosition(mouseX, mouseY);
    // If viewport not hovered  reset state and exit early
    if (!ImGui::IsItemHovered()) {
        firstMouse = true;           // So next frame resets deltas
        inputMgr.GGInputMouseWheelDelta(nullptr); // Clear scroll
        return;
    }

   
    // Calculate mouse delta
    if (firstMouse) {
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        firstMouse = false;
    }

    float xOffset = static_cast<float>(mouseX - lastMouseX);
    float yOffset = static_cast<float>(mouseY - lastMouseY);
    lastMouseX = mouseX;
    lastMouseY = mouseY;

    
    // Middle mouse button panning
    bool isPanning = inputMgr.GGInputCheckCurr(GLFW_MOUSE_BUTTON_MIDDLE);
    editorCamera->ProcessMouseInput(xOffset, yOffset, isPanning, false);

    // Mouse scroll zooming
    int scrollDelta = 0;
    inputMgr.GGInputMouseWheelDelta(&scrollDelta);
    if (scrollDelta != 0) {
        editorCamera->ProcessScrollInput(static_cast<float>(scrollDelta));
    }
}

/*************************************************************************
  \brief Handles keyboard shortcuts for camera and editor actions.
  \details
    - F: Frame selection
    - Q: Reset camera
    - G: Translate gizmo mode
    - H: Toggle gizmo visibility
    - R: Rotate gizmo mode
    - S: Scale gizmo mode
*************************************************************************/
void EditorViewNode::HandleKeyboardShortcuts()
{
    if (!ImGui::IsItemHovered()) {
        return;
    }
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

    // F: Frame selection
    if (inputMgr.GGInputCheckTriggered(GGKey_F)) {
        FocusOnSelectedObject();
    }

    // Q: Reset camera
    if (inputMgr.GGInputCheckTriggered(GGKey_Q)) {
        editorCamera->Reset();
    }

    // C: Clear target
    if (inputMgr.GGInputCheckTriggered(GGKey_C)) {
        editorCamera->ClearTarget();
    }
    // to be worked on in to future
    //// T: Set camera target to selected entity
    //if (inputMgr.GGInputCheckTriggered(GGKey_T)) {
    //    if (selectedEntity.IsValid()) {
    //        editorCamera->SetTarget(selectedEntity);
    //        std::cout << "[Editor] Camera now following entity " << selectedEntity.id << std::endl;
    //    }
    //}

   
    // H: Toggle gizmos
    if (inputMgr.GGInputCheckTriggered(GGKey_H)) {
        GGMeshRendererSystem::showMoveGizmo = !GGMeshRendererSystem::showMoveGizmo ;
    }

    if (inputMgr.GGInputCheckTriggered(GGKey_G)) {
        GGMeshRendererSystem::currentGizmoMode =
            GGMeshRendererSystem::GizmoMode::TRANSLATE;
        std::cout << "[Editor] Switched to TRANSLATE mode\n";
    }
    if (inputMgr.GGInputCheckTriggered(GGKey_R)) {
        GGMeshRendererSystem::currentGizmoMode =
            GGMeshRendererSystem::GizmoMode::ROTATE;
        std::cout << "[Editor] Switched to ROTATE mode\n";
    }
   
    if (inputMgr.GGInputCheckTriggered(GGKey_S)) {
        GGMeshRendererSystem::currentGizmoMode =
            GGMeshRendererSystem::GizmoMode::SCALE;
        std::cout << "[Editor] Switched to SCALE mode\n";
    }
    // DELETE KEY
    if (inputMgr.GGInputCheckTriggered(GGKey_DELETE))
    {
        if (selectedEntity != NULL_ENTITY)
        {
            World& world = GGEngine::GetEngine().GetSceneManager().GetActiveScene();
            DeleteEntityInView(world, selectedEntity);
            selectedEntity = NULL_ENTITY;
        }
    }
}


/*************************************************************************
  \brief Renders the editor viewport and updates ImGui elements.
  \details
    - Centers the viewport in available space.
    - Updates the camera matrix in InputManager.
    - Renders the framebuffer as an ImGui Image.
    - Tracks mouse position for picking.
    - Shows a tooltip with mouse position and selected entity info.
*************************************************************************/
void EditorViewNode::RenderEditorView()
{
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();


    glm::mat4 cameraProj = editorCamera->GetProjectionMatrix();
    glm::mat4 renderProj = renderMgr.GetDefaultProjection();

    /*if (cameraProj != renderProj) {
        std::cout << "WARNING: Camera projection mismatch!" << std::endl;
        std::cout << "Camera viewport: "
            << editorCamera->GetViewportSize().x << "x"
            << editorCamera->GetViewportSize().y << std::endl;
        std::cout << "Framebuffer: "
            << renderMgr.viewBuffer.getWidth() << "x"
            << renderMgr.viewBuffer.getHeight() << std::endl;
    }*/
    // Calculate centered position
    ImVec2 available = ImGui::GetContentRegionAvail();
    ImVec2 viewportSizeT(viewportSize.x, viewportSize.y);

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 offset = {
        (available.x - viewportSizeT.x) * 0.5f,
        (available.y - viewportSizeT.y) * 0.5f
    };
    ImGui::SetCursorPos({ cursorPos.x + offset.x, cursorPos.y + offset.y });

    // Update camera matrix for input manager
    glm::mat4 cameraViewProj = editorCamera->GetProjectionMatrix() *
        editorCamera->GetViewMatrix4D();
    inputMgr.UpdateCameraMatrix(cameraViewProj);

    // Render the framebuffer
    GLuint textureID = renderMgr.viewBuffer.GetFrameTexture();
    ImGui::Image(
        reinterpret_cast<void*>(static_cast<intptr_t>(textureID)),
        viewportSizeT,
        ImVec2(0, 1),
        ImVec2(1, 0)
    );

    //// for debuging the picking texture 
    //GLuint pickingTexture = renderMgr.pickingFramebuffer.GetTexture();
    //ImGui::Image((void*)(intptr_t)pickingTexture,
    //    viewportSizeT,
    //    ImVec2(0, 1), ImVec2(1, 0));
    // Store image position for picking
    imagePos = ImGui::GetItemRectMin();

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    // Get window size
    int windowWidth, windowHeight;
    glfwGetWindowPos(Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetMainWindow().GetGLFWwindow(), &windowWidth, &windowHeight);

    inputMgr.SetFramebufferOffset({ min.x - static_cast<float>(windowWidth), min.y - static_cast<float>(windowHeight) });
    inputMgr.SetFramebufferScale({ max.x - min.x, max.y - min.y });

    // Show debug tooltip on hover
    if (ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        mousePosLocal = ImVec2(mousePos.x - imagePos.x, mousePos.y - imagePos.y);

        ImGui::BeginTooltip();
        ImGui::Text("Mouse: (%.0f, %.0f)", mousePosLocal.x, mousePosLocal.y);
        if (selectedEntity.IsValid()) {
            ImGui::Text("Selected Entity: %u", selectedEntity.id);
        }
        ImGui::EndTooltip();
    }


}

/*************************************************************************
  \brief Determines whether the mouse is interacting with a gizmo handle.
  \param entity The entity whose gizmo is being checked.
  \param mouseWorld Mouse position in world coordinates.
  \details
    Detects interaction for:
      - Translate gizmo: X/Y axis lines
      - Rotate gizmo: rotation ring
      - Scale gizmo: corner handles + center (uniform scale)
    If interaction is detected:
      - The corresponding drag mode is activated.
      - Initial state needed for dragging is stored.
*************************************************************************/
void EditorViewNode::CheckGizmoInteraction(const Entity& entity, glm::vec2 mouseWorld)
{
    auto& world = GGEngine::GetEngine().GetActiveWorld();

    if (!world.HasComponent<WorldTransform>(entity)) {
        return;
    }

    auto* worldTransform = world.GetComponent<WorldTransform>(entity);
    auto* localTransform = world.GetComponent<LocalTransform>(entity);

    glm::vec2 entityPos(worldTransform->matrix.getTranslation().x,
        worldTransform->matrix.getTranslation().y);
    float entityScaleX = localTransform->scale.x;
    float entityScaleY = localTransform->scale.y;
    float avgEntityScale = (entityScaleX + entityScaleY) * 0.5f;
    float gizmoScaleFactor = avgEntityScale * 0.5f;
    gizmoScaleFactor = std::max(0.2f, std::min(gizmoScaleFactor, 5.0f));

    // ROTATE MODE
    if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::ROTATE) {
        float distToCenter = glm::length(mouseWorld - entityPos);
        float radius = GGMeshRendererSystem::rotationGizmoRadius * gizmoScaleFactor;
        float thickness = radius*0.5f ; // Scale thickness too

        bool onCircle = std::abs(distToCenter - radius) < thickness;
        std::cout << "[CheckGizmo ROTATE] distToCenter: " << distToCenter
            << ", radius: " << radius
            << ", thickness: " << thickness
            << ", onCircle: " << (onCircle ? "TRUE" : "FALSE") << "\n";

        if (onCircle) {
            GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::ROTATE;
            std::cout << "mouse in circle\n";
            GGMeshRendererSystem::draggingAxis = true;
            isDragging = true;

            // Store initial state
            GGMeshRendererSystem::rotationDragStartPos = mouseWorld;
            GGMeshRendererSystem::rotationStartAngle = atan2f(
                mouseWorld.y - entityPos.y,
                mouseWorld.x - entityPos.x
            );

            if (world.HasComponent<LocalTransform>(entity)) {
                GGMeshRendererSystem::entityStartRotation = localTransform->rotation;
            }

            std::cout << "[Editor] Started rotation drag\n";
            std::cout << "[Editor] Start angle: " << GGMeshRendererSystem::rotationStartAngle
                << " rad (" << (GGMeshRendererSystem::rotationStartAngle * 180.0f / M_PI)
                << ")\n";
        }
        return;
    }

    // SCALE MODE
    if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::SCALE) {
        float rot = atan2f(worldTransform->matrix(1, 0), worldTransform->matrix(0, 0));
        float scaleX = sqrtf(worldTransform->matrix(0, 0) * worldTransform->matrix(0, 0) +
            worldTransform->matrix(1, 0) * worldTransform->matrix(1, 0));
        float scaleY = sqrtf(worldTransform->matrix(0, 1) * worldTransform->matrix(0, 1) +
            worldTransform->matrix(1, 1) * worldTransform->matrix(1, 1));

        float handleSize = GGMeshRendererSystem::scaleHandleSize * gizmoScaleFactor;

        // Calculate object half-extents
        float halfWidth = scaleX * 0.5f;
        float halfHeight = scaleY * 0.5f;

        
        float insetDistance = handleSize * 0.6f;
        float insetX = std::min(insetDistance, halfWidth * 0.3f);
        float insetY = std::min(insetDistance, halfHeight * 0.3f);

        // Check center handle first 
        float centerHandleSize = handleSize * 1.0f; 
        float centerDist = glm::length(mouseWorld - glm::vec2(entityPos.x, entityPos.y));
        if (centerDist < centerHandleSize) { 
            GGMeshRendererSystem::draggedScaleHandle =
                GGMeshRendererSystem::ScaleHandle::CENTER;
            GGMeshRendererSystem::draggingAxis = true;
            isDragging = true;

            GGMeshRendererSystem::scaleStartEntityScale = { localTransform->scale.x, localTransform->scale.y };
            GGMeshRendererSystem::scaleStartMouseWorld = mouseWorld;

            std::cout << "[Editor] Started uniform scale drag (CENTER)\n";
            return;
        }

        // Calculate corner positions with inset (same as rendering)
        std::array<glm::vec2, 4> localCorners = {
            glm::vec2(-halfWidth + insetX, -halfHeight + insetY),  // Bottom-left (inset)
            glm::vec2(halfWidth - insetX, -halfHeight + insetY),  // Bottom-right (inset)
            glm::vec2(halfWidth - insetX,  halfHeight - insetY),  // Top-right (inset)
            glm::vec2(-halfWidth + insetX,  halfHeight - insetY)   // Top-left (inset)
        };

        // Transform corners to WORLD space
        std::array<glm::vec2, 4> corners;
        float cosR = cosf(rot);
        float sinR = sinf(rot);

        for (size_t i = 0; i < 4; ++i) {
            float localX = localCorners[i].x;
            float localY = localCorners[i].y;

            // Rotate
            float rotatedX = localX * cosR - localY * sinR;
            float rotatedY = localX * sinR + localY * cosR;

            // Translate
            corners[i] = glm::vec2(
                entityPos.x + rotatedX,
                entityPos.y + rotatedY
            );
        }

        // Check corner handles
        const std::array<GGMeshRendererSystem::ScaleHandle, 4> handles = {
            GGMeshRendererSystem::ScaleHandle::BOTTOM_LEFT,   // [0]
            GGMeshRendererSystem::ScaleHandle::BOTTOM_RIGHT,  // [1]
            GGMeshRendererSystem::ScaleHandle::TOP_RIGHT,     // [2]
            GGMeshRendererSystem::ScaleHandle::TOP_LEFT       // [3]
        };

        for (size_t i = 0; i < 4; ++i) {
            float cornerDist = glm::length(mouseWorld - corners[i]);
            if (cornerDist < handleSize) {
                GGMeshRendererSystem::draggedScaleHandle = handles[i];
                GGMeshRendererSystem::draggingAxis = true;
                isDragging = true;

                GGMeshRendererSystem::scaleStartEntityScale = { localTransform->scale.x, localTransform->scale.y };
                GGMeshRendererSystem::scaleStartMouseWorld = mouseWorld;

                const char* cornerNames[] = { "BOTTOM_LEFT", "BOTTOM_RIGHT", "TOP_RIGHT", "TOP_LEFT" };
                std::cout << "[Editor] Started corner scale drag: " << cornerNames[i] << "\n";
                return;
            }
        }
        return;
    }

    // TRANSLATE MODE 
    if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::TRANSLATE) {
        float lineLength = GGMeshRendererSystem::gizmoLineLength * gizmoScaleFactor;

        glm::vec2 xLineEnd = entityPos + glm::vec2(lineLength, 0.f);
        glm::vec2 yLineEnd = entityPos + glm::vec2(0.f, lineLength);


        float lineClickThreshold = 0.1f;

        bool onXLine = false;
        bool onYLine = false;

        // X-axis line check
        if (std::abs(mouseWorld.y - entityPos.y) <= lineClickThreshold &&
            mouseWorld.x >= entityPos.x &&
            mouseWorld.x <= xLineEnd.x) {
            onXLine = true;
        }

        // Y-axis line check
        if (std::abs(mouseWorld.x - entityPos.x) <= lineClickThreshold &&
            mouseWorld.y >= entityPos.y &&
            mouseWorld.y <= yLineEnd.y) {
            onYLine = true;
        }

        if (onXLine && onYLine) {
            float distToXLine = std::abs(mouseWorld.y - entityPos.y);
            float distToYLine = std::abs(mouseWorld.x - entityPos.x);

            if (distToXLine < distToYLine) {
                GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::X;
            }
            else {
                GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::Y;
            }
            GGMeshRendererSystem::draggingAxis = true;
            isDragging = true;
        }
        else if (onXLine) {
            GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::X;
            GGMeshRendererSystem::draggingAxis = true;
            isDragging = true;
        }
        else if (onYLine) {
            GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::Y;
            GGMeshRendererSystem::draggingAxis = true;
            isDragging = true;
        }
        else {
            GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::NONE;
            GGMeshRendererSystem::draggingAxis = false;
            isDragging = true;
        }
    }
}

/*************************************************************************
  \brief Handles object picking using right-click.
  \param mousePos Mouse position in local viewport coordinates.
  \details
    - Converts screen position to world position.
    - Picks entity under the mouse using InputManager.
    - Initiates dragging if an entity is selected.
*************************************************************************/
void EditorViewNode::HandleObjectPicking(glm::vec2 mousePos)
{
    (void)mousePos;
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    auto& world = GGEngine::GetEngine().GetActiveWorld();

    if (!ImGui::IsItemHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        return;
    }
   

    ImVec2 mouseImGui = ImGui::GetMousePos();
    glm::vec2 localPos(mouseImGui.x - imagePos.x, mouseImGui.y - imagePos.y);
    std::cout << localPos.x << ", " << localPos.y << std::endl;
    glm::vec2 mouseWorld = inputMgr.GetMouseWorldPosition();

    if (selectedEntity.IsValid() && GGMeshRendererSystem::showMoveGizmo) {
        if (world.HasComponent<WorldTransform>(selectedEntity)) {
            CheckGizmoInteraction(selectedEntity, mouseWorld);

            
            if (GGMeshRendererSystem::draggingAxis) {
                std::cout << "[Editor] Clicked gizmo - skipping entity picking\n";
                isDragging = true;
                dragStartMouseWorld = mouseWorld;
                return; //
            }
        }
    }
    auto result = inputMgr.PickObjectAtScreenPosition(localPos);

    if (result.IsValid()) {
        std::cout << "[Editor] Picked Entity ID: " << result.EntityID << std::endl;

        dragStartMouseWorld = mouseWorld;

        // Find and select the entity
        for (const Entity& entity : world.GetLivingEntities()) {
            if (entity.id == result.EntityID) {
                SelectObject(entity.id);

                // Initialize drag state
                if (world.HasComponent<WorldTransform>(entity) && world.HasComponent<LocalTransform>(entity)) {
                    if (auto* worldTransform = world.GetComponent<WorldTransform>(entity))
                    {
                        dragStartEntityPos = glm::vec2(
                            worldTransform->matrix.getTranslation().x,
                            worldTransform->matrix.getTranslation().y
                        );

                        std::cout << "[Editor] Entity world pos: (" << worldTransform->matrix.getTranslation().x << ", " << worldTransform->matrix.getTranslation().y << ")\n";
                        std::cout << "[Editor] Mouse world pos: (" << mouseWorld.x << ", " << mouseWorld.y << ")\n";

                        // Check gizmo interaction
                        CheckGizmoInteraction(entity, mouseWorld);
                    }

                    if (auto* transform = world.GetComponent<LocalTransform>(entity)) {
                        dragBeforeState.position = transform->position;
                        dragBeforeState.scale = transform->scale;
                        dragBeforeState.rotation = transform->rotation;
                    }
                }
                break;
            }
        }
    }
    else {
        std::cout << "[Editor] Clicked background (no entity)\n";
        ClearSelection();
    }
}

/*************************************************************************
  \brief Handles object dragging with right mouse button.
  \details
    - Calculates world-space delta based on mouse movement.
    - Updates entity position according to axis constraints.
    - Ends dragging when the right mouse button is released.
*************************************************************************/
void EditorViewNode::HandleObjectDragging()
{
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    auto& world = GGEngine::GetEngine().GetActiveWorld();

    // Start dragging
    if (!isDragging && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && selectedEntity.IsValid()) {
        isDragging = true;
    }

    // Continue dragging
    if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        glm::vec2 localPos(mousePos.x - imagePos.x, mousePos.y - imagePos.y);
        glm::vec2 currentMouseWorld = inputMgr.GetMouseWorldPosition();

        for (const Entity& entity : world.GetLivingEntities()) {
            if (entity.id == selectedEntity.id &&
                world.HasComponent<LocalTransform>(entity) &&
                world.HasComponent<WorldTransform>(entity)) {

                auto* transform = world.GetComponent<LocalTransform>(entity);
                auto* worldTransform = world.GetComponent<WorldTransform>(entity);
                glm::vec2 entityPos(worldTransform->matrix.getTranslation().x,
                    worldTransform->matrix.getTranslation().y);

                // ROTATION MODE
                if (GGMeshRendererSystem::currentGizmoMode == GGMeshRendererSystem::GizmoMode::ROTATE &&
                    GGMeshRendererSystem::draggedAxis == GGMeshRendererSystem::Axis::ROTATE && GGMeshRendererSystem::draggingAxis) {

                    // Calculate current angle
                    float currentAngle = atan2f(
                        currentMouseWorld.y - entityPos.y,
                        currentMouseWorld.x - entityPos.x
                    );
                    GGMeshRendererSystem::rotationCurrentAngle = currentAngle;
                    // Calculate angle delta
                    float angleDelta = currentAngle -
                        GGMeshRendererSystem::rotationStartAngle;

                    // Normalize 
                    angleDelta = NormalizeAngle(angleDelta);

                    // Apply rotation
                    transform->rotation = NormalizeAngle(
                        GGMeshRendererSystem::entityStartRotation + angleDelta
                    );

                    transform->isDirty = true;
                }
                // SCALE MODE
                else if (GGMeshRendererSystem::currentGizmoMode ==
                    GGMeshRendererSystem::GizmoMode::SCALE && 
                    GGMeshRendererSystem::draggingAxis) {

                    glm::vec2 startOffset = GGMeshRendererSystem::scaleStartMouseWorld - entityPos;
                    glm::vec2 currentOffset = currentMouseWorld - entityPos;

                    if (GGMeshRendererSystem::draggedScaleHandle ==
                        GGMeshRendererSystem::ScaleHandle::CENTER) {
                     

                        float startDist = glm::length(startOffset);
                        float currentDist = glm::length(currentOffset);

                        if (startDist > 0.001f) {
                            // Calculate raw ratio
                            float ratio = currentDist / startDist;

                            // Apply sensitivity/damping
                            float sensitivity = 0.3f; // Adjust as needed
                            float dampedRatio = 1.0f + (ratio - 1.0f) * sensitivity;

                            // Apply uniform scal
                            transform->scale.x = GGMeshRendererSystem::scaleStartEntityScale.x * dampedRatio;
                            transform->scale.y = GGMeshRendererSystem::scaleStartEntityScale.y * dampedRatio;

                            // Clamp
                            transform->scale.x = std::max(0.01f, std::min(100.0f, transform->scale.x));
                            transform->scale.y = std::max(0.01f, std::min(100.0f, transform->scale.y));
                        }
                    }
                    else {
                       
                        // Get current rotation to transform to local space
                        float currentRot = atan2f(worldTransform->matrix(1, 0),
                            worldTransform->matrix(0, 0));

                        // Transform mouse positions to LOCAL space (remove rotation)
                        float cosR = cosf(currentRot);
                        float sinR = sinf(currentRot);

                        // Start mouse in local space
                        glm::vec2 startOffsetLocal(
                            startOffset.x * cosR + startOffset.y * sinR,
                            -startOffset.x * sinR + startOffset.y * cosR
                        );

                        // Current mouse in local space
                        glm::vec2 currentOffsetLocal(
                            currentOffset.x * cosR + currentOffset.y * sinR,
                            -currentOffset.x * sinR + currentOffset.y * cosR
                        );

                        // Calculate per-axis ratios using the CORRECT FORMULA
                        float ratioX = 1.0f;
                        float ratioY = 1.0f;

                        // ratioX = (currentMouseX - entityX) / (startMouseX - entityX)
                        if (std::abs(startOffsetLocal.x) > 0.001f) {
                            ratioX = currentOffsetLocal.x / startOffsetLocal.x;
                        }

                        // ratioY = (currentMouseY - entityY) / (startMouseY - entityY)
                        if (std::abs(startOffsetLocal.y) > 0.001f) {
                            ratioY = currentOffsetLocal.y / startOffsetLocal.y;
                        }

                        // Handle negative ratios (dragging past center)
                        // We want to allow mirroring/flipping
                        bool allowNegativeScale = false; // Set to true if you want flipping

                        if (!allowNegativeScale) {
                            // Prevent negative scale by taking absolute value
                            ratioX = std::abs(ratioX);
                            ratioY = std::abs(ratioY);
                        }

                        // Apply scale: scaleX_new = scaleX_old * ratioX
                        transform->scale.x = GGMeshRendererSystem::scaleStartEntityScale.x * ratioX;
                        transform->scale.y = GGMeshRendererSystem::scaleStartEntityScale.y * ratioY;

                        // Clamp to prevent extreme values
                        transform->scale.x = std::max(0.01f, std::min(100.0f, transform->scale.x));
                        transform->scale.y = std::max(0.01f, std::min(100.0f, transform->scale.y));
                    }

                    transform->isDirty = true;
                }
                // TRANSLATE MODE
                else if (GGMeshRendererSystem::currentGizmoMode ==
                    GGMeshRendererSystem::GizmoMode::TRANSLATE) {
                    glm::vec2 deltaWorld = currentMouseWorld - dragStartMouseWorld;

                    GGMath::Vector2 difference = worldTransform->matrix.getTranslation() -
                        transform->position;

                    switch (GGMeshRendererSystem::draggedAxis) {
                        // when x axis is clicked
                    case GGMeshRendererSystem::Axis::X:
                        transform->position.x = dragStartEntityPos.x + deltaWorld.x - difference.x;
                        break;
                        // when y axis is clicked
                    case GGMeshRendererSystem::Axis::Y:
                        transform->position.y = dragStartEntityPos.y + deltaWorld.y - difference.y;
                        break;
                    default:
                        // free movement
                        transform->position.x = dragStartEntityPos.x + deltaWorld.x - difference.x;
                        transform->position.y = dragStartEntityPos.y + deltaWorld.y - difference.y;
                        break;
                    }

                    transform->isDirty = true;
                }

                break;
            }
        }
    }

    // End dragging
    if (isDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        std::cout << "[Editor] Stopped dragging entity " << selectedEntity.id << std::endl;
        isDragging = false;
        GGMeshRendererSystem::draggingAxis = false;
        GGMeshRendererSystem::draggedScaleHandle = GGMeshRendererSystem::ScaleHandle::NONE;

        // Capture after state and push command
        
        if (auto* transform = world.GetComponent<LocalTransform>(selectedEntity)) {
            TransformSnapshot afterState;
            afterState.position = transform->position;
            afterState.scale = transform->scale;
            afterState.rotation = transform->rotation;

            if (dragBeforeState != afterState) {
                std::cout << "Bef X: " << dragBeforeState.position.x
                    << "Aft X: " << afterState.position.x << std::endl;
                auto cmd = std::make_unique<TransformCommand>(selectedEntity, dragBeforeState, afterState);
                GetUndoStack()->Push(std::move(cmd));
            }
        }
    }
}

/*************************************************************************
  \brief Selects an object by its ID.
  \param objectID ID of the object to select.
  \details
    - Updates selected entity in the EditorViewNode.
    - Shows the move gizmo in GGMeshRendererSystem.
*************************************************************************/
void EditorViewNode::SelectObject(uint32_t objectID)
{
    auto& world = GGEngine::GetEngine().GetActiveWorld();

    for ( const Entity& entity : world.GetLivingEntities()) {
        if (entity.id == objectID) {
            SetSelectedEntity(entity);
            
            // update envent bus when clicked
            EditorEventBus::onEntitySelected.Emit(Entity(entity));

            GGMeshRendererSystem::selectedEntity = objectID;

            GGMeshRendererSystem::showMoveGizmo = true;
            break;
        }
    }
}

/*************************************************************************
  \brief Clears the current selection.
  \details
    - Stops dragging.
    - Resets selected entity and related gizmo states.
    - Emits entity selection event with NULL_ENTITY.
*************************************************************************/
void EditorViewNode::ClearSelection()
{
    isDragging = false;
    selectedEntity = NULL_ENTITY;
    EditorEventBus::onEntitySelected.Emit(Entity(NULL_ENTITY));
    GGMeshRendererSystem::showMoveGizmo = false;
    GGMeshRendererSystem::selectedEntity = 0;
    GGMeshRendererSystem::draggingAxis = false;
    GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::NONE;

    // Reset rotation state
    GGMeshRendererSystem::rotationStartAngle = 0.0f;
    GGMeshRendererSystem::entityStartRotation = 0.0f;

    // Reset scale state
    GGMeshRendererSystem::draggedScaleHandle = GGMeshRendererSystem::ScaleHandle::NONE;
    GGMeshRendererSystem::scaleStartEntityScale = glm::vec2(1.0f);
}

/*************************************************************************
  \brief Moves the camera to focus on the selected object.
  \details
    - Calls FrameSelection() on the editor camera if an entity is selected.
*************************************************************************/
void EditorViewNode::FocusOnSelectedObject()
{
    if (selectedEntity.IsValid()) {
        editorCamera->FrameSelection();
    }
}
/*************************************************************************
  \brief Converts screen coordinates to world coordinates.
  \param screenPos Screen-space 2D position.
  \return glm::vec2 World-space position.
  \details
    Uses the InputManager's ScreenToWorld2D function for conversion.
*************************************************************************/
glm::vec2 EditorViewNode::ScreenToWorld(glm::vec2 screenPos)
{
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    return inputMgr.ScreenToWorld2D(screenPos);
}

/*************************************************************************
  \brief Resets camera control state.
  \details
    - Resets mouse tracking and scroll delta to prevent stale input.
*************************************************************************/
void EditorViewNode::ResetCameraControlState()
{
    firstMouse = true; 
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    // Clear scroll delta
    int dummy = 0;
    inputMgr.GGInputMouseWheelDelta(&dummy);
}

/*************************************************************************
  \brief Deletes an entity in the active scene.
  \param world Reference to the active World.
  \param entity Entity to delete.
  \details
    - Saves a backup of the entity for undo.
    - Pushes a DeleteEntityCommand to the UndoStack.
    - Removes entity from the ECS world and clears selection if needed.
*************************************************************************/
void EditorViewNode::DeleteEntityInView(World& world, Entity entity)
{
    EntitySnapshot snapshot;
    snapshot.entity = entity;

    if (world.HasComponent<HierarchyComponent>(entity))
    {
        HierarchyComponent* hierarchyComponent = world.GetComponent<HierarchyComponent>(entity);
        if (hierarchyComponent)
        {
            snapshot.parentEntity = hierarchyComponent->parent;
        }
    }

    snapshot.serializedData = GetEngineInstance().GetPrefabManager().SaveEntityBackup(entity, world);

    auto command = std::make_unique<DeleteEntityCommand>(snapshot);
    command->SetExecuted(true);
    GetUndoStack()->Push(std::move(command));

    Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSHierarchy>()
        .DeleteEntity(world, entity);

    if (selectedEntity == entity) {
        EditorEventBus::onEntitySelected.Emit(Entity(NULL_ENTITY));

        GGMeshRendererSystem::selectedEntity = 0;
    }
}