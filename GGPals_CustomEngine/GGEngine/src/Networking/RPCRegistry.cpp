/*************************************************************************
  \file		  RPCRegistry.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       22 August 2026
  \version    1.0
  \brief
	Implements the capability registry for remotely-invocable Lua handlers.

  \dependencies
	- RPCRegistry.hpp
	- GGLogger.hpp
	- Lua 5.1 / LuaJIT

  \history
	- 1.0 (22 August 2026): Initial version.

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <RPCRegistry.hpp>

#include <GGLogger.hpp>

#include <algorithm>
#include <iostream>

namespace GGNetworking
{
	char const* ToString(RPCDirection dir)
	{
		return (dir == RPCDirection::ServerBound) ? "server-bound" : "client-bound";
	}

	bool RPCRegistry::IsValidName(std::string_view name)
	{
		if (name.empty() || name.size() > MAX_RPC_NAME_LEN) return false;

		for (char c : name)
		{
			bool const ok = (c >= 'A' && c <= 'Z')
			             || (c >= 'a' && c <= 'z')
			             || (c >= '0' && c <= '9')
			             || (c == '_');
			if (!ok) return false;
		}

		return true;
	}

	bool RPCRegistry::Contains(std::string_view name) const
	{
		return handlers.find(name) != handlers.end();
	}

	bool RPCRegistry::Register(std::string_view name, lua_State* L, int fnIndex,
	                           RPCDirection dir, uint8_t minArgs, uint8_t maxArgs,
	                           int8_t senderArgIndex, bool allowReplace)
	{
		if (!IsValidName(name))
		{
			GGLogger::Error("RPC.register: invalid handler name (must be 1-"
				+ std::to_string(MAX_RPC_NAME_LEN) + " chars of [A-Za-z0-9_])");
			return false;
		}

		// Normalise to an absolute stack index up front: lua_pushvalue below
		// shifts the top, which would invalidate a negative index.
		if (fnIndex < 0) fnIndex = lua_gettop(L) + 1 + fnIndex;

		if (!lua_isfunction(L, fnIndex))
		{
			GGLogger::Error("RPC.register: '" + std::string(name) + "' is not a function");
			return false;
		}

		if (minArgs > maxArgs || maxArgs > MAX_RPC_ARGS)
		{
			GGLogger::Error("RPC.register: '" + std::string(name)
				+ "' has invalid arity bounds (" + std::to_string(minArgs)
				+ ".." + std::to_string(maxArgs) + ")");
			return false;
		}

		if (senderArgIndex >= 0 && static_cast<int>(senderArgIndex) >= static_cast<int>(maxArgs))
		{
			GGLogger::Error("RPC.register: '" + std::string(name)
				+ "' senderArgIndex " + std::to_string(senderArgIndex)
				+ " is outside its own arity bounds");
			return false;
		}

		auto existing = handlers.find(name);
		if (existing != handlers.end())
		{
			if (!allowReplace)
			{
				GGLogger::Warning("RPC.register: '" + std::string(name)
					+ "' is already registered, ignoring duplicate");
				return false;
			}

			// Explicit replacement: release the old reference so it does not
			// leak in the Lua registry for the lifetime of the VM.
			if (existing->second.luaRef != LUA_NOREF)
			{
				luaL_unref(L, LUA_REGISTRYINDEX, existing->second.luaRef);
			}
			handlers.erase(existing);
		}

		// Capture a reference to the function object itself. This is what makes
		// a later reassignment of the global unable to redirect dispatch.
		lua_pushvalue(L, fnIndex);
		int const ref = luaL_ref(L, LUA_REGISTRYINDEX);

		if (ref == LUA_NOREF || ref == LUA_REFNIL)
		{
			GGLogger::Error("RPC.register: failed to ref '" + std::string(name) + "'");
			return false;
		}

		RPCHandler handler{};
		handler.luaRef         = ref;
		handler.direction      = dir;
		handler.minArgs        = minArgs;
		handler.maxArgs        = maxArgs;
		handler.senderArgIndex = senderArgIndex;

		handlers.emplace(std::string{ name }, handler);
		return true;
	}

	size_t RPCRegistry::RegisterAllByPrefix(lua_State* L, std::string_view prefix,
	                                        RPCDirection dir, int8_t senderArgIndex)
	{
		if (prefix.empty())
		{
			GGLogger::Error("RPC.registerAllByPrefix: empty prefix would expose every global");
			return 0;
		}

		size_t registered = 0;

		// LuaJIT is a Lua 5.1 API: globals live at LUA_GLOBALSINDEX rather than
		// behind lua_pushglobaltable.
		lua_pushvalue(L, LUA_GLOBALSINDEX);
		lua_pushnil(L);

		while (lua_next(L, -2) != 0)
		{
			// key at -2, value at -1.
			// Only inspect the key's type - never call lua_tostring on a
			// non-string key, as that mutates it in place and breaks lua_next.
			if (lua_type(L, -2) == LUA_TSTRING && lua_isfunction(L, -1))
			{
				size_t nameLen = 0;
				char const* rawName = lua_tolstring(L, -2, &nameLen);
				std::string_view name{ rawName, nameLen };

				if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
				{
					if (!IsValidName(name))
					{
						GGLogger::Warning("RPC.registerAllByPrefix: skipping '"
							+ std::string(name) + "' (name too long or bad charset)");
					}
					// An explicit registration always outranks the sweep, and
					// re-sweeping on reconnect is normal - neither is worth a
					// warning, so skip quietly rather than going through
					// Register's duplicate path.
					else if (!Contains(name))
					{
						if (Register(name, L, -1, dir, 0, MAX_RPC_ARGS, senderArgIndex, false))
						{
							++registered;
						}
					}
				}
			}

			lua_pop(L, 1);  // pop value, leave key for the next iteration
		}

		lua_pop(L, 1);      // pop globals table

		GGLogger::Info("RPC.registerAllByPrefix: registered " + std::to_string(registered)
			+ " " + ToString(dir) + " handler(s) for prefix '" + std::string(prefix) + "'");

		return registered;
	}

	RPCLookupResult RPCRegistry::Lookup(std::string_view name, RPCDirection expected,
	                                    RPCHandler const*& out) const
	{
		out = nullptr;

		auto it = handlers.find(name);
		if (it == handlers.end()) return RPCLookupResult::NotRegistered;

		if (it->second.direction != expected) return RPCLookupResult::WrongDirection;

		out = &it->second;
		return RPCLookupResult::Found;
	}

	void RPCRegistry::Clear(lua_State* L)
	{
		if (L)
		{
			for (auto& [name, handler] : handlers)
			{
				if (handler.luaRef != LUA_NOREF)
				{
					luaL_unref(L, LUA_REGISTRYINDEX, handler.luaRef);
				}
			}
		}

		handlers.clear();
	}

	std::vector<std::string> RPCRegistry::DumpNames() const
	{
		std::vector<std::string> names;
		names.reserve(handlers.size());

		for (auto const& [name, handler] : handlers) names.push_back(name);

		std::sort(names.begin(), names.end());
		return names;
	}

	RPCRegistry& GetRPCRegistry()
	{
		static RPCRegistry registry;
		return registry;
	}

	size_t EnsureDefaultRPCHandlersRegistered(lua_State* L)
	{
		if (!L)
		{
			GGLogger::Error("RPC: cannot register handlers, no Lua state");
			return 0;
		}

		static bool swept = false;
		if (swept) return 0;

		RPCRegistry& registry = GetRPCRegistry();

		size_t const before = registry.Count();

		// ServerRPC_* take the caller's clientID as argument 1 by convention,
		// so the transport-authenticated PlayerID is reconciled against arg 0.
		registry.RegisterAllByPrefix(L, "ServerRPC_", RPCDirection::ServerBound, 0);

		// ClientRPC_* come from the server and carry no client-asserted
		// identity, so there is nothing to reconcile.
		registry.RegisterAllByPrefix(L, "ClientRPC_", RPCDirection::ClientBound, -1);

		swept = true;

		size_t const added = registry.Count() - before;

		GGLogger::Info("RPC: " + std::to_string(registry.Count())
			+ " handler(s) reachable from the network ("
			+ std::to_string(added) + " added by convention sweep)");

		return added;
	}
}
