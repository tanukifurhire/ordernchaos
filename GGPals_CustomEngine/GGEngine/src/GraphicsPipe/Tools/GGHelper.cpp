/*************************************************************************
  \file       GGHelper.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       20 November 2025
  \version    2.1
  \brief
    Implementation of GGHelper: initializes the GLFW window, OpenGL
    context, GLEW, and registers all system-level event callbacks.

  \details
    This file provides wrapper functions for initializing the main window,
    setting up the OpenGL context, and registering all GLFW callbacks.
    It also contains callback functions that forward events to the engine's
    InputManager, as well as utility functions to apply the initial window
    mode after creation.
  \history
    - 2.0 (20 November 2025): Initial version.
    - 2.1 (29 March 2026): Set window title bar and taskbar icon at runtime
      via WM_SETICON using the embedded .rc resource, so the icon appears
      correctly in the installed game build. IDI_APPICON in GameIcon.rc has
      no numeric #define so it is stored as a named string resource; LoadImage
      must be called with the literal string "IDI_APPICON" rather than
      MAKEINTRESOURCE.
*************************************************************************/


#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

// glfw3native.h exposes glfwGetWin32Window to retrieve the HWND.
// Must come after glfw3.h.
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

#include <GGHelper.hpp>
#include <GGInputManager.hpp>
#include <GGEngine.hpp>
#include <GGSystemManagers.hpp>
#include <GGInterruptHandler.hpp>

GGWindow* GGHelper::_Swindow = nullptr;

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
    On Windows, after the GLFW window is created the Win32 HWND is retrieved
    via glfwGetWin32Window and WM_SETICON is used to apply the icon embedded
    in the executable via GameIcon.rc. GLFW does not forward embedded resource
    icons to the title bar or taskbar automatically.
    GameIcon.rc uses "IDI_APPICON ICON ..." with no numeric #define, so the
    RC compiler stores it as a named string resource. LoadImage must receive
    the literal string "IDI_APPICON" rather than MAKEINTRESOURCE.
*************************************************************************/
bool GGHelper::InitWindow(GGWindow& mainWindow, int width, int height, const std::string& name)
{
    // set basic data
    mainWindow.Init(width, height, name);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    mainWindow.SetGLFWwindow(glfwCreateWindow(mainWindow.GetWindowWidth(), mainWindow.GetWindowHeight(),
        mainWindow.GetName().c_str(), nullptr, nullptr));

    if (!mainWindow.GetGLFWwindow())
    {
        std::cerr << "Failed to create GLFW Window!" << std::endl;
        glfwTerminate();
        return false;
    }

#ifdef _WIN32
    // IDI_APPICON in GameIcon.rc has no numeric #define, so the RC compiler
    // stores it as a named string resource "IDI_APPICON". Pass the string
    // directly -- MAKEINTRESOURCE would look up a numeric ID and return NULL.
    HICON hIcon = static_cast<HICON>(LoadImage(
        GetModuleHandle(NULL),  // load from this exe's own resource section
        "IDI_APPICON",          // named string resource from GameIcon.rc
        IMAGE_ICON,
        0, 0,                   // 0,0 = use the image's own dimensions
        LR_DEFAULTSIZE | LR_SHARED
    ));

    if (hIcon)
    {
        HWND hwnd = glfwGetWin32Window(mainWindow.GetGLFWwindow());
        // ICON_BIG   = Alt-Tab switcher and restored window title bar
        // ICON_SMALL = taskbar button and minimised title bar
        SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
    }
    else
    {
        std::cerr << "GGHelper: failed to load window icon (IDI_APPICON) from resources" << std::endl;
    }
#endif

    glfwMakeContextCurrent(mainWindow.GetGLFWwindow());

    int major{}, minor{}, rev{};
    glfwGetVersion(&major, &minor, &rev);
    std::cout << "GLFW Version: " << major << "." << minor << "." << rev << std::endl;

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        glfwDestroyWindow(mainWindow.GetGLFWwindow());
        glfwTerminate();
        return false;
    }

    GGHelper::setup_event_callbacks(mainWindow);
    std::cout << "GLEW initialized successfully!" << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    return true;
}

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
void GGHelper::FrameBufferSize_callback([[maybe_unused]] GLFWwindow* ptr_win, [[maybe_unused]] int width, [[maybe_unused]] int height)
{
    // do something
}

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
void GGHelper::Key_callback(GLFWwindow* pwin, int key, int scancode, int action, int mod)
{
    Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().OnKeyEvent(pwin, key, scancode, action, mod);
}

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
void GGHelper::MouseButton_callback(GLFWwindow* pwin, int button, int action, int mod)
{
    Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().OnMouseButtonEvent(pwin, button, action, mod);
}

/*************************************************************************
  \brief
    Callback for mouse movement events.
  \param[in] pwin
    Pointer to the GLFW window receiving the mouse movement event.
  \param[in] xpos
    New X-coordinate of the mouse cursor.
  \param[in] ypos
    New Y-coordinate of the mouse cursor.
  \details
    All mouse movement events are forwarded to the engine's input manager.
*************************************************************************/
void GGHelper::MousePostion_callback(GLFWwindow* pwin, double xpos, double ypos)
{
    Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().OnMouseMoveEvent(pwin, xpos, ypos);
}

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
void GGHelper::MouseSrcoll_callback(GLFWwindow* pwin, double x_offset, double y_offset)
{
    Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().OnMouseScrollEvent(pwin, x_offset, y_offset);
}

void GGHelper::Drop_callback([[maybe_unused]] GLFWwindow* window, int count, const char** paths)
{
    // force unpause on drop
    GGInterruptHandler* interrupt = GGInterruptHandler::GetInstance();
    if (interrupt)
    {
        interrupt->ForceActive();
    }

    auto assetManager = GGEngine::GetEngine().GetAssetManager();

    for (int i = 0; i < count; ++i)
    {
        std::string droppedPath(paths[i]);
        std::cout << "File dropped: " << droppedPath << std::endl;

        if (assetManager)
        {
            assetManager->HandleDroppedFile(droppedPath);
        }
    }
}

/*************************************************************************
  \brief
    Registers GLFW event callbacks for the specified window.
  \param[in,out] win
    GGWindow to which event callbacks will be attached.
  \details
    This function sets up the following GLFW callbacks:
    - Framebuffer resize
    - Key input
    - Mouse button input
    - Mouse cursor movement
    - Mouse scroll
    - File drop callback
*************************************************************************/
void GGHelper::setup_event_callbacks(GGWindow& win)
{
    _Swindow = &win;
    glfwSetFramebufferSizeCallback(win.GetGLFWwindow(), GGHelper::FrameBufferSize_callback);
    glfwSetKeyCallback(win.GetGLFWwindow(), GGHelper::Key_callback);
    glfwSetMouseButtonCallback(win.GetGLFWwindow(), GGHelper::MouseButton_callback);
    glfwSetCursorPosCallback(win.GetGLFWwindow(), GGHelper::MousePostion_callback);
    glfwSetScrollCallback(win.GetGLFWwindow(), GGHelper::MouseSrcoll_callback);
    glfwSetDropCallback(win.GetGLFWwindow(), GGHelper::Drop_callback);
}


/*************************************************************************
  \brief
    Applies window mode after window creation.
  \param[in] mainWindow
    Reference to GGWindow to apply mode to.
  \param[in] mode
    Desired WindowMode (WINDOWED, FULLSCREEN, BORDERLESS_WINDOWED).
  \param[in] useBestResolution
    If true, automatically uses the best available resolution.
  \details
    Call this AFTER InitWindow to set the initial display mode.
    Automatically updates projection and all systems.
*************************************************************************/
void GGHelper::SetInitialWindowMode(GGWindow& mainWindow, WindowMode mode, bool useBestResolution)
{
    if (!mainWindow.GetGLFWwindow())
    {
        std::cerr << "Cannot set window mode - window not initialized!" << std::endl;
        return;
    }

    // Apply best resolution first if requested
    if (useBestResolution)
    {
        mainWindow.SetBestResolution();
        Resolution best = mainWindow.GetBestResolution();
        std::cout << "Using best resolution: " << best.toString() << std::endl;
    }

    mainWindow.SetWindowMode(mode);

    // Update projection after mode change
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    Resolution res = mainWindow.GetCurrentResolution();
    renderMgr.SetDefaultProjection(res.width, res.height);

    const char* modeNames[] = { "Windowed", "Fullscreen", "Borderless Windowed" };
    std::cout << "Initial window mode set to: " << modeNames[static_cast<int>(mode)]
        << " (" << res.width << "x" << res.height << ")" << std::endl;
}