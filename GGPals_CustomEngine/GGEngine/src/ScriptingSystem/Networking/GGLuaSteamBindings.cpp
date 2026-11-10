/*************************************************************************
  \file       GGLuaSteamBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief      Implements the Lua `Steam` table.

  \details
	Every function here is safe without the Steamworks SDK: the underlying
	SteamPlatform / SteamLobbyManager entry points exist in both build shapes,
	so nothing in this file needs an #ifdef and neither does any script.

  \dependencies
	- GGLuaSteamBindings.hpp, GGLuaVM.hpp
	- SteamPlatform.hpp, SteamLobby.hpp, INetTransport.hpp
	- GameServer.hpp, GameClient.hpp, GGLogger.hpp

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaSteamBindings.hpp>
#include <GGLuaVM.hpp>

#include <GameClient.hpp>
#include <GameServer.hpp>
#include <INetTransport.hpp>
#include <SteamLobby.hpp>
#include <SteamPlatform.hpp>

#include <GGLogger.hpp>

#include <string>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

void RegisterSteamBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	// --- Platform ---
	lua_pushcfunction(L, lua_Steam_Init);
	lua_setfield(L, -2, "init");

	lua_pushcfunction(L, lua_Steam_Shutdown);
	lua_setfield(L, -2, "shutdown");

	lua_pushcfunction(L, lua_Steam_IsAvailable);
	lua_setfield(L, -2, "isAvailable");

	lua_pushcfunction(L, lua_Steam_IsCompiledIn);
	lua_setfield(L, -2, "isCompiledIn");

	lua_pushcfunction(L, lua_Steam_GetPersonaName);
	lua_setfield(L, -2, "getPersonaName");

	lua_pushcfunction(L, lua_Steam_GetSteamID);
	lua_setfield(L, -2, "getSteamID");

	// --- Matchmaking ---
	lua_pushcfunction(L, lua_Steam_CreateLobby);
	lua_setfield(L, -2, "createLobby");

	lua_pushcfunction(L, lua_Steam_RequestLobbyList);
	lua_setfield(L, -2, "requestLobbyList");

	lua_pushcfunction(L, lua_Steam_JoinLobby);
	lua_setfield(L, -2, "joinLobby");

	lua_pushcfunction(L, lua_Steam_LeaveLobby);
	lua_setfield(L, -2, "leaveLobby");

	lua_pushcfunction(L, lua_Steam_OpenInviteOverlay);
	lua_setfield(L, -2, "openInviteOverlay");

	lua_pushcfunction(L, lua_Steam_GetLobbyID);
	lua_setfield(L, -2, "getLobbyId");

	lua_pushcfunction(L, lua_Steam_GetHostSteamID);
	lua_setfield(L, -2, "getHostSteamId");

	lua_pushcfunction(L, lua_Steam_IsHost);
	lua_setfield(L, -2, "isHost");

	lua_pushcfunction(L, lua_Steam_GetMemberCount);
	lua_setfield(L, -2, "getMemberCount");

	lua_pushcfunction(L, lua_Steam_GetMemberAt);
	lua_setfield(L, -2, "getMemberAt");

	lua_pushcfunction(L, lua_Steam_SetLobbyData);
	lua_setfield(L, -2, "setLobbyData");

	lua_pushcfunction(L, lua_Steam_GetLobbyData);
	lua_setfield(L, -2, "getLobbyData");

	// --- Transport selection and diagnostics ---
	lua_pushcfunction(L, lua_Steam_SetTransport);
	lua_setfield(L, -2, "setTransport");

	lua_pushcfunction(L, lua_Steam_GetTransport);
	lua_setfield(L, -2, "getTransport");

	lua_pushcfunction(L, lua_Steam_SetNetSim);
	lua_setfield(L, -2, "setNetSim");

	lua_pushcfunction(L, lua_Steam_GetNetStats);
	lua_setfield(L, -2, "getNetStats");

	lua_setglobal(L, "Steam");
}

// ---------------------------------------------------------------------------
// Platform
// ---------------------------------------------------------------------------

int lua_Steam_Init([[maybe_unused]] lua_State* L)
{
	bool const ok = GGNetworking::SteamPlatform::Init();

	if (ok)
	{
		GGLogger::Info("Steam OK, user = " + GGNetworking::SteamPlatform::GetPersonaName()
			+ " id = " + GGNetworking::SteamPlatform::GetSteamIDString());
	}

	lua_pushboolean(L, ok);
	return 1;
}

int lua_Steam_Shutdown([[maybe_unused]] lua_State* L)
{
	// Leave the lobby first: shutting the API down underneath an open lobby
	// leaves other members watching a ghost until Steam times it out.
	GGNetworking::GetSteamLobby().LeaveLobby();
	GGNetworking::SteamPlatform::Shutdown();
	return 0;
}

int lua_Steam_IsAvailable(lua_State* L)
{
	lua_pushboolean(L, GGNetworking::SteamPlatform::IsAvailable());
	return 1;
}

int lua_Steam_IsCompiledIn(lua_State* L)
{
	lua_pushboolean(L, GGNetworking::SteamPlatform::IsCompiledIn());
	return 1;
}

int lua_Steam_GetPersonaName(lua_State* L)
{
	lua_pushstring(L, GGNetworking::SteamPlatform::GetPersonaName().c_str());
	return 1;
}

int lua_Steam_GetSteamID(lua_State* L)
{
	// A string, always. See the warning in the header - a SteamID64 does not
	// survive a LuaJIT number.
	lua_pushstring(L, GGNetworking::SteamPlatform::GetSteamIDString().c_str());
	return 1;
}

// ---------------------------------------------------------------------------
// Matchmaking
// ---------------------------------------------------------------------------

int lua_Steam_CreateLobby(lua_State* L)
{
	int maxMembers = 4;
	if (lua_gettop(L) >= 1 && lua_isnumber(L, 1))
	{
		maxMembers = static_cast<int>(lua_tointeger(L, 1));
	}

	std::string lobbyName;
	if (lua_gettop(L) >= 2 && lua_isstring(L, 2))
	{
		lobbyName = lua_tostring(L, 2);
	}

	GGNetworking::GetSteamLobby().CreateLobby(maxMembers, lobbyName);
	return 0;
}

int lua_Steam_RequestLobbyList([[maybe_unused]] lua_State* L)
{
	GGNetworking::GetSteamLobby().RequestLobbyList();
	return 0;
}

int lua_Steam_JoinLobby(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "Steam.joinLobby: lobby ID must be a string, not a number "
			"(64-bit IDs do not survive a Lua number)");
	}

	GGNetworking::GetSteamLobby().JoinLobby(lua_tostring(L, 1));
	return 0;
}

int lua_Steam_LeaveLobby([[maybe_unused]] lua_State* L)
{
	GGNetworking::GetSteamLobby().LeaveLobby();
	return 0;
}

int lua_Steam_OpenInviteOverlay([[maybe_unused]] lua_State* L)
{
	GGNetworking::GetSteamLobby().OpenInviteOverlay();
	return 0;
}

int lua_Steam_GetLobbyID(lua_State* L)
{
	lua_pushstring(L, GGNetworking::GetSteamLobby().GetCurrentLobbyID().c_str());
	return 1;
}

int lua_Steam_GetHostSteamID(lua_State* L)
{
	lua_pushstring(L, GGNetworking::GetSteamLobby().GetHostSteamID().c_str());
	return 1;
}

int lua_Steam_IsHost(lua_State* L)
{
	lua_pushboolean(L, GGNetworking::GetSteamLobby().IsHost());
	return 1;
}

int lua_Steam_GetMemberCount(lua_State* L)
{
	lua_pushinteger(L, GGNetworking::GetSteamLobby().GetMemberCount());
	return 1;
}

int lua_Steam_GetMemberAt(lua_State* L)
{
	if (!lua_isnumber(L, 1))
	{
		return luaL_error(L, "Steam.getMemberAt: expected a 1-based index");
	}

	// Lua indexes from 1, the Steam API from 0.
	int const index = static_cast<int>(lua_tointeger(L, 1)) - 1;

	lua_pushstring(L, GGNetworking::GetSteamLobby().GetMemberAt(index).c_str());
	return 1;
}

int lua_Steam_SetLobbyData(lua_State* L)
{
	if (!lua_isstring(L, 1) || !lua_isstring(L, 2))
	{
		return luaL_error(L, "Steam.setLobbyData: expected (key, value) as strings");
	}

	GGNetworking::GetSteamLobby().SetLobbyData(lua_tostring(L, 1), lua_tostring(L, 2));
	return 0;
}

int lua_Steam_GetLobbyData(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "Steam.getLobbyData: expected a key string");
	}

	lua_pushstring(L, GGNetworking::GetSteamLobby().GetLobbyData(lua_tostring(L, 1)).c_str());
	return 1;
}

// ---------------------------------------------------------------------------
// Transport selection and diagnostics
// ---------------------------------------------------------------------------

int lua_Steam_SetTransport(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "Steam.setTransport: expected \"steam\" or \"winsock\"");
	}

	std::string const requested = lua_tostring(L, 1);

	GGNetworking::TransportKind kind = GGNetworking::TransportKind::Winsock;

	if (requested == "steam")        kind = GGNetworking::TransportKind::Steam;
	else if (requested == "winsock") kind = GGNetworking::TransportKind::Winsock;
	else
	{
		return luaL_error(L, "Steam.setTransport: unknown transport '%s'", requested.c_str());
	}

	// Returns what is ACTUALLY in effect, which may differ from what was asked
	// for: requesting Steam in a build without the SDK, or before Steam.init()
	// succeeded, falls back to Winsock. Scripts should branch on the return
	// value rather than assume the request took.
	GGNetworking::TransportKind const actual = GGNetworking::SetPreferredTransport(kind);

	lua_pushstring(L, GGNetworking::TransportKindName(actual));
	return 1;
}

int lua_Steam_GetTransport(lua_State* L)
{
	// Report the running session's transport when there is one, since that is
	// what the player is actually playing over; the preference only describes
	// what the next session would use.
	if (GGNetworking::GetGameServer()->IsRunning())
	{
		lua_pushstring(L, GGNetworking::TransportKindName(
			GGNetworking::GetGameServer()->GetTransportKind()));
		return 1;
	}

	if (GGNetworking::GetGameClient()->IsRunning())
	{
		lua_pushstring(L, GGNetworking::TransportKindName(
			GGNetworking::GetGameClient()->GetTransportKind()));
		return 1;
	}

	lua_pushstring(L, GGNetworking::TransportKindName(GGNetworking::GetPreferredTransport()));
	return 1;
}

int lua_Steam_SetNetSim(lua_State* L)
{
	int const loss      = lua_isnumber(L, 1) ? static_cast<int>(lua_tointeger(L, 1)) : 0;
	int const lagMs     = lua_isnumber(L, 2) ? static_cast<int>(lua_tointeger(L, 2)) : 0;
	int const reorder   = lua_isnumber(L, 3) ? static_cast<int>(lua_tointeger(L, 3)) : 0;
	int const duplicate = lua_isnumber(L, 4) ? static_cast<int>(lua_tointeger(L, 4)) : 0;

	bool const applied = GGNetworking::SteamPlatform::SetNetworkSimulation(
		loss, lagMs, reorder, duplicate);

	if (!applied)
	{
		GGLogger::Warning("Steam.setNetSim: the simulator did not take effect. "
			"Either Steam is unavailable, or this SDK build compiled the knobs out - "
			"shape traffic with Clumsy instead and do not report a clean run as a pass.");
	}

	lua_pushboolean(L, applied);
	return 1;
}

int lua_Steam_GetNetStats(lua_State* L)
{
	// The project's own reliability numbers, which are the interesting ones:
	// they say what OUR layer did. Put them beside Steam's per-connection
	// status in the debug panel and it becomes obvious which layer owns a
	// problem, instead of an afternoon of guessing.
	GGNetworking::ReliabilityLayer::ReliabilityStats stats{};
	char const* role = "none";

	if (GGNetworking::GetGameServer()->IsRunning())
	{
		stats = GGNetworking::GetGameServer()->GetStats();
		role  = "server";
	}
	else if (GGNetworking::GetGameClient()->IsRunning())
	{
		stats = GGNetworking::GetGameClient()->GetStats();
		role  = "client";
	}

	lua_createtable(L, 0, 8);

	lua_pushstring(L, role);
	lua_setfield(L, -2, "role");

	lua_pushstring(L, GGNetworking::TransportKindName(GGNetworking::GetPreferredTransport()));
	lua_setfield(L, -2, "transport");

	lua_pushinteger(L, stats.packetsSent);
	lua_setfield(L, -2, "packetsSent");

	lua_pushinteger(L, stats.packetsReceived);
	lua_setfield(L, -2, "packetsReceived");

	lua_pushinteger(L, stats.packetsRetransmitted);
	lua_setfield(L, -2, "packetsRetransmitted");

	lua_pushinteger(L, stats.packetsLost);
	lua_setfield(L, -2, "packetsLost");

	lua_pushinteger(L, stats.duplicatesReceived);
	lua_setfield(L, -2, "duplicatesReceived");

	lua_pushnumber(L, stats.averageRoundTripTime);
	lua_setfield(L, -2, "averageRoundTripTime");

	return 1;
}
