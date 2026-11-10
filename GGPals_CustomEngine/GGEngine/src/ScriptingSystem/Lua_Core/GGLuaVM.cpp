/*************************************************************************
  \file       GGLuaVM.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       25 October 2025
  \version    1.0
  \brief
    Implementation of LuaJIT virtual machine wrapper.
  \details
    Implements Lua state management, script execution, error handling,
    and hot-reload functionality. Uses protected calls (lua_pcall) for
    all script execution to prevent crashes from Lua errors.

    JIT is disabled by default via jit.off() for deterministic lockstep
    networking in multiplayer games. This ensures bit-exact execution
    across all clients without runtime optimization variability.
  \dependencies
    GGLuaVM.hpp, lua.h, lualib.h, lauxlib.h, <fstream>, <sstream>, <iostream>
  \see
    GGLuaVM.hpp, GGLuaBindings.hpp
  \history
    - 1.0 (25 October 2025): Initial implementation with error sandboxing.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGLuaVM.hpp"
#include <RPCRegistry.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

/*************************************************************************
  \brief
    Constructs Lua VM and initializes standard libraries.
  \param[in] enableJIT
    If false, disables JIT for deterministic execution.
  \details
    Creates new lua_State and loads standard libraries. If JIT is disabled,
    executes "jit.off()" to force pure interpreter mode for deterministic
    lockstep networking.
*************************************************************************/
GGLuaVM::GGLuaVM(bool enableJIT)
    : L(nullptr)
{
    // Create new Lua state
    L = luaL_newstate();
    if (!L)
    {
        throw std::runtime_error("Failed to create Lua state");
    }

    // Load standard libraries
    luaL_openlibs(L);

    // Disable JIT if requested (for deterministic networking)
    if (!enableJIT)
    {
        ExecuteString("if jit then jit.off() end", "jit_disable");
    }

    // Set default error callback
    errorCallback = [](const std::string& msg, const std::string& path)
        {
            std::cout << "[Lua Error] " << path << ": " << msg << std::endl;
        };
}

/*************************************************************************
  \brief
    Destructor closes Lua state and releases resources.
  \details
    Drops every RPC handler reference before closing, then calls lua_close()
    to free all Lua VM memory. All Lua references become invalid after this
    point.
  \note
    The RPC registry holds LUA_REGISTRYINDEX references into this state. They
    must be released here, otherwise a subsequent VM would inherit stale
    reference IDs that could resolve to unrelated values.
  \see
    GGNetworking::RPCRegistry::Clear
*************************************************************************/
GGLuaVM::~GGLuaVM()
{
    if (L)
    {
        GGNetworking::GetRPCRegistry().Clear(L);

        lua_close(L);
        L = nullptr;
    }
}

/*************************************************************************
  \brief
    Executes Lua script string in protected mode.
  \param[in] scriptCode
    Lua source code to execute.
  \param[in] chunkName
    Name for error messages.
  \return
    true if execution succeeded, false on error.
  \details
    Compiles and executes Lua code via luaL_loadbuffer + lua_pcall.
    Errors are caught and reported via error callback without crashing.
*************************************************************************/
bool GGLuaVM::ExecuteString(const std::string& scriptCode, const std::string& chunkName)
{
    // Compile script
    if (luaL_loadbuffer(L, scriptCode.c_str(), scriptCode.size(), chunkName.c_str()) != LUA_OK)
    {
        // Compilation error
        const char* errorMsg = lua_tostring(L, -1);
        if (errorCallback)
        {
            errorCallback(errorMsg ? errorMsg : "Unknown compilation error", chunkName);
        }
        lua_pop(L, 1);
        return false;
    }

    // Execute script
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)
    {
        // Runtime error
        const char* errorMsg = lua_tostring(L, -1);
        if (errorCallback)
        {
            errorCallback(errorMsg ? errorMsg : "Unknown runtime error", chunkName);
        }
        lua_pop(L, 1);
        return false;
    }

    return true;
}

/*************************************************************************
  \brief
    Loads and executes Lua script from file.
  \param[in] filePath
    Path to .lua file.
  \return
    true if file loaded and executed successfully, false on error.
  \details
    Reads file contents via ReadFileContents, then executes via ExecuteString.
*************************************************************************/
bool GGLuaVM::ExecuteFile(const std::string& filePath)
{
    // Read file contents
    std::string fileContents;
    if (!ReadFileContents(filePath, fileContents))
    {
        if (errorCallback)
        {
            errorCallback("Failed to read file", filePath);
        }
        return false;
    }

    // Execute as string
    return ExecuteString(fileContents, filePath);
}

/*************************************************************************
  \brief
    Reloads script file by clearing package.loaded cache.
  \param[in] filePath
    Path to .lua file to reload.
  \return
    true if reload succeeded, false on error.
  \details
    Clears package.loaded cache entry then re-executes file for hot-reload.
*************************************************************************/
bool GGLuaVM::ReloadScript(const std::string& filePath)
{
    // Clear package.loaded cache
    std::string clearCache = "package.loaded['" + filePath + "'] = nil";
    ExecuteString(clearCache, "reload_clear_cache");

    // Re-execute file
    return ExecuteFile(filePath);
}

/*************************************************************************
  \brief
    Sets error callback for script failures.
  \param[in] callback
    Function to call on Lua errors.
  \details
    Overrides default stderr logging with custom error handler.
*************************************************************************/
void GGLuaVM::SetErrorCallback(ErrorCallback callback)
{
    errorCallback = callback;
}

/*************************************************************************
  \brief
    Reads entire file into string.
  \param[in] filePath
    Path to file to read.
  \param[out] outContent
    String to store file contents.
  \return
    true if file read successfully, false on I/O error.
  \details
    Helper for ExecuteFile(). Uses std::ifstream for synchronous file reading.
*************************************************************************/
bool GGLuaVM::ReadFileContents(const std::string& filePath, std::string& outContent)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    outContent = buffer.str();

    return true;
}