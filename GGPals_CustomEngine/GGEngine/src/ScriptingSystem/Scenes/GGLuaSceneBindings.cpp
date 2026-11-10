/*************************************************************************
  \file       GGLuaSceneBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Exposes scene management functionality to Lua scripts.

  \details
	Provides Lua bindings to:
	- Set the active scene by name (Scene.setActive)

  \dependencies
	- GGLuaVM.hpp
	- GGEngine.hpp
	- Lua 5.x headers (lua.h, lauxlib.h, lualib.h)

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaSceneBindings.hpp>

#include <GGLuaVM.hpp>
#include <GGEngine.hpp>

#include <GGSystemManagers.hpp>

#include <iostream>

/*************************************************************************
  \brief
	Registers scene-related Lua functions (e.g., setActive) in the given
	Lua VM under the global table "Scene".
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterSceneBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	lua_pushcfunction(L, lua_Scene_SetActive);
	lua_setfield(L, -2, "setActive");

	lua_setglobal(L, "Scene");

	std::cout << "[LuaBindings] Scene management registered" << std::endl;
}

/*************************************************************************
  \brief
	Sets the active scene by name in the engine.
  \param[in] L
	Pointer to the Lua state, expects the scene name as the first argument.
  \return
	Returns true to Lua if the scene was successfully activated, false otherwise.
*************************************************************************/
int lua_Scene_SetActive(lua_State* L)
{
	const char* sceneName = luaL_checkstring(L, 1);

	Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().SetNextFrameScene(sceneName);

	return 0;
}