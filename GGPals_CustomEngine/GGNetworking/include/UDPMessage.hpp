/*************************************************************************
  \file       UDPMessage.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       17 Mar 2026
  \version    1.0
  \brief
    Application-layer message definitions with type-safe serialization
  \details
    Defines NetworkMessage class and MessageHeader struct for game-level
    communication over UDP. Provides templated ReadValue/WriteValue methods
    constrained by IsNetworkSerializable concept for compile-time safety.

    Separates application concerns (game messages, ECS components) from
    transport concerns (packet headers, checksums, sequencing). Messages
    are encapsulated within DATA packets at the reliability layer.

    Includes predefined network data structures (NetworkConnectionData,
    NetworkPing) demonstrating serializable types. Enforces POD-like
    constraints (trivially copyable, standard layout) for deterministic
    binary serialization without custom marshaling logic.
  \dependencies
    UDPTypes.hpp, <vector>, <chrono>, <typeinfo>, <cstring>, <concepts>
  \see
    NetworkMessage, MessageHeader, IsNetworkSerializable, UDPMessage.cpp
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <UDPTypes.hpp>

#include <vector>
#include <chrono>
#include <typeinfo>

/*************************************************************************
  \concept IsComponent
  \brief
    Compile-time constraint for ECS component validity.
  \tparam T
    Type to validate as a legal component.
  \details
    Enforces the following invariants for component types:
    - Must expose COMPONENT_ID as constexpr uint16_t for O(1) type lookup
    - Must expose SERIALIZED_SIZE as constexpr size_t for binary I/O
    - Must be trivially copyable (memcpy-safe for serialization)
    - Must be standard layout (predictable memory layout across compilers)
    - Cannot be pointer or reference (entity-component relationships are
      managed externally to prevent dangling references)

    These constraints enable zero-copy serialization, memory-mapped file I/O,
    and deterministic network replication without custom serialization logic.
  \note
    The trivially_copyable constraint prevents use of std::string, std::vector,
    or std::unique_ptr in components. Use fixed-size buffers (char[N]) or
    asset ID handles (uint32_t) with external resource management instead.
  \warning
    GLM types (vec3, mat4) satisfy these constraints but may contain padding
    bytes that are undefined. Always zero-initialize GLM components or use
    explicit padding fields for deterministic serialization.
  \par Rationale
    POD-like components enable the ECS to achieve data-oriented design goals:
    tight packing in memory, minimal indirection, and predictable performance
    characteristics. The concept acts as a compiler-enforced contract preventing
    introduction of heap allocations or virtual dispatch into the hot path.
  \see
    ComponentArray, ComponentTypeInfo, SERIALIZED_SIZE
*************************************************************************/
template<typename T>
concept IsNetworkSerializable = requires {
        requires std::is_trivially_copyable_v<T>;
        requires std::is_standard_layout_v<T>;
        requires !std::is_pointer_v<T>;
        requires !std::is_reference_v<T>;
};

struct NetworkConnectionData
{
    int64_t     timeSent{};
};

struct NetworkConnectionDataReturn
{
    int64_t     serverTimeReceived{};
    uint32_t    playerID{};
    uint8_t     connectionStatus{};
};

struct NetworkPing
{
    int64_t     clientTimeSent{};
};

struct NetworkPingReturn
{
    int64_t     serverTimeReceived{};
};

// -----------------------------------------------------------------------
// Lobby protocol data structures
// -----------------------------------------------------------------------

// Single lobby entry sent in SERVER_LOBBY_LIST
struct LobbyInfo
{
    uint32_t    lobbyID{};
    char        lobbyName[32]{};
    uint32_t    hostPlayerID{};
    uint8_t     playerCount{};
    uint8_t     maxPlayers{ 4 };
    uint8_t     inGame{};           // 0 = in lobby, 1 = game in progress
};

// Sent in SERVER_LOBBY_LIST: header followed by LobbyInfo array
struct LobbyListHeader
{
    uint32_t    lobbyCount{};
};

// Sent in CLIENT_CREATE_LOBBY
struct CreateLobbyRequest
{
    char        lobbyName[32]{};
    uint8_t     maxPlayers{ 4 };
};

// Sent in SERVER_CREATE_LOBBY_RESPONSE
struct CreateLobbyResponse
{
    uint32_t    lobbyID{};
    uint8_t     success{};          // 1 = created, 0 = failed
};

// Sent in CLIENT_JOIN_LOBBY
struct JoinLobbyRequest
{
    uint32_t    lobbyID{};
};

// Sent in SERVER_JOIN_LOBBY_RESPONSE
struct JoinLobbyResponse
{
    uint32_t    lobbyID{};
    uint32_t    hostPlayerID{};
    uint8_t     success{};          // 1 = joined, 0 = failed (full, not found, etc.)
    uint8_t     currentPlayerCount{};
};

// Sent in SERVER_CLIENT_JOINED_LOBBY / SERVER_CLIENT_LEFT_LOBBY
struct LobbyPlayerEvent
{
    uint32_t    lobbyID{};
    uint32_t    playerID{};
    uint8_t     currentPlayerCount{};
};

// Sent in SERVER_LOBBY_CLOSED
struct LobbyClosed
{
    uint32_t    lobbyID{};
    uint8_t     reason{};           // 0 = host left, 1 = server shutdown
};

// Sent in SERVER_GAME_STARTING
struct GameStarting
{
    uint32_t    lobbyID{};
    uint32_t    hostPlayerID{};
    uint8_t     playerCount{};
};

namespace GGNetworking
{
#pragma pack(push, 1)
    /*************************************************************************
      \struct MessageHeader
      \brief
        Application-layer message header for NetworkMessage.
      \details
        10-byte header identifying message type and payload for deserialization.
        Separate from PacketHeader; messages are payload within DATA packets.
      \see
        NetworkMessage, MessageType
    *************************************************************************/
    struct MessageHeader
    {
        MessageType     messageType;
        uint32_t        payloadSize;
        uint32_t        messageID;

        static constexpr size_t HEADER_SIZE = 10;
    };

    static_assert(MessageHeader::HEADER_SIZE == sizeof(MessageHeader), "HEADER_SIZE does not match struct size!");

#pragma pack(pop)

    /*************************************************************************
      \class NetworkMessage
      \brief
        Type-safe application-layer message with serialization.
      \details
        Encapsulates game messages with automatic header management and type-safe
        payload serialization. Uses MessageHeader for type identification and
        ECS component constraints for serialization safety. Supports bidirectional
        conversion to/from byte streams for network transmission.
      \see
        MessageHeader, MessageType, IsComponent
    *************************************************************************/
    class NetworkMessage
    {
    public:
        enum class MessageDeserializeStatus
        {
            BAD_MESSAGE = 600,
            BYTES_NOT_SAME_TOTAL = 601,
            INVALID_HEADER = 602,

            NO_PAYLOAD = 604,
            MESSAGE_DESERIALIZE_SUCCESS = 605
        };

        /*************************************************************************
          \brief
            Default constructor initializing empty network message.
          \details
            Creates NetworkMessage with uninitialized messageHeader. Payload vector
            is default-constructed (empty). ReadOffset starts at 0.
        *************************************************************************/
        NetworkMessage();
        /*************************************************************************
          \brief
            Constructs network message with specified type.
          \param type
            MessageType enum value to set in message header.
          \details
            Initializes messageHeader.messageType to provided value. Payload remains
            empty until WriteValue calls. Auto-assigns unique messageID from static counter.
        *************************************************************************/
        NetworkMessage(MessageType type);
        /*************************************************************************
          \brief
            Reads component value from payload with bounds checking.
          \tparam T
            Component type satisfying IsComponent concept.
          \return
            Deserialized component instance.
          \throws std::out_of_range
            If payload insufficient for sizeof(T).
          \details
            Performs memcpy from payload at readOffset, advances offset by sizeof(T).
            Suitable for trivially copyable types only (enforced by IsComponent).
          \note
            Sequential reads must match WriteValue call order. No type verification;
            caller must ensure T matches written type.
        *************************************************************************/
        template <IsNetworkSerializable T>
        inline T ReadValue() const
        {
            if (readOffset + sizeof(T) > payload.size())
            {
                throw std::out_of_range("ReadValue: Buffer underrun");
            }

            T value{};
            std::memcpy(&value, payload.data() + readOffset, sizeof(T));

            readOffset += sizeof(T);

            return value;
        }
        /*************************************************************************
          \brief
            Writes component value to payload.
          \tparam T
            Component type satisfying IsComponent concept.
          \param[in] componentVal
            Component instance to serialize.
          \details
            Appends sizeof(T) bytes to payload via memcpy. Updates payloadSize in
            header. No type tagging; receiver must know expected sequence.
        *************************************************************************/
        template <IsNetworkSerializable T>
        inline void WriteValue(T const& componentVal)
        {
            uint8_t const* ptr = reinterpret_cast<uint8_t const*>(
                static_cast<const void*>(&componentVal)
                );

            payload.insert(payload.end(), ptr, ptr + sizeof(T));
            messageHeader.payloadSize = static_cast<uint32_t>(payload.size());
        }

        inline void WriteRawBytes(const uint8_t* data, size_t len)
        {
            payload.insert(payload.end(), data, data + len);
            messageHeader.payloadSize = static_cast<uint32_t>(payload.size());
        }

        inline void ReadRawBytes(uint8_t* dst, size_t len) const
        {
            if (readOffset + len > payload.size())
            {
                throw std::out_of_range("ReadRawBytes: Buffer underrun");
            }
            std::memcpy(dst, payload.data() + readOffset, len);
            readOffset += len;
        }

        inline size_t GetRemainingPayload() const
        {
            return (readOffset < payload.size()) ? (payload.size() - readOffset) : 0;
        }

        /*************************************************************************
          \brief
            Serializes message to byte array for packet payload.
          \return
            Byte vector containing MessageHeader + payload.
          \details
            Packs header fields in little-endian, appends payload. Total size =
            MessageHeader::HEADER_SIZE + payload.size().
        *************************************************************************/
        std::vector<uint8_t> Serialize() const;
        /*************************************************************************
          \brief
            Deserializes message from byte array.
          \param[in] data
            Raw bytes from packet payload.
          \param[in] size
            Byte count.
          \return
            Status code indicating success or failure.
          \details
            Validates header fields, extracts payload. Resets readOffset to 0 for
            sequential ReadValue calls.
          \see
            MessageDeserializeStatus
        *************************************************************************/
        MessageDeserializeStatus Deserialize(uint8_t const* data, size_t size);
        /*************************************************************************
          \brief
            Sets message type (modifies header).
          \param[in] type
            New MessageType value.
        *************************************************************************/
        inline void SetMessageType(MessageType type) { messageHeader.messageType = type; }
        /*************************************************************************
          \brief
            Retrieves message type from header.
          \return
            MessageType enum value.
        *************************************************************************/
        inline MessageType GetMessageType() const { return messageHeader.messageType; }
        /*************************************************************************
          \brief
            Retrieves unique message identifier.
          \return
            Auto-assigned messageID.
        *************************************************************************/
        inline uint32_t GetMessageID() const { return messageHeader.messageID; }
        /*************************************************************************
          \brief
            Retrieves payload byte count.
          \return
            Size of serialized component data.
        *************************************************************************/
        inline size_t GetPayloadSize() const { return messageHeader.payloadSize; }

    private:
        MessageHeader           messageHeader;
        std::vector<uint8_t>    payload;
        mutable size_t          readOffset{ 0 };
        static uint32_t         nextMessageID;
        /*************************************************************************
          \brief
            Validates message header fields.
          \param[in] header
            Header to validate.
          \return
            true if fields within valid ranges.
          \details
            Checks payloadSize <= MAX_MESSAGE_SIZE, messageType is valid enum value.
        *************************************************************************/
        bool ValidateHeader(MessageHeader const& header);
    };
}