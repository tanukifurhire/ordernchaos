/*************************************************************************
  \file       GGLuaInput.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       19 January 2026
  \version    1.1
  \brief
    Implementation of input system bindings for Lua.
  \details
    Bridges GLFW input system to Lua scripts through the InputManager.
  \dependencies
    GGLuaInput.hpp, GGInputManager.hpp, GLFW/glfw3.h
  \see
    GGLuaInput.hpp
  \history
    - 1.0 (1 November 2025): Initial implementation.
    - 1.1 (19 January 2026): Added mouse wheel scroll delta support.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

// IMPORTANT: Include GLEW before any GL headers to avoid errors
#ifdef _WIN32
#include <GL/glew.h>  // Must be before GLFW
#endif

#include "GGLuaInput.hpp"
#include "GGLuaVM.hpp"
#include "GGInputManager.hpp"
#include "GGSystemManagers.hpp"
#include "GGWindow.hpp"
#include <GLFW/glfw3.h>  // After GLEW
#include <iostream>
#include <cstdio>  // For sprintf

// Rest of the implementation remains the same...

/*************************************************************************
  \brief
    Registers key code constants in Lua.
  \param[in] L
    Lua state.
  \details
    Creates global KEY_* and MOUSE_* constants for input queries.
    Maps directly to GLFW key codes for consistency.
*************************************************************************/
void RegisterInputConstants(lua_State* L)
{
    // Common keys
    lua_pushinteger(L, GLFW_KEY_SPACE);
    lua_setglobal(L, "KEY_SPACE");

    lua_pushinteger(L, GLFW_KEY_ENTER);
    lua_setglobal(L, "KEY_ENTER");

    lua_pushinteger(L, GLFW_KEY_ESCAPE);
    lua_setglobal(L, "KEY_ESCAPE");

    lua_pushinteger(L, GLFW_KEY_TAB);
    lua_setglobal(L, "KEY_TAB");

    lua_pushinteger(L, GLFW_KEY_LEFT_SHIFT);
    lua_setglobal(L, "KEY_SHIFT");

    lua_pushinteger(L, GLFW_KEY_LEFT_CONTROL);
    lua_setglobal(L, "KEY_CTRL");

    // Arrow keys
    lua_pushinteger(L, GLFW_KEY_UP);
    lua_setglobal(L, "KEY_UP");

    lua_pushinteger(L, GLFW_KEY_DOWN);
    lua_setglobal(L, "KEY_DOWN");

    lua_pushinteger(L, GLFW_KEY_LEFT);
    lua_setglobal(L, "KEY_LEFT");

    lua_pushinteger(L, GLFW_KEY_RIGHT);
    lua_setglobal(L, "KEY_RIGHT");

    // WASD keys
    lua_pushinteger(L, GLFW_KEY_W);
    lua_setglobal(L, "KEY_W");

    lua_pushinteger(L, GLFW_KEY_A);
    lua_setglobal(L, "KEY_A");

    lua_pushinteger(L, GLFW_KEY_S);
    lua_setglobal(L, "KEY_S");

    lua_pushinteger(L, GLFW_KEY_D);
    lua_setglobal(L, "KEY_D");

    // Number keys
    for (int i = 0; i <= 9; i++)
    {
        char name[16];
        sprintf(name, "KEY_%d", i);
        lua_pushinteger(L, GLFW_KEY_0 + i);
        lua_setglobal(L, name);
    }

    // Function keys
    for (int i = 1; i <= 12; i++)
    {
        char name[16];
        sprintf(name, "KEY_F%d", i);
        lua_pushinteger(L, GLFW_KEY_F1 + i - 1);
        lua_setglobal(L, name);
    }

    // Mouse buttons
    lua_pushinteger(L, 0);
    lua_setglobal(L, "MOUSE_LEFT");

    lua_pushinteger(L, 1);
    lua_setglobal(L, "MOUSE_RIGHT");

    lua_pushinteger(L, 2);
    lua_setglobal(L, "MOUSE_MIDDLE");
}

/*************************************************************************
  \brief
    Gets InputManager instance from registry.
  \param[in] L
    Lua state.
  \return
    Reference to InputManager.
  \details
    Uses the singleton pattern through SystemsRegistry.
*************************************************************************/
static Systems::InputManager& GetInputManager()
{
    return Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
}

/*************************************************************************
  \brief
    Lua: Input.isKeyPressed(keyCode)
  \details
    Queries current key state from InputManager.
*************************************************************************/
int lua_Input_IsKeyPressed(lua_State* L)
{
    int keyCode = static_cast<int>(luaL_checkinteger(L, 1));

    bool pressed = GetInputManager().GGInputCheckCurr(keyCode);
    lua_pushboolean(L, pressed);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.isKeyTriggered(keyCode)
  \details
    Checks if key was just pressed this frame.
*************************************************************************/
int lua_Input_IsKeyTriggered(lua_State* L)
{
    int keyCode = static_cast<int>(luaL_checkinteger(L, 1));

    bool triggered = GetInputManager().GGInputCheckTriggered(keyCode);
    lua_pushboolean(L, triggered);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.isKeyReleased(keyCode)
  \details
    Checks if key was just released this frame.
*************************************************************************/
int lua_Input_IsKeyReleased(lua_State* L)
{
    int keyCode = static_cast<int>(luaL_checkinteger(L, 1));

    bool released = GetInputManager().GGInputCheckReleased(keyCode);
    lua_pushboolean(L, released);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.getMousePosition()
  \details
    Returns mouse position in screen coordinates.
*************************************************************************/
int lua_Input_GetMousePosition(lua_State* L)
{
    double x = 0, y = 0;

    GetInputManager().GetMousePosition(x, y);

    // Return as table
    lua_newtable(L);

    lua_pushnumber(L, x);
    lua_setfield(L, -2, "x");

    lua_pushnumber(L, y);
    lua_setfield(L, -2, "y");

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.getMouseWorldPosition()
  \details
    Transforms mouse screen position to world coordinates.
    For 2D games, this accounts for camera offset and zoom.
*************************************************************************/
int lua_Input_GetMouseWorldPosition(lua_State* L)
{
    glm::vec2 pos = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().GetMouseWorldPosition();

    // Return as table
    lua_newtable(L);

    lua_pushnumber(L, pos.x);
    lua_setfield(L, -2, "x");

    lua_pushnumber(L, pos.y);
    lua_setfield(L, -2, "y");

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.isMouseButtonPressed(button)
  \details
    Queries current mouse button state.
*************************************************************************/
int lua_Input_IsMouseButtonPressed(lua_State* L)
{
    int button = static_cast<int>(luaL_checkinteger(L, 1));

    // Map to GLFW mouse button codes
    int glfwButton = GLFW_MOUSE_BUTTON_LEFT;
    if (button == 0) glfwButton = GLFW_MOUSE_BUTTON_LEFT;
    else if (button == 1) glfwButton = GLFW_MOUSE_BUTTON_RIGHT;
    else if (button == 2) glfwButton = GLFW_MOUSE_BUTTON_MIDDLE;

    bool pressed = GetInputManager().GGInputCheckCurr(glfwButton);
    lua_pushboolean(L, pressed);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.isMouseButtonTriggered(button)
  \details
    Checks if mouse button was just clicked this frame.
*************************************************************************/
int lua_Input_IsMouseButtonTriggered(lua_State* L)
{
    int button = static_cast<int>(luaL_checkinteger(L, 1));

    // Map to GLFW mouse button codes
    int glfwButton = GLFW_MOUSE_BUTTON_LEFT;
    if (button == 0) glfwButton = GLFW_MOUSE_BUTTON_LEFT;
    else if (button == 1) glfwButton = GLFW_MOUSE_BUTTON_RIGHT;
    else if (button == 2) glfwButton = GLFW_MOUSE_BUTTON_MIDDLE;

    bool triggered = GetInputManager().GGInputCheckTriggered(glfwButton);
    lua_pushboolean(L, triggered);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.isMouseButtonReleased(button)
  \details
    Checks if mouse button was just released this frame.
*************************************************************************/
int lua_Input_IsMouseButtonReleased(lua_State* L)
{
    int button = static_cast<int>(luaL_checkinteger(L, 1));

    bool released = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>().GGInputCheckReleased(button);

    lua_pushboolean(L, released);

    return 1;
}

/*************************************************************************
  \brief
    Lua: Input.getMouseWheelDelta()
  \details
    Returns mouse wheel scroll delta since last frame.
*************************************************************************/
int lua_Input_GetMouseWheelDelta(lua_State* L)
{
    int delta = 0;
    GetInputManager().GGInputMouseWheelDelta(&delta);

    lua_pushinteger(L, delta);

    return 1;
}

/*************************************************************************
  \brief
    Registers all input functions in Lua.
  \param[in] luaVM
    Lua VM instance.
  \details
    Creates global Input table and registers all functions and constants.
*************************************************************************/
void RegisterInputBindings(GGLuaVM& luaVM)
{
    lua_State* L = luaVM.GetState();

    // Register constants first
    RegisterInputConstants(L);

    // Create Input table
    lua_newtable(L);

    // Keyboard functions
    lua_pushcfunction(L, lua_Input_IsKeyPressed);
    lua_setfield(L, -2, "isKeyPressed");

    lua_pushcfunction(L, lua_Input_IsKeyTriggered);
    lua_setfield(L, -2, "isKeyTriggered");

    lua_pushcfunction(L, lua_Input_IsKeyReleased);
    lua_setfield(L, -2, "isKeyReleased");

    // Mouse functions
    lua_pushcfunction(L, lua_Input_GetMousePosition);
    lua_setfield(L, -2, "getMousePosition");

    lua_pushcfunction(L, lua_Input_GetMouseWorldPosition);
    lua_setfield(L, -2, "getMouseWorldPosition");

    lua_pushcfunction(L, lua_Input_IsMouseButtonPressed);
    lua_setfield(L, -2, "isMouseButtonPressed");

    lua_pushcfunction(L, lua_Input_IsMouseButtonTriggered);
    lua_setfield(L, -2, "isMouseButtonTriggered");

    lua_pushcfunction(L, lua_Input_IsMouseButtonReleased);
    lua_setfield(L, -2, "isMouseButtonReleased");

    lua_pushcfunction(L, lua_Input_GetMouseWheelDelta);
    lua_setfield(L, -2, "getMouseWheelDelta");

    // Set as global
    lua_setglobal(L, "Input");

    std::cout << "[LuaInput] Input bindings registered" << std::endl;
}