/*************************************************************************
  \file       LuaSystemBindings.cpp
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
	- LuaSystemBindings.hpp
	- GGSystemManagers
	- Lua 5.x headers (lua.h, lauxlib.h, lualib.h)

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <LuaSystemBindings.hpp>
#include <GGLuaVM.hpp>

#include <GGSystemManagers.hpp>
#include <GGEngineRender.hpp>


void RegisterSystemBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	lua_pushcfunction(L, lua_System_Shutdown);
	lua_setfield(L, -2, "shutdown");

	lua_pushcfunction(L, lua_System_Pause);
	lua_setfield(L, -2, "pause");

	lua_pushcfunction(L, lua_System_Resume);
	lua_setfield(L, -2, "resume");

	lua_pushcfunction(L, lua_System_GetScreenDimensions);
	lua_setfield(L, -2, "getScreenDimensions");

	lua_setglobal(L, "System");

	std::cout << "[System] System bindings registered" << std::endl;
}

int lua_System_Shutdown(lua_State*)
{
	GlobalEventBus::shutdownEvent.Emit();
	return 0;
}

int lua_System_Pause(lua_State*)
{
	Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().SetTimeScale(0.f);

	return 0;
}

int lua_System_Resume(lua_State*)
{
	Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().SetTimeScale(1.f);

	return 0;
}

int lua_System_GetScreenDimensions(lua_State* L)
{
	auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
	GGCamera2D* camera = renderMgr.GetActiveCamera();

	if (!camera)
	{
		lua_pushnil(L);
		return 1;
	}

	glm::vec2 viewportSize = camera->GetViewportSize();

	lua_newtable(L);
	lua_pushnumber(L, viewportSize.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, viewportSize.y);
	lua_setfield(L, -2, "y");

	return 1;
}