/*************************************************************************
  \file       UDPTypes.hpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
    Core type definitions and constants for UDP networking system
  \details
    Provides fundamental type aliases and configuration constants for custom
    UDP reliability protocol and game networking. Defines:
    - Type aliases: PlayerID, SessionID, SequenceNum for type safety
    - Network constants: port numbers, size limits, timeout values
    - Enumerations: PacketType (transport layer), MessageType (application layer)

    Separates transport concerns (CONNECT, ACK, DATA packets) from application
    concerns (CLIENT_CONNECT_REQUEST, SERVER_GAME_STATE messages).

    Configuration tuned for turn-based gameplay with moderate player counts
    (MAX_PLAYERS=256). Adjust timeouts for real-time games or high-latency networks.
  \dependencies
    <cstdint>
  \see
    PacketType, MessageType, UDPPacket.hpp, UDPMessage.hpp
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <cstdint>

namespace GGNetworking
{
    using PlayerID = uint32_t;
    using MessageID = uint16_t;
    using SessionID = uint64_t;
    using SequenceNum = uint32_t;
    using AcknowledgementNum = uint32_t;

    // Wire-protocol version. Neither PacketHeader nor MessageHeader carries a
    // version field, so two mismatched builds will connect happily and then
    // fail in confusing ways. Until a version byte is added to PacketHeader
    // (a wire-breaking change, to be made deliberately in one commit), the
    // only guard is filtering on this at the lobby level - see SteamLobby's
    // gg_build key. Bump it whenever the wire format or RPC contract changes.
    static constexpr uint32_t GG_PROTOCOL_VERSION = 1;

    static constexpr uint16_t DEFAULT_SERVER_PORT = 9081;       // DO NOT CHANGE THIS!!!
    static constexpr uint32_t MAX_PLAYERS = 256;
    static constexpr uint32_t MAX_PACKET_SIZE = 1400;           // 1400 Bytes per packet, with/without fragmentation
    static constexpr uint32_t MAX_MESSAGE_SIZE = 16384;         // 16KB max message payload (fragmented if > MAX_PACKET_SIZE)
    static constexpr uint32_t MAX_DATAGRAM_SIZE = MAX_PACKET_SIZE + 32; // Max recv buffer per single UDP datagram
    static constexpr uint32_t CONNECTION_TIMEOUT_MS = 30000;    // 30 Seconds timeout
    static constexpr uint32_t PACKET_TIMEOUT_MS = 1000;         // 1 second for retransmission (initial RTO)
    static constexpr double   MIN_RTO_MS = 20.0;                  // Minimum adaptive RTO (20ms prevents spurious retransmits under multi-client I/O thread load)
    static constexpr double   MAX_RTO_MS = 1000.0;               // Maximum adaptive RTO cap
    static constexpr uint32_t INITIAL_SSTHRESH = 64;              // Initial slow start threshold
    static constexpr uint32_t CLIENT_PING_INTERVAL_MS = 2000;   // Client sends ping every 2 seconds
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 5000;     // 5 second heartbeat (for server-side use)
    static constexpr int64_t  HEARTBEAT_TIMEOUT_US = 10000000L; // 10 seconds timeout (2x safety margin over old 5s)
    static constexpr uint32_t MAX_RETRANSMISSIONS = 25;          // Max retransmissions (~20s retry window with exponential backoff from 20ms, capped at 1000ms)

    // RPC rate limiting. A turn-based game sends single-digit RPCs per second,
    // so these bounds are generous; tighten them once you have real traffic data.
    static constexpr uint32_t RPC_TOKEN_BURST          = 120;    // Max tokens a connection may bank
    static constexpr double   RPC_TOKEN_REFILL_PER_SEC = 60.0;   // Sustained RPCs per second per connection
    static constexpr uint32_t RPC_MAX_VIOLATIONS       = 100;    // Violations before the connection is dropped
    static constexpr uint32_t MAX_MESSAGES_PER_TICK    = 256;    // Cap main-thread drain so one peer cannot monopolise a frame

    // Fragmentation constants
    static constexpr size_t FRAGMENT_HEADER_SIZE = 8;           // messageID(4) + fragmentIndex(2) + fragmentTotal(2)
    static constexpr size_t MAX_FRAGMENT_PAYLOAD = MAX_PACKET_SIZE - 9 - FRAGMENT_HEADER_SIZE; // ~1383 bytes per fragment
    static constexpr uint32_t FRAGMENT_TIMEOUT_MS = 10000;      // 10 seconds to reassemble all fragments

#pragma pack(push, 1)
    /*************************************************************************
      \struct FragmentHeader
      \brief
        Header prepended to each fragment's payload inside a FRAGMENT packet.
      \details
        Sits between PacketHeader and the fragment data. All fragments of the
        same original message share the same messageID. fragmentIndex is 0-based
        and fragmentTotal is the count of fragments needed to reassemble.
    *************************************************************************/
    struct FragmentHeader
    {
        uint32_t messageID;
        uint16_t fragmentIndex;
        uint16_t fragmentTotal;

        static constexpr size_t HEADER_SIZE = 8;
    };

    static_assert(FragmentHeader::HEADER_SIZE == sizeof(FragmentHeader), "FragmentHeader size mismatch!");
#pragma pack(pop)

    /*************************************************************************
      \brief
        UDP packet types for reliability layer
      \details
        Low-level packet classification for the custom reliability implementation.
        These are transport layer concerns, not game message types.
    *************************************************************************/
    enum class PacketType : uint8_t
    {
        DATA = 0,           // Regular data packet
        ACK = 1,            // Acknowledgment packet
        HEARTBEAT = 2,      // Keep-alive packet
        CONNECT = 3,        // Connection initiation
        DISCONNECT = 4,     // Clean disconnection
        FRAGMENT = 5,       // Message fragment (for large messages)
        NAK = 6,            // Negative acknowledgment (request retransmission due to corruption)
        UNRELIABLE_DATA = 7,// Fire-and-forget data packet (no ACK, no retransmission)

        BAD = 8,
        MAX_PACKET_TYPES = 9
    };

    /*************************************************************************
      \brief
        Game message types for turn-based communication
      \details
        Application-layer message types. These travel inside DATA packets
        after being processed by the reliability layer.
    *************************************************************************/
    enum class MessageType : MessageID
    {
        // Connection management
        CLIENT_CONNECT_REQUEST = 100,
        SERVER_CONNECT_RESPONSE = 101,
        CLIENT_DISCONNECT_REQUEST = 102,
        CLIENT_DISCONNECT_RESPONSE = 103,
        CLIENT_PING = 104,
        SERVER_PONG = 105,

        // Game state management  
        RPC_CALL = 199,
        SERVER_GAME_STATE = 200,
        SERVER_TURN_START = 201,
        CLIENT_TURN_ACTION = 202,
        SERVER_TURN_RESULT = 203,
        SERVER_TURN_TIMEOUT = 204,

        // Player management
        SERVER_PLAYER_JOINED = 300,
        SERVER_PLAYER_LEFT = 301,
        SERVER_PLAYER_LIST = 302,

        // Game flow
        SERVER_GAME_START = 400,
        SERVER_GAME_END = 401,
        CLIENT_READY = 402,

        // Lobby management
        CLIENT_REQUEST_LOBBY_LIST = 700,    // Client requests list of available lobbies
        SERVER_LOBBY_LIST = 701,            // Server responds with lobby list
        CLIENT_CREATE_LOBBY = 702,          // Client requests to create a new lobby (becomes host)
        SERVER_CREATE_LOBBY_RESPONSE = 703, // Server responds to lobby creation
        CLIENT_JOIN_LOBBY = 704,            // Client requests to join a lobby
        SERVER_JOIN_LOBBY_RESPONSE = 705,   // Server responds to lobby join request
        CLIENT_LEAVE_LOBBY = 706,           // Client leaves lobby (or host closes lobby)
        SERVER_LOBBY_CLOSED = 707,          // Server notifies all clients that lobby was closed (host left)
        SERVER_CLIENT_JOINED_LOBBY = 708,   // Server notifies lobby members a new client joined
        SERVER_CLIENT_LEFT_LOBBY = 709,     // Server notifies lobby members a client left
        CLIENT_START_GAME = 710,            // Host requests game start
        SERVER_GAME_STARTING = 711,         // Server notifies all lobby members game is starting

        // Real-time gameplay (unreliable channel)
        CLIENT_INPUT = 600,             // Client sends input buffer (unreliable)
        SERVER_SNAPSHOT = 601,          // Server broadcasts world state (unreliable)

        // Error handling
        SERVER_ERROR = 500
    };
}