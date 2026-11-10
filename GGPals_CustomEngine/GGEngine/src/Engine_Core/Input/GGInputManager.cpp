/*************************************************************************
  \file       GGInputManager.cpp
  \author     Tok Duan Jie Nelson, Patchamuthu Abhishek Akilandeswari
  \par        Email: t.duanjienelson@digipen.edu, a.patchamuthu@digipen.edu
  \date       20 September 2025
  \version    1.1
  \contribute Nelson(80%), Ahbishek(20% Base template)
  \history    
    1.1 added mouse scroll input logic
  \brief
    Handles mouse and keyboard input for the game engine.
  \details
    Tracks current and previous input states, mouse position, and button hold times.
    Provides functions for checking if keys or mouse buttons are pressed, triggered, or held.
  \dependencies
    <iostream>, <GGInputManager.hpp>
  \see
  \history
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGInputManager.hpp"
#include <iostream>
#include <GGSystemManagers.hpp>

/*************************************************************************
  \brief
    Initializes the input manager.
  \return
    Always returns true (placeholder for future initialization logic)
*************************************************************************/
bool GGInputManager::Init() {

    return true;
}

/*************************************************************************
  \brief
    Checks if an input (key or mouse button) is currently held down.
  \param[int] input
    GLFW key or mouse button code
  \return
    returns true if input is currently pressed, else false
*************************************************************************/
bool GGInputManager::GGInputCheckCurr(int input) const {

    auto inputIterator = inputState.find(input);
    if (inputIterator != inputState.end() && inputIterator->second) {
        return true;
    }
    return false;
}

/*************************************************************************
  \brief
    Checks if an input (key or mouse button) was triggered this frame.
  \param[in] input
    GLFW key or mouse button code
  \return
    returns true if input was pressed this frame with false in the previous frame
*************************************************************************/
bool GGInputManager::GGInputCheckTriggered(int input) const {

    auto currInputIterator = inputState.find(input);
    auto prevInputIterator = prevInputState.find(input);

    bool curr = (currInputIterator != inputState.end() && currInputIterator->second);
    bool prev = (prevInputIterator != prevInputState.end() && prevInputIterator ->second);

    return (curr && !prev);
}

bool GGInputManager::GGInputCheckReleased(int input) const
{
    auto currInputIterator = inputState.find(input);
    auto prevInputIterator = prevInputState.find(input);

    bool curr = (currInputIterator != inputState.end() && currInputIterator->second);
    bool prev = (prevInputIterator != prevInputState.end() && prevInputIterator->second);

    return (!curr && prev);
}

/*************************************************************************
  \brief
    Retrieves the vertical mouse wheel scroll delta since the last update.
  \param[in] pDelta
    Pointer to an integer that will receive the scroll delta:
      - Positive value: scrolled away from user (scroll up)
      - Negative value: scrolled towards user (scroll down)
      - Zero : no scroll since last update
  \details
    This function accumulates scroll values from GLFW scroll events (OnMouseScrollEvent)
    and returns them once per call. After retrieval, the stored delta is rest to 0 so
    values are frame-accurate.
*************************************************************************/
void GGInputManager::GGInputMouseWheelDelta(int* pDelta) {
    if (!pDelta) {
        return;
    }

    *pDelta = static_cast<int>(mouseWheelDelta);

    mouseWheelDelta = 0.0;

    // -------- how to use -------- 
    //int scrollDelta = 0;
    //GGInputManager::GGInputMouseWheelDelta(&scrollDelta);

    //if (scrollDelta > 0) {
        //std::cout << "Scrolled up: " << scrollDelta << std::endl;
    //}
    //else if (scrollDelta < 0) {
        //std::cout << "Scrolled down: " << scrollDelta << std::endl;
    //}
}
/*double GGInputManager::GetMouseButtonHoldTime(int input) const {
    auto itnerator = buttonHoldTime.find(input);
    if (itnerator != buttonHoldTime.end()) {
        auto now = GGTime::SystemTimeManager::GetSystemTime();
        return std::chrono::duration<double>(now - itnerator->second).count();
    }
    return 0.0;
}*/

/*************************************************************************
  \brief
    Returns how long a mouse button has been held for.
  \param[in] input
    GLFW mouse button code
  \return
    returns time in seconds for the button beening held, 0.0 if not pressed
*************************************************************************/
double GGInputManager::GetMouseButtonHoldTime(int input) const {

    auto itnerator = buttonHoldTime.find(input);
    if (itnerator != buttonHoldTime.end()) {
        return glfwGetTime() - itnerator->second;
    }
    return 0.0;
}

/*************************************************************************
  \brief
    Retrieves the current mouse cursor position.
  \param[out] x
    X-coordinate of the mouse cursor
  \param[out] y
    Y-coordinate of the mouse cursor
*************************************************************************/
void GGInputManager::GetMousePosition(double& x, double& y) const {
    x = mouseX - static_cast<double>(imguiFramebufferOffset.x);
    y = mouseY - static_cast<double>(imguiFramebufferOffset.y);
}

/*************************************************************************
  \brief
    Handles key press and release events from GLFW.
  \param[in] window
    GLFW window pointer
  \param[in] key
    GLFW key code
  \param[in] scancode
    Platform-specific scancode
  \param[in] action
    GLFW_PRESS or GLFW_RELEASE
  \param[in] mods
    Modifier keys
*************************************************************************/
void GGInputManager::OnKeyEvent([[maybe_unused]] GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {

    if (action == GLFW_PRESS) {
        inputState[key] = true;
    }
    else if (action == GLFW_RELEASE) {
        inputState[key] = false;
    }
}

/*************************************************************************
  \brief
    Handles mouse button press and release events from GLFW.
  \param[in] window
    GLFW window pointer
  \param[in] button
    GLFW mouse button code
  \param[in] action
    GLFW_PRESS or GLFW_RELEASE
  \param[in] mods
    Modifier keys
*************************************************************************/
void GGInputManager::OnMouseButtonEvent([[maybe_unused]] GLFWwindow* window, int button, int action, [[maybe_unused]] int mods) {

    if (action == GLFW_PRESS) {
        inputState[button] = true;
        buttonHoldTime[button] = glfwGetTime();
        //isDragging[button] = false;
        //buttonHoldTime[button] = GGTime::SystemTimeManager::GetSystemTime();
    }
    else if (action == GLFW_RELEASE) {
        //double buttonHeldFor = glfwGetTime() - buttonHoldTime[button];
        //if (buttonHeldFor > 0.1) {
            //isDragging[button] = true;
            //holding
        //}
        //else {
            //clicking
        //}
        //buttonHoldTime[button] = 0.0;
        //isDragging[button] = false;
        inputState[button] = false;
        buttonHoldTime.erase(button);
    } 
}

/*************************************************************************
  \brief
    Handles mouse movement events.
  \param[in] window
    GLFW window pointer
  \param[in] xpos
    New X-coordinate of cursor
  \param[in] ypos
    New Y-coordinate of cursor
*************************************************************************/
void GGInputManager::OnMouseMoveEvent([[maybe_unused]] GLFWwindow* window, double xpos, double ypos) {

    mouseX = xpos;
    mouseY = ypos;
   // std::cout << "mouse x pos" << xpos << "y pos " << ypos << std::endl;
}

/*************************************************************************
  \brief
    Handles mouse scroll events.
  \param[in] window
    GLFW window pointer
  \param[in] xoffset
    Horizontal scroll amount
  \param[in] yoffset
    Vertical scroll amount
  \param[out] mouseWheelDelta
    save yoffset into mouseWheelDelta
*************************************************************************/
void GGInputManager::OnMouseScrollEvent([[maybe_unused]] GLFWwindow* window, double xoffset, double yoffset)
{
    if (yoffset > 0) {
        //std::cout << "scrolled up" << std::endl;
    }
    else {
        //std::cout << "scrolled down" << std::endl;
    }
    if (xoffset > 0) {
        //std::cout << "scrolled left" << std::endl;
    }
    else {
        //std::cout << "scrolled right" << std::endl;
    }
    mouseWheelDelta += yoffset;
}

/*************************************************************************
  \brief
    Updates the previous input state at the end of the frame.
*************************************************************************/
void GGInputManager::InputUpdate() {

    prevInputState = inputState;
}

/*************************************************************************
  \brief
    Main update function called once per frame.
  \details
    Updates input states and can be used for debugging tests
*************************************************************************/
void GGInputManager::Update()
{
    GGInputManager::InputUpdate();
    
    // debugging example
    //if (GGInputManager::GGIsCheckCurr(GGMouse_LEFT)) {
        //double holdTime = GetMouseButtonHoldTime(GLFW_MOUSE_BUTTON_LEFT);
        //std::cout << "A" << std::endl;
    //}
    
    //int scrollDelta = 0.0;
    //GGInputManager::GGInputMouseWheelDelta(&scrollDelta);

    //if (scrollDelta > 0) {
        //std::cout << "Scrolled up: " << scrollDelta << std::endl;
    //}
    //else if (scrollDelta < 0) {
        //std::cout << "Scrolled down: " << scrollDelta << std::endl;
    //}
}

/*************************************************************************
  \brief
    Converts a screen-space position (pixels, top-left origin) into
    world-space coordinates using the current camera matrix.
  \param[in] screenPos
    Mouse or GUI position in screen coordinates (0,0 = top-left).
  \return
    World-space position (X,Y) in your game world.
*************************************************************************/
glm::vec2 GGInputManager::ScreenToWorld2D(glm::vec2 screenPos) const
{
    const_cast<GGInputManager*>(this)->RecalculateInverseMatrix();

    glm::vec2 ndc = ScreenToNDC(screenPos);
    glm::vec4 ndc4(ndc, 0.0f, 1.0f); // Z=0 for 2D
    glm::vec4 world4 = m_invCameraMatrix * ndc4;

   
    return { world4.x, world4.y };
}

glm::vec2 GGInputManager::ScreenToWorld2DWithPerspective(
    glm::vec2 screenPos, GGMath::float32 perspectiveFactor) const
{
    const_cast<GGInputManager*>(this)->RecalculateInverseMatrix();
    glm::vec2 ndc = ScreenToNDC(screenPos);

    float clipY = ndc.y / (1.0f - ndc.y * perspectiveFactor);
    float w = 1.0f + clipY * perspectiveFactor;

    // Reconstruct clip space position
    glm::vec4 clipPos(ndc.x * w, clipY, 0.0f, w);

    glm::vec4 world4 = m_invCameraMatrix * clipPos;
    return { world4.x / world4.w, world4.y / world4.w };
}

/*************************************************************************
  \brief
    Converts a world-space position into screen-space coordinates.
  \param[in] worldPos
    Position in the game world.
  \return
    Screen-space coordinates (pixels). Useful for UI overlays or gizmos.
*************************************************************************/
glm::vec2 GGInputManager::WorldToScreen2D(glm::vec2 worldPos) const
{
    glm::vec4 clip = m_currentCameraMatrix * glm::vec4(worldPos, 0.0f, 1.0f);
    glm::vec2 ndc = { clip.x, clip.y }; // ignore Z for 2D
    return NDCToScreen(ndc);
}


/*************************************************************************
  \brief
    Gets the current mouse position in world-space.
  \return
    World-space (X,Y) under the cursor.
*************************************************************************/
glm::vec2 GGInputManager::GetMouseWorldPosition() const
{
    glm::vec2 mouseScreen(mouseX, mouseY);
    return ScreenToWorld2D(mouseScreen);
}

glm::vec2 GGInputManager::GetMouseWorldPositionWithPerspective(GGMath::float32 perspective) const
{
    glm::vec2 mouseScreen(mouseX, mouseY);
    return ScreenToWorld2DWithPerspective(mouseScreen, perspective);
}

/*************************************************************************
  \brief
    Calculates which object (if any) is under a given screen position by
    reading pixel data from the picking framebuffer.
  \param[in] screenPos
    Mouse click or hover position (screen-space).
  \return
    PickingResult containing EntityID, object type, or invalid if empty.
*************************************************************************/
GGPickingFramebuffer::PickingResult GGInputManager::PickObjectAtScreenPosition(glm::vec2 screenPos)
{

    if (!m_pickingFramebuffer) {
        std::cerr << "[GGInputManager] Picking framebuffer not initialized!\n";
        return GGPickingFramebuffer::PickingResult{};
    }

    // Step 1: Flip Y (ImGui top-left -> OpenGL bottom-left)
    screenPos.y = m_currentViewportSize.y - screenPos.y;

    // Step 2: Scale to framebuffer resolution
    float scaleX = float(m_pickingFramebuffer->getWidth()) / m_currentViewportSize.x;
    float scaleY = float(m_pickingFramebuffer->getHeight()) / m_currentViewportSize.y;

    unsigned int px = static_cast<unsigned int>(screenPos.x * scaleX);
    unsigned int py = static_cast<unsigned int>(screenPos.y * scaleY);

    // Step 3: Clamp to framebuffer bounds
    px = glm::clamp(px, 0u, m_pickingFramebuffer->getWidth() - 1);
    py = glm::clamp(py, 0u, m_pickingFramebuffer->getHeight() - 1);

    // Step 4: Read pixel
    auto result = m_pickingFramebuffer->ReadPixel(px, py);

    //if (result.IsValid()) {
    //    std::cout << "[GGInputManager] Picked Entity ID: " << result.EntityID
    //        << " at framebuffer coords (" << px << ", " << py << ")" << std::endl;
    //}
    //else {
    //    std::cout << "[GGInputManager] Clicked empty background at ("
    //        << px << ", " << py << ")" << std::endl;
    //}

    //std::cout << "Screen (" << screenPos.x << ", " << screenPos.y
    //    << "), scaled to (" << px << ", " << py << ") of framebuffer "
    //    << m_pickingFramebuffer->getWidth() << "x"
    //    << m_pickingFramebuffer->getHeight() << std::endl;

    return result;
}

/*************************************************************************
  \brief
    Convenience wrapper for picking using the current mouse location.
*************************************************************************/
GGPickingFramebuffer::PickingResult GGInputManager::PickObjectAtMouse()
{
    glm::vec2 mouseScreen(mouseX, mouseY);
    return PickObjectAtScreenPosition(mouseScreen);
}


/*************************************************************************
  \brief
    Updates the stored camera matrix used for coordinate conversions.
  \details
    Marks the inverse matrix as dirty so it can be recalculated lazily.
*************************************************************************/
void GGInputManager::UpdateCameraMatrix(const glm::mat4& cameraMatrix)
{
    m_currentCameraMatrix = cameraMatrix;
    m_cameraMatrixDirty = true;
}

/*************************************************************************
  \brief
    Stores the active viewport size so that screen <-> NDC conversion
    functions remain accurate.
*************************************************************************/
void GGInputManager::UpdateViewportSize(glm::vec2 viewportSize)
{
    m_currentViewportSize = viewportSize;
}

/*************************************************************************
  \brief
    Recomputes the inverse camera matrix only when needed.
*************************************************************************/
void GGInputManager::RecalculateInverseMatrix()
{
    if (m_cameraMatrixDirty) {
        m_invCameraMatrix = glm::inverse(m_currentCameraMatrix);
        m_cameraMatrixDirty = false;
    }
}

/*************************************************************************
  \brief
    Converts screen-space pixel coordinates to NDC (-1 to +1 range).
*************************************************************************/
glm::vec2 GGInputManager::ScreenToNDC(glm::vec2 screenPos) const
{
    int windowWidth, windowHeight;
    glfwGetWindowPos(Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetMainWindow().GetGLFWwindow(), &windowWidth, &windowHeight);

    screenPos -= glm::vec2(imguiFramebufferOffset.x, imguiFramebufferOffset.y);
    
    // Direct conversion: window coordinates NDC
    float ndcX = (2.0f * screenPos.x) / m_currentViewportSize.x - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y) / m_currentViewportSize.y;

    return glm::vec2(ndcX, ndcY);
}

/*************************************************************************
  \brief
    Converts normalized device coordinates back into screen pixels.
*************************************************************************/
glm::vec2 GGInputManager::NDCToScreen(glm::vec2 ndc) const
{
    
    float screenX = (ndc.x + 1.0f) * 0.5f * m_currentViewportSize.x;
    float screenY = (1.0f - ndc.y) * 0.5f * m_currentViewportSize.y;
    return { screenX, screenY };
}
