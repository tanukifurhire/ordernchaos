/*************************************************************************
  \file       GGLuaInput.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       19 January 2026
  \version    1.1
  \brief
    Input system bindings for Lua gameplay scripts.
  \details
    Provides direct access to keyboard and mouse input from Lua scripts,
    enabling responsive gameplay without C++ intervention. Supports both
    immediate state queries and edge detection (triggered/released).

    All input polling happens on the main thread through the existing
    InputManager system, ensuring thread safety and consistency with
    the rest of the engine.
  \dependencies
    lua.h, GGLuaVM.hpp, GGInputManager.hpp, GLFW/glfw3.h
  \see
    GGInputManager, GGLuaVM
  \history
    - 1.0 (1 November 2025): Initial implementation with keyboard and mouse.
    - 1.1 (19 January 2026): Added mouse wheel scroll delta support.
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

// Forward declarations
class GGLuaVM;

/*************************************************************************
  \brief
    Registers Input API in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \details
    Creates global "Input" table with functions for keyboard and mouse.
    Key codes are also registered as global constants (e.g., KEY_SPACE).

    Keyboard functions:
    - Input.isKeyPressed(keyCode) - Current state
    - Input.isKeyTriggered(keyCode) - Just pressed
    - Input.isKeyReleased(keyCode) - Just released

    Mouse functions:
    - Input.getMousePosition() - Screen coordinates
    - Input.getMouseWorldPosition() - World coordinates
    - Input.isMouseButtonPressed(button) - Current state
    - Input.isMouseButtonTriggered(button) - Just clicked

    Mouse buttons: 0=left, 1=right, 2=middle
  \par Usage
  \code
    RegisterInputBindings(luaVM);

    // In Lua:
    if Input.isKeyTriggered(KEY_SPACE) then
        fireWeapon()
    end

    local pos = Input.getMousePosition()
    print("Mouse at", pos.x, pos.y)
  \endcode
  \note
    Input state is only valid during Update() calls. Polling outside
    the main game loop may return stale or undefined data.
*************************************************************************/
void RegisterInputBindings(GGLuaVM& luaVM);

/*************************************************************************
  \brief
    Registers GLFW key constants as Lua globals.
  \param[in] L
    Lua state.
  \details
    Creates KEY_* constants matching GLFW definitions:
    KEY_SPACE, KEY_ENTER, KEY_ESCAPE, KEY_W, KEY_A, KEY_S, KEY_D, etc.

    Also registers mouse button constants:
    MOUSE_LEFT (0), MOUSE_RIGHT (1), MOUSE_MIDDLE (2)
*************************************************************************/
void RegisterInputConstants(lua_State* L);

/*************************************************************************
  KEYBOARD INPUT FUNCTIONS
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Input.isKeyPressed(keyCode) -> boolean
  \param[in] 1 (Lua stack)
    Key code (integer, e.g., KEY_SPACE).
  \return (Lua stack)
    Boolean true if key is currently held down.
  \details
    Queries current frame's key state. True for entire duration of press.
  \par Lua Usage
  \code
    -- Continuous movement while held
    if Input.isKeyPressed(KEY_W) then
        player.y = player.y + speed * dt
    end
  \endcode
*************************************************************************/
int lua_Input_IsKeyPressed(lua_State* L);

/*************************************************************************
  \brief
    Lua: Input.isKeyTriggered(keyCode) -> boolean
  \param[in] 1 (Lua stack)
    Key code (integer).
  \return (Lua stack)
    Boolean true if key was pressed this frame.
  \details
    Edge detection - only true on the frame the key goes from up to down.
    Useful for single actions like jumping or menu selection.
  \par Lua Usage
  \code
    -- Single action on press
    if Input.isKeyTriggered(KEY_SPACE) then
        jump()
    end
  \endcode
*************************************************************************/
int lua_Input_IsKeyTriggered(lua_State* L);

/*************************************************************************
  \brief
    Lua: Input.isKeyReleased(keyCode) -> boolean
  \param[in] 1 (Lua stack)
    Key code (integer).
  \return (Lua stack)
    Boolean true if key was released this frame.
  \details
    Edge detection - only true on the frame the key goes from down to up.
    Useful for charge-and-release mechanics.
  \par Lua Usage
  \code
    -- Release to fire charged shot
    if Input.isKeyReleased(KEY_SPACE) then
        fireChargedShot(chargeLevel)
    end
  \endcode
*************************************************************************/
int lua_Input_IsKeyReleased(lua_State* L);

/*************************************************************************
  MOUSE INPUT FUNCTIONS
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Input.getMousePosition() -> {x, y}
  \return (Lua stack)
    Table with x, y fields in screen coordinates.
  \details
    Returns mouse position in screen space (pixels from top-left).
    Origin (0,0) is top-left corner of window.
  \par Lua Usage
  \code
    local mouse = Input.getMousePosition()
    if mouse.x < 100 and mouse.y < 50 then
        -- Mouse in top-left region
    end
  \endcode
*************************************************************************/
int lua_Input_GetMousePosition(lua_State* L);

/*************************************************************************
  \brief
    Lua: Input.getMouseWorldPosition() -> {x, y}
  \return (Lua stack)
    Table with x, y fields in world coordinates.
  \details
    Transforms mouse screen position to world space using current camera.
    Accounts for camera position, zoom, and coordinate system.
  \par Lua Usage
  \code
    local worldPos = Input.getMouseWorldPosition()
    local gridX = math.floor(worldPos.x / CELL_SIZE)
    local gridY = math.floor(worldPos.y / CELL_SIZE)
  \endcode
  \note
    Assumes 2D world with orthographic projection. Returns center of
    screen in world space if no camera is active.
*************************************************************************/
int lua_Input_GetMouseWorldPosition(lua_State* L);

/*************************************************************************
  \brief
    Lua: Input.isMouseButtonPressed(button) -> boolean
  \param[in] 1 (Lua stack)
    Mouse button (0=left, 1=right, 2=middle).
  \return (Lua stack)
    Boolean true if button is currently held.
  \details
    Current state query - true for entire duration of press.
  \par Lua Usage
  \code
    -- Drag while left button held
    if Input.isMouseButtonPressed(MOUSE_LEFT) then
        dragObject(selectedEntity)
    end
  \endcode
*************************************************************************/
int lua_Input_IsMouseButtonPressed(lua_State* L);

/*************************************************************************
  \brief
    Lua: Input.isMouseButtonTriggered(button) -> boolean
  \param[in] 1 (Lua stack)
    Mouse button (0=left, 1=right, 2=middle).
  \return (Lua stack)
    Boolean true if button was clicked this frame.
  \details
    Edge detection - only true on the frame the button goes down.
  \par Lua Usage
  \code
    -- Single click action
    if Input.isMouseButtonTriggered(MOUSE_LEFT) then
        local pos = Input.getMouseWorldPosition()
        Event.emitLocal("CellClicked", {x = pos.x, y = pos.y})
    end
  \endcode
*************************************************************************/
int lua_Input_IsMouseButtonTriggered(lua_State* L);

/*************************************************************************
  \brief
    Lua: Input.isMouseButtonReleased(button) -> boolean
  \param[in] 1 (Lua stack)
    Mouse button (0=left, 1=right, 2=middle).
  \return (Lua stack)
    Boolean true if button was released this frame.
  \details
    Edge detection - only true on the frame the button goes up.
  \par Lua Usage
  \code
    -- Complete drag operation
    if Input.isMouseButtonReleased(MOUSE_LEFT) then
        dropObject(draggedEntity)
    end
  \endcode
*************************************************************************/
int lua_Input_IsMouseButtonReleased(lua_State* L);
/*************************************************************************
  \brief
    Lua: Input.getMouseWheelDelta() -> number
  \return (Lua stack)
    Integer representing mouse wheel scroll delta.
    Positive values indicate scrolling up (away from user).
    Negative values indicate scrolling down (towards user).
    Zero indicates no scrolling since last frame.
  \details
    Returns the accumulated mouse wheel scroll delta since the last frame.
    The delta is automatically reset after retrieval, ensuring frame-accurate
    scroll values. Useful for implementing scrollable lists, zoom controls,
    or camera panning.
  \par Lua Usage
  \code
    -- Scroll through list
    local delta = Input.getMouseWheelDelta()
    if delta > 0 then
        scrollUp()
    elseif delta < 0 then
        scrollDown()
    end

    -- Zoom camera
    local scroll = Input.getMouseWheelDelta()
    camera.zoom = camera.zoom + scroll * 0.1
  \endcode
  \note
    Delta values typically range from -1 to +1 per scroll notch, but
    can vary depending on input device and OS settings.
*************************************************************************/
int lua_Input_GetMouseWheelDelta(lua_State* L);