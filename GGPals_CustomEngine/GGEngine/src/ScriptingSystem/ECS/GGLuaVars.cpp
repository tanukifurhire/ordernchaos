/*************************************************************************
  \file       GGLuaVars.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Exposes ECS script variable access to Lua (set/get integer variables).

  \details
	Provides Lua bindings for interacting with ECS script instance variables:
	- Vars.setInt(instanceID, index, name, value)
	- Vars.getInt(instanceID, index)

  \dependencies
	- GGLuaVM.hpp
	- GGLuaBindings.hpp
	- GGSystemManagers.hpp
	- Lua headers (lua.h, lauxlib.h, lualib.h)
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaVars.hpp>
#include <GGLuaVM.hpp>
#include <GGLuaBindings.hpp>
#include <GGSystemManagers.hpp>

#include <iostream>

/*************************************************************************
  \brief
	Registers Lua functions for variable access (setInt/getInt) under the
	global table "Vars".
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterVarBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	lua_pushcfunction(L, lua_Vars_SetInt);
	lua_setfield(L, -2, "setInt");

	lua_pushcfunction(L, lua_Vars_GetInt);
	lua_setfield(L, -2, "getInt");

	lua_setglobal(L, "Vars");

	std::cout << "[LuaVars] Vars bindings registered" << std::endl;
}

/*************************************************************************
  \brief
	Sets an integer variable in a script instance.
  \param[in] L
	Lua state pointer. Expects:
	  1. instance ID (integer)
	  2. variable index (integer)
	  3. variable name (string)
	  4. value to set (integer)
  \return
	Returns 1 to Lua (success, not used further).
*************************************************************************/
int lua_Vars_SetInt(lua_State* L)
{
	size_t instance = static_cast<size_t>(luaL_checkinteger(L, 1));
	size_t index = static_cast<size_t>(luaL_checkinteger(L, 2));
	char const* name = luaL_checkstring(L, 3);
	int32_t value = static_cast<int32_t>(luaL_checkinteger(L, 4));

	std::cout << "SetInt: instance " << instance << ", index: " << index << ", name: " << name << ", value: " << value << std::endl;

	Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().SetIntVar(instance, index, name, value);

	return 1;
}

/*************************************************************************
  \brief
	Retrieves an integer variable from a script instance.
  \param[in] L
	Lua state pointer. Expects:
	  1. instance ID (integer)
	  2. variable index (integer)
  \return
	Pushes the integer value onto the Lua stack and returns 1.
*************************************************************************/
int lua_Vars_GetInt(lua_State* L)
{
	size_t instance = static_cast<size_t>(luaL_checkinteger(L, 1));
	size_t index = static_cast<size_t>(luaL_checkinteger(L, 2));

	std::cout << "GetInt: instance " << instance << ", index: " << index << std::endl;

	int32_t value = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().GetIntVar(instance, index);

	lua_pushinteger(L, value);

	return 1;
}