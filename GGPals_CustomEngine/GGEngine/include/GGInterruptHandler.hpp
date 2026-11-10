/*************************************************************************
  \file       GGInterruptHandler.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       23 November 2025
  \version    1.4
  \brief
    Handles OS-level application interrupts including ALT-TAB and
    CTRL-ALT-DEL events for proper system pause coordination.
  \details
    The GGInterruptHandler manages OS-level interruptions that require
    pausing all engine systems. It handles window focus loss/gain,
    minimize/restore events, and coordinates system-wide pause state.
    This handler is strictly for OS/system interrupts, NOT game logic
    pauses. Includes startup delay to prevent framebuffer corruption.

    RELEASE MODE DEBUGGING:
    Two options to enable file logging in Release builds:
    1. Define GG_INTERRUPT_HANDLER_ENABLE_LOGGING in GGInterruptHandler.cpp
    2. Run with command-line argument: OrderNChaos.exe -log
  \dependencies
    <GLFW/glfw3.h>, <chrono>, GGSystemInterface.hpp, Windows.h (Windows-only)
  \see
    GGGameCoordinator.hpp
  \history
    - 1.0 (11 November 2025): Initial version for interrupt handling.
    - 1.1 (13 November 2025): Added startup delay to fix init glitches.
    - 1.2 (23 November 2025): Added Windows key polling and window minimization.
    - 1.3 (23 November 2025): Replaced std::cout with GGLogger for Release mode
                               debugging support.
    - 1.4 (23 November 2025): Added command-line argument support (-log or --enable-logging)
                               for runtime logging control without recompiling.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

// CRITICAL: GLEW must be included before any OpenGL headers
#ifdef _WIN32
#include <GL/glew.h>  // Include GLEW first on Windows
#include <Windows.h>  // For GetAsyncKeyState to detect CTRL-ALT-DEL and ALT-TAB
#endif

#include <GLFW/glfw3.h>   // Now safe to include GLFW
#include <chrono>
#include "GGSystemInterface.hpp"

/*************************************************************************
  \brief
    Manages OS-level application interrupts and system-wide pause state
  \details
    Handles CTRL-ALT-DEL and ALT-TAB interrupts. Ensures clean pause/resume
    of all engine systems during OS-level focus events.
    Does NOT handle game logic states.
*************************************************************************/
class GGInterruptHandler : public SystemManagerInterface
{
public:
    /*************************************************************************
      \brief
        Interrupt state enumeration for OS-level events
    *************************************************************************/
    enum class InterruptState
    {
        ACTIVE,         // Application is active and has focus
        INTERRUPTED,    // ALT-TAB or lost focus but window visible
        MINIMIZED,      // Window is minimized/iconified
        UNRESPONSIVE    // No response for >3 seconds
    };

private:
    // Singleton instance
    static GGInterruptHandler* s_instance;

    // Current interrupt state
    InterruptState m_currentState;
    InterruptState m_previousState;

    // Timing for unresponsive detection
    std::chrono::steady_clock::time_point m_interruptStartTime;
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    static constexpr std::chrono::seconds UNRESPONSIVE_TIMEOUT{ 3 };

    // Startup delay to prevent framebuffer corruption
    std::chrono::steady_clock::time_point m_initTime;
    static constexpr std::chrono::milliseconds STARTUP_DELAY{ 2000 };  // 2 seconds
    bool m_isReady;  // Ready to process interrupts

    // System pause state
    bool m_systemPaused;

    // GLFW window reference
    GLFWwindow* m_window;

    /*************************************************************************
      \brief
        Private constructor for singleton pattern
    *************************************************************************/
    GGInterruptHandler();

    /*************************************************************************
      \brief
        Transitions to a new interrupt state
      \param[in] newState
        The state to transition to
    *************************************************************************/
    void TransitionToState(InterruptState newState);

    /*************************************************************************
      \brief
        Pauses all engine systems for OS interrupt
      \details
        Pauses physics, audio, time accumulation, and marks input for reset.
        This is OS-level pause, not game logic pause.
    *************************************************************************/
    void PauseEngineSystems();

    /*************************************************************************
      \brief
        Resumes all engine systems after OS interrupt
      \details
        Resumes physics, audio, resets input state, and clears time delta.
    *************************************************************************/
    void ResumeEngineSystems();

public:
    /*************************************************************************
      \brief
        Gets the singleton instance
      \return
        Pointer to the InterruptHandler instance
    *************************************************************************/
    static GGInterruptHandler* GetInstance();

    /*************************************************************************
      \brief
        Destroys the singleton instance
    *************************************************************************/
    static void DestroyInstance();

    /*************************************************************************
      \brief
        Manually force system to ACTIVE state (for drag-and-drop)
      \details
        Use this when you need to override interrupt detection
        (e.g., during drag-and-drop operations that trigger false focus loss)
    *************************************************************************/
    void ForceActive();

    /*************************************************************************
      \brief
        Initializes the interrupt handler and registers GLFW callbacks
      \return
        True if initialization successful
      \details
        Sets up window focus and iconify callbacks for ALT-TAB and
        minimize detection. Must be called after window creation.
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief
        Updates the interrupt handler
      \details
        Checks for 3-second unresponsive timeout.
        Includes startup delay to prevent framebuffer corruption.
    *************************************************************************/
    void Update() override;

    /*************************************************************************
      \brief
        Sets the GLFW window to monitor for interrupts
      \param[in] window
        GLFW window pointer
    *************************************************************************/
    void SetWindow(GLFWwindow* window);

    /*************************************************************************
      \brief
        Checks if CTRL-ALT-DEL or ALT-TAB key combination is currently pressed
      \return
        True if either key combo is detected, false otherwise
      \details
        Windows-specific polling using GetAsyncKeyState. Returns false on
        non-Windows platforms. Used to differentiate OS interrupts from
        normal focus loss events.
    *************************************************************************/
    static bool IsOSInterruptKeyComboPressed();

    /*************************************************************************
      \brief
        Gets the current interrupt state
      \return
        Current InterruptState enum value
    *************************************************************************/
    InterruptState GetCurrentState() const { return m_currentState; }

    /*************************************************************************
      \brief
        Checks if systems are paused due to OS interrupt
      \return
        True if paused by OS interrupt, false otherwise
      \par Usage
      \code
        // In any system update:
        if (GGInterruptHandler::GetInstance()->IsSystemPaused())
            return; // Skip update during OS interrupt
      \endcode
    *************************************************************************/
    bool IsSystemPaused() const { return m_systemPaused; }

    /*************************************************************************
      \brief
        Checks if application is active (not interrupted)
      \return
        True if state is ACTIVE, false otherwise
    *************************************************************************/
    bool IsActive() const { return m_currentState == InterruptState::ACTIVE; }

    /*************************************************************************
      \brief
        GLFW window focus callback for ALT-TAB detection
      \param[in] window
        GLFW window that received the event
      \param[in] focused
        GLFW_TRUE if focused, GLFW_FALSE if unfocused
      \details
        Detects ALT-TAB and CTRL-ALT-DEL focus loss events
    *************************************************************************/
    static void OnWindowFocusCallback(GLFWwindow* window, int focused);

    /*************************************************************************
      \brief
        GLFW window iconify callback for minimize detection
      \param[in] window
        GLFW window that received the event
      \param[in] iconified
        GLFW_TRUE if minimized, GLFW_FALSE if restored
    *************************************************************************/
    static void OnWindowIconifyCallback(GLFWwindow* window, int iconified);

    /*************************************************************************
      \brief
        Gets interrupt state as string for debugging
      \param[in] state
        State to convert
      \return
        String representation of the state
    *************************************************************************/
    static const char* GetStateString(InterruptState state);
};