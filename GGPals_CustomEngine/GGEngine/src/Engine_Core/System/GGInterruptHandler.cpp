/*************************************************************************
  \file       GGInterruptHandler.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       30 November 2025
  \version    1.6
  \brief
    Implementation of OS-level interrupt handling for ALT-TAB and
    CTRL-ALT-DEL events.
  \details
    Implements the interrupt handler that manages OS-level application
    interrupts and coordinates system-wide pause/resume operations.
    Handles window focus, minimize, and unresponsive timeout detection.
    Includes startup delay to prevent framebuffer corruption during init.

    RELEASE MODE DEBUGGING:
    Define GG_INTERRUPT_HANDLER_ENABLE_LOGGING to enable file logging in
    Release builds. This creates engine.log for debugging without console.
  \dependencies
    GGInterruptHandler.hpp, GGSystemManagers.hpp, GGEngine.hpp,
    GGAudioManager.hpp, GGInputManager.hpp, GGTime.hpp, Windows.h,
    Shellapi.h (Windows-only)
  \see
    GGInterruptHandler.hpp
  \history
    - 1.0 (11 November 2025): Initial implementation.
    - 1.1 (13 November 2025): Added startup delay to fix init glitches.
    - 1.2 (23 November 2025): Added Windows key polling and window minimization.
    - 1.3 (23 November 2025): Replaced std::cout with GGLogger for Release mode
                               debugging support.
    - 1.4 (23 November 2025): Added command-line argument support (-log or --enable-logging)
                               for runtime logging control without recompiling.
    - 1.5 (23 November 2025): Fixed ReleaseGame crash by wrapping ImGui calls in
                               GG_EDITOR_BUILD guard. ImGui is only available in editor
                               builds, not game builds.
    - 1.6 (30 November 2025): Changed interrupt handler to be disabled in ALL editor builds
                               (both DebugEditor and ReleaseEditor) to avoid ImGui focus spam.
                               Only ReleaseGame now has interrupt handling enabled. Switched from
                               early return to #if/#else preprocessor blocks to completely exclude
                               unreachable code from editor builds, removing "unreachable code" warnings.
                               Wrapped GGFMODWrapper audio calls in #ifndef GG_EDITOR_BUILD to fix
                               E0833 incomplete type errors - audio pause/resume only needed in
                               ReleaseGame where interrupt handler is active.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

// ============================================================================
// RELEASE MODE DEBUG LOGGING TOGGLE
// ============================================================================
// Option 1: Compile-time (uncomment line below)
// #define GG_INTERRUPT_HANDLER_ENABLE_LOGGING
//
// Option 2: Runtime via command-line argument (no recompile needed!)
// Run: OrderNChaos.exe -log
//
// This creates "engine.log" in the executable directory.
// Useful for debugging interrupt handling behavior without a console window.
//
// When enabled:
//   - All engine logs (including interrupt events) written to engine.log
//   - Key state polling visible
//   - State transitions captured
//   - Works in both Debug and Release
//
// When disabled (default):
//   - No file logging overhead
//   - Cleaner release builds
//
// Command-line usage example:
//   OrderNChaos.exe -log
// ============================================================================

#include "GGInterruptHandler.hpp"
#include "GGAccessibilityShortcuts.hpp"
#include "GGSystemManagers.hpp"
#include "GGEngine.hpp"
#include "GGInputManager.hpp"
#include "GGLogger.hpp"
#include <iostream>

#ifdef _WIN32
#include <Shellapi.h>  // For CommandLineToArgvW
#endif

// Only include audio manager in ReleaseGame builds where it's actually used
#ifndef GG_EDITOR_BUILD
#include "GGAudioManager.hpp"   // For GGFMODWrapper - SetMasterVolume (ReleaseGame only)
#endif

#include <imgui.h>
#include <imgui_internal.h>

// Initialize static member
GGInterruptHandler* GGInterruptHandler::s_instance = nullptr;

/*************************************************************************
  \brief
    Private constructor for singleton pattern
  \details
    Initializes with active state (not interrupted)
*************************************************************************/
GGInterruptHandler::GGInterruptHandler()
    : m_currentState(InterruptState::ACTIVE)
    , m_previousState(InterruptState::ACTIVE)
    , m_systemPaused(false)
    , m_window(nullptr)
    , m_isReady(false)  // Start not ready
{
    m_lastUpdateTime = std::chrono::steady_clock::now();
    m_interruptStartTime = std::chrono::steady_clock::now();
    m_initTime = std::chrono::steady_clock::now();
}

/*************************************************************************
  \brief
    Gets the singleton instance
  \return
    Pointer to the InterruptHandler instance
*************************************************************************/
GGInterruptHandler* GGInterruptHandler::GetInstance()
{
    if (!s_instance)
    {
        s_instance = new GGInterruptHandler();
    }
    return s_instance;
}

/*************************************************************************
  \brief
    Destroys the singleton instance
*************************************************************************/
void GGInterruptHandler::DestroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

/*************************************************************************
  \brief
    Manually force system to ACTIVE state
  \details
    Use this when you need to override interrupt detection
    (e.g., during drag-and-drop operations that trigger false focus loss)
*************************************************************************/
void GGInterruptHandler::ForceActive()
{
    if (m_currentState != InterruptState::ACTIVE)
    {
        std::cout << "[InterruptHandler] Manually forcing ACTIVE state!" << std::endl;
        TransitionToState(InterruptState::ACTIVE);
    }
}

/*************************************************************************
  \brief
    Initializes the interrupt handler and registers GLFW callbacks
  \return
    True if initialization successful
*************************************************************************/
bool GGInterruptHandler::Init()
{
    // Check for command-line argument to enable logging
    // Usage: OrderNChaos.exe --enable-logging
    bool enableLoggingViaArgs = false;

#ifdef _WIN32
    int argc;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvW)
    {
        for (int i = 1; i < argc; ++i)
        {
            // Convert wide string to narrow string for comparison
            int size = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr);
            if (size > 0)
            {
                std::string arg(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &arg[0], size, nullptr, nullptr);

                if (arg.find("--enable-logging") != std::string::npos ||
                    arg.find("-log") != std::string::npos)
                {
                    enableLoggingViaArgs = true;
                    break;
                }
            }
        }
        LocalFree(argvW);
    }
#endif

    // Enable logging if either compile-time flag OR command-line arg is set
#ifdef GG_INTERRUPT_HANDLER_ENABLE_LOGGING
    bool enableLogging = true;
#else
    bool enableLogging = enableLoggingViaArgs;
#endif

    if (enableLogging)
    {
        // Get executable directory for log file placement
        std::string logFilePath;

#ifdef _WIN32
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exePathStr(exePath);
        size_t lastSlash = exePathStr.find_last_of("\\/");
        if (lastSlash != std::string::npos)
        {
            logFilePath = exePathStr.substr(0, lastSlash + 1) + "engine.log";
        }
        else
        {
            logFilePath = "engine.log"; // Fallback to working directory
        }
#else
        logFilePath = "engine.log"; // Non-Windows: use working directory
#endif

        // Enable file logging for Release mode debugging
        GGLogger::SetFileLogging(true, logFilePath);
        GGLogger::Info("[InterruptHandler] Initializing interrupt handler with file logging enabled");
        GGLogger::Info("[InterruptHandler] Log file: " + logFilePath);

        if (enableLoggingViaArgs)
        {
            GGLogger::Info("[InterruptHandler] Logging enabled via command-line argument");
        }
    }
    else
    {
        GGLogger::Info("[InterruptHandler] Initializing interrupt handler (file logging disabled)");
    }

    // Get window from RenderManager
    auto& renderManager = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    m_window = renderManager.GetMainWindow().GetGLFWwindow();

    if (!m_window)
    {
        GGLogger::Error("[InterruptHandler] Error: No GLFW window available!");
        return false;
    }

    // Register GLFW callbacks for interrupt detection
    glfwSetWindowFocusCallback(m_window, OnWindowFocusCallback);
    glfwSetWindowIconifyCallback(m_window, OnWindowIconifyCallback);

    // Store pointer for callbacks to access
    glfwSetWindowUserPointer(m_window, this);

    // Mark init time for startup delay
    m_initTime = std::chrono::steady_clock::now();
    m_isReady = false;

    GGLogger::Info("[InterruptHandler] Initialized (5 second startup delay to prevent glitches)");

    return true;
}

/*************************************************************************
  \brief
    Updates the interrupt handler
  \details
    Checks for 3-second unresponsive timeout per rubric requirement.
    Includes startup delay to prevent framebuffer corruption.
*************************************************************************/
void GGInterruptHandler::Update()
{
    // Extended startup delay - wait longer before processing ANY interrupts
    if (!m_isReady)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_initTime);

        // Increase delay to 5 seconds (or however long your startup takes)
        static constexpr std::chrono::milliseconds EXTENDED_STARTUP_DELAY{ 5000 };

        if (elapsed < EXTENDED_STARTUP_DELAY)
        {
            // Force ACTIVE state during startup - ignore ALL focus events
            m_currentState = InterruptState::ACTIVE;
            m_systemPaused = false;
            return;  // Don't process anything
        }

        // Ready to start processing
        m_isReady = true;
        GGLogger::Info("[InterruptHandler] Now actively monitoring for interrupts");
    }

    // Normal processing after startup
    auto currentTime = std::chrono::steady_clock::now();
    m_lastUpdateTime = currentTime;

    // Check for unresponsive timeout when interrupted
    if ((m_currentState == InterruptState::INTERRUPTED ||
        m_currentState == InterruptState::MINIMIZED) &&
        m_currentState != InterruptState::UNRESPONSIVE)
    {
        auto timeSinceInterrupt = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - m_interruptStartTime);

        if (timeSinceInterrupt >= UNRESPONSIVE_TIMEOUT)
        {
            std::cout << "[InterruptHandler] Application unresponsive (>3 seconds)" << std::endl;
            TransitionToState(InterruptState::UNRESPONSIVE);
        }
    }
    
    // Polling for CTRL+ALT+DEL and ALT+TAB commands (ONLY in ReleaseGame build config)
#ifndef GG_EDITOR_BUILD

    [[maybe_unused]] auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

    // Check ALT key state
    bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

    // Check TAB key state (for ALT-TAB)
    bool tabPressed = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

    // Check CTRL key state (for CTRL-ALT-DEL)
    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

    // Check DELETE key state (for CTRL-ALT-DEL)
    bool delPressed = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;

    if (m_window &&
        altPressed &&
        ctrlPressed &&
        delPressed)
    {
        glfwIconifyWindow(m_window);
    }

    if (m_window &&
        altPressed &&
        tabPressed)
    {
        glfwIconifyWindow(m_window);
    }

#endif
}

/*************************************************************************
  \brief
    Sets the GLFW window to monitor for interrupts
  \param[in] window
    GLFW window pointer
*************************************************************************/
void GGInterruptHandler::SetWindow(GLFWwindow* window)
{
    m_window = window;
    if (m_window)
    {
        glfwSetWindowUserPointer(m_window, this);
        glfwSetWindowFocusCallback(m_window, OnWindowFocusCallback);
        glfwSetWindowIconifyCallback(m_window, OnWindowIconifyCallback);
    }
}

/*************************************************************************
  \brief
    Transitions to a new interrupt state
  \param[in] newState
    The state to transition to
*************************************************************************/
void GGInterruptHandler::TransitionToState(InterruptState newState)
{
    if (m_currentState == newState)
        return;

    // Don't transition during startup delay
    if (!m_isReady)
        return;

    m_previousState = m_currentState;
    m_currentState = newState;

    std::cout << "[InterruptHandler] State: "
        << GetStateString(m_previousState) << " -> "
        << GetStateString(m_currentState) << std::endl;

    // Handle interrupt start (ALT-TAB, CTRL-ALT-DEL, minimize)
    if (m_previousState == InterruptState::ACTIVE &&
        m_currentState != InterruptState::ACTIVE)
    {
        m_interruptStartTime = std::chrono::steady_clock::now();
        PauseEngineSystems();
    }
    // Handle interrupt end (focus regained)
    else if (m_currentState == InterruptState::ACTIVE &&
        m_previousState != InterruptState::ACTIVE)
    {
        ResumeEngineSystems();
    }
}

/*************************************************************************
  \brief
    Pauses all engine systems for OS interrupt
  \details
    Coordinates pause across physics, audio, and input systems.
    Audio pause only active in ReleaseGame where interrupt handler is enabled.
*************************************************************************/
void GGInterruptHandler::PauseEngineSystems()
{
    if (m_systemPaused)
        return;

    std::cout << "[InterruptHandler] Pausing engine systems (OS interrupt)" << std::endl;
    m_systemPaused = true;

#ifndef GG_EDITOR_BUILD
    // Pause audio (only in ReleaseGame where interrupt handler is active)
    // In editor builds, interrupt handler is disabled so this code is unnecessary
    // and would cause E0833 errors due to GGFMODWrapper include order issues
    auto* audioSystem = GGEngine::GetEngine().GetAudioSystem();
    if (audioSystem)
    {
        // Mute master volume (preserves individual channel states)
        audioSystem->SetMasterVolume(0.0f);
    }
#endif

    // Input will be cleared on resume
    // Physics and Time check IsSystemPaused() in their updates
}

/*************************************************************************
  \brief
    Resumes all engine systems after OS interrupt
  \details
    Coordinates resume across all systems and clears input state.
    Audio resume only active in ReleaseGame where interrupt handler is enabled.
*************************************************************************/
void GGInterruptHandler::ResumeEngineSystems()
{
    if (!m_systemPaused)
        return;

    std::cout << "[InterruptHandler] Resuming engine systems (interrupt ended)" << std::endl;
    m_systemPaused = false;

#ifndef GG_EDITOR_BUILD
    // Resume audio (only in ReleaseGame where interrupt handler is active)
    // In editor builds, interrupt handler is disabled so this code is unnecessary
    // and would cause E0833 errors due to GGFMODWrapper include order issues
    auto* audioSystem = GGEngine::GetEngine().GetAudioSystem();
    if (audioSystem)
    {
        audioSystem->SetMasterVolume(1.0f);
    }
#endif

    // Clear input state to prevent stuck keys
    auto& inputManager = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    inputManager.InputUpdate(); // Clears previous state

    // Force time manager to reset delta
    auto& timeManager = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
    timeManager.Update(); // Reset delta time
}

/*************************************************************************
  \brief
    Checks if CTRL-ALT-DEL or ALT-TAB key combination is currently pressed
  \return
    True if either key combo is detected, false otherwise
  \details
    Uses Windows GetAsyncKeyState API to poll for:
    - ALT + TAB
    - CTRL + ALT + DEL

    This allows differentiating between OS-level interrupts and normal
    focus loss (e.g., clicking outside window). Per rubric 1701/1702,
    we must minimize the window specifically for these key combos.
*************************************************************************/
bool GGInterruptHandler::IsOSInterruptKeyComboPressed()
{
#ifdef _WIN32
    // Check ALT key state
    bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

    // Check TAB key state (for ALT-TAB)
    bool tabPressed = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

    // Check CTRL key state (for CTRL-ALT-DEL)
    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

    // Check DELETE key state (for CTRL-ALT-DEL)
    bool delPressed = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;

    // Debug logging
    GGLogger::Info("[InterruptHandler][KeyPoll] ALT=" + std::to_string(altPressed) +
        " TAB=" + std::to_string(tabPressed) +
        " CTRL=" + std::to_string(ctrlPressed) +
        " DEL=" + std::to_string(delPressed));

    // ALT-TAB combo
    if (altPressed && tabPressed)
    {
        GGLogger::Info("[InterruptHandler][KeyPoll] ALT-TAB DETECTED!");
        return true;
    }

    // CTRL-ALT-DEL combo
    if (ctrlPressed && altPressed && delPressed)
    {
        GGLogger::Info("[InterruptHandler][KeyPoll] CTRL-ALT-DEL DETECTED!");
        return true;
    }

    GGLogger::Info("[InterruptHandler][KeyPoll] No OS interrupt keys detected");
#else
    GGLogger::Info("[InterruptHandler][KeyPoll] Non-Windows platform, no key polling available");
#endif

    return false;
}

/*************************************************************************
  \brief
    GLFW window focus callback for ALT-TAB detection
  \param[in] window
    GLFW window that received the event
  \param[in] focused
    GLFW_TRUE if focused, GLFW_FALSE if unfocused
*************************************************************************/
void GGInterruptHandler::OnWindowFocusCallback(GLFWwindow* window, int focused)
{
    // Accessibility shortcuts first, ahead of every early-out below and
    // outside the GG_EDITOR_BUILD gate.
    //
    // These OS settings are process-wide and outlive us if we get them wrong,
    // so the restore path must never be skipped:
    //   - The m_isReady guard drops focus events for the first 5 seconds. An
    //     ALT-TAB inside that window would otherwise leave the suppression on
    //     while another application has focus.
    //   - The editor builds compile the rest of this function away. They still
    //     need the restore, for the same reason.
    // Suppression is armed at start-up in GGEngine::Init and lifted in the
    // shutdown path there, so this call only tracks focus changes in between.
    GGAccessibilityShortcuts::SetGameHasFocus(focused == GLFW_TRUE);

    auto* handler = static_cast<GGInterruptHandler*>(glfwGetWindowUserPointer(window));
    if (!handler || !handler->m_isReady)  // Skip if not ready
        return;

    // ============================================================================
    // INTERRUPT HANDLER BUILD CONFIGURATION
    // ============================================================================
    // Behavior across build configurations:
    //   - DebugEditor:   Interrupt handler DISABLED (avoid ImGui focus spam during development)
    //   - ReleaseEditor: Interrupt handler DISABLED (avoid ImGui focus spam during testing)
    //   - ReleaseGame:   Interrupt handler ENABLED  (production behavior for TCR compliance)
    //
    // Implementation:
    //   We check for GG_EDITOR_BUILD being defined. This covers both editor builds:
    //     - DebugEditor:   GG_EDITOR_BUILD=YES -> Condition TRUE  -> Empty function
    //     - ReleaseEditor: GG_EDITOR_BUILD=YES -> Condition TRUE  -> Empty function
    //     - ReleaseGame:   GG_EDITOR_BUILD=NO  -> Condition FALSE -> Full handler
    //
    // Using #if/#else instead of early return to completely exclude unreachable code
    // from editor builds, avoiding MSVC C4702 warnings.
    // ============================================================================

#ifdef GG_EDITOR_BUILD
    // Editor builds: Skip focus handling entirely to avoid pause spam from ImGui windows
    (void)window;
    (void)focused;
#else
    // ReleaseGame only: Full interrupt handling enabled for TCR compliance

    if (focused == GLFW_TRUE)
    {
        GGLogger::Info("[InterruptHandler] ========== FOCUS GAINED EVENT ==========");
        GGLogger::Info("[InterruptHandler] Window focus gained");

        // Only activate if not minimized
        if (handler->m_currentState != InterruptState::MINIMIZED)
        {
            GGLogger::Info("[InterruptHandler] Not minimized - transitioning to ACTIVE");
            handler->TransitionToState(InterruptState::ACTIVE);
        }
        else
        {
            GGLogger::Info("[InterruptHandler] Still minimized - NOT transitioning to ACTIVE yet");
        }

        GGLogger::Info("[InterruptHandler] ========== FOCUS GAINED HANDLED ==========");
    }
    else
    {
        GGLogger::Info("[InterruptHandler] ========== FOCUS LOST EVENT ==========");
        GGLogger::Info("[InterruptHandler] Focus lost - checking for OS interrupt keys...");

        // Check if this is an OS-level interrupt (CTRL-ALT-DEL or ALT-TAB)
        bool isOSInterrupt = IsOSInterruptKeyComboPressed();

        GGLogger::Info("[InterruptHandler] OS interrupt key check result: " + std::string(isOSInterrupt ? "TRUE" : "FALSE"));

        if (isOSInterrupt)
        {
            GGLogger::Info("[InterruptHandler] *** OS INTERRUPT CONFIRMED *** - minimizing window per rubric");

            // Per TCR rubric 1701/1702: Window must be minimized when CTRL-ALT-DEL or ALT-TAB pressed
            glfwIconifyWindow(window);

            GGLogger::Info("[InterruptHandler] glfwIconifyWindow() called - waiting for iconify callback");

            // State transition will be handled by iconify callback
        }
        else
        {
            GGLogger::Info("[InterruptHandler] Normal focus change (no OS interrupt keys) - NOT minimizing");

            // Normal focus loss (clicking outside, etc.) - just interrupt, don't minimize
            handler->TransitionToState(InterruptState::INTERRUPTED);
        }

        GGLogger::Info("[InterruptHandler] ========== FOCUS LOST HANDLED ==========");
    }

#endif  // GG_EDITOR_BUILD
}

/*************************************************************************
  \brief
    GLFW window iconify callback for minimize detection
  \param[in] window
    GLFW window that received the event
  \param[in] iconified
    GLFW_TRUE if minimized, GLFW_FALSE if restored
*************************************************************************/
void GGInterruptHandler::OnWindowIconifyCallback(GLFWwindow* window, int iconified)
{
    auto* handler = static_cast<GGInterruptHandler*>(glfwGetWindowUserPointer(window));
    if (!handler || !handler->m_isReady)  // Skip if not ready
        return;

    if (iconified)
    {
        GGLogger::Info("[InterruptHandler] ========== WINDOW ICONIFY EVENT ==========");
        GGLogger::Info("[InterruptHandler] Window minimized/iconified");
        handler->TransitionToState(InterruptState::MINIMIZED);
        GGLogger::Info("[InterruptHandler] ========== ICONIFY HANDLED ==========");
    }
    else
    {
        GGLogger::Info("[InterruptHandler] ========== WINDOW RESTORE EVENT ==========");
        GGLogger::Info("[InterruptHandler] Window restored from minimize");

        // Check if we have focus after restore
        int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
        GGLogger::Info("[InterruptHandler] Focus status after restore: " + std::string(focused ? "HAS FOCUS" : "NO FOCUS"));

        if (focused)
        {
            GGLogger::Info("[InterruptHandler] Transitioning to ACTIVE (has focus)");
            handler->TransitionToState(InterruptState::ACTIVE);
        }
        else
        {
            GGLogger::Info("[InterruptHandler] Transitioning to INTERRUPTED (no focus yet)");
            handler->TransitionToState(InterruptState::INTERRUPTED);
        }

        GGLogger::Info("[InterruptHandler] ========== RESTORE HANDLED ==========");
    }
}

/*************************************************************************
  \brief
    Gets interrupt state as string for debugging
  \param[in] state
    State to convert
  \return
    String representation of the state
*************************************************************************/
const char* GGInterruptHandler::GetStateString(InterruptState state)
{
    switch (state)
    {
    case InterruptState::ACTIVE:
        return "ACTIVE";
    case InterruptState::INTERRUPTED:
        return "INTERRUPTED";
    case InterruptState::MINIMIZED:
        return "MINIMIZED";
    case InterruptState::UNRESPONSIVE:
        return "UNRESPONSIVE";
    default:
        return "UNKNOWN";
    }
}