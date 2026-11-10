/*************************************************************************
  \file       GGWindow.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       03 October 2025
  \brief
    Implementation of GGWindow, a wrapper for GLFW window management.

  \details
    This file contains the function definitions for GGWindow, including:
    - Window creation and destruction
    - Window size and title management
    - FPS display updates in the title
    - Getter and setter for native GLFWwindow pointer
*************************************************************************/
#include <GGWindow.hpp>
#include <sstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

/*************************************************************************
  \brief Default constructor.
  \details
    Initializes a GGWindow object with default width, height, and name.
*************************************************************************/
GGWindow::GGWindow() = default;

/*************************************************************************
  \brief Constructs a GGWindow with specified size and name.
  \param width Initial window width in pixels.
  \param height Initial window height in pixels.
  \param name Initial window title.
*************************************************************************/
GGWindow::GGWindow(int width, int height, std::string const& name)
    : WindowWidth(width), WindowHeight(height), windowName(name) {
}

/*************************************************************************
  \brief Destructor.
  \details
    Destroys the GLFW window if it exists.
*************************************************************************/
GGWindow::~GGWindow() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
}

/*************************************************************************
  \brief Initializes the GGWindow object.
  \param width Window width in pixels.
  \param height Window height in pixels.
  \param name Window title.
  \details
    Sets the window size and name; actual GLFW window creation should
    be done separately.
*************************************************************************/
void GGWindow::Init(int width, int height, std::string const& name) {
    SetSize(width, height);
    SetName(name);
}

/*************************************************************************
  \brief Sets the window size.
  \param width New window width in pixels.
  \param height New window height in pixels.
  \details
    Updates internal width/height values and resizes the GLFW window if it exists.
*************************************************************************/
void GGWindow::SetSize(int width, int height) {
    WindowWidth = width;
    WindowHeight = height;
    if (window) {
        glfwSetWindowSize(window, width, height);
    }
}

/*************************************************************************
  \brief Updates the window title to display FPS.
  \param fps Frames per second to display.
  \details
    Combines the default window name with FPS information in the title.
*************************************************************************/
void GGWindow::setfps(int fps)
{
    std::stringstream FPS;
    FPS << fps;
    SetName(defaultname +"| fps:"+FPS.str() + "|");
}

/*************************************************************************
  \brief Sets the window title.
  \param name New window title.
  \details
    Updates both the internal title string and the GLFW window if it exists.
*************************************************************************/
void GGWindow::SetName(std::string const& name) {
    windowName = name;
    if (window) {
        glfwSetWindowTitle(window, windowName.c_str());
    }
}

/*************************************************************************
  \brief Sets the window mode (windowed, fullscreen, borderless).
  \param mode The desired WindowMode.
  \details
    Delegates to the GGWindowConfig to handle the mode transition.
*************************************************************************/
void GGWindow::SetWindowMode(WindowMode mode) {
    if (window) {
        windowConfig.SetWindowMode(window, mode);

        // Update internal dimensions to match current resolution
        Resolution res = windowConfig.GetCurrentResolution();
        WindowWidth = res.width;
        WindowHeight = res.height;
    }
}

/*************************************************************************
  \brief Toggles between windowed and fullscreen modes.
  \details
    Convenience function to switch between windowed and fullscreen.
*************************************************************************/
void GGWindow::ToggleFullscreen() {
    if (window) {
        windowConfig.ToggleFullscreen(window);

        // Update internal dimensions
        Resolution res = windowConfig.GetCurrentResolution();
        WindowWidth = res.width;
        WindowHeight = res.height;
    }
}

/*************************************************************************
  \brief Gets the current window mode.
  \return The current WindowMode.
*************************************************************************/
WindowMode GGWindow::GetWindowMode() const {
    return windowConfig.GetWindowMode();
}

/*************************************************************************
  \brief Checks if the window is in fullscreen mode.
  \return True if fullscreen, false if windowed.
*************************************************************************/
bool GGWindow::IsFullscreen() const {
    return windowConfig.IsFullscreen();
}

/*************************************************************************
  \brief Sets the resolution using width and height.
  \param width Desired width in pixels.
  \param height Desired height in pixels.
  \details
    Changes the resolution while maintaining the current window mode.
*************************************************************************/
void GGWindow::SetResolution(int width, int height) {
    if (window) {
        windowConfig.SetResolution(window, width, height);

        // Update internal dimensions
        WindowWidth = width;
        WindowHeight = height;
    }
}

/*************************************************************************
  \brief Sets the resolution using a Resolution object.
  \param res The desired Resolution.
*************************************************************************/
void GGWindow::SetResolution(const Resolution& res) {
    if (window) {
        windowConfig.SetResolution(window, res);

        // Update internal dimensions
        WindowWidth = res.width;
        WindowHeight = res.height;
    }
}

/*************************************************************************
  \brief Sets the window to use the best available resolution.
  \details
    Automatically detects and applies the native monitor resolution.
*************************************************************************/
void GGWindow::SetBestResolution() {
    if (window) {
        windowConfig.SetBestResolution(window);

        // Update internal dimensions
        Resolution res = windowConfig.GetCurrentResolution();
        WindowWidth = res.width;
        WindowHeight = res.height;
    }
}

/*************************************************************************
  \brief Gets the best available resolution for this system.
  \return The optimal Resolution for the primary monitor.
*************************************************************************/
Resolution GGWindow::GetBestResolution() const {
    return windowConfig.GetBestResolution();
}

/*************************************************************************
  \brief Gets the current resolution.
  \return The current Resolution.
*************************************************************************/
Resolution GGWindow::GetCurrentResolution() const {
    return windowConfig.GetCurrentResolution();
}

/*************************************************************************
  \brief Gets all available resolutions for the primary monitor.
  \return Vector of available Resolution objects.
*************************************************************************/
std::vector<Resolution> GGWindow::GetAvailableResolutions() const {
    return windowConfig.GetAvailableResolutions();
}

/*************************************************************************
  \brief Checks whether the window is currently resizable.
  \return True if the window can be resized by the user, false otherwise.
*************************************************************************/
void GGWindow::SetResizable(bool resizable) {
    if (window) {
        glfwSetWindowAttrib(window, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
    }
}


/*************************************************************************
  \brief Checks whether the window is currently resizable.
  \return True if the window can be resized by the user, false otherwise.
*************************************************************************/
bool GGWindow::IsResizable() const {
    if (window) {
        return glfwGetWindowAttrib(window, GLFW_RESIZABLE) == GLFW_TRUE;
    }
    return false;
}


//getters
int GGWindow::GetWindowWidth()const { return WindowWidth; }
int GGWindow::GetWindowHeight()const { return WindowHeight; }
GLFWwindow* GGWindow::GetGLFWwindow()const { return window; }

std::string GGWindow::GetName()const { return windowName; }
void GGWindow::SetGLFWwindow(GLFWwindow* win) { window = win; }
