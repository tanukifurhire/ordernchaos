/*************************************************************************
  \file       main.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    2.4
  \brief
    GGEngine application entry point with editor/game build separation
  \details
    This file provides the WinMain entry point for the GGEngine executable.
    Debug builds launch the full editor with ImGui interface.
    Release builds launch the game only (Order N Chaos) without editor.
    Uses preprocessor directives (_DEBUG) to conditionally compile editor code.
    Includes single instance enforcement to prevent multiple game instances.
  \dependencies
    <windows.h>, <iostream>, <fcntl.h>, <io.h>, <crtdbg.h>,
    GGSingleInstanceLock.hpp, GGEngine.hpp, GGSystemManagers.hpp,
    DefaultEditorScene.hpp (Debug only)
  \see
    GGEngine, Systems::EditorManager, DefaultEditorScene, GGSingleInstanceLock
  \history
    - 1.0 (3 October 2025): Initial version with console and debug support.
    - 1.1 (11 November 2025): Added single instance enforcement.
    - 2.0 (15 November 2025): Added editor/game build separation for installer.
    - 2.1 (15 November 2025): Added tilemap editor integration.
    - 2.2 (23 November 2025): Added early command-line argument parsing for -log flag.
                               File logging now initializes BEFORE engine startup.
    - 2.3 (23 November 2025): Debug mode writes logs to build folder, Release to AppData.
                               No admin rights required in Release mode.
    - 2.4 (23 November 2025): Smart log location - tries exe directory first, falls back
                               to AppData if not writable (handles Release IDE builds).
  \copyright
    Copyright (C) 2025 GGPals. DigiPen Institute of Technology.
*************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <Shellapi.h>
#include <ShlObj.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <io.h>
#include <GGEngine.hpp>
#include <GGSystemManagers.hpp>
#include <GGSingleInstanceLock.hpp>

// Editor includes - for both DebugEditor and ReleaseEditor
#ifdef GG_EDITOR_BUILD
#include <DefaultEditorScene.hpp>
#include <AnimationNode.hpp>
#include <AnimatorNode.hpp>
#include <AssetManagerNode.hpp>
#include <ConsoleNode.hpp>
#include <EditorViewNode.hpp>
#include <GameViewNode.hpp>
#include <HierarchyNode.hpp>
#include <InspectorNode.hpp>
#include <StopPlayToolbar.hpp>
#include <PrefabInspectorNode.hpp>
#include <TilemapNode.hpp>
#include <LayerManagerNode.hpp>
#endif

#include <GGJSONConfig.hpp>
#include <GGLogger.hpp>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)  // Enables file & line tracking

/*************************************************************************
  \brief
    Windows application entry point for GGEngine.
  \param[in] hInstance
    Handle to the current instance of the application.
  \param[in] hPrevInstance
    Handle to the previous instance (always NULL in Win32).
  \param[in] lpCmdLine
    Command line arguments as a wide string.
  \param[in] nCmdShow
    Window display state (minimized, maximized, etc.).
  \return
    Application exit code. Returns 0 on success.
  \details
    Debug builds: Launches full editor with ImGui interface and console.
    Release builds: Launches game only (Order N Chaos) without editor.

    Execution flow:
    1. Checks for existing instance using mutex lock
    2. Configures CRT debug heap for memory leak detection
    3. Parses command-line arguments for -log flag
    4. [Debug only] Attaches console for logging
    5. [Debug only] Registers all editor UI nodes
    6. Transfers control to GGEngine::StartEngine()
    7. [Debug only] Waits for user input before termination
    8. [Debug only] Releases console resources
*************************************************************************/
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)nCmdShow;
    (void)hPrevInstance;
    (void)hInstance;
    (void)lpCmdLine;

    //// Single instance check must be first to prevent resource duplication - DISABLED
    //GGSingleInstanceLock instanceLock("GGPals_OrderNChaos");

    //if (!instanceLock.HasLock())
    //{
    //    // Another instance is already running
    //    instanceLock.ActivateExistingWindow();

    //    MessageBoxW(nullptr,
    //        L"Order N Chaos is already running!\n\n"
    //        L"The existing window has been brought to the foreground.",
    //        L"GGPals - Order N Chaos",
    //        MB_OK | MB_ICONINFORMATION);

    //    return 0;  // Exit immediately without initializing anything
    //}

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // ============================================================================
    // EARLY COMMAND-LINE ARGUMENT PROCESSING
    // ============================================================================
    // Parse command-line arguments BEFORE engine initialization to enable
    // file logging from the very start. This ensures engine.log captures
    // all initialization messages, not just post-Init() messages.
    //
    // Supported arguments:
    //   -log            Enable file logging to engine.log
    //   --enable-logging  Enable file logging to engine.log
    //
    // Without this early check, file logging would only start after
    // GGInterruptHandler::Init() is called, missing critical startup logs.
    //
    // Log file location (smart detection):
    //   1. Try exe directory (works for IDE builds and development)
    //   2. If not writable (Program Files), use %LOCALAPPDATA%\DigiPen\OrderNChaos\
    // ============================================================================
    bool enableFileLogging = false;

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

                // Check for logging flags
                if (arg.find("--enable-logging") != std::string::npos ||
                    arg.find("-log") != std::string::npos)
                {
                    enableFileLogging = true;
                    break;
                }
            }
        }
        LocalFree(argvW);
    }
#endif

    // Enable file logging IMMEDIATELY if requested via command-line
    if (enableFileLogging)
    {
        std::string logFilePath;
        bool useAppData = false;

#ifdef _WIN32
        // Get exe directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exePathStr(exePath);
        size_t lastSlash = exePathStr.find_last_of("\\/");
        std::string exeDir;

        if (lastSlash != std::string::npos)
        {
            exeDir = exePathStr.substr(0, lastSlash + 1);
        }
        else
        {
            exeDir = ".\\";
        }

        // Try exe directory first (works for IDE builds)
        std::string testPath = exeDir + "engine.log";

        // Test if we can write to exe directory
        std::ofstream testFile(testPath, std::ios::app);
        if (testFile.is_open())
        {
            // Exe directory is writable (IDE build or local installation)
            logFilePath = testPath;
            testFile.close();
        }
        else
        {
            // Exe directory not writable (Program Files) - use AppData
            useAppData = true;

            char appDataPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath)))
            {
                // Build paths step by step
                std::string appDataStr(appDataPath);
                std::string digipenDir = appDataStr + "\\DigiPen";
                std::string orderNChaosDir = digipenDir + "\\OrderNChaos";

                // Create directories (fails silently if already exists)
                CreateDirectoryA(digipenDir.c_str(), NULL);
                CreateDirectoryA(orderNChaosDir.c_str(), NULL);

                logFilePath = orderNChaosDir + "\\engine.log";
            }
            else
            {
                // Last resort fallback
                logFilePath = "engine.log";
            }
        }
#else
        logFilePath = "engine.log"; // Non-Windows: use working directory
#endif

        // Enable file logging BEFORE any engine systems initialize
        GGLogger::SetFileLogging(true, logFilePath);
        GGLogger::Info("[main] File logging enabled via command-line argument: " + logFilePath);

        if (useAppData)
        {
            GGLogger::Info("[main] Exe directory not writable - using AppData (no admin required)");
        }
        else
        {
            GGLogger::Info("[main] Writing to exe directory (IDE build or local installation)");
        }
    }
    // ============================================================================


#ifdef GG_EDITOR_BUILD
#ifdef _DEBUG
    // Debug console - only in DebugEditor (not ReleaseEditor)
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
    {
        // Redirect standard I/O
        FILE* fDummy;
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);

        // Enable Unicode output
        if (_setmode(_fileno(stdout), _O_TEXT));
        else return false;
        if (_setmode(_fileno(stderr), _O_TEXT));
        else return false;

        std::ios::sync_with_stdio();
        std::cout.clear();
        std::cin.clear();
        std::cerr.clear();
        std::wcout.clear();
        std::wcin.clear();
        std::wcerr.clear();

        std::cout << "============= " << GGEngineConfig::GGEngineInfo::EngineName() << " by GGPals 2025 ============ = " << std::endl;
        std::cout << "[EDITOR MODE] Initializing ImGui editor interface" << std::endl;
    }
    else return -1;
#endif // _DEBUG - End of console block

    // Editor interface - for both DebugEditor and ReleaseEditor
    // Toolbar
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetStopPlayToolbar()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetStopPlayToolbar()->updateCallbackHandle;

    // Animation
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetAnimationNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetAnimationNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetAnimationNode()->updateCallbackHandle;

    // Animator
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetAnimatorNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetAnimatorNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetAnimatorNode()->updateCallbackHandle;

    // Editor Camera
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetEditorViewNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetEditorViewNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetEditorViewNode()->updateCallbackHandle;

    // Game Camera
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetGameViewNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetGameViewNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetGameViewNode()->updateCallbackHandle;

    // Hierarchy
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetHierarchyNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetHierarchyNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetHierarchyNode()->updateCallbackHandle;

    // Default Editor
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetDefaultEditorScene()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetDefaultEditorScene()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetDefaultEditorScene()->updateCallbackHandle;

    // Inspector
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetInspectorNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetInspectorNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetInspectorNode()->updateCallbackHandle;

    // Prefab Inspector
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetPrefabInspectorNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetPrefabInspectorNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetPrefabInspectorNode()->updateCallbackHandle;
    // Layer Manager
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetLayerManagerNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetLayerManagerNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetLayerManagerNode()->updateCallbackHandle;
    // Asset Manager
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetAssetManagerNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetAssetManagerNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetAssetManagerNode()->updateCallbackHandle;

    // Tilemap Node
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetTilemapNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetTilemapNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetTilemapNode()->updateCallbackHandle;

    
    // Console
    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onInitEventHandler
        += GetConsoleNode()->initCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onDockBuildEventHandler
        += GetConsoleNode()->dockBuildCallbackHandle;

    Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().onUpdateEventHandler
        += GetConsoleNode()->updateCallbackHandle;

#else
    // ReleaseGame mode - NO editor interface
    GGLogger::Info("[GAME MODE] Launching Order N Chaos");
    std::cout << "[GAME MODE] Launching Order N Chaos" << std::endl;
#endif

    // Start the engine (both Debug and Release)
    try 
    {
        GGEngine::StartEngine();
    }
    catch (std::exception& e)
    {
        std::cout << "Failed to start engine: " << e.what() << std::endl;
    }

#ifdef _DEBUG
    // Debug stop console from dying
    std::cout << "Press Enter to continue . . .";
    std::string temp;
    std::getline(std::cin, temp);

    FreeConsole();
#endif

    return 0;
}