/*************************************************************************
  \file		  GGLuaAnimationBindings.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Lua bindings for the ECS SpriteAnimator component.
  \details
	Provides functions that allow Lua scripts to interact with the ECS
	SpriteAnimator component. Scripts can query the current animation
	state of an entity or set a new animation state.

  \dependencies
	- lua.h, lualib.h, lauxlib.h
	- GGLuaVM (forward declaration)
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
	Retrieves the current animation information of a SpriteAnimator component
	for a given entity and pushes it onto the Lua stack as a table.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 1 value on the Lua stack: a table with keys animationAssetID,
	frameTime, currentState, and currentFrameIndex, or nil if no animator exists.
*************************************************************************/
int lua_ECS_GetAnimation(lua_State* L);

/*************************************************************************
  \brief
	Sets the next animation state for a SpriteAnimator component of a given entity.
  \param[in] L
	Pointer to the Lua state.
  \details
	Expects the entity as the first Lua argument and the desired animation
	state name as the second argument. Raises a Lua error if the entity
	does not have a SpriteAnimator component.
  \return
	Returns 0 values on the Lua stack.
*************************************************************************/
int lua_ECS_SetAnimation(lua_State* L);