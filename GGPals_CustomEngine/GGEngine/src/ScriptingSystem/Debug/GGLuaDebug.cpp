/*************************************************************************
  \file       GGLuaDebug.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
    Implementation of debug utilities for Lua scripts.
  \details
    Implements logging, profiling, and error handling functions that
    can be called from Lua scripts for debugging and development.
  \dependencies
    GGLuaDebug.hpp, GGLogger.hpp, lua.h
  \see
    GGLuaDebug.hpp
  \history
    - 1.0 (1 November 2025): Initial implementation.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGLuaDebug.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <GGLogger.hpp>

// Static member definitions
std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point>
GGLuaDebug::profileTimers;

std::function<void(const std::string&, int)> GGLuaDebug::logCallback = nullptr;

/*************************************************************************
  \brief
    Lua function to log informational messages.
  \param[in] L
    Lua state with message on stack.
  \return
    0 (no return values).
*************************************************************************/
int GGLuaDebug::lua_Log(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    std::string message = "[Lua] ";
    message += msg;

    std::cout << message << std::endl;
    GGLogger::Info(message);

    if (logCallback)
    {
        logCallback(message, 0);  // 0 = info level
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua function to log warning messages.
  \param[in] L
    Lua state with message on stack.
  \return
    0 (no return values).
*************************************************************************/
int GGLuaDebug::lua_LogWarning(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    std::string message = "[Lua] ";
    message += msg;

    std::cout << "WARNING: " << message << std::endl;
    GGLogger::Warning(message);

    if (logCallback)
    {
        logCallback(message, 1);  // 1 = warning level
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua function to log error messages.
  \param[in] L
    Lua state with message on stack.
  \return
    0 (no return values).
*************************************************************************/
int GGLuaDebug::lua_LogError(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    std::string message = "[Lua] ";
    message += msg;

    std::cout << "ERROR: " << message << std::endl;
    GGLogger::Error(message);

    if (logCallback)
    {
        logCallback(message, 2);  // 2 = error level
    }

    return 0;
}

/*************************************************************************
  \brief
    Lua function to get current stack trace.
  \param[in] L
    Lua state.
  \return
    1 (stack trace string).
*************************************************************************/
int GGLuaDebug::lua_StackTrace(lua_State* L)
{
    std::string trace = FormatStackTrace(L);
    lua_pushstring(L, trace.c_str());
    return 1;
}

/*************************************************************************
  \brief
    Error handler that adds stack trace to error messages.
  \param[in] L
    Lua state with error message on stack.
  \return
    1 (enhanced error message).
*************************************************************************/
int GGLuaDebug::ErrorHandler(lua_State* L)
{
    // Get error message
    const char* msg = lua_tostring(L, 1);
    if (!msg)
    {
        msg = "(no error message)";
    }

    // Build enhanced error message
    std::stringstream ss;
    ss << msg << "\n";
    ss << "Stack trace:\n";
    ss << FormatStackTrace(L);

    // Replace error message on stack
    lua_pushstring(L, ss.str().c_str());

    return 1;
}

/*************************************************************************
  \brief
    Start a named profiling timer.
  \param[in] L
    Lua state with profile name on stack.
  \return
    0 (no return values).
*************************************************************************/
int GGLuaDebug::lua_StartProfile(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    profileTimers[name] = std::chrono::high_resolution_clock::now();

    return 0;
}

/*************************************************************************
  \brief
    Stop a named profiling timer and log elapsed time.
  \param[in] L
    Lua state with profile name on stack.
  \return
    0 (no return values).
*************************************************************************/
int GGLuaDebug::lua_EndProfile(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    auto it = profileTimers.find(name);
    if (it == profileTimers.end())
    {
        std::cout << "WARNING: " << std::string("[Profile] No timer found for '") + name + "'" << std::endl;
        return 0;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>
        (endTime - it->second);

    double milliseconds = duration.count() / 1000.0;

    std::stringstream ss;
    ss << "[Profile] " << name << ": "
        << std::fixed << std::setprecision(2) << milliseconds << "ms";

    std::cout << ss.str() << std::endl;

    profileTimers.erase(it);

    return 0;
}

/*************************************************************************
  \brief
    Get current Lua memory usage in KB.
  \param[in] L
    Lua state.
  \return
    1 (memory usage number).
*************************************************************************/
int GGLuaDebug::lua_GetMemory(lua_State* L)
{
    int kb = lua_gc(L, LUA_GCCOUNT, 0);
    int bytes = lua_gc(L, LUA_GCCOUNTB, 0);

    double memory = kb + (bytes / 1024.0);

    lua_pushnumber(L, memory);
    return 1;
}

/*************************************************************************
  \brief
    Register all debug functions in Lua global namespace.
  \param[in] L
    Lua state.
*************************************************************************/
void GGLuaDebug::RegisterDebugBindings(lua_State* L)
{
    // Create Log table
    lua_newtable(L);

    lua_pushcfunction(L, lua_Log);
    lua_setfield(L, -2, "info");

    lua_pushcfunction(L, lua_LogWarning);
    lua_setfield(L, -2, "warning");

    lua_pushcfunction(L, lua_LogError);
    lua_setfield(L, -2, "error");

    lua_setglobal(L, "Log");

    // Create Debug table
    lua_newtable(L);

    lua_pushcfunction(L, lua_StackTrace);
    lua_setfield(L, -2, "stackTrace");

    lua_pushcfunction(L, lua_GetMemory);
    lua_setfield(L, -2, "getMemory");

    lua_setglobal(L, "Debug");

    // Create Profile table
    lua_newtable(L);

    lua_pushcfunction(L, lua_StartProfile);
    lua_setfield(L, -2, "start");

    lua_pushcfunction(L, lua_EndProfile);
    lua_setfield(L, -2, "stop");

    lua_setglobal(L, "Profile");

    std::cout << "[LuaDebug] Debug bindings registered (Log, Debug, Profile)" << std::endl;
}

/*************************************************************************
  \brief
    Set callback for Lua log messages.
  \param[in] callback
    Function to call with log messages.
*************************************************************************/
void GGLuaDebug::SetLogCallback(std::function<void(const std::string&, int)> callback)
{
    logCallback = callback;
}

/*************************************************************************
  \brief
    Format a readable stack trace from current Lua state.
  \param[in] L
    Lua state.
  \return
    Formatted stack trace string.
*************************************************************************/
std::string GGLuaDebug::FormatStackTrace(lua_State* L)
{
    std::stringstream ss;

    lua_Debug ar;
    int level = 0;

    while (lua_getstack(L, level, &ar))
    {
        lua_getinfo(L, "nSl", &ar);

        ss << "  [" << level << "] ";

        // Function name
        if (ar.name)
        {
            ss << ar.name << "()";
        }
        else if (ar.what)
        {
            if (strcmp(ar.what, "main") == 0)
            {
                ss << "main chunk";
            }
            else if (strcmp(ar.what, "C") == 0)
            {
                ss << "(C function)";
            }
            else if (strcmp(ar.what, "Lua") == 0)
            {
                ss << "(Lua function)";
            }
            else
            {
                ss << "(" << ar.what << ")";
            }
        }
        else
        {
            ss << "(unknown)";
        }

        // Source location
        if (ar.source && ar.currentline > 0)
        {
            // Clean up source name (remove @ prefix if present)
            const char* source = ar.source;
            if (source[0] == '@')
                source++;

            // Extract just filename if it's a path
            const char* filename = source;
            const char* slash = strrchr(source, '/');
            const char* backslash = strrchr(source, '\\');

            if (slash && (!backslash || slash > backslash))
                filename = slash + 1;
            else if (backslash)
                filename = backslash + 1;

            ss << " at " << filename << ":" << ar.currentline;
        }

        ss << "\n";

        level++;

        // Limit depth to prevent huge traces
        if (level >= 20)
        {
            ss << "  ... (trace truncated)\n";
            break;
        }
    }

    return ss.str();
}