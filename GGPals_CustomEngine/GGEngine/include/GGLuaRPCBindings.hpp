/*************************************************************************
  \file       GGLuaRPCBindings.hpp
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
	- RPCMessage.hpp
	- Lua 5.x headers (lua.h, lauxlib.h, lualib.h)

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

#include <RPCMessage.hpp>

class GGLuaVM;

/*************************************************************************
  \brief
	Registers RPC-related Lua functions (call, startServer, connectToServer)
	in the given Lua VM under the global table "RPC".
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterRPCBindings(GGLuaVM& luaVM);

/*************************************************************************
  \brief
	Executes an RPC call by serializing the function name and arguments,
	then sending it to the server or broadcasting via the server.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 0 values to Lua (no return value for the RPC call itself).
*************************************************************************/
int lua_RPC_Call(lua_State* L);

/*************************************************************************
  \brief
	Converts a Lua value at the given stack index into an RPC argument
	for serialization.
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
bool AddLuaValueToRPC(lua_State* L, int stackIndex, GGNetworking::LuaRPCSerializer& serializer);

/*************************************************************************
  \brief
	Recursively writes a Lua value (including tables/arrays) into the
	serializer's arg data without incrementing the top-level arg count.
  \param[in] L
	Pointer to the Lua state.
  \param[in] stackIndex
	Index of the value on the Lua stack.
  \param[in,out] serializer
	Reference to the LuaRPCSerializer.
  \param[in] depth
	Current nesting depth (max 8).
  \return
	Returns true if the value was successfully written; false on error.
*************************************************************************/
bool WriteLuaValueToRPC(lua_State* L, int stackIndex, GGNetworking::LuaRPCSerializer& serializer, int depth = 0);

/*************************************************************************
  \brief
    Connects the client to an RPC server at the specified address and port.
    Uses defaults if parameters are not provided.
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns true to Lua if the client connected successfully, false otherwise.
*************************************************************************/
int lua_RPC_ConnectToServer(lua_State* L);

/*************************************************************************
  \brief
    Starts an RPC server on the specified port (default if not provided).
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns true to Lua if the server started successfully, false otherwise.
*************************************************************************/
int lua_RPC_StartServer(lua_State* L);

/*************************************************************************
  \brief
	Hosts over Steam P2P instead of a bound UDP port.
	Lua usage: RPC.startSteamHost()
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns true to Lua when the listen transport came up.
  \details
	Selects the Steam transport, then does exactly what RPC.startServer does.
	There is no port to bind and no port forwarding to arrange: peers reach
	the host by SteamID through Valve's relay network.

	Normally called from SteamCB_OnLobbyCreated, once a lobby exists to
	advertise the host's SteamID in.
*************************************************************************/
int lua_RPC_StartSteamHost(lua_State* L);

/*************************************************************************
  \brief
	Connects to a host over Steam P2P.
	Lua usage: RPC.connectToSteamHost(hostSteamIdString)
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns true to Lua when the connection attempt started.
  \details
	Normally called from SteamCB_OnLobbyEntered with the host_steamid the
	lobby carries. Above the transport everything is identical to
	RPC.connectToServer - the same handshake, the same PlayerID assignment,
	the same ClientRPC_OnConnectedToServer.
  \warning
	Takes the SteamID as a STRING. It will not survive a Lua number.
*************************************************************************/
int lua_RPC_ConnectToSteamHost(lua_State* L);

/*************************************************************************
  \brief
    Sends an RPC call to a specific client by PlayerID.
    Lua usage: RPC.sendToClient(playerID, functionName, arg1, arg2, ...)
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns 0 values to Lua.
*************************************************************************/
int lua_RPC_SendToClient(lua_State* L);

/*************************************************************************
  \brief
    Disconnects the client from the server and stops the client.
    Lua usage: RPC.disconnect()
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns 0 values to Lua.
*************************************************************************/
int lua_RPC_Disconnect(lua_State* L);

/*************************************************************************
  \brief
	Shuts down the server this process is running, if any.
	Lua usage: RPC.stopServer()
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 0 values to Lua.
  \details
	Matters for a listen server. The host's server previously had no way to
	be stopped from script, so leaving the lobby stopped the host's own client
	while the server carried on holding an I/O thread and a Steam listen
	socket - still advertised, still accepting peers, with no UI attached to it.
  \note
	Safe when not hosting; it simply does nothing.
*************************************************************************/
int lua_RPC_StopServer(lua_State* L);

/*************************************************************************
  \brief
	Whether this process is currently running a server.
	Lua usage: RPC.isHosting()
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns a boolean to Lua.
*************************************************************************/
int lua_RPC_IsHosting(lua_State* L);

/*************************************************************************
  \brief
    Registers a Lua function as a remotely-invocable server-bound handler.
    Lua usage: RPC.registerServer(name, fn [, minArgs [, maxArgs [, senderArgIndex]]])
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns true to Lua on success, false otherwise.
  \details
    Only registered names can be invoked over the network. senderArgIndex
    defaults to 0, meaning argument 1 must equal the caller's authenticated
    PlayerID; pass -1 for handlers that do not follow that convention.
  \see
    GGNetworking::RPCRegistry
*************************************************************************/
int lua_RPC_RegisterServer(lua_State* L);

/*************************************************************************
  \brief
    Registers a Lua function as a remotely-invocable client-bound handler.
    Lua usage: RPC.registerClient(name, fn [, minArgs [, maxArgs [, senderArgIndex]]])
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns true to Lua on success, false otherwise.
  \details
    senderArgIndex defaults to -1: messages from the server carry no
    client-asserted identity to reconcile.
*************************************************************************/
int lua_RPC_RegisterClient(lua_State* L);

/*************************************************************************
  \brief
    Registers every global function matching a name prefix.
    Lua usage: RPC.registerAllByPrefix("ServerRPC_", "server" [, senderArgIndex])
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns the number of handlers registered.
  \details
    Migration convenience for codebases with many existing convention-named
    handlers. Must run after all scripts have loaded. Prefer explicit
    RPC.registerServer / RPC.registerClient calls long-term.
*************************************************************************/
int lua_RPC_RegisterAllByPrefix(lua_State* L);

/*************************************************************************
  \brief
    Returns how many handlers are currently registered.
    Lua usage: RPC.registeredCount()
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns 1 value to Lua (integer count).
*************************************************************************/
int lua_RPC_RegisteredCount(lua_State* L);

/*************************************************************************
  \brief
    Returns a sorted array of every remotely-invocable handler name.
    Lua usage: RPC.dumpHandlers()
  \param[in] L
    Pointer to the Lua state.
  \return
    Returns 1 value to Lua (array table of strings).
  \details
    The audit surface. Log this at start-up and review it: it is the exact
    list of entry points a network peer can reach.
*************************************************************************/
int lua_RPC_DumpHandlers(lua_State* L);