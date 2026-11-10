/*************************************************************************
  \file       GGAccessibilityShortcuts.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       29 August 2026
  \version    1.0
  \brief
    Implementation of the Windows accessibility shortcut suppression.
  \details
    See GGAccessibilityShortcuts.hpp for what this is for and why the
    user's own settings are preserved rather than overwritten.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include "GGAccessibilityShortcuts.hpp"

#ifdef _WIN32
#include <windows.h>
#include <cstdlib>
#include "GGLogger.hpp"
#endif

namespace GGAccessibilityShortcuts
{
#ifdef _WIN32

    namespace
    {
        // The user's settings as they were before we touched anything.
        // cbSize must be filled in before SystemParametersInfo will read them.
        STICKYKEYS g_savedStickyKeys{ sizeof(STICKYKEYS), 0 };
        FILTERKEYS g_savedFilterKeys{ sizeof(FILTERKEYS), 0, 0, 0, 0, 0 };
        TOGGLEKEYS g_savedToggleKeys{ sizeof(TOGGLEKEYS), 0 };

        bool g_saved = false;       // Save() has captured the originals
        bool g_suppressed = false;  // Disable() is currently in effect
        bool g_atexitHooked = false;

        /*************************************************************************
          \brief
            Restores settings from a process-exit path
          \details
            Registered with atexit the first time Disable() runs. The normal
            shutdown in GGEngine::Update already calls Restore(), so this only
            matters for an exit that bypasses it. It cannot help on a hard
            crash -- nothing can, short of a watchdog -- but it costs nothing.
        *************************************************************************/
        void RestoreOnExit()
        {
            Restore();
        }
    }

    /*************************************************************************
      \brief
        Captures the user's current accessibility settings
    *************************************************************************/
    void Save()
    {
        // Guard against a second Save() capturing our own suppressed values
        // and making the suppression permanent for the user.
        if (g_saved)
            return;

        BOOL const okSticky = SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_savedStickyKeys, 0);
        BOOL const okFilter = SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &g_savedFilterKeys, 0);
        BOOL const okToggle = SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_savedToggleKeys, 0);

        if (!okSticky || !okFilter || !okToggle)
        {
            // Partial reads are the dangerous case: restoring a struct we
            // never successfully populated would write junk into the user's
            // settings. Refuse to arm at all instead.
            GGLogger::Info("[AccessibilityShortcuts] Failed to read current settings; suppression disabled");
            return;
        }

        g_saved = true;

        GGLogger::Info("[AccessibilityShortcuts] Saved user settings (StickyKeys "
            + std::string((g_savedStickyKeys.dwFlags & SKF_STICKYKEYSON) ? "ON" : "OFF")
            + ", FilterKeys "
            + std::string((g_savedFilterKeys.dwFlags & FKF_FILTERKEYSON) ? "ON" : "OFF")
            + ", ToggleKeys "
            + std::string((g_savedToggleKeys.dwFlags & TKF_TOGGLEKEYSON) ? "ON" : "OFF")
            + ")");
    }

    /*************************************************************************
      \brief
        Stops the key patterns from activating the accessibility features
    *************************************************************************/
    void Disable()
    {
        Save();

        // Save() failed to read the originals, so there is nothing we could
        // safely put back later. Leave the user's machine alone.
        if (!g_saved || g_suppressed)
            return;

        // Sticky Keys: only clear the activation hotkey if the user does not
        // already have the feature switched on. Someone who relies on Sticky
        // Keys keeps it working inside the game.
        if ((g_savedStickyKeys.dwFlags & SKF_STICKYKEYSON) == 0)
        {
            STICKYKEYS off = g_savedStickyKeys;
            off.dwFlags &= ~SKF_HOTKEYACTIVE;    // five-Shift pattern no longer activates
            off.dwFlags &= ~SKF_CONFIRMHOTKEY;   // and no confirmation dialog
            SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &off, 0);
        }

        // Filter Keys: right Shift held for eight seconds.
        if ((g_savedFilterKeys.dwFlags & FKF_FILTERKEYSON) == 0)
        {
            FILTERKEYS off = g_savedFilterKeys;
            off.dwFlags &= ~FKF_HOTKEYACTIVE;
            off.dwFlags &= ~FKF_CONFIRMHOTKEY;
            SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &off, 0);
        }

        // Toggle Keys: Num Lock held for five seconds.
        if ((g_savedToggleKeys.dwFlags & TKF_TOGGLEKEYSON) == 0)
        {
            TOGGLEKEYS off = g_savedToggleKeys;
            off.dwFlags &= ~TKF_HOTKEYACTIVE;
            off.dwFlags &= ~TKF_CONFIRMHOTKEY;
            SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &off, 0);
        }

        g_suppressed = true;

        if (!g_atexitHooked)
        {
            atexit(RestoreOnExit);
            g_atexitHooked = true;
        }

        GGLogger::Info("[AccessibilityShortcuts] Shortcut activation suppressed while focused");
    }

    /*************************************************************************
      \brief
        Puts the user's original settings back
    *************************************************************************/
    void Restore()
    {
        if (!g_saved || !g_suppressed)
            return;

        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_savedStickyKeys, 0);
        SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &g_savedFilterKeys, 0);
        SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_savedToggleKeys, 0);

        g_suppressed = false;

        GGLogger::Info("[AccessibilityShortcuts] User settings restored");
    }

    /*************************************************************************
      \brief
        Disables or restores according to whether the game window has focus
    *************************************************************************/
    void SetGameHasFocus(bool hasFocus)
    {
        if (hasFocus)
            Disable();
        else
            Restore();
    }

    /*************************************************************************
      \brief
        Whether the shortcuts are currently suppressed by this process
    *************************************************************************/
    bool IsSuppressed()
    {
        return g_suppressed;
    }

#else

    // Non-Windows: nothing to suppress.
    void Save() {}
    void Disable() {}
    void Restore() {}
    void SetGameHasFocus(bool) {}
    bool IsSuppressed() { return false; }

#endif
}
