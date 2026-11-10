/*************************************************************************
  \file		  RPCRegistry.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       22 August 2026
  \version    1.0
  \brief
	Capability registry for remotely-invocable Lua RPC handlers.

  \details
	Replaces the previous dispatch strategy of calling lua_getglobal() with
	an attacker-supplied function name. A network peer must never be able to
	name an arbitrary Lua global: doing so puts loadstring, dofile, require
	and every engine binding one packet away from remote invocation.

	Instead, handlers are registered explicitly (or swept by naming
	convention) at a trusted moment during start-up. Registration captures a
	LUA_REGISTRYINDEX reference to the function object itself, so dispatch
	becomes lua_rawgeti() against a table this process built. The name on the
	wire is only ever a key into that table:

	  - An unregistered name fails closed. It cannot reach _G at all.
	  - The reference is bound to the function object, not to the global
		binding, so a later overwrite of the global cannot redirect dispatch.
	  - lua_rawgeti on the registry is a raw array index; it cannot fire a
		metatable __index hook.
	  - The registry is the audit surface: DumpNames() enumerates exactly
		what is remotely reachable.

	Each entry additionally carries the metadata needed to reject malformed
	or out-of-context calls before any Lua code runs: direction (server-bound
	vs client-bound), argument count bounds, and the index of the argument
	that must match the transport-authenticated PlayerID.

  \dependencies
	- Lua 5.1 / LuaJIT headers (lua.h, lauxlib.h)

  \history
	- 1.0 (22 August 2026): Initial version.

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GGNetworking
{
	/*************************************************************************
	  \brief
		Hard limits applied to every inbound RPC before it reaches Lua.
	  \details
		These bound attacker-controlled counts read straight off the wire.
		MAX_RPC_DEPTH deliberately mirrors the serializer's own nesting cap in
		WriteLuaValueToRPC so a legitimately-produced message always fits.
	*************************************************************************/
	static constexpr uint8_t  MAX_RPC_ARGS       = 32;    // real handlers use <= 8
	static constexpr int      MAX_RPC_DEPTH      = 8;     // matches WriteLuaValueToRPC
	static constexpr uint16_t MAX_TABLE_PREALLOC = 256;   // cap lua_createtable hint
	static constexpr uint8_t  MAX_RPC_NAME_LEN   = 62;    // RPCMessageHeader::functionName is char[63]

	/*************************************************************************
	  \brief
		Which side of the connection is permitted to receive a given handler.
	  \details
		A client must not be able to drive client-only logic on the host by
		sending a ClientRPC_ name to the server, and vice versa. Direction is
		fixed at registration and checked on every dispatch.
	*************************************************************************/
	enum class RPCDirection : uint8_t
	{
		ServerBound = 0,    // callable only on a GameServer (named ServerRPC_*)
		ClientBound = 1     // callable only on a GameClient (named ClientRPC_*)
	};

	/*************************************************************************
	  \brief
		Outcome of a registry lookup, distinguished for diagnostics.
	*************************************************************************/
	enum class RPCLookupResult : uint8_t
	{
		Found = 0,
		NotRegistered,      // name is not an exported handler at all
		WrongDirection      // name exists but is not callable on this side
	};

	/*************************************************************************
	  \struct RPCHandler
	  \brief
		One remotely-invocable Lua function and its call contract.
	*************************************************************************/
	struct RPCHandler
	{
		// Reference into LUA_REGISTRYINDEX, obtained via luaL_ref at
		// registration. Dispatch uses lua_rawgeti with this value.
		int          luaRef{ LUA_NOREF };

		RPCDirection direction{ RPCDirection::ServerBound };

		// Inclusive bounds on the wire-declared argCount.
		uint8_t      minArgs{ 0 };
		uint8_t      maxArgs{ MAX_RPC_ARGS };

		// Zero-based index of the argument that must equal the
		// transport-authenticated PlayerID, or -1 when the handler does not
		// follow the "clientID first" convention.
		int8_t       senderArgIndex{ -1 };
	};

	/*************************************************************************
	  \struct TransparentStringHash
	  \brief
		Heterogeneous hash so lookups can be done from a string_view or a
		raw char* without constructing a std::string per inbound packet.
	*************************************************************************/
	struct TransparentStringHash
	{
		using is_transparent = void;

		size_t operator()(std::string_view sv) const noexcept
		{
			return std::hash<std::string_view>{}(sv);
		}
		size_t operator()(std::string const& s) const noexcept
		{
			return std::hash<std::string_view>{}(std::string_view{ s });
		}
		size_t operator()(char const* s) const noexcept
		{
			return std::hash<std::string_view>{}(std::string_view{ s });
		}
	};

	/*************************************************************************
	  \class RPCRegistry
	  \brief
		Allowlist of remotely-invocable Lua handlers, keyed by wire name.
	  \details
		Not thread-safe by design. Registration happens during script load and
		dispatch happens in GameServer::Update / GameClient::Update, both on
		the main thread. Do not touch this from the reliability layer's I/O
		thread.
	  \see
		LuaRPCDeserializer::Deserialize, RegisterRPCBindings
	*************************************************************************/
	class RPCRegistry
	{
	public:
		/*************************************************************************
		  \brief
			Registers a single Lua function as remotely invocable.
		  \param[in] name
			Wire name peers will use. Must be 1..MAX_RPC_NAME_LEN characters of
			[A-Za-z0-9_].
		  \param[in] L
			Lua state owning the function.
		  \param[in] fnIndex
			Stack index of the function (absolute or negative).
		  \param[in] dir
			Side on which this handler may be invoked.
		  \param[in] minArgs, maxArgs
			Inclusive bounds on accepted argument count.
		  \param[in] senderArgIndex
			Index of the argument carrying the caller's PlayerID, or -1.
		  \param[in] allowReplace
			When true, an existing entry of the same name is unreferenced and
			replaced. Explicit registrations pass true so they can tighten a
			contract the convention sweep established; the sweep itself passes
			false so it never clobbers a deliberate registration.
		  \return
			true on success; false if the name is invalid, the value is not a
			function, or the name is taken and allowReplace is false.
		*************************************************************************/
		bool Register(std::string_view name, lua_State* L, int fnIndex,
		              RPCDirection dir, uint8_t minArgs, uint8_t maxArgs,
		              int8_t senderArgIndex, bool allowReplace = false);

		/*************************************************************************
		  \brief
			Tests whether a wire name is already registered, in any direction.
		*************************************************************************/
		bool Contains(std::string_view name) const;

		/*************************************************************************
		  \brief
			Registers every global function whose name starts with a prefix.
		  \param[in] L
			Lua state to sweep.
		  \param[in] prefix
			Name prefix, e.g. "ServerRPC_".
		  \param[in] dir
			Direction applied to every handler found.
		  \param[in] senderArgIndex
			senderArgIndex applied to every handler found.
		  \return
			Number of handlers registered.
		  \details
			Migration convenience so an existing codebase does not need 200
			hand-written registration lines. The security property is preserved
			because the sweep runs once, in-process, at a trusted moment: the
			wire can only ever select from the resulting snapshot.
		  \warning
			This still inherits an implicit-allowlist failure mode - a handler
			added for debugging is swept in and ships as a live remote endpoint.
			Prefer explicit Register() calls once the codebase allows it.
		*************************************************************************/
		size_t RegisterAllByPrefix(lua_State* L, std::string_view prefix,
		                           RPCDirection dir, int8_t senderArgIndex);

		/*************************************************************************
		  \brief
			Looks up a handler by wire name and required direction.
		  \param[in] name
			Attacker-controlled name from RPCMessageHeader.
		  \param[in] expected
			Direction this process accepts (ServerBound on a host).
		  \param[out] out
			Set to the handler on success, nullptr otherwise.
		  \return
			Found, NotRegistered, or WrongDirection.
		*************************************************************************/
		RPCLookupResult Lookup(std::string_view name, RPCDirection expected,
		                       RPCHandler const*& out) const;

		/*************************************************************************
		  \brief
			Releases every Lua reference held by the registry.
		  \param[in] L
			Lua state the references belong to.
		  \details
			Must be called before the Lua VM is destroyed or reloaded, otherwise
			the stored refs dangle and dispatch will find a non-function.
		*************************************************************************/
		void Clear(lua_State* L);

		inline size_t Count() const { return handlers.size(); }

		/*************************************************************************
		  \brief
			Enumerates every registered wire name.
		  \return
			Sorted list of names, for start-up logging and code review.
		*************************************************************************/
		std::vector<std::string> DumpNames() const;

		/*************************************************************************
		  \brief
			Validates a wire name's length and character set.
		  \param[in] name
			Candidate name.
		  \return
			true if the name is 1..MAX_RPC_NAME_LEN chars of [A-Za-z0-9_].
		  \details
			Applied both at registration and on every inbound packet, so a
			malformed name is rejected before it is ever used as a map key or
			written into a log line.
		*************************************************************************/
		static bool IsValidName(std::string_view name);

	private:
		std::unordered_map<std::string, RPCHandler,
		                   TransparentStringHash, std::equal_to<>> handlers;
	};

	/*************************************************************************
	  \brief
		Accessor for the process-wide RPC registry.
	*************************************************************************/
	RPCRegistry& GetRPCRegistry();

	/*************************************************************************
	  \brief
		Registers the conventionally-named handlers if it has not run already.
	  \param[in] L
		Lua state to sweep.
	  \return
		Number of handlers registered by this call (0 on subsequent calls).
	  \details
		Sweeps ServerRPC_* as server-bound with senderArgIndex 0, and
		ClientRPC_* as client-bound with senderArgIndex -1.

		Called from RPC.startServer and RPC.connectToServer rather than from a
		scene script, for two reasons:
		  - By the time either runs, every gameplay script has loaded, so the
			sweep cannot miss a handler that had not been defined yet. Driving
			it from a scene-attached script would make correctness depend on
			script load order.
		  - It runs strictly before the socket is listening or connected, so no
			packet can be dispatched against a partially-populated registry.

		Idempotent: safe to call on every connect, including reconnects.
		Names registered explicitly beforehand are left untouched.
	*************************************************************************/
	size_t EnsureDefaultRPCHandlersRegistered(lua_State* L);

	/*************************************************************************
	  \brief
		Human-readable direction name for log messages.
	*************************************************************************/
	char const* ToString(RPCDirection dir);
}
