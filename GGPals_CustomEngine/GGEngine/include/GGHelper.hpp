/*************************************************************************
  \file       GGHelper.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email:  a.patchamuthu@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Helper class for GLFW window initialization and input/event handling.
  \details
    GGHelper provides static utility functions for initializing a GLFW window,
    registering input callbacks (keyboard, mouse, scroll, drop), and managing
    window modes. It forwards all input events to the engine's input manager
    and handles engine-specific setup like OpenGL/GLEW initialization.
*************************************************************************/
#pragma once
#include "GGWindow.hpp"

class GGHelper {
public:


    /*************************************************************************
      \brief
        Initializes a GLFW window with the specified dimensions and name, sets
        up an OpenGL context, and initializes GLEW.
      \param[in,out] mainWindow
        Reference to a GGWindow object to initialize.
      \param[in] width
        Desired window width in pixels.
      \param[in] height
        Desired window height in pixels.
      \param[in] name
        Window title to display.
      \return
        true if initialization succeeded; false otherwise.
      \details
        This function sets up GLFW, creates a window, initializes the OpenGL
        context, initializes GLEW, prints version information, and sets up
        engine-specific event callbacks.
    *************************************************************************/
	static bool InitWindow(GGWindow& mainWindow, int width, int height, const std::string& name);

    /*************************************************************************
      \brief
        Callback for framebuffer size changes.
      \param[in] ptr_win
        Pointer to the GLFW window whose framebuffer size changed.
      \param[in] width
        New width of the framebuffer in pixels.
      \param[in] height
        New height of the framebuffer in pixels.
      \details
        Intended to handle window resize events. Currently not implemented.
    *************************************************************************/
	static void FrameBufferSize_callback(GLFWwindow* ptr_win, int width, int height);

    /*************************************************************************
      \brief
        Callback for key press and release events.
      \param[in] pwin
        Pointer to the GLFW window receiving the key event.
      \param[in] key
        the keyboard key that was pressed or released.
      \param[in] scancode
        Platform-specific scan code of the key.
      \param[in] action
        GLFW_PRESS, GLFW_RELEASE, or GLFW_REPEAT.
      \param[in] mod
        Bitfield describing which modifier keys were held down.
      \details
        Pressing ESCAPE will close the window. All key events are forwarded to the
        engine's input manager.
    *************************************************************************/
	static void Key_callback(GLFWwindow* pwin, int key, int scancode, int action, int mod);
	
    /*************************************************************************
     \brief
       Callback for mouse button press and release events.
     \param[in] pwin
       Pointer to the GLFW window receiving the mouse event.
     \param[in] button
       The mouse button that was pressed or released.
     \param[in] action
       GLFW_PRESS or GLFW_RELEASE.
     \param[in] mod
       Bitfield describing which modifier keys were hold down.
     \details
       All mouse button events are forwarded to the engine's input manager.
   *************************************************************************/
    static void MouseButton_callback(GLFWwindow* pwin, int button, int action, int mod);
	
    /*************************************************************************
      \brief
        Callback for mouse mouse movement events.
      \param[in] pWindow
        Pointer to the GLFW window receiving the mouse movement event.
      \param[in] xpos
        New X-coordinate of the mouse cursor.
      \param[in] ypos
        New Y-coordinate of the mouse cursor.
      \details
        All mouse movement events are forwarded to the engine's input manager.
    *************************************************************************/
    static void MousePostion_callback(GLFWwindow* pwin, double xpos, double ypos);

    /*************************************************************************
      \brief
        Callback for mouse scroll events.
      \param[in] pwin
        Pointer to the GLFW window receiving the scroll event.
      \param[in] x_offset
        Horizontal scroll delta.
      \param[in] y_offset
        Vertical scroll delta.
      \details
        All scroll events are forwarded to the engine's input manager.
    *************************************************************************/
	static void MouseSrcoll_callback(GLFWwindow* pwin, double x_offset, double y_offset);

    static void Drop_callback(GLFWwindow* window, int count, const char** paths);

    /*************************************************************************
      \brief
        Registers GLFW event callbacks for the specified window.
      \param[in,out] win
        GGWindow to which event callbacks will be attached.
      \details
        This function sets up the following GLFW callbacks:
        - Framebuffers resize
        - Key input
        - Mouse button input
        - Mouse cursor movement
        -mouse scroll
    *************************************************************************/
    static void setup_event_callbacks(GGWindow& win);


    // Window mode management (call AFTER InitWindow)
   static void SetInitialWindowMode(GGWindow& mainWindow, WindowMode mode, bool useBestResolution = false);

private:

	static GGWindow* _Swindow;

};
