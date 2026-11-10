/*************************************************************************
  \file       GGLuaVM.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       25 October 2025
  \version    1.0
  \brief
    LuaJIT virtual machine wrapper with hot-reload and error sandboxing.
  \details
    Encapsulates LuaJIT state management, script execution, and error
    handling for gameplay scripting. Provides hot-reload support for
    iteration and sandboxed execution to prevent script errors from
    crashing the engine.

    Design principles:
    - Single Lua state per World (no threading)
    - JIT disabled for deterministic lockstep networking
    - Protected calls (pcall) for all script execution
    - RAII-based Lua state lifecycle management

    Scripting Engine Comparison:

    LuaJIT (JIT enabled):
    - Performance: 10-50x faster than interpreter (CPU-intensive scripts)
    - Determinism: NON-DETERMINISTIC - different machines produce different results
    - Reason: JIT compiler makes optimization decisions based on runtime profiling,
              CPU architecture, and memory layout (varies per machine/run)
    - Use case: Single-player games, non-networked gameplay

    LuaJIT (JIT disabled - CURRENT CHOICE):
    - Performance: 2-3x faster than vanilla Lua 5.4
    - Determinism: DETERMINISTIC - bit-exact execution across all clients
    - Reason: Pure bytecode interpreter, no runtime optimization variability
    - Trade-off: Slower than JIT, but required for lockstep multiplayer
    - Bonus: Better performance than vanilla Lua due to optimized interpreter

    Vanilla Lua 5.4:
    - Performance: Baseline (slowest of the three)
    - Determinism: DETERMINISTIC - pure interpreter
    - Drawbacks: Slower interpreter, breaking C API changes from 5.1, less ecosystem support

    Decision: LuaJIT with JIT disabled gives us deterministic execution for
    multiplayer networking while maintaining better performance than vanilla Lua.
    Can enable JIT later for single-player/non-networked scenarios if needed.

    Note: LuaJIT 2.1 is based on Lua 5.1 language specification (not Lua 5.4).
    Version numbers are independent: LuaJIT 2.1 implements Lua 5.1 API.
  \dependencies
    lua.h, lualib.h, lauxlib.h (LuaJIT 2.1), <string>, <functional>
  \see
    GGLuaBindings.hpp, GGECSWorld.hpp
  \history
    - 1.0 (25 October 2025): Initial version with error handling and hot-reload.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <string>
#include <functional>

/*************************************************************************
  \class GGLuaVM
  \brief
    Manages LuaJIT state and script execution lifecycle.
  \details
    Wraps lua_State* with RAII, error handling, and hot-reload utilities.
    All script execution is protected via lua_pcall to sandbox errors.

    Threading model:
    - One GGLuaVM per game world (single-threaded execution)
    - No concurrent script execution (deterministic for networking)

    JIT configuration:
    - JIT disabled via jit.off() for deterministic lockstep
    - Can enable JIT for non-networked gameplay (testing/single-player)
  \warning
    Not thread-safe. Do not share GGLuaVM across threads or call methods
    concurrently. Each thread needs its own Lua state.
  \see
    GGLuaBindings, World
*************************************************************************/
class GGLuaVM
{
public:
    /*************************************************************************
      \brief
        Error callback function signature for script failures.
      \details
        Called when script compilation or execution fails. Receives error
        message and script path. Use for logging, debugging UI, or error
        aggregation for designers.
      \par Example
      \code
        luaVM.SetErrorCallback([](const std::string& msg, const std::string& path) {
            std::cerr << "[Lua Error] " << path << ": " << msg << std::endl;
            // Optionally show in editor UI
        });
      \endcode
    *************************************************************************/
    using ErrorCallback = std::function<void(const std::string& errorMsg, const std::string& scriptPath)>;

    /*************************************************************************
      \brief
        Constructs Lua VM and initializes standard libraries.
      \param[in] enableJIT
        If false, disables JIT for deterministic execution. Default: false.
      \details
        Creates new lua_State, loads standard libraries (base, string, table,
        math), and optionally disables JIT compiler. Sets error callback to
        default stderr logging.

        Standard libraries loaded:
        - base (print, type, pairs, ipairs, etc.)
        - string (string.sub, string.format, etc.)
        - table (table.insert, table.concat, etc.)
        - math (math.sin, math.sqrt, etc.)

        NOT loaded (security/determinism):
        - io (file access - use C++ API instead)
        - os (time, execute - non-deterministic)
        - debug (introspection - performance overhead)
      \note
        Constructor may throw std::runtime_error if Lua state creation fails.
    *************************************************************************/
    GGLuaVM(bool enableJIT = false);

    /*************************************************************************
      \brief
        Destructor closes Lua state and releases resources.
      \details
        Calls lua_close() to free Lua VM memory. All Lua references (tables,
        functions, etc.) are invalidated. Ensure no Lua callbacks are pending
        before destruction.
    *************************************************************************/
    ~GGLuaVM();

    // Disable copy (Lua state is unique)
    GGLuaVM(const GGLuaVM&) = delete;
    GGLuaVM& operator=(const GGLuaVM&) = delete;

    /*************************************************************************
      \brief
        Executes Lua script string in protected mode.
      \param[in] scriptCode
        Lua source code to execute.
      \param[in] chunkName
        Name for error messages (e.g., "player_ai.lua"). Default: "chunk".
      \return
        true if execution succeeded, false if compilation or runtime error.
      \details
        Compiles and executes Lua code via luaL_loadbuffer + lua_pcall.
        If error occurs, calls error callback with message and returns false.
        Game continues running (error is sandboxed).

        Use for:
        - Loading initial scripts
        - Reloading scripts (hot-reload)
        - Executing console commands (debugging)
      \par Usage
      \code
        GGLuaVM vm;
        if (!vm.ExecuteString("print('Hello from Lua!')", "console")) {
            // Error already logged via callback, game continues
        }
      \endcode
      \note
        For file-based scripts, use ExecuteFile() instead (handles file I/O).
    *************************************************************************/
    bool ExecuteString(const std::string& scriptCode, const std::string& chunkName = "chunk");

    /*************************************************************************
      \brief
        Loads and executes Lua script from file.
      \param[in] filePath
        Path to .lua file (relative or absolute).
      \return
        true if file loaded and executed successfully, false on error.
      \details
        Reads file contents, compiles via luaL_loadbuffer, executes via
        lua_pcall. File I/O errors (missing file, permissions) are reported
        via error callback.

        File reading is synchronous (blocking). For async loading, wrap in
        job system or use file watching for hot-reload.
      \par Usage
      \code
        GGLuaVM vm;
        if (vm.ExecuteFile("scripts/player_init.lua")) {
            // Script loaded, Lua globals/functions now available
        }
      \endcode
      \warning
        Relative paths are resolved from executable directory, not script
        directory. Use absolute paths or prepend asset directory.
    *************************************************************************/
    bool ExecuteFile(const std::string& filePath);

    /*************************************************************************
      \brief
        Reloads script file by clearing package.loaded cache.
      \param[in] filePath
        Path to .lua file to reload.
      \return
        true if reload succeeded, false on error.
      \details
        Hot-reload implementation:
        1. Set package.loaded[filePath] = nil (clear cache)
        2. Execute file again via ExecuteFile()

        This causes require() to re-execute the script file. For stateless
        scripts (pure functions), this is sufficient. For stateful scripts
        (global variables, entity references), you may need custom reload
        logic in the script itself:

        \code
        -- player.lua
        if onReload then
            onReload() -- Cleanup function called before reload
        end

        function onReload()
            -- Save state, unregister callbacks, etc.
        end
        \endcode
      \par Usage
      \code
        // File watcher detects change
        luaVM.ReloadScript("scripts/player_ai.lua");
        // Lua functions updated without engine restart
      \endcode
      \warning
        Reloading does not update already-bound C functions or closures.
        Only Lua-side function definitions and global variables refresh.
    *************************************************************************/
    bool ReloadScript(const std::string& filePath);

    /*************************************************************************
      \brief
        Sets error callback for script failures.
      \param[in] callback
        Function to call on Lua errors (compilation or runtime).
      \details
        Default callback logs to stderr. Override for custom error handling:
        - Show errors in editor UI
        - Aggregate errors for designers
        - Disable error spam in production builds
      \par Usage
      \code
        luaVM.SetErrorCallback([](const std::string& msg, const std::string& path) {
            EditorLog::Error("Lua Error in {}: {}", path, msg);
        });
      \endcode
    *************************************************************************/
    void SetErrorCallback(ErrorCallback callback);

    /*************************************************************************
      \brief
        Gets raw Lua state for advanced operations.
      \return
        Pointer to lua_State (owned by GGLuaVM).
      \details
        Exposes lua_State* for:
        - Registering C functions (bindings)
        - Calling Lua functions from C++
        - Stack manipulation (advanced users)

        Use this for binding ECS API.
      \warning
        Do not call lua_close() on returned pointer. GGLuaVM manages lifetime.
        Do not store this pointer - it invalidates on GGLuaVM destruction.
      \par Usage
      \code
        lua_State* L = luaVM.GetState();
        lua_register(L, "createEntity", lua_CreateEntity);
      \endcode
    *************************************************************************/
    lua_State* GetState() const { return L; }

private:
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
        Helper for ExecuteFile(). Uses C++ file streams (std::ifstream).
        Reports errors via error callback.
    *************************************************************************/
    bool ReadFileContents(const std::string& filePath, std::string& outContent);

    lua_State* L;                // Lua state (owned)
    ErrorCallback errorCallback; // Error reporting function
};