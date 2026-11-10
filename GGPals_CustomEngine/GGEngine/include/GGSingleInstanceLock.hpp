/*************************************************************************
  \file       GGSingleInstanceLock.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       11 November 2025
  \version    1.0
  \brief
    Prevents multiple instances of the game using a Windows mutex lock
  \details
    Uses a named mutex to ensure only one instance of the game runs.
    If a second instance launches, it detects the lock and exits after
    optionally bringing the first instance to foreground.
  \dependencies
    <windows.h> (Windows), <string>
  \see
    main.cpp
  \history
    - 1.0 (11 November 2025): Initial version with mutex lock.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

/*************************************************************************
  \brief
    Simple mutex lock to prevent multiple game instances
  \details
    Creates a system-wide mutex with a unique name. If the mutex already
    exists, another instance is running. That's it. Simple.
*************************************************************************/
class GGSingleInstanceLock
{
public:
    /*************************************************************************
      \brief
        Result of trying to acquire the lock
    *************************************************************************/
    enum class LockResult
    {
        ACQUIRED, // Got the lock, we're the only instance
        ALREADY_LOCKED, // Another instance has the lock
        FAILED // Something went wrong
    };

private:
#ifdef _WIN32
    HANDLE m_mutex; // The actual mutex handle
#endif
    std::string m_lockName; // Unique identifier for this game
    bool m_hasLock; // Do we hold the lock?

public:
    /*************************************************************************
      \brief
        Constructor
      \param[in] lockName
        Unique name for the game
    *************************************************************************/
    explicit GGSingleInstanceLock(const std::string& lockName);

    /*************************************************************************
      \brief
        Destructor
    *************************************************************************/
    ~GGSingleInstanceLock();

    /*************************************************************************
      \brief
        Attempts to acquire the single instance lock
      \return
        LockResult indicating if we got the lock
    *************************************************************************/
    LockResult TryAcquire();

    /*************************************************************************
      \brief
        Checks if we have the lock
      \return
        True if this instance holds the lock
    *************************************************************************/
    bool HasLock() const { return m_hasLock; }

    /*************************************************************************
      \brief
        Finds and activates the existing game window
      \return
        True if found and activated
      \details
        Searches for the window of the already running instance
        and brings it to foreground
    *************************************************************************/
    bool ActivateExistingWindow();

private:
    // No copying
    GGSingleInstanceLock(const GGSingleInstanceLock&) = delete;
    GGSingleInstanceLock& operator=(const GGSingleInstanceLock&) = delete;

#ifdef _WIN32
    /*************************************************************************
      \brief
        Helper to find window by title pattern
    *************************************************************************/
    static HWND FindGameWindow();
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
#endif
};