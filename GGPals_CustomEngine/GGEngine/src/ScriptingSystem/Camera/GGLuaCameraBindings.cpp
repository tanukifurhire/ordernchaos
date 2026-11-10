/*************************************************************************
  \file       GGLuaCameraBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Lua bindings for controlling the 2D game camera.
  \details
	Provides Lua functions to get/set camera position, follow an entity,
	and clear the follow target. These functions allow scripts to manipulate
	the game camera in a safe, ECS-compatible way.
  \dependencies
	- GGLuaVM
	- GGGameCamera2D
	- GGSystemManagers
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaCameraBindings.hpp>
#include <GGSystemManagers.hpp>

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
void RegisterCameraBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	lua_pushcfunction(L, lua_Camera_GetPosition);
	lua_setfield(L, -2, "getPosition");

	lua_pushcfunction(L, lua_Camera_SetPosition);
	lua_setfield(L, -2, "setPosition");

	lua_pushcfunction(L, lua_Camera_SetFollowEntity);
	lua_setfield(L, -2, "setFollowEntity");

	lua_pushcfunction(L, lua_Camera_ClearFollowTarget);
	lua_setfield(L, -2, "clearFollowTarget");

	lua_pushcfunction(L, lua_Camera_StartShake);
	lua_setfield(L, -2, "startShake");

	lua_pushcfunction(L, lua_Camera_SetFocusEntity);
	lua_setfield(L, -2, "setFocusEntity");

	lua_pushcfunction(L, lua_Camera_ResetBounds);
	lua_setfield(L, -2, "resetBounds");

	lua_setglobal(L, "Camera");

	std::cout << "[CameraBindings] Camera system registered" << std::endl;
}

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
int lua_Camera_SetPosition(lua_State* L)
{
	if (lua_gettop(L) != 2)
	{
		luaL_error(L, "Expected 2 arguments: x, y");
		return 0;
	}

	GGMath::float32 x = static_cast<GGMath::float32>(luaL_checknumber(L, 1));
	GGMath::float32 y = static_cast<GGMath::float32>(luaL_checknumber(L, 2));

	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	camera->SetPosition(glm::vec2(x, y));

	return 0;
}

/*************************************************************************
  \brief
	Lua: Camera.getPosition() -> { x, y }
  \details
	Gets current game camera position.
  \return
	Returns world position coordinates in table with members 'x' and 'y'
*************************************************************************/
int lua_Camera_GetPosition(lua_State* L)
{
	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	lua_newtable(L);
	lua_pushstring(L, "x");
	lua_pushnumber(L, camera->GetPosition().x);
	lua_settable(L, -3);
	lua_pushstring(L, "y");
	lua_pushnumber(L, camera->GetPosition().y);
	lua_settable(L, -3);

	return 1;
}

int lua_Camera_StartShake(lua_State* L) 
{
	if (lua_gettop(L) != 2)
	{
		luaL_error(L, "Expected 2 arguments: intensity, duration");
		return 0;
	}

	GGMath::float32 intensity = static_cast<GGMath::float32>(luaL_checknumber(L, 1));
	GGMath::float32 duration = static_cast<GGMath::float32>(luaL_checknumber(L, 2));
	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();
	
	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	camera->StartShake(intensity, duration);
	return 0;
}

/*************************************************************************
  \brief
	Lua: Camera.setFollowEntity(entity)
  \details
	Forces the Game Camera to follow an entity from a particular world
*************************************************************************/
int lua_Camera_SetFollowEntity(lua_State* L)
{
	World* world = GetWorldFromLua(L);
	Entity entity = GetValidatedEntityHandle(L, 1, *world);

	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	camera->SetFollowTarget(entity);

	return 0;
}

/*************************************************************************
  \brief
	Lua: Camera.clearFollowTarget()
  \details
	Clears follow target for game camera
*************************************************************************/
int lua_Camera_ClearFollowTarget(lua_State* L)
{
	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	camera->ClearFollowTarget();
	
	return 0;
}

int lua_Camera_SetFocusEntity(lua_State* L)
{
	World* world = GetWorldFromLua(L);
	Entity entity = GetValidatedEntityHandle(L, 1, *world);

	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	if (world->HasComponent<LocalTransform>(entity) && 
		world->HasComponent<WorldTransform>(entity))
	{
		LocalTransform* transform = world->GetComponent<LocalTransform>(entity);
		WorldTransform* worldTransform = world->GetComponent<WorldTransform>(entity);

		if (transform && worldTransform)
		{
			GGMath::Vector2 translation = worldTransform->matrix.getTranslation();
			GGMath::Vector2 scale = worldTransform->matrix.getScale();

			camera->FocusOnBounds(
				glm::vec4(
					(translation.x - scale.x * .5f),
					(translation.y - scale.y * .5f),
					scale.x,
					scale.y
				)
			);
		}
		else
		{
			luaL_error(L, "GetComponent failed for LocalTransform and WorldTransform!");
			return 0;
		}
	}
	else
	{
		luaL_error(L, "Entity has no LocalTransform component!");
		return 0;
	}

	return 0;
}

int lua_Camera_ResetBounds(lua_State* L)
{
	GGGameCamera2D* camera = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetGameCamera();

	if (!camera)
	{
		luaL_error(L, "No active Game Camera!");
		return 0;
	}

	camera->RemoveBounds();

	return 0;
}