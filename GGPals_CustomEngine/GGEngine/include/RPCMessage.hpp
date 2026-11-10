/*************************************************************************
  \file		  RPCMessage.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Defines Lua RPC message structures, serialization, and deserialization.

  \details
	Provides:
	- RPCMessageHeader: Packed header for RPC calls
	- LuaRPCSerializer: Builds and serializes RPC calls
	- LuaRPCDeserializer: Deserializes RPC messages and pushes arguments to Lua

  \dependencies
	- UDPMessage.hpp
	- GGLuaVM


  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <UDPMessage.hpp>
#include <RPCRegistry.hpp>

class GGLuaVM;

namespace GGNetworking
{
	// Supported argument types for Lua RPC calls
	enum class ArgType : uint8_t
	{
		ARG_NIL = 0,
		ARG_BOOLEAN,
		ARG_INTEGER,
		ARG_STRING,
		ARG_NUMBER,
		ARG_ENTITY,
		ARG_ARRAY,
		ARG_TABLE
	};

	// Packed header for RPC messages
	#pragma pack(push, 1)
	struct RPCMessageHeader
	{
		uint8_t functionNameLength{};	// 0-62 (actual string length)
		char	functionName[63];		// 62 chars + \0
		uint8_t	argCount{};				// 0-255 args

		static constexpr size_t HEADER_SIZE = 65;
	};
	#pragma pack(pop)

	// Compile-time checks
	static_assert(sizeof(RPCMessageHeader) == 65, "Header must be 65 bytes");
	static_assert(std::is_standard_layout_v<RPCMessageHeader>, "Must be POD");
	static_assert(offsetof(RPCMessageHeader, functionName) == 1, "No padding");

	/*************************************************************************
	  \class LuaRPCSerializer
	  \brief 
		Helper class for constructing RPC messages to send over the network.
	  \details
		- Stores the function name in a packed header
		- Allows adding arguments of multiple types (bool, int, double, string, entity, table, array)
		- Serializes the header and argument data into a NetworkMessage
	***************************************************************************/
	class LuaRPCSerializer
	{
	public:
		explicit LuaRPCSerializer(const char* functionName);

		void AddArg(bool value);
		void AddArg(double value);
		void AddArg(int32_t value);
		void AddArg(const char* value);
		void AddArgEntity(uint64_t packedEntity);

		// Raw write access for composite types (tables/arrays)
		void WriteTypeByte(ArgType type);
		void WriteRawByte(uint8_t b);
		void WriteRawData(const void* data, size_t len);
		void IncrementArgCount();

		void Serialize(NetworkMessage& outMessage) const;
	
		inline uint8_t GetArgCount() const { return header.argCount; }
	private:
		RPCMessageHeader		header;
		std::vector<uint8_t>	argData;
	};

	/*************************************************************************
	  \class LuaRPCDeserializer
	  \brief
		Helper class for reading RPC messages from the network.
	  \details
		- Parses a NetworkMessage
		- Pushes arguments to a Lua state (lua_State*)
		- Provides ReadAndPushArg() internally for argument-by-argument deserialization
	***************************************************************************/
	class LuaRPCDeserializer
	{
	public:
		explicit LuaRPCDeserializer();

		/*************************************************************************
		  \brief
			Validates and dispatches one inbound RPC message into Lua.
		  \param[in] message
			Message payload. Every field is treated as hostile input.
		  \param[in] senderID
			Transport-authenticated PlayerID of the peer that sent this message,
			as resolved by ReliabilityLayer::FindPlayerByEndpoint. This is the
			authoritative identity - never the one the sender claims in its args.
		  \param[in] direction
			Direction this process accepts: ServerBound on a host, ClientBound
			on a client. Handlers registered for the other direction are rejected.
		  \return
			true if a registered handler ran to completion without error.
		  \details
			Rejection path, in order, all failing closed:
			  1. Header read out of bounds
			  2. Function name length / character set invalid
			  3. Name not in the RPC registry, or registered for the other side
			  4. argCount outside global or per-handler arity bounds
			  5. Lua stack cannot be grown to hold the arguments
			  6. Any argument fails to deserialize (depth, size, or type)
			  7. Sender argument does not match the authenticated senderID
			Dispatch uses lua_rawgeti against the handler's captured reference.
			It never calls lua_getglobal with a network-supplied name.
		  \note
			Main thread only - Lua is not re-entrant across threads.
		  \see
			RPCRegistry, GameServer::Update, GameClient::Update
		*************************************************************************/
		bool Deserialize(const NetworkMessage& message,
		                 PlayerID senderID,
		                 RPCDirection direction);

	private:

		/*************************************************************************
		  \brief
			Reads one argument from the message and pushes it onto the Lua stack.
		  \param[in] L
			Lua state.
		  \param[in] message
			Source message, positioned at the next argument.
		  \param[in] depth
			Current nesting level. Recursion is refused past MAX_RPC_DEPTH so a
			hand-crafted deeply-nested payload cannot exhaust the C stack.
		  \return
			true on success; false on malformed data, excessive nesting, or an
			un-growable Lua stack.
		*************************************************************************/
		bool ReadAndPushArg(lua_State* L, const NetworkMessage& message, int depth);
	};

	// Singleton-style accessor for a global RPC deserializer
	inline std::unique_ptr<LuaRPCDeserializer>& GetRPCDeserializer()
	{
		static std::unique_ptr<LuaRPCDeserializer> rpcDeserializer = std::make_unique<LuaRPCDeserializer>();
		return rpcDeserializer;
	}
}