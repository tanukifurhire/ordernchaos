/*************************************************************************
  \file       GGLuaSceneBindings.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Exposes scene management functionality to Lua scripts.

  \details
    Provides Lua bindings for scene management:
    - Scene.setActive(sceneName)

  \dependencies
    - GGLuaVM.hpp
    - GGEngine.hpp
    - Lua headers (lua.h, lauxlib.h, lualib.h)


  \history
    - 1.0 (10 October 2025): Initial version.

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

class GGLuaVM;

/*************************************************************************
  \brief
	Registers scene-related Lua functions (e.g., setActive) in the given
	Lua VM under the global table "Scene".
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterSceneBindings(GGLuaVM& luaVM);

/*************************************************************************
  \brief
    Sets the active scene by name in the engine.
  \param[in] L
    Pointer to the Lua state, expects the scene name as the first argument.
  \return
    Returns true to Lua if the scene was successfully activated, false otherwise.
*************************************************************************/
int lua_Scene_SetActive(lua_State* L);