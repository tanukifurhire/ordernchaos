/*************************************************************************
  \file       LuaSystemBindings.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       11 December 2025
  \version    1.0
  \brief
	Exposes System bindings to lua to control pause/quit logic. Functions are
	self-explanatory.

  \details
	Provides Lua bindings to:
	- Stop the game (System.shutdown())
	- Pause the game (System.pause())
	- Resume the game (System.resume())

  \dependencies
	- GGLuaVM.hpp
	- GGEngineEventBus.hpp
	- Lua 5.x headers (lua.h, lauxlib.h, lualib.h)

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <GGEngineEventBus.hpp>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class GGLuaVM;

void RegisterSystemBindings(GGLuaVM& luaVM);

int lua_System_Shutdown(lua_State* L);

int lua_System_Pause(lua_State* L);

int lua_System_Resume(lua_State* L);

int lua_System_GetScreenDimensions(lua_State* L);