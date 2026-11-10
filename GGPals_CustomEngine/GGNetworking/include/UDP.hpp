/*************************************************************************
  \file       UDP.hpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
    Client connection state tracking for UDP reliability protocol
  \details
    Defines ClientConnection struct representing per-client state on server side.
    Maintains all data required for reliability protocol implementation:
    - Network addressing (NetEndpoint, backend-agnostic send targeting)
    - Identity tracking (PlayerID, SessionID for authentication)
    - Sequence number management (send/receive for ordering and duplication detection)
    - Retransmission queues (unacknowledged packets pending resend)
    - Timeout detection (activity timestamps for connection liveness)
    - Fragment reassembly (buffers for reconstructing large messages)

    Server maintains one ClientConnection instance per connected player in
    ReliabilityLayer::connections map. Accessed via thread-safe lookups.
  \dependencies
    PlatformWindows.hpp, NetEndpoint.hpp, UDPTypes.hpp, UDPPacket.hpp,
    <unordered_map>, <chrono>
  \see
    ClientConnection, ReliabilityLayer, UDPReliabilityLayer.hpp
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <NetEndpoint.hpp>
#include <PlatformWindows.hpp>
#include <UDPTypes.hpp>

#include <unordered_map>
#include <chrono>

#include <UDPPacket.hpp>

namespace GGNetworking
{
#pragma region Connection
    /*************************************************************************
      \struct ClientConnection
      \brief
        Per-client connection state for server-side tracking.
      \details
        Maintains all state required for reliability protocol per connected client:
        - Network endpoint for send targeting (IPv4 or Steam)
        - Identity (PlayerID, SessionID)
        - Sequence tracking for ordering and duplicate detection
        - Retransmission queues for unacknowledged packets
        - Activity timestamps for timeout detection
        - Fragment reassembly buffers
      \note
        Server maintains one ClientConnection instance per connected player in
        ReliabilityLayer::connections map.
      \see
        ReliabilityLayer, GameServer
    *************************************************************************/
    struct ClientConnection {
        // Backend-agnostic peer address. Was a raw sockaddr_in, which welded
        // every layer above the socket to Winsock; a Steam peer has no IP or
        // port to put here, only a connection handle. See NetEndpoint.hpp.
        NetEndpoint     endpoint{};

        // Player and Session details
        PlayerID        playerID{ 0 };
        SessionID       sessionID{ 0 };

        // Sequence tracking (Check for potential dropped packets)
        SequenceNum     nextSendSequence{ 1 };
        SequenceNum     expectedReceiveSequence{ 1 };

        std::unordered_map<SequenceNum, Packet> pendingPackets;
        std::unordered_map<SequenceNum, bool>   receivedPackets;

        std::chrono::steady_clock::time_point   lastHeartbeat{ std::chrono::steady_clock::now() };
        std::chrono::steady_clock::time_point   lastActivity{ std::chrono::steady_clock::now() };
        bool                                    isConnected{ false };
        bool                                    timeout{ false };

        // Adaptive RTO (Kurose: EstimatedRTT + 4 * DevRTT)
        double estimatedRTT{ 0.0 };
        double devRTT{ 0.0 };
        double rto{ static_cast<double>(PACKET_TIMEOUT_MS) };

        // Congestion control (slow start + AIMD)
        uint32_t cwnd{ 10 };
        uint32_t ssthresh{ INITIAL_SSTHRESH };
        uint32_t cwndAckCount{ 0 };

        std::unordered_map<uint32_t, std::vector<std::vector<uint8_t>>> fragmentBuffer;

        // RPC rate limiting (token bucket).
        // RPC dispatch runs on the main thread inside GameServer::Update, so an
        // authenticated peer flooding well-formed RPCs is a direct frame-rate
        // attack. Tokens refill at RPC_TOKEN_REFILL_PER_SEC up to RPC_TOKEN_BURST;
        // each dispatched RPC spends one. An empty bucket drops the message and
        // counts a violation; enough violations drop the connection.
        double   rpcTokens{ static_cast<double>(RPC_TOKEN_BURST) };
        uint32_t rpcViolations{ 0 };
        std::chrono::steady_clock::time_point lastRPCRefill{ std::chrono::steady_clock::now() };
    };
}