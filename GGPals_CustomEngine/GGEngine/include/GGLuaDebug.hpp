/*************************************************************************
  \file       GGLuaDebug.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
    Debug utilities for Lua scripts including logging and profiling.
  \details
    Provides Lua-callable logging functions, stack traces, and basic
    profiling capabilities. Integrates with GGLogger to display Lua
    messages in the engine's console. Includes error handlers for
    better debugging of script errors.
  \dependencies
    lua.h, GGLogger.hpp, <string>, <chrono>
  \see
    GGLogger, GGLuaVM, GGEventBus
  \history
    - 1.0 (1 November 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <functional>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

/*************************************************************************
  \class GGLuaDebug
  \brief
    Static utility class providing debug functionality for Lua scripts.
  \details
    All methods are static and designed to be registered as Lua C
    functions. Provides logging at different levels, stack traces for
    errors, and basic profiling for performance analysis. Integrates
    with the engine's logging system for unified output.
*************************************************************************/
class GGLuaDebug
{
public:
    /*************************************************************************
      \brief
        Lua function: Log.info(message)
      \param[in] L
        Lua state. Stack contains message string.
      \return
        Number of Lua return values (0).
      \details
        Logs an informational message from Lua. Automatically prepends
        [Lua] tag for identification in console.
      \par Lua Usage
      \code
        Log.info("Player initialized successfully")
      \endcode
    *************************************************************************/
    static int lua_Log(lua_State* L);

    /*************************************************************************
      \brief
        Lua function: Log.warning(message)
      \param[in] L
        Lua state. Stack contains message string.
      \return
        Number of Lua return values (0).
      \details
        Logs a warning message from Lua. Shows in yellow/orange in
        console for visibility.
      \par Lua Usage
      \code
        Log.warning("Invalid grid position, using default")
      \endcode
    *************************************************************************/
    static int lua_LogWarning(lua_State* L);

    /*************************************************************************
      \brief
        Lua function: Log.error(message)
      \param[in] L
        Lua state. Stack contains message string.
      \return
        Number of Lua return values (0).
      \details
        Logs an error message from Lua. Shows in red in console.
        Does not stop execution (use error() for that).
      \par Lua Usage
      \code
        Log.error("Failed to load ship configuration")
      \endcode
    *************************************************************************/
    static int lua_LogError(lua_State* L);

    /*************************************************************************
      \brief
        Lua function: Debug.stackTrace()
      \param[in] L
        Lua state.
      \return
        Number of Lua return values (1 - stack trace string).
      \details
        Returns current Lua call stack as a formatted string. Useful
        for debugging callback chains and event handlers.
      \par Lua Usage
      \code
        Log.info("Call stack: " .. Debug.stackTrace())
      \endcode
    *************************************************************************/
    static int lua_StackTrace(lua_State* L);

    /*************************************************************************
      \brief
        Error handler with stack trace for lua_pcall.
      \param[in] L
        Lua state with error message on stack.
      \return
        Number of return values (1 - enhanced error message).
      \details
        Used as error handler for protected calls. Appends stack trace
        to error messages for better debugging. Pass as 4th argument
        to lua_pcall.
      \par C++ Usage
      \code
        lua_pushcfunction(L, GGLuaDebug::ErrorHandler);
        int errorFunc = lua_gettop(L);
        lua_pcall(L, nargs, nresults, errorFunc);
      \endcode
    *************************************************************************/
    static int ErrorHandler(lua_State* L);

    /*************************************************************************
      \brief
        Lua function: Profile.start(name)
      \param[in] L
        Lua state. Stack contains profile name string.
      \return
        Number of Lua return values (0).
      \details
        Starts a named profiling timer. Can have multiple concurrent
        profiles with different names.
      \par Lua Usage
      \code
        Profile.start("pathfinding")
        -- ... expensive operation ...
        Profile.stop("pathfinding")
      \endcode
    *************************************************************************/
    static int lua_StartProfile(lua_State* L);

    /*************************************************************************
      \brief
        Lua function: Profile.stop(name)
      \param[in] L
        Lua state. Stack contains profile name string.
      \return
        Number of Lua return values (0).
      \details
        Stops a named profile and logs the elapsed time in milliseconds.
        Logs warning if profile name not found.
      \par Lua Usage
      \code
        Profile.start("AI_turn")
        CalculateAIMove()
        Profile.stop("AI_turn")  -- Logs: [Profile] AI_turn: 45.3ms
      \endcode
    *************************************************************************/
    static int lua_EndProfile(lua_State* L);

    /*************************************************************************
      \brief
        Lua function: Debug.getMemory()
      \param[in] L
        Lua state.
      \return
        Number of Lua return values (1 - memory in KB).
      \details
        Returns current Lua memory usage in kilobytes. Useful for
        detecting memory leaks in scripts.
      \par Lua Usage
      \code
        local memory = Debug.getMemory()
        Log.info("Lua memory: " .. memory .. " KB")
      \endcode
    *************************************************************************/
    static int lua_GetMemory(lua_State* L);

    /*************************************************************************
      \brief
        Register all debug functions in Lua.
      \param[in] L
        Lua state to register functions in.
      \details
        Creates global Log, Debug, and Profile tables with their
        respective functions. Call during Lua initialization.
      \par Functions Registered
        - Log.info(message)
        - Log.warning(message)
        - Log.error(message)
        - Debug.stackTrace()
        - Debug.getMemory()
        - Profile.start(name)
        - Profile.stop(name)
    *************************************************************************/
    static void RegisterDebugBindings(lua_State* L);

    /*************************************************************************
      \brief
        Set callback for Lua log messages.
      \param[in] callback
        Function to call with log messages.
      \details
        Allows integration with engine's logging system or custom
        console. Callback receives message and log level.
    *************************************************************************/
    static void SetLogCallback(std::function<void(const std::string&, int)> callback);

private:
    // Active profiling timers
    static std::unordered_map<std::string,
        std::chrono::high_resolution_clock::time_point> profileTimers;

    // Log callback
    static std::function<void(const std::string&, int)> logCallback;

    /*************************************************************************
      \brief
        Format stack trace from Lua debug info.
      \param[in] L
        Lua state.
      \return
        Formatted stack trace string.
      \details
        Walks the Lua call stack and formats function names, file names,
        and line numbers into readable trace.
    *************************************************************************/
    static std::string FormatStackTrace(lua_State* L);
};