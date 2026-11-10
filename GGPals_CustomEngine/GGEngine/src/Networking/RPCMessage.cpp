/*************************************************************************
  \file		  RPCMessage.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Implements Lua RPC serialization and deserialization.

  \details
	Provides LuaRPCSerializer to serialize Lua function calls with arguments
	into NetworkMessage, and LuaRPCDeserializer to deserialize and execute
	them in Lua.

  \dependencies
	- GGLuaVM.hpp
	- GGEngine.hpp
	- GGLogger.hpp

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <RPCMessage.hpp>
#include <RPCRegistry.hpp>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <GGLuaVM.hpp>
#include <GGEngine.hpp>
#include <GGLogger.hpp>

namespace GGNetworking
{
	LuaRPCSerializer::LuaRPCSerializer(const char* functionName)
	{
		size_t nameLen = std::strlen(functionName);

		if (nameLen > 62)
		{
			std::cerr << "RPC function name {" << functionName << "} exceeds 62 chars, truncating "
				<< functionName << std::endl;

			GGLogger::Error("RPC function name" + std::string(functionName) + "exceeds 62 chars, truncating...");

			nameLen = 62;
		}

		header.functionNameLength = static_cast<uint8_t>(nameLen);
		std::memcpy(header.functionName, functionName, nameLen);
		header.functionName[nameLen] = '\0';
		header.argCount = 0;

		// Pre-allocate memory for args
		argData.reserve(256);
	}

	void LuaRPCSerializer::AddArg(bool value)
	{
		argData.push_back(static_cast<uint8_t>(ArgType::ARG_BOOLEAN));
		argData.push_back(value ? 1 : 0);
		
		header.argCount++;
	}

	void LuaRPCSerializer::AddArg(double value)
	{
		argData.push_back(static_cast<uint8_t>(ArgType::ARG_NUMBER));
		
		const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
		argData.insert(argData.end(), ptr, ptr + sizeof(double));
		
		header.argCount++;
	}

	void LuaRPCSerializer::AddArg(int32_t value)
	{
		argData.push_back(static_cast<uint8_t>(ArgType::ARG_INTEGER));
		
		const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
		argData.insert(argData.end(), ptr, ptr + sizeof(uint32_t));

		header.argCount++;
	}

	void LuaRPCSerializer::AddArg(const char* value)
	{
		argData.push_back(static_cast<uint8_t>(ArgType::ARG_STRING));
		
		size_t strLen = std::strlen(value);

		if (strLen > 255)
		{
			std::cerr << "Warning: RPC string argument exceeds 255 bytes, truncating" << std::endl;
			GGLogger::Warning("RPC string argument exceeds 255 bytes, truncating...");
			strLen = 255;
		}

		argData.push_back(static_cast<uint8_t>(strLen));

		argData.insert(argData.end(),
			reinterpret_cast<const uint8_t*>(value),
			reinterpret_cast<const uint8_t*>(value) + strLen);

		header.argCount++;
	}

	// Do we really need this? Ig we could yea...
	void LuaRPCSerializer::AddArgEntity(uint64_t packedEntity)
	{
		argData.push_back(static_cast<uint8_t>(ArgType::ARG_ENTITY));

		const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&packedEntity);
		argData.insert(argData.end(), ptr, ptr + sizeof(uint64_t));

		header.argCount++;
	}

	void LuaRPCSerializer::WriteTypeByte(ArgType type)
	{
		argData.push_back(static_cast<uint8_t>(type));
	}

	void LuaRPCSerializer::WriteRawByte(uint8_t b)
	{
		argData.push_back(b);
	}

	void LuaRPCSerializer::WriteRawData(const void* data, size_t len)
	{
		const uint8_t* ptr = static_cast<const uint8_t*>(data);
		argData.insert(argData.end(), ptr, ptr + len);
	}

	void LuaRPCSerializer::IncrementArgCount()
	{
		header.argCount++;
	}

	void LuaRPCSerializer::Serialize(NetworkMessage& outMessage) const
	{
		outMessage.SetMessageType(MessageType::RPC_CALL);
		outMessage.WriteValue(header);
		outMessage.WriteRawBytes(argData.data(), argData.size());
	}

	LuaRPCDeserializer::LuaRPCDeserializer()
	{

	}

	/*************************************************************************
	  \brief
		lua_pcall message handler that decorates the error with a traceback.
	  \param[in] L
		Lua state, with the error value at stack index 1.
	  \return
		1 - the decorated error string.
	  \details
		Resolves debug.traceback dynamically rather than calling luaL_traceback
		so this compiles against both stock Lua 5.1 and LuaJIT. Degrades to the
		bare message if the debug library is unavailable.
	*************************************************************************/
	static int LuaRPCTracebackHandler(lua_State* L)
	{
		char const* msg = lua_tostring(L, 1);
		if (!msg) msg = "(non-string error)";

		lua_getglobal(L, "debug");
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			lua_pushstring(L, msg);
			return 1;
		}

		lua_getfield(L, -1, "traceback");
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 2);
			lua_pushstring(L, msg);
			return 1;
		}

		lua_pushstring(L, msg);
		lua_pushinteger(L, 2);      // skip this handler's own frame
		lua_call(L, 2, 1);
		lua_remove(L, -2);          // drop the debug table, leave the traceback
		return 1;
	}

	bool LuaRPCDeserializer::Deserialize(const NetworkMessage& message,
	                                     PlayerID senderID,
	                                     RPCDirection direction)
	{
		lua_State* L = GetEngineInstance().GetLuaVM()->GetState();

		RPCMessageHeader header;
		try
		{
			header = message.ReadValue<RPCMessageHeader>();
		}
		catch (const std::out_of_range&)
		{
			GGLogger::Error("RPC deserialize failed: Header read out of bounds");
			return false;
		}

		// --- 1. Name length -------------------------------------------------
		// Bounded write: functionName is char[63], so index <= 62 is in range.
		if (header.functionNameLength > MAX_RPC_NAME_LEN || header.functionNameLength == 0)
		{
			GGLogger::Warning("RPC rejected: invalid function name length "
				+ std::to_string(header.functionNameLength));
			return false;
		}

		header.functionName[header.functionNameLength] = '\0';

		std::string_view const fnName{ header.functionName, header.functionNameLength };

		// --- 2. Name charset ------------------------------------------------
		// Rejected before the name is used as a map key or written to a log,
		// so control characters and escape sequences cannot reach either.
		if (!RPCRegistry::IsValidName(fnName))
		{
			GGLogger::Warning("RPC rejected: function name failed charset validation (from player "
				+ std::to_string(senderID) + ")");
			return false;
		}

		// --- 3. Registry lookup ---------------------------------------------
		// This replaces lua_getglobal. An unregistered name can no longer reach
		// _G, so loadstring / dofile / require / engine bindings are unreachable
		// from the network regardless of what the peer sends.
		RPCHandler const* handler = nullptr;

		switch (GetRPCRegistry().Lookup(fnName, direction, handler))
		{
		case RPCLookupResult::Found:
			break;

		case RPCLookupResult::NotRegistered:
			GGLogger::Warning("RPC rejected: '" + std::string(fnName)
				+ "' is not a registered handler (from player " + std::to_string(senderID) + ")");
			return false;

		case RPCLookupResult::WrongDirection:
			GGLogger::Warning("RPC rejected: '" + std::string(fnName)
				+ "' is not " + ToString(direction) + " (from player "
				+ std::to_string(senderID) + ")");
			return false;
		}

		// --- 4. Arity -------------------------------------------------------
		if (header.argCount > MAX_RPC_ARGS)
		{
			GGLogger::Warning("RPC rejected: '" + std::string(fnName) + "' argCount "
				+ std::to_string(header.argCount) + " exceeds MAX_RPC_ARGS");
			return false;
		}

		if (header.argCount < handler->minArgs || header.argCount > handler->maxArgs)
		{
			GGLogger::Warning("RPC rejected: '" + std::string(fnName) + "' argCount "
				+ std::to_string(header.argCount) + " outside declared bounds "
				+ std::to_string(handler->minArgs) + ".." + std::to_string(handler->maxArgs));
			return false;
		}

		// --- 5. Lua stack ---------------------------------------------------
		// The C API does not grow the stack for us. Without this, argCount up
		// to 255 overruns LUA_MINSTACK (20) and corrupts the VM.
		// +8 covers the traceback handler, the function, and table scratch.
		if (!lua_checkstack(L, static_cast<int>(header.argCount) + 8))
		{
			GGLogger::Error("RPC rejected: cannot grow Lua stack for "
				+ std::to_string(header.argCount) + " args");
			return false;
		}

		// Every failure path below unwinds with lua_settop(baseTop), which is
		// robust against miscounted pops in the nested argument reader.
		int const baseTop = lua_gettop(L);

		lua_pushcfunction(L, LuaRPCTracebackHandler);
		int const errFuncIdx = lua_gettop(L);

		lua_rawgeti(L, LUA_REGISTRYINDEX, handler->luaRef);

		if (!lua_isfunction(L, -1))
		{
			// Stale ref: the VM was reloaded without RPCRegistry::Clear.
			GGLogger::Error("RPC rejected: '" + std::string(fnName)
				+ "' has a stale registry reference - was the Lua VM reloaded?");
			lua_settop(L, baseTop);
			return false;
		}

		// --- 6. Arguments ---------------------------------------------------
		for (uint8_t i{}; i < header.argCount; i++)
		{
			if (!ReadAndPushArg(L, message, 0))
			{
				GGLogger::Warning("RPC rejected: '" + std::string(fnName)
					+ "' argument " + std::to_string(i) + " failed to deserialize");
				lua_settop(L, baseTop);
				return false;
			}
		}

		// --- 7. Sender reconciliation ---------------------------------------
		// The caller's identity travels as a normal argument by convention
		// (see ConnectionCallbacks.lua / NetworkedGameClient.lua), which makes
		// it client-asserted. Cross-check it against the identity the transport
		// actually authenticated, so a peer cannot act as another player.
		if (handler->senderArgIndex >= 0)
		{
			int const senderIdx = static_cast<int>(handler->senderArgIndex);

			if (senderIdx >= static_cast<int>(header.argCount))
			{
				GGLogger::Warning("RPC rejected: '" + std::string(fnName)
					+ "' expects sender at arg " + std::to_string(senderIdx)
					+ " but only " + std::to_string(header.argCount) + " args were sent");
				lua_settop(L, baseTop);
				return false;
			}

			// args begin one slot above the function, which sits above errFunc
			int const slot = errFuncIdx + 2 + senderIdx;

			if (!lua_isnumber(L, slot)
				|| static_cast<PlayerID>(lua_tointeger(L, slot)) != senderID)
			{
				GGLogger::Warning("RPC rejected: '" + std::string(fnName)
					+ "' sender mismatch - claimed "
					+ (lua_isnumber(L, slot) ? std::to_string(lua_tointeger(L, slot))
					                         : std::string("<not a number>"))
					+ ", authenticated " + std::to_string(senderID));
				lua_settop(L, baseTop);
				return false;
			}
		}

		// --- 8. Dispatch ----------------------------------------------------
		int const result = lua_pcall(L, header.argCount, 0, errFuncIdx);

		if (result != LUA_OK)
		{
			const char* errorMsg = lua_tostring(L, -1);

			GGLogger::Error("RPC call '" + std::string(fnName) + "' failed: "
				+ std::string(errorMsg ? errorMsg : "unknown error"));

			lua_settop(L, baseTop);
			return false;
		}

		lua_settop(L, baseTop);
		return true;
	}

	bool LuaRPCDeserializer::ReadAndPushArg(lua_State* L, const NetworkMessage& message, int depth)
	{
		// The serializer caps nesting at MAX_RPC_DEPTH, so any payload deeper
		// than this was hand-crafted. Without this check each nesting level is
		// an unbounded C recursion: ~3 bytes of wire data per frame against a
		// 16KB message cap is thousands of frames, which exhausts the stack.
		if (depth > MAX_RPC_DEPTH)
		{
			GGLogger::Warning("RPC rejected: argument nesting exceeds MAX_RPC_DEPTH ("
				+ std::to_string(MAX_RPC_DEPTH) + ")");
			return false;
		}

		// Each level pushes a table plus a key/value pair.
		if (!lua_checkstack(L, 4))
		{
			GGLogger::Error("RPC rejected: cannot grow Lua stack for nested argument");
			return false;
		}

		try
		{
			ArgType argType = message.ReadValue<ArgType>();

			switch (argType)
			{
			case ArgType::ARG_NIL:
			{
				lua_pushnil(L);
				break;
			}
			case ArgType::ARG_BOOLEAN:
			{
				uint8_t value = message.ReadValue<uint8_t>();
				lua_pushboolean(L, value != 0);
				break;
			}
			case ArgType::ARG_NUMBER:
			{
				double value = message.ReadValue<double>();
				lua_pushnumber(L, value);
				break;
			}
			case ArgType::ARG_INTEGER:
			{
				int32_t value = message.ReadValue<int32_t>();
				lua_pushinteger(L, value);
				break;
			}
			case ArgType::ARG_STRING:
			{
				uint8_t strLen = message.ReadValue<uint8_t>();

				// Read string bytes
				std::vector<char> strBuffer(strLen + 1);  // +1 for null terminator
				for (uint8_t i = 0; i < strLen; ++i)
				{
					strBuffer[i] = static_cast<char>(message.ReadValue<uint8_t>());
				}
				strBuffer[strLen] = '\0';

				// Length-explicit push: an embedded NUL must not silently
				// truncate the string, which would let a peer smuggle a short
				// value past a Lua-side length or content check.
				lua_pushlstring(L, strBuffer.data(), strLen);
				break;
			}
			case ArgType::ARG_ENTITY:
			{
				uint64_t packedEntity = message.ReadValue<uint64_t>();
				lua_pushlightuserdata(L, reinterpret_cast<void*>(packedEntity));
				break;
			}
			case ArgType::ARG_ARRAY:
			{
				uint16_t count = message.ReadValue<uint16_t>();

				// Preallocation hint only - never trust the declared count.
				// It is a uint16 read straight off the wire and is honoured
				// before a single element is read, so an unclamped count turns
				// ~3 bytes of payload into a 65535-slot allocation. The table
				// still grows naturally if the data really is that large.
				lua_createtable(L, (std::min)(count, MAX_TABLE_PREALLOC), 0);

				for (uint16_t i = 0; i < count; i++)
				{
					// An inflated count now costs a bounds-checked ReadValue
					// throwing out_of_range on the first element past the
					// payload, rather than the allocation above.
					if (!ReadAndPushArg(L, message, depth + 1))
					{
						lua_pop(L, 1); // Pop the partially built table
						return false;
					}
					lua_rawseti(L, -2, static_cast<int>(i + 1));
				}
				break;
			}
			case ArgType::ARG_TABLE:
			{
				uint16_t count = message.ReadValue<uint16_t>();

				lua_createtable(L, 0, (std::min)(count, MAX_TABLE_PREALLOC));

				for (uint16_t i = 0; i < count; i++)
				{
					// Read and push key
					if (!ReadAndPushArg(L, message, depth + 1))
					{
						lua_pop(L, 1); // Pop the partially built table
						return false;
					}
					// Read and push value
					if (!ReadAndPushArg(L, message, depth + 1))
					{
						lua_pop(L, 2); // Pop key + table
						return false;
					}
					lua_rawset(L, -3); // t[key] = value, pops key and value
				}
				break;
			}
			default:
				std::cout << "Error: Unknown RPC arg type " << static_cast<int>(argType);
				GGLogger::Error("Unknown RPC arg type " + std::to_string(static_cast<int>(argType)));
				return false;
			}

			return true;
		}
		catch (const std::out_of_range&)
		{
			std::cerr << "RPC arg read out of bounds" << std::endl;
			GGLogger::Error("RPC arg read out of bounds!");
			return false;
		}
	}
}