/*************************************************************************
  \file       GGLuaDefineType.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Lua bindings for defining scripted types and managing instance behavior.
  \details
    Provides functions to define new Lua types with fields, serialized members,
    and functions. Also implements metamethods for indexing, assignment, and
    garbage collection of scripted instances.

  \dependencies
    - GGLuaVM
    - GGScriptAsset
    - GGScriptMetadata
    - Lua 5.x
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

#include <GGScriptAsset.hpp>

class GGLuaVM;

/*************************************************************************
  \brief 
    Registers the DefineType Lua function in the given Lua VM.
  \param[in,out] luaVM
    Reference to the Lua VM.
*************************************************************************/
void RegisterDefineTypeBindings(GGLuaVM& luaVM);

/*************************************************************************
  \brief
    Implements the Lua DefineType function, creating a new type with the given
    name and field definitions.
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns 1 value on the Lua stack: the created type table.
*************************************************************************/
int lua_DefineType(lua_State* L);

/*************************************************************************
  \brief
    Handles Lua indexing for multiple inheritance types.
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns the value found in the type table or parent types, or nil if not found.
*************************************************************************/
int lua_MultiInheritIndex(lua_State* L);

/*************************************************************************
  \brief
    Handles Lua __newindex metamethod for a scripted instance.
  \param[in] L
    Pointer to the Lua state.
  \return
    Sets the field to the given value, or raises an error if types mismatch.
*************************************************************************/
int InstanceNewIndex(lua_State* L);

/*************************************************************************
  \brief
    Handles Lua __index metamethod for a scripted instance.
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns the value of the requested field on the Lua stack, or nil if not found.
*************************************************************************/
int InstanceIndex(lua_State* L);

/*************************************************************************
  \brief
    Handles Lua garbage collection (__gc) for a scripted instance.
  \param[in] L
    Pointer to the Lua state.
  \return
    Cleans up the InstanceUserdata without deleting the underlying instance.
*************************************************************************/
int InstanceGC(lua_State* L);

/*************************************************************************
  \brief
    Creates a metatable for a given GGScriptMetadata type and stores it in
    the Lua registry.
  \param[in] L
    Pointer to the Lua state.
  \param[in,out] meta
    Metadata describing the type for which the metatable is created.
*************************************************************************/
void CreateTypeMetatable(lua_State* L, GGScriptMetadata& meta);