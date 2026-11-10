/*************************************************************************
  \file       GGSingleInstanceLock.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       11 November 2025
  \version    1.0
  \brief
    Implementation of single instance lock using Windows mutex
  \details
    Mutex based lock to prevent multiple game instances.
    Just a lock that uses Windows-specific APIs
    (CreateMutex, FindWindow) wrapped in platform defines (_WIN32)
    for cross-platform compatibility. Non-Windows platforms will
    default to allowing multiple instances until platform-specific
    implementation is added.
  \dependencies
    GGSingleInstanceLock.hpp, <windows.h>, <iostream>
  \see
    GGSingleInstanceLock.hpp
  \history
    - 1.0: Initial implementation with Windows mutex
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGSingleInstanceLock.hpp"
#include <iostream>

#ifdef _WIN32
#include <cstring>
#endif

/*************************************************************************
  \brief
    Constructor
  \param[in] lockName
    Unique name for the game
*************************************************************************/
GGSingleInstanceLock::GGSingleInstanceLock(const std::string& lockName): m_lockName(lockName), m_hasLock(false)
#ifdef _WIN32
    , m_mutex(nullptr)
#endif
{
    TryAcquire();
}

/*************************************************************************
  \brief
    Destructor
*************************************************************************/
GGSingleInstanceLock::~GGSingleInstanceLock()
{
#ifdef _WIN32
    if (m_mutex && m_hasLock)
    {
        std::cout << "[SingleInstanceLock] Lock will be released on exit" << std::endl;
    }
#endif
}

/*************************************************************************
  \brief
    Attempts to acquire the single instance lock
  \return
    LockResult indicating if we got the lock
*************************************************************************/
GGSingleInstanceLock::LockResult GGSingleInstanceLock::TryAcquire()
{
#ifdef _WIN32
    std::string mutexName = "Global\\Mutex_" + m_lockName;
    m_mutex = CreateMutexA(nullptr, FALSE, mutexName.c_str());

    if (!m_mutex)
    {
        std::cerr << "[SingleInstanceLock] Failed to create mutex" << std::endl;
        return LockResult::FAILED;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        std::cout << "[SingleInstanceLock] Another instance is running" << std::endl;
        CloseHandle(m_mutex);
        m_mutex = nullptr;
        return LockResult::ALREADY_LOCKED;
    }

    m_hasLock = true;
    std::cout << "[SingleInstanceLock] Lock acquired" << std::endl;
    return LockResult::ACQUIRED;
#else
    // Non-Windows: always succeed for now
    m_hasLock = true;
    return LockResult::ACQUIRED;
#endif
}

/*************************************************************************
  \brief
    Finds and activates the existing game window
  \return
    True if found and activated
  \details
    Searches for the window of the already running instance
    and brings it to foreground
*************************************************************************/
bool GGSingleInstanceLock::ActivateExistingWindow()
{
#ifdef _WIN32
    HWND existingWindow = FindGameWindow();
    if (existingWindow)
    {
        if (IsIconic(existingWindow))
        {
            ShowWindow(existingWindow, SW_RESTORE);
        }
        SetForegroundWindow(existingWindow);
        FlashWindow(existingWindow, TRUE);
        std::cout << "[SingleInstanceLock] Activated existing window" << std::endl;
        return true;
    }
    return false;
#else
    return false;
#endif
}

#ifdef _WIN32
/*************************************************************************
  \brief
    Helper to find window by title pattern
*************************************************************************/
HWND GGSingleInstanceLock::FindGameWindow()
{
    HWND foundWindow = nullptr;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&foundWindow));
    return foundWindow;
}

/*************************************************************************
  \brief
    Helper to find window by title pattern
*************************************************************************/
BOOL CALLBACK GGSingleInstanceLock::EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    char windowTitle[256];
    char className[256];

    if (GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle)) > 0)
    {
        if (strstr(windowTitle, "GGPals") || strstr(windowTitle, "Order N Chaos") || strstr(windowTitle, "Order and Chaos"))
        {
            if (GetClassNameA(hwnd, className, sizeof(className)) > 0)
            {
                if (strstr(className, "GLFW"))
                {
                    *reinterpret_cast<HWND*>(lParam) = hwnd;
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}
#endif