/*************************************************************************
  \file       GGLuaRPCBindings.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Exposes RPC (Remote Procedure Call) functionality to Lua scripts for
	client-server communication and server management.

  \details
	Provides Lua bindings to:
	- Execute RPC calls (RPC.call)
	- Start an RPC server (RPC.startServer)
	- Connect to an RPC server as a client (RPC.connectToServer)

  \dependencies
	- GGLuaVM.hpp
	- GameClient.hpp
	- GameServer.hpp
	- GGLogger.hpp
	- Lua 5.x headers (lua.h, lauxlib.h, lualib.h)
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaRPCBindings.hpp>
#include <GGLuaVM.hpp>

#include <GameClient.hpp>
#include <GameServer.hpp>
#include <INetTransport.hpp>
#include <RPCRegistry.hpp>
#include <SteamPlatform.hpp>

#include <GGLogger.hpp>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

/*************************************************************************
  \brief
	Registers RPC-related Lua functions (call, startServer, connectToServer)
	in the given Lua VM under the global table "RPC".
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterRPCBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_newtable(L);

	lua_pushcfunction(L, lua_RPC_Call);
	lua_setfield(L, -2, "call");

	lua_pushcfunction(L, lua_RPC_StartServer);
	lua_setfield(L, -2, "startServer");

	lua_pushcfunction(L, lua_RPC_ConnectToServer);
	lua_setfield(L, -2, "connectToServer");

	// --- Steam P2P variants ---
	// Same session semantics, different transport underneath. Kept on the RPC
	// table (rather than on Steam) because starting and joining a session is
	// an RPC-layer concern; the Steam table stays about the platform itself.
	lua_pushcfunction(L, lua_RPC_StartSteamHost);
	lua_setfield(L, -2, "startSteamHost");

	lua_pushcfunction(L, lua_RPC_ConnectToSteamHost);
	lua_setfield(L, -2, "connectToSteamHost");

	lua_pushcfunction(L, lua_RPC_SendToClient);
	lua_setfield(L, -2, "sendToClient");

	lua_pushcfunction(L, lua_RPC_Disconnect);
	lua_setfield(L, -2, "disconnect");

	lua_pushcfunction(L, lua_RPC_StopServer);
	lua_setfield(L, -2, "stopServer");

	lua_pushcfunction(L, lua_RPC_IsHosting);
	lua_setfield(L, -2, "isHosting");

	// --- Handler registration (see RPCRegistry.hpp) ---
	// Nothing is remotely invocable until it is registered here-side.
	lua_pushcfunction(L, lua_RPC_RegisterServer);
	lua_setfield(L, -2, "registerServer");

	lua_pushcfunction(L, lua_RPC_RegisterClient);
	lua_setfield(L, -2, "registerClient");

	lua_pushcfunction(L, lua_RPC_RegisterAllByPrefix);
	lua_setfield(L, -2, "registerAllByPrefix");

	lua_pushcfunction(L, lua_RPC_RegisteredCount);
	lua_setfield(L, -2, "registeredCount");

	lua_pushcfunction(L, lua_RPC_DumpHandlers);
	lua_setfield(L, -2, "dumpHandlers");

	lua_setglobal(L, "RPC");
}

/*************************************************************************
  \brief
	Shared implementation for RPC.registerServer / RPC.registerClient.
  \param[in] L
	Lua state. Stack: 1 = name, 2 = function, 3 = minArgs, 4 = maxArgs,
	5 = senderArgIndex (3-5 optional).
  \param[in] dir
	Direction to register under.
  \param[in] defaultSenderArgIndex
	senderArgIndex used when argument 5 is absent.
  \return
	1 - boolean success.
*************************************************************************/
static int RegisterHandlerCommon(lua_State* L, GGNetworking::RPCDirection dir,
	int defaultSenderArgIndex)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "RPC.register: first argument must be the handler name (string)");
	}

	if (!lua_isfunction(L, 2))
	{
		return luaL_error(L, "RPC.register: second argument must be a function");
	}

	char const* name = lua_tostring(L, 1);

	int minArgs = 0;
	int maxArgs = GGNetworking::MAX_RPC_ARGS;
	int senderArgIndex = defaultSenderArgIndex;

	if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) minArgs = static_cast<int>(lua_tointeger(L, 3));
	if (lua_gettop(L) >= 4 && lua_isnumber(L, 4)) maxArgs = static_cast<int>(lua_tointeger(L, 4));
	if (lua_gettop(L) >= 5 && lua_isnumber(L, 5)) senderArgIndex = static_cast<int>(lua_tointeger(L, 5));

	if (minArgs < 0 || maxArgs < 0 || maxArgs > GGNetworking::MAX_RPC_ARGS || minArgs > maxArgs)
	{
		return luaL_error(L, "RPC.register: '%s' has invalid arity bounds (%d..%d, max %d)",
			name, minArgs, maxArgs, static_cast<int>(GGNetworking::MAX_RPC_ARGS));
	}

	if (senderArgIndex < -1 || senderArgIndex > 127)
	{
		return luaL_error(L, "RPC.register: '%s' has invalid senderArgIndex %d", name, senderArgIndex);
	}

	// allowReplace: an explicit registration is a deliberate act and should be
	// able to tighten a contract the convention sweep already established.
	bool const ok = GGNetworking::GetRPCRegistry().Register(
		name, L, 2, dir,
		static_cast<uint8_t>(minArgs),
		static_cast<uint8_t>(maxArgs),
		static_cast<int8_t>(senderArgIndex),
		true);

	lua_pushboolean(L, ok);
	return 1;
}

int lua_RPC_RegisterServer(lua_State* L)
{
	// Default senderArgIndex 0: the established convention is that every
	// server-bound call passes the caller's clientID as argument 1.
	return RegisterHandlerCommon(L, GGNetworking::RPCDirection::ServerBound, 0);
}

int lua_RPC_RegisterClient(lua_State* L)
{
	// Server-to-client messages carry no client-asserted identity to check.
	return RegisterHandlerCommon(L, GGNetworking::RPCDirection::ClientBound, -1);
}

int lua_RPC_RegisterAllByPrefix(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "RPC.registerAllByPrefix: first argument must be a prefix (string)");
	}

	if (!lua_isstring(L, 2))
	{
		return luaL_error(L, "RPC.registerAllByPrefix: second argument must be \"server\" or \"client\"");
	}

	char const* prefix = lua_tostring(L, 1);
	char const* dirStr = lua_tostring(L, 2);

	GGNetworking::RPCDirection dir{};
	int defaultSenderArgIndex = 0;

	if (std::strcmp(dirStr, "server") == 0)
	{
		dir = GGNetworking::RPCDirection::ServerBound;
		defaultSenderArgIndex = 0;
	}
	else if (std::strcmp(dirStr, "client") == 0)
	{
		dir = GGNetworking::RPCDirection::ClientBound;
		defaultSenderArgIndex = -1;
	}
	else
	{
		return luaL_error(L, "RPC.registerAllByPrefix: direction must be \"server\" or \"client\"");
	}

	int senderArgIndex = defaultSenderArgIndex;
	if (lua_gettop(L) >= 3 && lua_isnumber(L, 3))
	{
		senderArgIndex = static_cast<int>(lua_tointeger(L, 3));

		if (senderArgIndex < -1 || senderArgIndex > 127)
		{
			return luaL_error(L, "RPC.registerAllByPrefix: invalid senderArgIndex %d", senderArgIndex);
		}
	}

	size_t const count = GGNetworking::GetRPCRegistry().RegisterAllByPrefix(
		L, prefix, dir, static_cast<int8_t>(senderArgIndex));

	lua_pushinteger(L, static_cast<lua_Integer>(count));
	return 1;
}

int lua_RPC_RegisteredCount(lua_State* L)
{
	lua_pushinteger(L, static_cast<lua_Integer>(GGNetworking::GetRPCRegistry().Count()));
	return 1;
}

int lua_RPC_DumpHandlers(lua_State* L)
{
	std::vector<std::string> const names = GGNetworking::GetRPCRegistry().DumpNames();

	lua_createtable(L, static_cast<int>(names.size()), 0);

	for (size_t i = 0; i < names.size(); ++i)
	{
		lua_pushlstring(L, names[i].data(), names[i].size());
		lua_rawseti(L, -2, static_cast<int>(i + 1));
	}

	return 1;
}

/*************************************************************************
  \brief
	Executes an RPC call by serializing the function name and arguments,
	then sending it to the server or broadcasting via the server.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 0 values to Lua (no return value for the RPC call itself).
*************************************************************************/
int lua_RPC_Call(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "RPC.call: First argument must be function name (string)");
	}

	/*
	* RPC call structure
	* [string: function name] [int: argument count] [arg1] [arg2] [...]
	* 
	* Always respect the above structure!
	*/
	const char* functionName = lua_tostring(L, 1);
	int argCount = lua_gettop(L) - 1;

	GGNetworking::LuaRPCSerializer serializer(functionName);

	// Serializing all arguments
	for (int i = 2; i <= argCount + 1; i++)
	{
		if (!AddLuaValueToRPC(L, i, serializer))
		{
			return luaL_error(L, "RPC.call: Unsupported argument type at index %d", i - 1);
		}
	}

	GGNetworking::NetworkMessage message{};
	serializer.Serialize(message);

	if (GGNetworking::GetGameClient()->IsRunning())
	{
		GGNetworking::GetGameClient()->SendToServer(message);
	}
	else if (GGNetworking::GetGameServer()->IsRunning())
	{
		GGLogger::Info("Returning RPC call...");
		GGNetworking::GetGameServer()->BroadcastMessage(message);
	}
	else
	{
		return luaL_error(L, "RPC.call: No client or server available");
	}

	return 0; // We do not return anything
}

/*************************************************************************
  \brief
	Recursively writes a Lua value (including tables/arrays) into the
	serializer's raw arg data. Does NOT increment top-level argCount.
  \param[in] L
	Pointer to the Lua state.
  \param[in] stackIndex
	Index of the value on the Lua stack.
  \param[in,out] serializer
	Reference to the LuaRPCSerializer to which the binary data is written.
  \param[in] depth
	Current recursion depth for nested tables (max 8).
  \return
	Returns true if the value was successfully written; false if the type
	is unsupported or nesting is too deep.
*************************************************************************/
bool WriteLuaValueToRPC(lua_State* L, int stackIndex, GGNetworking::LuaRPCSerializer& serializer, int depth)
{
	int luaType = lua_type(L, stackIndex);

	switch (luaType)
	{
	case LUA_TBOOLEAN:
	{
		bool value = lua_toboolean(L, stackIndex) != 0;
		serializer.WriteTypeByte(GGNetworking::ArgType::ARG_BOOLEAN);
		serializer.WriteRawByte(value ? 1 : 0);
		return true;
	}
	case LUA_TNUMBER:
	{
		lua_Number num = lua_tonumber(L, stackIndex);
		if (num == floor(num) && num >= INT_MIN && num <= INT_MAX)
		{
			int32_t value = static_cast<int32_t>(num);
			serializer.WriteTypeByte(GGNetworking::ArgType::ARG_INTEGER);
			serializer.WriteRawData(&value, sizeof(int32_t));
		}
		else
		{
			double value = static_cast<double>(num);
			serializer.WriteTypeByte(GGNetworking::ArgType::ARG_NUMBER);
			serializer.WriteRawData(&value, sizeof(double));
		}
		return true;
	}
	case LUA_TSTRING:
	{
		const char* value = lua_tostring(L, stackIndex);
		size_t strLen = std::strlen(value);

		if (strLen > 255)
		{
			GGLogger::Warning("RPC: String argument exceeds 255 bytes, truncating...");
			strLen = 255;
		}

		serializer.WriteTypeByte(GGNetworking::ArgType::ARG_STRING);
		serializer.WriteRawByte(static_cast<uint8_t>(strLen));
		serializer.WriteRawData(value, strLen);
		return true;
	}
	case LUA_TLIGHTUSERDATA:
	{
		void* ptr = lua_touserdata(L, stackIndex);
		uint64_t packedEntity = reinterpret_cast<uint64_t>(ptr);
		serializer.WriteTypeByte(GGNetworking::ArgType::ARG_ENTITY);
		serializer.WriteRawData(&packedEntity, sizeof(uint64_t));
		return true;
	}
	case LUA_TTABLE:
	{
		if (depth > 8)
		{
			GGLogger::Error("RPC: Table nesting depth exceeds maximum of 8");
			return false;
		}

		// Convert relative index to absolute (Lua 5.1 compatible)
		int absIdx = (stackIndex > 0 || stackIndex <= LUA_REGISTRYINDEX)
			? stackIndex
			: lua_gettop(L) + stackIndex + 1;

		// Get sequence length (# operator)
		size_t seqLen = lua_objlen(L, absIdx);

		// Count total entries to determine if this is a pure array
		int totalEntries = 0;
		lua_pushnil(L);
		while (lua_next(L, absIdx) != 0)
		{
			totalEntries++;
			lua_pop(L, 1); // Pop value, keep key for next iteration
		}

		bool isArray = (totalEntries == 0) ||
			(seqLen > 0 && totalEntries == static_cast<int>(seqLen));

		if (isArray)
		{
			uint16_t count = static_cast<uint16_t>(seqLen);
			serializer.WriteTypeByte(GGNetworking::ArgType::ARG_ARRAY);
			serializer.WriteRawData(&count, sizeof(uint16_t));

			for (uint16_t i = 1; i <= count; i++)
			{
				lua_rawgeti(L, absIdx, static_cast<int>(i));
				if (!WriteLuaValueToRPC(L, -1, serializer, depth + 1))
				{
					lua_pop(L, 1);
					return false;
				}
				lua_pop(L, 1);
			}
		}
		else
		{
			uint16_t count = static_cast<uint16_t>(totalEntries);
			serializer.WriteTypeByte(GGNetworking::ArgType::ARG_TABLE);
			serializer.WriteRawData(&count, sizeof(uint16_t));

			lua_pushnil(L);
			while (lua_next(L, absIdx) != 0)
			{
				// Stack: key at -2, value at -1
				if (!WriteLuaValueToRPC(L, -2, serializer, depth + 1))
				{
					lua_pop(L, 2); // Pop value + key
					return false;
				}
				if (!WriteLuaValueToRPC(L, -1, serializer, depth + 1))
				{
					lua_pop(L, 2); // Pop value + key
					return false;
				}
				lua_pop(L, 1); // Pop value, keep key for next iteration
			}
		}
		return true;
	}
	case LUA_TNIL:
	{
		serializer.WriteTypeByte(GGNetworking::ArgType::ARG_NIL);
		return true;
	}
	default:
		GGLogger::Error("RPC: Unsupported argument type: " + std::string(lua_typename(L, luaType)));
		return false;
	}
}

/*************************************************************************
  \brief
	Converts a Lua value at the given stack index into an RPC argument
	for serialization. Handles all types including tables and arrays.
  \param[in] L
	Pointer to the Lua state.
  \param[in] stackIndex
	Index of the value on the Lua stack.
  \param[in,out] serializer
	Reference to the LuaRPCSerializer to which the value will be added.
  \return
	Returns true if the value was successfully serialized; false if the type
	is unsupported.
*************************************************************************/
bool AddLuaValueToRPC(lua_State* L, int stackIndex, GGNetworking::LuaRPCSerializer& serializer)
{
	if (!WriteLuaValueToRPC(L, stackIndex, serializer, 0))
		return false;

	serializer.IncrementArgCount();
	return true;
}

/*************************************************************************
  \brief
	Starts an RPC server on the specified port (default if not provided).
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns true to Lua if the server started successfully, false otherwise.
*************************************************************************/
int lua_RPC_StartServer(lua_State* L)
{
	uint16_t port = GGNetworking::DEFAULT_SERVER_PORT;
	if (lua_gettop(L) >= 1 && lua_isnumber(L, 1))
	{
		port = static_cast<uint16_t>(lua_tointeger(L, 1));
	}

	if (GGNetworking::GetGameServer()->IsRunning())
	{
		GGLogger::Warning("RPC.startServer: Server already initialized");
		lua_pushboolean(L, false);
		return 1;
	}

	// Populate the RPC allowlist before the socket starts listening. Every
	// gameplay script has loaded by now, and nothing can arrive on the wire
	// until StartServer succeeds below.
	GGNetworking::EnsureDefaultRPCHandlersRegistered(L);

	if (!GGNetworking::GetGameServer()->Init())
	{
		GGLogger::Error("RPC.startServer: Server initialization failed");
		lua_pushboolean(L, false);
		return 1;
	}

	if (!GGNetworking::GetGameServer()->StartServer(port))
	{
		GGLogger::Error("RPC.startServer: Start Server failed");
		lua_pushboolean(L, false);
		return 1;
	}

	GGLogger::Info("RPC.startServer: Server initialized on port " + std::to_string(port));
	lua_pushboolean(L, true);

	return 1;
}

int lua_RPC_StartSteamHost(lua_State* L)
{
	if (GGNetworking::GetGameServer()->IsRunning())
	{
		GGLogger::Warning("RPC.startSteamHost: Server already initialized");
		lua_pushboolean(L, false);
		return 1;
	}

	// Ask for Steam and check what we actually got. Requesting it in a build
	// without the SDK, or before Steam.init() succeeded, silently falls back
	// to Winsock - which would then try to bind a UDP port and "work" in a way
	// no remote player could reach.
	GGNetworking::TransportKind const actual =
		GGNetworking::SetPreferredTransport(GGNetworking::TransportKind::Steam);

	if (actual != GGNetworking::TransportKind::Steam)
	{
		GGLogger::Error("RPC.startSteamHost: Steam transport unavailable "
			"(Steam client running and Steam.init() called?)");
		lua_pushboolean(L, false);
		return 1;
	}

	// Same trusted moment as RPC.startServer: every gameplay script has loaded
	// and nothing can arrive on the wire until the listen transport is up.
	GGNetworking::EnsureDefaultRPCHandlersRegistered(L);

	if (!GGNetworking::GetGameServer()->Init())
	{
		GGLogger::Error("RPC.startSteamHost: Server initialization failed");
		lua_pushboolean(L, false);
		return 1;
	}

	// The port argument is meaningless on this backend; passed only because
	// StartServer's signature still carries it for the Winsock path.
	if (!GGNetworking::GetGameServer()->StartServer(GGNetworking::DEFAULT_SERVER_PORT))
	{
		GGLogger::Error("RPC.startSteamHost: Failed to open the Steam listen socket");
		lua_pushboolean(L, false);
		return 1;
	}

	GGLogger::Info("RPC.startSteamHost: hosting over Steam P2P as "
		+ GGNetworking::SteamPlatform::GetSteamIDString());
	lua_pushboolean(L, true);
	return 1;
}

int lua_RPC_ConnectToSteamHost(lua_State* L)
{
	if (!lua_isstring(L, 1))
	{
		return luaL_error(L, "RPC.connectToSteamHost: expected the host SteamID as a string "
			"(a 64-bit ID does not survive a Lua number)");
	}

	std::string const hostSteamID = lua_tostring(L, 1);

	if (GGNetworking::GetGameClient()->IsRunning() || GGNetworking::GetGameClient()->IsConnected())
	{
		GGLogger::Warning("RPC.connectToSteamHost: Client still active, stopping first");
		GGNetworking::GetGameClient()->StopClient();
	}

	// Populate the allowlist before the connection exists, so no inbound
	// message can be dispatched against an empty registry.
	GGNetworking::EnsureDefaultRPCHandlersRegistered(L);

	if (!GGNetworking::GetGameClient()->Init())
	{
		GGLogger::Error("RPC.connectToSteamHost: Client initialization failed");
		lua_pushboolean(L, false);
		return 1;
	}

	if (!GGNetworking::GetGameClient()->ConnectToSteamHost(hostSteamID))
	{
		GGLogger::Error("RPC.connectToSteamHost: connection to " + hostSteamID + " failed");
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

/*************************************************************************
  \brief
	Connects the client to an RPC server at the specified address and port.
	Uses defaults if parameters are not provided.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns true to Lua if the client connected successfully, false otherwise.
*************************************************************************/
int lua_RPC_ConnectToServer(lua_State* L)
{
	std::string serverAddr = "eisenfaust.ddns.net";
	uint16_t port = GGNetworking::DEFAULT_SERVER_PORT;

	if (lua_gettop(L) >= 1 && lua_isstring(L, 1))
	{
		serverAddr = lua_tostring(L, 1);
	}

	if (lua_gettop(L) >= 2 && lua_isnumber(L, 2))
	{
		port = static_cast<uint16_t>(lua_tointeger(L, 2));
	}

	if (GGNetworking::GetGameClient()->IsRunning() || GGNetworking::GetGameClient()->IsConnected())
	{
		GGLogger::Warning("RPC.connectToServer: Client still active, stopping first");
		GGNetworking::GetGameClient()->StopClient();
	}

	// Populate the RPC allowlist before the connection is established, so no
	// server message can be dispatched against an empty registry.
	GGNetworking::EnsureDefaultRPCHandlersRegistered(L);

	if (!GGNetworking::GetGameClient()->Init())
	{
		GGLogger::Error("RPC.connectToServer: Client initialization failed");
		lua_pushboolean(L, false);
		return 1;
	}

	if (!GGNetworking::GetGameClient()->ConnectToServer(serverAddr, port))
	{
		GGLogger::Error("RPC.connectToServer: Connection to " + serverAddr + " at "
			+ std::to_string(port) + " failed.");
		lua_pushboolean(L, false);
		return 1;
	}

	GGLogger::Info("RPC.connectToServer: Connecting to " + serverAddr + " at "
		+ std::to_string(port) + "...");
	lua_pushboolean(L, true);
	return 1;
}

/*************************************************************************
  \brief
	Sends an RPC call to a specific client by PlayerID.
	Lua usage: RPC.sendToClient(playerID, functionName, arg1, arg2, ...)
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 0 values to Lua.
*************************************************************************/
int lua_RPC_SendToClient(lua_State* L)
{
	if (!lua_isnumber(L, 1))
	{
		return luaL_error(L, "RPC.sendToClient: First argument must be playerID (number)");
	}

	if (!lua_isstring(L, 2))
	{
		return luaL_error(L, "RPC.sendToClient: Second argument must be function name (string)");
	}

	if (!GGNetworking::GetGameServer()->IsRunning())
	{
		return luaL_error(L, "RPC.sendToClient: Server is not running");
	}

	GGNetworking::PlayerID targetPlayer = static_cast<GGNetworking::PlayerID>(lua_tointeger(L, 1));
	const char* functionName = lua_tostring(L, 2);
	int argCount = lua_gettop(L) - 2;

	GGNetworking::LuaRPCSerializer serializer(functionName);

	for (int i = 3; i <= argCount + 2; i++)
	{
		if (!AddLuaValueToRPC(L, i, serializer))
		{
			return luaL_error(L, "RPC.sendToClient: Unsupported argument type at index %d", i - 2);
		}
	}

	GGNetworking::NetworkMessage message{};
	serializer.Serialize(message);

	GGNetworking::GetGameServer()->SendToClient(message, targetPlayer);

	return 0;
}

/*************************************************************************
  \brief
	Disconnects the client from the server and stops the client.
	Lua usage: RPC.disconnect()
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 0 values to Lua.
*************************************************************************/
int lua_RPC_StopServer([[maybe_unused]] lua_State* L)
{
	auto& server = GGNetworking::GetGameServer();

	if (!server->IsRunning())
	{
		// Not an error - most players never host, and the teardown path runs
		// for everyone.
		return 0;
	}

	GGLogger::Info("RPC.stopServer: shutting the server down");
	server->StopServer();

	return 0;
}

int lua_RPC_IsHosting(lua_State* L)
{
	lua_pushboolean(L, GGNetworking::GetGameServer()->IsRunning());
	return 1;
}

int lua_RPC_Disconnect([[maybe_unused]] lua_State* L)
{
	auto& client = GGNetworking::GetGameClient();

	if (!client->IsRunning())
	{
		GGLogger::Warning("RPC.disconnect: Client is not running");
		return 0;
	}

	GGLogger::Info("RPC.disconnect: Disconnecting from server...");
	client->StopClient();

	return 0;
}