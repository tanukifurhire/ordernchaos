/*************************************************************************
  \file       GGAccessibilityShortcuts.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       29 August 2026
  \version    1.0
  \brief
    Suppresses the Windows accessibility shortcut keys while the game
    holds focus, so gameplay input cannot trigger them.
  \details
    Windows watches for three key patterns and pops a modal dialog when it
    sees one:

      Sticky Keys   Shift pressed five times in a row
      Filter Keys   Right Shift held for eight seconds
      Toggle Keys   Num Lock held for five seconds

    Any of these steals focus from a fullscreen game and, on the Sticky Keys
    path, can leave modifier keys latched afterwards. Order N Chaos reads
    Shift for lobby text entry, so the five-tap pattern is reachable in
    ordinary play. The Steam submission requirements call this out
    explicitly under INPUT REQUIREMENTS.

    What this does NOT do is turn the features off for the user. It clears
    only the HOTKEYACTIVE and CONFIRMHOTKEY flags, which are what make the
    key pattern *activate* the feature. A player who has deliberately
    enabled Sticky Keys in Windows keeps it: the ON flags are checked first
    and their settings are left untouched. This is the behaviour Microsoft
    documents for games in "Guidelines for Fullscreen Applications".

    The user's original settings are captured once at Save() and restored
    verbatim by Restore(). Restore must run when the game loses focus and
    again at shutdown, otherwise the suppression outlives the process and
    the player is left with a silently broken accessibility feature.

    Every function is a no-op on non-Windows platforms and safe to call in
    any order or more than once.
  \dependencies
    <windows.h> (Windows only)
  \see
    GGInterruptHandler.hpp, GGEngine.cpp
  \history
    - 1.0 (29 August 2026): Initial version for Steam submission input
                            requirements.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

/*************************************************************************
  \brief
    Windows accessibility shortcut suppression for the focused game window
  \details
    Free functions rather than a system class: this owns three process-wide
    OS settings, has no per-frame work, and needs to be callable from the
    GLFW focus callback where no system registry is reachable.
*************************************************************************/
namespace GGAccessibilityShortcuts
{
    /*************************************************************************
      \brief
        Captures the user's current accessibility settings
      \details
        Call once, before the first Disable(). Repeat calls are ignored, so
        a Disable/Restore cycle can never overwrite the saved state with the
        suppressed state -- which would make the suppression permanent.
    *************************************************************************/
    void Save();

    /*************************************************************************
      \brief
        Stops the key patterns from activating the accessibility features
      \details
        Clears SKF_HOTKEYACTIVE / SKF_CONFIRMHOTKEY (and the Filter Keys and
        Toggle Keys equivalents). Features the user already has switched on
        are left alone. Calls Save() first if it has not run yet.
    *************************************************************************/
    void Disable();

    /*************************************************************************
      \brief
        Puts the user's original settings back
      \details
        Safe to call without a matching Disable(). Must run on focus loss
        and at shutdown.
    *************************************************************************/
    void Restore();

    /*************************************************************************
      \brief
        Disables or restores according to whether the game window has focus
      \param[in] hasFocus
        True when the game window gained focus, false when it lost focus
      \par Usage
      \code
        // In the GLFW window focus callback:
        GGAccessibilityShortcuts::SetGameHasFocus(focused == GLFW_TRUE);
      \endcode
    *************************************************************************/
    void SetGameHasFocus(bool hasFocus);

    /*************************************************************************
      \brief
        Whether the shortcuts are currently suppressed by this process
      \return
        True if Disable() is in effect, false otherwise
    *************************************************************************/
    bool IsSuppressed();
}
