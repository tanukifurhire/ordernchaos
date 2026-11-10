/*************************************************************************
  \file       GGLuaCameraBindings.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Lua bindings for controlling the 2D game camera.
  \details
    Provides functions to get/set camera position, follow an entity,
    and clear the follow target via Lua scripts.
  \dependencies
    - GGLuaVM
    - GGGameCamera2D
    - GGSystemManagers
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <cstdint>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class GGLuaVM;

/*************************************************************************
  \brief
    Register all Camera system bindings in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \details
    Creates global "Camera" table with all grid operations:
    - Camera Get Position
    - Camera Set Position
    - Camera Set Follow Target
    - Camera Clear Follow Target
  \par Usage
  \code
    GGLuaVM luaVM;
    RegisterCameraBindings(luaVM);
    luaVM.ExecuteString("local pos = Camera.getPosition()");
  \endcode
  \note
    Must be called after GameCamera is initialized.
*************************************************************************/
void RegisterCameraBindings(GGLuaVM& luaVM);

/*************************************************************************
  \brief
    Lua: Camera.setPosition(x, y)
  \param[in] 1 (Lua stack)
    Camera x position (number)
  \param[in] 2 (Lua stack)
    Camera y position (number)
  \details
    Sets the game camera's position in world coordinates
*************************************************************************/
int lua_Camera_SetPosition(lua_State* L);

/*************************************************************************
  \brief
    Lua: Camera.getPosition() -> { x, y }
  \details
    Gets current game camera position.
  \return
    Returns world position coordinates in table with members 'x' and 'y'
*************************************************************************/
int lua_Camera_GetPosition(lua_State* L);

/*************************************************************************
  \brief
    Lua: Camera.setFollowEntity(entityHandle)
  \param[in] 1 (Lua stack)
    Entity handle (lightuserdata)
  \details
    Sets the currently followed entity for game camera
*************************************************************************/
int lua_Camera_SetFollowEntity(lua_State* L);

/*************************************************************************
  \brief
    Lua: Camera.clearFollowTarget()
  \details
    Clears follow target for game camera
*************************************************************************/
int lua_Camera_ClearFollowTarget(lua_State* L);

/*************************************************************************
  \brief
    Lua: Camera.startShake(intensity, duration)
  \param[in] 1 (Lua stack)
    intensity (number)
  \param[in] 2 (Lua stack)
	duration (number)
  \details
	Sets the camera to shake for X seconds with the given intensity. 
    Intensity controls the maximum offset
*************************************************************************/
int lua_Camera_StartShake(lua_State* L);

int lua_Camera_SetFocusEntity(lua_State* L);

int lua_Camera_ResetBounds(lua_State* L);