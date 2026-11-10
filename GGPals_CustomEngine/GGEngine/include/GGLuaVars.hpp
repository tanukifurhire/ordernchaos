/*************************************************************************
  \file       GGLuaVars.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Exposes ECS script variable access (integer type) to Lua.

  \details
    Provides Lua bindings to set and get integer variables from ECS script
    instances. The bindings are available under the global "Vars" table:
      - Vars.setInt(instanceID, index, name, value)
      - Vars.getInt(instanceID, index)

  \dependencies
    - GGLuaVM.hpp
    - Lua headers (lua.h, lauxlib.h, lualib.h)

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
	Registers Lua functions for variable access (setInt/getInt) under the
	global table "Vars".
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterVarBindings(GGLuaVM& luaVM);

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
int lua_Vars_SetInt(lua_State* L);

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
int lua_Vars_GetInt(lua_State* L);