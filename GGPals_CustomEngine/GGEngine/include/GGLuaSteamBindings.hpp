/*************************************************************************
  \file       GGLuaSteamBindings.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief      Exposes Steam platform and matchmaking to Lua as a `Steam` table.

  \details
	A separate table rather than more entries on `RPC`, so `RPC.*` stays
	transport-agnostic: scripts that only send and receive RPCs need not know
	whether the session is running over Steam or Winsock, and the ones that do
	care reach for `Steam.*` explicitly.

	Everything here is safe to call in a build with no Steamworks SDK. The
	functions still exist and report that Steam is unavailable, so a script can
	branch on `Steam.isAvailable()` rather than being written twice.

	\warning
	Every SteamID and lobby ID crosses this boundary as a STRING, never a
	number. LuaJIT numbers are doubles, a SteamID64 needs 64 bits, and the
	corruption from squeezing one into the other is silent and only affects
	some IDs - which is the worst possible combination to debug.

	Lua-side callbacks, all optional globals:

	  SteamCB_OnLobbyCreated(lobbyId, success)
	  SteamCB_OnLobbyList(lobbies)            -- array of {id, hostSteamId, name, players, maxPlayers}
	  SteamCB_OnLobbyEntered(lobbyId, hostSteamId, success)
	  SteamCB_OnLobbyMemberChanged(steamId, joined, memberCount)
	  SteamCB_OnLobbyDataChanged(lobbyId)
	  SteamCB_OnJoinRequested(lobbyId)        -- friend invite or "Join Game"

	The SteamCB_ prefix matters. These are engine-to-Lua calls, not network
	RPCs; naming them ServerRPC_ or ClientRPC_ would make the RPC registry's
	convention sweep publish them as remotely callable entry points.

  \dependencies
	- GGLuaVM.hpp
	- SteamPlatform.hpp, SteamLobby.hpp

  \see
	SteamPlatform, SteamLobbyManager, GGLuaRPCBindings

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

class GGLuaVM;
struct lua_State;

/*************************************************************************
  \brief
	Registers the global `Steam` table in the given Lua VM.
  \param[in,out] luaVM
	VM to register into.
*************************************************************************/
void RegisterSteamBindings(GGLuaVM& luaVM);

// --- Platform ---
int lua_Steam_Init(lua_State* L);              // Steam.init()             -> bool
int lua_Steam_Shutdown(lua_State* L);          // Steam.shutdown()
int lua_Steam_IsAvailable(lua_State* L);       // Steam.isAvailable()      -> bool
int lua_Steam_IsCompiledIn(lua_State* L);      // Steam.isCompiledIn()     -> bool
int lua_Steam_GetPersonaName(lua_State* L);    // Steam.getPersonaName()   -> string
int lua_Steam_GetSteamID(lua_State* L);        // Steam.getSteamID()       -> string

// --- Matchmaking ---
int lua_Steam_CreateLobby(lua_State* L);       // Steam.createLobby(max, name)
int lua_Steam_RequestLobbyList(lua_State* L);  // Steam.requestLobbyList()
int lua_Steam_JoinLobby(lua_State* L);         // Steam.joinLobby(idString)
int lua_Steam_LeaveLobby(lua_State* L);        // Steam.leaveLobby()
int lua_Steam_OpenInviteOverlay(lua_State* L); // Steam.openInviteOverlay()
int lua_Steam_GetLobbyID(lua_State* L);        // Steam.getLobbyId()       -> string
int lua_Steam_GetHostSteamID(lua_State* L);    // Steam.getHostSteamId()   -> string
int lua_Steam_IsHost(lua_State* L);            // Steam.isHost()           -> bool
int lua_Steam_GetMemberCount(lua_State* L);    // Steam.getMemberCount()   -> int
int lua_Steam_GetMemberAt(lua_State* L);       // Steam.getMemberAt(i)     -> string
int lua_Steam_SetLobbyData(lua_State* L);      // Steam.setLobbyData(k, v)  -- host only
int lua_Steam_GetLobbyData(lua_State* L);      // Steam.getLobbyData(k)    -> string

// --- Transport selection and diagnostics ---
int lua_Steam_SetTransport(lua_State* L);      // Steam.setTransport(name) -> string (actual)
int lua_Steam_GetTransport(lua_State* L);      // Steam.getTransport()     -> string
int lua_Steam_SetNetSim(lua_State* L);         // Steam.setNetSim(loss, lagMs, reorder, dup) -> bool
int lua_Steam_GetNetStats(lua_State* L);       // Steam.getNetStats()      -> table
