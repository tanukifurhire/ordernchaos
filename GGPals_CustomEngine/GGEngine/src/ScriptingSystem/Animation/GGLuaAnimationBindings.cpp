/*************************************************************************
  \file       GGLuaAnimationBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Lua bindings for SpriteAnimator component functions in the ECS.
  \details
	This file exposes ECS SpriteAnimator component functionality to Lua
	scripts, allowing Lua scripts to get and set animation states of
	entities.
  \dependencies
	- GGLuaAnimationBindings.hpp
	- GGEngine.hpp
	- GGECSWorld.hpp
	- GGECSComponents.hpp
	- GGSystemManagers.hpp
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaAnimationBindings.hpp>
#include <GGEngine.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>
#include <GGSystemManagers.hpp>

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
int lua_ECS_GetAnimation(lua_State* L)
{
	auto& world = GetEngineInstance().GetSceneManager().GetActiveScene();

	Entity entity = GetValidatedEntityHandle(L, 1, world);

	if (!world.HasComponent<SpriteAnimator>(entity))
	{
		lua_pushnil(L);
		return 1;
	}

	SpriteAnimator* animator = world.GetComponent<SpriteAnimator>(entity);
	auto& animSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();

	if (animator)
	{
		const auto& animation = animSystem.GetAnimation(animator->animationAssetID);
		std::string currentStateName = "Unknown";
		if (animator->currentAnimState < animation.animStates.size())
		{
			currentStateName = animation.animStates[animator->currentAnimState].stateName;
		}

		lua_newtable(L);

		lua_pushinteger(L, animator->animationAssetID);
		lua_setfield(L, -2, "animationAssetID");

		lua_pushnumber(L, animator->frameTime);
		lua_setfield(L, -2, "frameTime");

		lua_pushstring(L, currentStateName.c_str());
		lua_setfield(L, -2, "currentState");

		lua_pushinteger(L, animator->currentFrameIndex);
		lua_setfield(L, -2, "currentFrameIndex");

		return 1;
	}
	else
	{
		lua_pushnil(L);

		return 1;
	}
}

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
int lua_ECS_SetAnimation(lua_State* L)
{
	auto& world = GetEngineInstance().GetSceneManager().GetActiveScene();
	Entity entity = GetValidatedEntityHandle(L, 1, world);

	const char* newState = luaL_checkstring(L, 2);

	if (!world.HasComponent<SpriteAnimator>(entity))
	{
		luaL_error(L, "Entity does not have SpriteAnimator component");
	}

	SpriteAnimator* animator = world.GetComponent<SpriteAnimator>(entity);

	animator->SetNextAnimationState(newState);

	return 0;
}