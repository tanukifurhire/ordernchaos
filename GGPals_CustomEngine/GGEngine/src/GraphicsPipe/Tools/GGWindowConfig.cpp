/*************************************************************************
  \file       GGWindowConfig.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       20 November 2025
  \brief
    Implementation of GGWindowConfig for window mode and resolution management.
  \details
    Provides functionality for switching between windowed/fullscreen modes,
    managing resolutions, and querying available display modes.
*************************************************************************/
#include <GGWindowConfig.hpp>
#include <GLFW/glfw3.h>
#include <sstream>
#include <algorithm>
#include <iostream>

std::string Resolution::toString() const {
    std::stringstream ss;
    ss << width << "x" << height << " at " << refreshRate << " Hz";
    return ss.str();
}
float Resolution::getAspectRatio() const {
    return height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 0.0f;
}

/*************************************************************************
  \brief Constructor.
  \details
    Initializes with default windowed mode at 1920x1080.
*************************************************************************/
GGWindowConfig::GGWindowConfig()
    : currentMode(WindowMode::WINDOWED),
    currentResolution(1600, 900, 60),
    windowedWidth(1600),
    windowedHeight(900),
    windowedPosX(500),
    windowedPosY(500) {
}

/*************************************************************************
  \brief Gets the primary monitor.
  \return Pointer to the primary GLFWmonitor.
*************************************************************************/
GLFWmonitor* GGWindowConfig::GetPrimaryMonitor() const {
    return glfwGetPrimaryMonitor();
}

/*************************************************************************
  \brief Gets the video mode of a monitor.
  \param monitor The monitor to query.
  \return Pointer to the GLFWvidmode structure.
*************************************************************************/
const GLFWvidmode* GGWindowConfig::GetMonitorVideoMode(GLFWmonitor* monitor) const {
    return glfwGetVideoMode(monitor);
}

/*************************************************************************
  \brief Retrieves all available resolutions for the primary monitor.
  \return Vector of Resolution objects.
  \details
    Queries GLFW for all video modes and creates unique Resolution entries.
*************************************************************************/
std::vector<Resolution> GGWindowConfig::GetAvailableResolutions() const {
    std::vector<Resolution> resolutions;
    GLFWmonitor* monitor = GetPrimaryMonitor();

    if (!monitor) return resolutions;

    int count;
    const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);

    for (int i = 0; i < count; ++i) {
        Resolution res(modes[i].width, modes[i].height, modes[i].refreshRate);

        // Avoid duplicates (same width/height, different refresh rates)
        auto it = std::find(resolutions.begin(), resolutions.end(), res);
        if (it == resolutions.end()) {
            resolutions.push_back(res);
        }
        else if (res.refreshRate > it->refreshRate) {
            // Keep the higher refresh rate
            it->refreshRate = res.refreshRate;
        }
    }

    // Sort by resolution (largest first)
    std::sort(resolutions.begin(), resolutions.end(),
        [](const Resolution& a, const Resolution& b) {
            if (a.width != b.width) return a.width > b.width;
            return a.height > b.height;
        });

    return resolutions;
}

/*************************************************************************
  \brief Saves the current windowed mode state.
  \param window The GLFW window.
  \details
    Stores position and size for restoration when exiting fullscreen.
*************************************************************************/
void GGWindowConfig::SaveWindowedState(GLFWwindow* window) {
    if (!window) return;

  

    glfwGetWindowPos(window, &windowedPosX, &windowedPosY);
   // glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

    std::cout << "Saved windowed state: pos("
        << windowedPosX << "," << windowedPosY
        << ") size(" << windowedWidth << "x" << windowedHeight
        << ")\n";

}

/*************************************************************************
  \brief Restores the saved windowed mode state.
  \param window The GLFW window.
*************************************************************************/
void GGWindowConfig::RestoreWindowedState(GLFWwindow* window) {
    if (!window) return;
    // Put the title bar back; borderless mode takes it away.
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
    std::cout << "Restoring windowed: pos("
        << windowedPosX << "," << windowedPosY
        << ") size(" << windowedWidth << "x" << windowedHeight
        << ")\n";
    glfwSetWindowMonitor(window, nullptr,
        windowedPosX, windowedPosY,
        windowedWidth, windowedHeight, GLFW_DONT_CARE);
}

/*************************************************************************
  \brief Applies true fullscreen mode.
  \param window The GLFW window.
  \details
    Uses exclusive fullscreen with the current resolution's refresh rate.
*************************************************************************/
void GGWindowConfig::ApplyFullscreen(GLFWwindow* window) {
    GLFWmonitor* monitor = GetPrimaryMonitor();
    if (!monitor) return;

    glfwSetWindowMonitor(window, monitor, 0, 0,
        currentResolution.width,
        currentResolution.height,
        currentResolution.refreshRate);
}

/*************************************************************************
  \brief Applies borderless windowed mode.
  \param window The GLFW window.
  \details
    Creates a borderless window that covers the entire monitor.
*************************************************************************/
void GGWindowConfig::ApplyBorderlessWindowed(GLFWwindow* window) {
    GLFWmonitor* monitor = GetPrimaryMonitor();
    if (!monitor) return;

    const GLFWvidmode* mode = GetMonitorVideoMode(monitor);
    if (!mode) return;

    // Drop the title bar and border. Without this the window is merely
    // monitor-sized, so the decorations push the client area off-screen.
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);

    // The monitor's origin on the virtual desktop, not (0,0). On a multi-monitor
    // setup the primary is not always the top-left one, and hardcoding 0,0 drops
    // the window onto whichever monitor happens to own that coordinate.
    int monitorX = 0, monitorY = 0;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    // Deliberately passing nullptr for the monitor: that is what makes this a
    // window rather than an exclusive-fullscreen surface, so no display mode is
    // set and Windows never rearranges everybody else's windows.
    glfwSetWindowMonitor(window, nullptr, monitorX, monitorY,
        mode->width, mode->height, GLFW_DONT_CARE);

    // Update current resolution to match monitor
    currentResolution = Resolution(mode->width, mode->height, mode->refreshRate);
}

/*************************************************************************
  \brief Applies windowed mode.
  \param window The GLFW window.
*************************************************************************/
void GGWindowConfig::ApplyWindowed(GLFWwindow* window) {
    RestoreWindowedState(window);
}

/*************************************************************************
  \brief Sets the window mode (windowed, fullscreen, borderless).
  \param window The GLFW window.
  \param mode The desired WindowMode.
*************************************************************************/
void GGWindowConfig::SetWindowMode(GLFWwindow* window, WindowMode mode) {

    if (!window || currentMode == mode) return;

    const char* modeNames[] = { "Windowed", "Fullscreen", "Borderless" };

    
    if (currentMode == WindowMode::WINDOWED) {
        
        SaveWindowedState(window);
        std::cout << "Saved windowed state: pos("
            << windowedPosX << "," << windowedPosY
            << ") size(" << windowedWidth << "x" << windowedHeight
            << ")\n";
    }

    std::cout << "Switching from " << modeNames[static_cast<int>(currentMode)]
        << " to " << modeNames[static_cast<int>(mode)] << "\n";
    isChangingMode = true;
    // Apply new mode
    switch (mode) {
    case WindowMode::WINDOWED:
        RestoreWindowedState(window);
        //currentResolution = Resolution(windowedWidth, windowedHeight, currentResolution.refreshRate);
        break;

    case WindowMode::FULLSCREEN:
        ApplyFullscreen(window);
        break;

    case WindowMode::BORDERLESS_WINDOWED:
        ApplyBorderlessWindowed(window);
        break;
    }

    // Update mode after applying
    currentMode = mode;
    isChangingMode = true;
    std::cout << "Mode change complete: now in " << modeNames[static_cast<int>(currentMode)] << "\n";
}

/*************************************************************************
  \brief Toggles between windowed and fullscreen modes.
  \param window The GLFW window.
*************************************************************************/
void GGWindowConfig::ToggleFullscreen(GLFWwindow* window) {
    // check if current mode windowed if yes set full screen 
    // else
    if (currentMode == WindowMode::WINDOWED) {
        SetWindowMode(window, WindowMode::BORDERLESS_WINDOWED);
    }
    else {
        SetWindowMode(window, WindowMode::WINDOWED);
    }
}

/*************************************************************************
  \brief Sets and locks a specific aspect ratio.
  \param aspectRatio The aspect ratio to lock (e.g., 16.0f/9.0f for 16:9).
  \details
    Once set, all resolution changes will only consider resolutions
    matching this aspect ratio. Call ClearAspectRatio() to unlock.
*************************************************************************/
void GGWindowConfig::SetAspectRatio(float aspectRatio) {
    if (aspectRatio <= 0.0f) {
        std::cout << "Invalid aspect ratio: " << aspectRatio << std::endl;
        return;
    }

    aspectRatioLocked = true;
    lockedAspectRatio = aspectRatio;
    std::cout << "Aspect ratio locked to: " << aspectRatio << std::endl;
}

/*************************************************************************
  \brief Removes the aspect ratio lock.
  \details
    After calling this, resolution changes will no longer be restricted
    to a specific aspect ratio.
*************************************************************************/
void GGWindowConfig::ClearAspectRatio() {
    aspectRatioLocked = false;
    lockedAspectRatio = 0.0f;
    std::cout << "Aspect ratio lock removed" << std::endl;
}
/*************************************************************************
  \brief Sets the resolution (width, height, and refresh rate).
  \param window The GLFW window.
  \param res The desired Resolution.
*************************************************************************/
void GGWindowConfig::SetResolution(GLFWwindow* window, const Resolution& res) {
    if (!window) return;

    // Find closest lower resolution if the requested one is not available
    Resolution finalRes = res;
    auto availableRes = GetAvailableResolutions();
    auto it = std::find(availableRes.begin(), availableRes.end(), res);
    if (it == availableRes.end()) {
        finalRes = GetClosestLowerResolution(res);
        std::cout << "Requested resolution not available. Using closest lower: "
            << finalRes.toString() << std::endl;
    }

    currentResolution = finalRes;

    if (currentMode == WindowMode::WINDOWED) {
        // Update windowed size
        windowedWidth = finalRes.width;
        windowedHeight = finalRes.height; 
        glfwSetWindowSize(window, finalRes.width, finalRes.height);
    }
    else if (currentMode == WindowMode::FULLSCREEN) {
        // Re-apply fullscreen with new resolution
       /* SetBestResolution(window);
        ApplyWindowed(window);*/
        ApplyFullscreen(window);

        
    }
}

/*************************************************************************
  \brief Sets the resolution (width and height only).
  \param window The GLFW window.
  \param width Desired width in pixels.
  \param height Desired height in pixels.
  \details
    Keeps the current refresh rate.
*************************************************************************/
void GGWindowConfig::SetResolution(GLFWwindow* window, int width, int height) {
    windowedWidth = width;   
    windowedHeight = height;

    Resolution res(width, height, currentResolution.refreshRate);
    SetResolution(window, res);
}

/*************************************************************************
\brief Gets the best resolution for the primary monitor.
\return The highest resolution with the highest refresh rate.
\details
Returns the native monitor resolution with the best refresh rate.
This is typically what you want for fullscreen gaming.
*************************************************************************/
Resolution GGWindowConfig::GetBestResolution() const {
    GLFWmonitor* monitor = GetPrimaryMonitor();
    if (!monitor) {
        return Resolution(1600, 900, 60); // Fallback
    }

    const GLFWvidmode* mode = GetMonitorVideoMode(monitor);
    if (!mode) {
        return Resolution(1600, 900, 60); // Fallback
    }
    // If aspect ratio is locked, find best resolution matching that aspect ratio
    if (aspectRatioLocked) {
        auto availableRes = GetAvailableResolutions();
        Resolution best(0, 0, 0);
        int maxPixels = 0;

        for (const auto& res : availableRes) {
            float resAspect = res.getAspectRatio();

            // Check if aspect ratio matches (with small tolerance)
            if (std::abs(resAspect - lockedAspectRatio) < 0.01f) {
                int pixels = res.width * res.height;

                // Keep the highest resolution with matching aspect ratio
                if (pixels > maxPixels ||
                    (pixels == maxPixels && res.refreshRate > best.refreshRate)) {
                    maxPixels = pixels;
                    best = res;
                }
            }
        }

        // If we found a matching resolution, use it
        if (best.width > 0) {
            std::cout << "Best resolution with locked aspect ratio "
                << lockedAspectRatio << ": " << best.toString() << std::endl;
            return best;
        }

        // If no matching aspect ratio found, fall back to native resolution
        std::cout << "Warning: No resolution found matching locked aspect ratio "
            << lockedAspectRatio << ". Using native resolution." << std::endl;
    }


    // Native monitor resolution is the best
    return Resolution(mode->width, mode->height, mode->refreshRate);
}

/*************************************************************************
  \brief Sets the window to use the best available resolution.
  \param window The GLFW window.
  \details
    Automatically detects and applies the native monitor resolution
    with the highest refresh rate.
*************************************************************************/
void GGWindowConfig::SetBestResolution(GLFWwindow* window) {
    Resolution bestRes = GetBestResolution();
    SetResolution(window, bestRes);

    std::cout << "Best resolution applied: " << bestRes.toString() << std::endl;
}
/*************************************************************************
  \brief Finds the closest lower available resolution.
  \param desired The desired resolution.
  \return The closest lower available resolution, or the lowest one if none are smaller.
*************************************************************************/
Resolution GGWindowConfig::GetClosestLowerResolution(const Resolution& desired) const {
    auto availableRes = GetAvailableResolutions();

    if (availableRes.empty()) {
        return Resolution(1600, 900, 60);
    }

    // Use locked aspect ratio if set, otherwise use desired resolution's aspect ratio
    float targetAspect = aspectRatioLocked ? lockedAspectRatio : desired.getAspectRatio();
    int targetPixels = desired.width * desired.height;

    Resolution closest = availableRes[0];
    int minDiff = std::abs((closest.width * closest.height) - targetPixels);
    bool foundMatchingAspect = false;

    // First, try to find resolutions with matching aspect ratio
    for (const auto& res : availableRes) {
        float resAspect = res.getAspectRatio();

        // Check if aspect ratio matches (with small tolerance for floating point)
        if (std::abs(resAspect - targetAspect) < 0.01f) {
            int pixels = res.width * res.height;
            int diff = std::abs(pixels - targetPixels);

            if (!foundMatchingAspect || diff < minDiff) {
                minDiff = diff;
                closest = res;
                foundMatchingAspect = true;
            }
        }
    }

    // If no matching aspect ratio found, just find closest by pixel count
    if (!foundMatchingAspect) {
        std::cout << "Warning: No resolutions found matching aspect ratio "
            << targetAspect << ". Using closest by size." << std::endl;
        for (const auto& res : availableRes) {
            int pixels = res.width * res.height;
            int diff = std::abs(pixels - targetPixels);

            if (diff < minDiff) {
                minDiff = diff;
                closest = res;
            }
        }
    }

    return closest;
}