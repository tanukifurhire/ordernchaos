/*************************************************************************
  \file       GGWindowConfig.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       20 November 2025
  \brief
    Configuration class for managing window display modes and resolutions.
  \details
    Handles fullscreen/windowed mode transitions, resolution management,
    and available display mode querying.
*************************************************************************/
#pragma once
#include <vector>
#include <string>

struct GLFWwindow;
struct GLFWmonitor;
struct GLFWvidmode;

struct Resolution {
    int width;
    int height;
    int refreshRate;

    Resolution(int w = 1600, int h = 900, int refresh = 60)
        : width(w), height(h), refreshRate(refresh) {
    }

    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }

    std::string toString() const;
    float getAspectRatio() const;
};

enum class WindowMode {
    WINDOWED,
    FULLSCREEN,
    BORDERLESS_WINDOWED
};

class GGWindowConfig {
public:
    GGWindowConfig();

    /*************************************************************************
      \brief Sets the window mode (windowed, fullscreen, borderless).
      \param window The GLFW window.
      \param mode The desired WindowMode.
    *************************************************************************/
    void SetWindowMode(GLFWwindow* window, WindowMode mode);

    /*************************************************************************
      \brief Toggles between windowed and fullscreen modes.
      \param window The GLFW window.
    *************************************************************************/
    void ToggleFullscreen(GLFWwindow* window);

    /*************************************************************************
      \brief Sets and locks a specific aspect ratio.
      \param aspectRatio The aspect ratio to lock (e.g., 16.0f/9.0f for 16:9).
      \details
        Once set, all resolution changes will only consider resolutions
        matching this aspect ratio. Call ClearAspectRatio() to unlock.
    *************************************************************************/
    void SetAspectRatio(float aspectRatio);

    /*************************************************************************
      \brief Removes the aspect ratio lock.
      \details
        After calling this, resolution changes will no longer be restricted
        to a specific aspect ratio.
    *************************************************************************/
    void ClearAspectRatio();
    float GetLockedAspectRatio() const { return lockedAspectRatio; }
    bool IsAspectRatioLocked() const { return aspectRatioLocked; }


    /*************************************************************************
      \brief Sets the resolution (width, height, and refresh rate).
      \param window The GLFW window.
      \param res The desired Resolution.
    *************************************************************************/
    void SetResolution(GLFWwindow* window, const Resolution& res);
    void SetResolution(GLFWwindow* window, int width, int height);

    /*************************************************************************
      \brief Sets the window to use the best available resolution.
      \param window The GLFW window.
      \details
        Automatically detects and applies the native monitor resolution
        with the highest refresh rate.
    *************************************************************************/
    void SetBestResolution(GLFWwindow* window);

    /*************************************************************************
      \brief Finds the closest lower available resolution.
      \param desired The desired resolution.
      \return The closest lower available resolution, or the lowest one if none are smaller.
    *************************************************************************/
    Resolution GetClosestLowerResolution(const Resolution& desired) const;

    /*************************************************************************
      \brief Retrieves all available resolutions for the primary monitor.
      \return Vector of Resolution objects.
      \details
        Queries GLFW for all video modes and creates unique Resolution entries.
    *************************************************************************/
    std::vector<Resolution> GetAvailableResolutions() const;

    Resolution GetCurrentResolution() const { return currentResolution; }

    /*************************************************************************
    \brief Gets the best resolution for the primary monitor.
    \return The highest resolution with the highest refresh rate.
    \details
    Returns the native monitor resolution with the best refresh rate.
    This is typically what you want for fullscreen gaming.
    *************************************************************************/
    Resolution GetBestResolution() const;

    /*************************************************************************
      \brief Gets the primary monitor.
      \return Pointer to the primary GLFWmonitor.
    *************************************************************************/
    GLFWmonitor* GetPrimaryMonitor() const;

    /*************************************************************************
      \brief Gets the video mode of a monitor.
      \param monitor The monitor to query.
      \return Pointer to the GLFWvidmode structure.
    *************************************************************************/
    const GLFWvidmode* GetMonitorVideoMode(GLFWmonitor* monitor) const;

    // Getters
    WindowMode GetWindowMode() const { return currentMode; }
    bool IsFullscreen() const { return currentMode != WindowMode::WINDOWED; }

    // Saved windowed mode properties (for restoring from fullscreen)
    int GetWindowedWidth() const { return windowedWidth; }
    int GetWindowedHeight() const { return windowedHeight; }
    int GetWindowedPosX() const { return windowedPosX; }
    int GetWindowedPosY() const { return windowedPosY; }
    bool IsChangingMode() const { return isChangingMode; }

private:
    void SaveWindowedState(GLFWwindow* window);
    void RestoreWindowedState(GLFWwindow* window);
    void ApplyFullscreen(GLFWwindow* window);
    void ApplyBorderlessWindowed(GLFWwindow* window);
    void ApplyWindowed(GLFWwindow* window);

    // Aspect ratio locking
    bool aspectRatioLocked = false;
    float lockedAspectRatio = 0.0f;

    WindowMode currentMode;
    Resolution currentResolution;
    bool isChangingMode = false;
    // Saved windowed mode state
    int windowedWidth;
    int windowedHeight;
    int windowedPosX;
    int windowedPosY;
};
