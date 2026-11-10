/*************************************************************************
  \file       GGInputManager.hpp
  \author     Tok Duan Jie Nelson, Patchamuthu Abhishek Akilandeswari
  \par        Email: t.duanjienelson@digipen.edu, a.patchamuthu@digipen.edu
  \date       20 September 2025
  \version    1.0
  \contribute Nelson(80%), Ahbishek(20% Base template)
  \brief
    Handles mouse and keyboard input for the game engine.
  \details
    Tracks current and previous input states, mouse position, and button hold times.
    Provides functions for checking if keys or mouse buttons are pressed, triggered, or held.
  \dependencies
    <unordered_map>, <GLFW/glfw3.h>, <GGSystemInterface.hpp>, GGInputCodes.hpp
  \see
  \history
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <GGSystemInterface.hpp>
#include "GGInputCodes.hpp"
#include <GGPickingFramebuffer.hpp>
#include <memory> 
#include <GGMath/Vector2.hpp>

//#include <chrono>


class GGInputManager : public SystemManagerInterface {
public:
    /*************************************************************************
      \brief
        Initializes the input manager.
      \return
        Always returns true (placeholder for future initialization logic)
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief
        Checks if an input (key or mouse button) is currently held down.
      \param[int] input
        GLFW key or mouse button code
      \return
        returns true if input is currently pressed, else false
    *************************************************************************/
    bool GGInputCheckCurr(int input) const;                                                 // check if Input is pressed (holding)

    /*************************************************************************
      \brief
        Checks if an input (key or mouse button) was triggered this frame.
      \param[in] input
        GLFW key or mouse button code
      \return
        returns true if input was pressed this frame with false in the previous frame
    *************************************************************************/
    bool GGInputCheckTriggered(int input) const;                                            // check if Input been triggered

    /*************************************************************************
      \brief
        Checks if an input (key or mouse button) was released this frame.
      \param[in] input
        GLFW key or mouse button code.
      \return
        True if input was released this frame.
    *************************************************************************/
    bool GGInputCheckReleased(int input) const;

    /*************************************************************************
      \brief
        Updates the previous input state at the end of the frame.
    *************************************************************************/
    void InputUpdate();                                                                     // to update prevInputState and inputState;

    /*************************************************************************
      \brief
        Returns how long a mouse button has been held for.
      \param[in] input
        GLFW mouse button code
      \return
        returns time in seconds for the button beening held, 0.0 if not pressed
    *************************************************************************/
    double GetMouseButtonHoldTime(int input) const;

    /*************************************************************************
      \brief
        Main update function called once per frame.
      \details
        Updates input states and can be used for debugging tests
    *************************************************************************/
    void Update() override;
    
    /*************************************************************************
      \brief
        Retrieves the current mouse cursor position.
      \param[out] x
        X-coordinate of the mouse cursor
      \param[out] y
        Y-coordinate of the mouse cursor
    *************************************************************************/
    void GetMousePosition(double& x, double& y) const;                                      // to find mouse position in the game window

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
    void OnKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods);       // key input callback function

    /*************************************************************************
      \brief
        Handles mouse scroll events.
      \param[in] window
        GLFW window pointer
      \param[in] xoffset
        Horizontal scroll amount
      \param[in] yoffset
        Vertical scroll amount
    *************************************************************************/
    void OnMouseButtonEvent(GLFWwindow* window, int button, int action, int mods);          // mouse input callback function

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
    void OnMouseMoveEvent(GLFWwindow* window, double xpos, double ypos);

    /*************************************************************************
      \brief
        Handles mouse scroll events.
      \param[in] window
        GLFW window pointer
      \param[in] xoffset
        Horizontal scroll amount
      \param[in] yoffset
        Vertical scroll amount
    *************************************************************************/
    void OnMouseScrollEvent(GLFWwindow* window, double xoffset, double yoffset);

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
    void GGInputMouseWheelDelta(int* pDelta);

private:
    std::unordered_map<int, double> buttonHoldTime;                                         // to track the timing of holding (clicking vs holding logic)
    std::unordered_map<int,bool> isDragging;                                                // use as a logic for holding/dragging interectable objects
    //std::unordered_map<int, std::chrono::steady_clock::time_point> buttonHoldTime;

    //std::unordered_map<int, bool> keyState;
    //std::unordered_map<int, bool> prevKeyState;
    //std::unordered_map<int, bool> mouseState;
    //std::unordered_map<int, bool> prevMouseState;
    std::unordered_map<int, bool> inputState;
    std::unordered_map<int, bool> prevInputState;                                           // use as a check if previous input state running in the game loop is true or false, logic used for isKeyTriggered() function
    double mouseX = 0.0;
    double mouseY = 0.0;
    double mouseWheelDelta = 0.0;
    double scrollOffset_X = 0.0;
    double scrollOffset_y = 0.0;

    GGMath::Vector2 imguiFramebufferOffset{};
    GGMath::Vector2 imguiFramebufferScale{};

     private:
         // New 2D coordinate transformation members
         glm::mat4 m_currentCameraMatrix{ 1.0f }; // Use 4x4 for correct projection + view
         glm::mat4 m_invCameraMatrix{ 1.0f };
         glm::vec2 m_currentViewportSize{ 1600.0f, 900.0f };
         mutable bool m_cameraMatrixDirty = true;

         /*************************************************************************
           \brief
             Retrieves the vertical mouse wheel scroll delta since the last update.
           \param[out] pDelta
             Pointer to an integer that receives the scroll delta.
         *************************************************************************/
         GGPickingFramebuffer* m_pickingFramebuffer = nullptr;
    public:
        /*************************************************************************
          \brief
            Converts a screen-space position to world-space coordinates.
          \param[in] screenPos
            Screen-space position.
          \return
            Corresponding world-space position.
        *************************************************************************/
        glm::vec2 ScreenToWorld2D(glm::vec2 screenPos) const;
        
        /*************************************************************************
          \brief
            Converts a screen-space position to world-space coordinates with perspective scaling.
          \param[in] screenPos
            Screen-space position.
          \param[in] perspective
            Perspective scale factor.
          \return
            Corresponding world-space position.
        *************************************************************************/
        glm::vec2 ScreenToWorld2DWithPerspective(
            glm::vec2 screenPos, GGMath::float32 perspective) const;
       
        /*************************************************************************
          \brief
            Converts a world-space position to screen-space coordinates.
          \param[in] worldPos
            World-space position.
          \return
            Corresponding screen-space position.
        *************************************************************************/
        glm::vec2 WorldToScreen2D(glm::vec2 worldPos) const;
        
        /*************************************************************************
          \brief
            Retrieves the mouse position in world coordinates.
          \return
            Mouse world-space position.
        *************************************************************************/
        glm::vec2 GetMouseWorldPosition() const;

        /*************************************************************************
          \brief
            Retrieves the mouse position in world coordinates with perspective scaling.
          \param[in] perspective
            Perspective scale factor.
          \return
            Mouse world-space position.
        *************************************************************************/
        glm::vec2 GetMouseWorldPositionWithPerspective(GGMath::float32 perspective) const;

        /*************************************************************************
          \brief
            Picks an object at the given screen-space position.
          \param[in] screenPos
            Screen-space position.
          \return
            Picking result.
        *************************************************************************/
        GGPickingFramebuffer::PickingResult PickObjectAtScreenPosition(glm::vec2 screenPos);
        
        /*************************************************************************
          \brief
            Picks an object at the current mouse position.
          \return
            Picking result.
        *************************************************************************/
        GGPickingFramebuffer::PickingResult PickObjectAtMouse();

        /*************************************************************************
          \brief
            Updates the camera transformation matrix.
          \param[in] cameraMatrix
            4x4 camera matrix to apply.
        *************************************************************************/
        void UpdateCameraMatrix(const glm::mat4& cameraMatrix);

        /*************************************************************************
          \brief
            Updates the viewport size used for screen-to-world transformations.
          \param[in] viewportSize
            New viewport dimensions.
        *************************************************************************/
        void UpdateViewportSize(glm::vec2 viewportSize);

        /*************************************************************************
          \brief
            Assigns the engine's picking framebuffer for object picking.
          \param[in] framebuffer
            Pointer to the picking framebuffer.
        *************************************************************************/
        void SetPickingFramebuffer(GGPickingFramebuffer* framebuffer)
        {
            // Reset unique_ptr to point to external framebuffer but do NOT delete it
            m_pickingFramebuffer = framebuffer;
        }

        /*************************************************************************
          \brief
            Sets ImGui framebuffer offset for 2D transformations.
          \param[in] offset
            Offset vector.
        *************************************************************************/
        void SetFramebufferOffset(GGMath::Vector2 const& offset)
        {
            imguiFramebufferOffset = offset;
        }

        /*************************************************************************
          \brief
            Sets ImGui framebuffer scaling for 2D transformations.
          \param[in] scale
            Scale vector.
        *************************************************************************/
        void SetFramebufferScale(GGMath::Vector2 const& scale)
        {
            imguiFramebufferScale = scale;
        }

    private:
        void RecalculateInverseMatrix();
        glm::vec2 ScreenToNDC(glm::vec2 screenPos) const;
        glm::vec2 NDCToScreen(glm::vec2 ndc) const;
   
};