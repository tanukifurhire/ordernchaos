/*************************************************************************
  \file       UDPReliabliltyLayer.hpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
    UDP reliability layer with threading and connection management
  \details
    Declares ReliabilityLayer class providing TCP-like reliability over UDP.
    Manages dedicated I/O thread for asynchronous packet reception, thread-safe
    connection tracking via std::unordered_map with shared_mutex, player ID
    allocation from reusable queue, and incoming/outgoing message queues for
    application-layer consumption.

    Provides statistics tracking (ReliabilityStats) for network performance
    monitoring: packets sent/received/lost/retransmitted, RTT measurements.

    Academic implementation demonstrating reliability protocol concepts
    (sequencing, retransmission, acknowledgments, timeouts). Production
    systems should use established libraries (ENet, RakNet, QUIC) for
    battle-tested implementations with advanced features (congestion control,
    NAT traversal, encryption).
  \dependencies
    INetTransport.hpp, PlatformWindows.hpp, UDP.hpp, UDPTypes.hpp,
    UDPMessage.hpp, <memory>, <shared_mutex>, <queue>, <thread>, <atomic>,
    <unordered_map>
  \see
    ReliabilityLayer, ClientConnection, Packet, UDPReliabilityLayer.cpp
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <INetTransport.hpp>
#include <PlatformWindows.hpp>
#include <UDP.hpp>
#include <UDPTypes.hpp>
#include <UDPMessage.hpp>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <vector>

namespace GGNetworking
{
    /*************************************************************************
      \class ReliabilityLayer
      \brief
        Transport-layer reliability implementation for UDP with sequencing and retransmission.
      \details
        Manages packet sequencing, acknowledgment, retransmission, and connection
        tracking for all clients. Runs dedicated I/O thread for asynchronous
        socket operations. Provides thread-safe queues for application-layer
        message passing. Academic implementation demonstrating reliability concepts;
        production systems should use established protocols (QUIC, ENet, RakNet).
      \see
        Packet, ClientConnection, GameServer, GameClient
    *************************************************************************/
    class ReliabilityLayer
    {
    public:
        /*************************************************************************
          \brief
            Constructs the reliability layer over an already-opened transport.
          \param[in] netTransport
            Transport this layer owns and drives. Must already have had
            OpenListen or OpenConnect called on it successfully.
          \details
            Took a raw SOCKET before, which welded the whole layer to Winsock
            and left it closing a handle it did not own. It now owns the
            transport outright and closes it in Stop().
        *************************************************************************/
        explicit ReliabilityLayer(std::unique_ptr<INetTransport> netTransport);
        ~ReliabilityLayer();

        /*************************************************************************
          \brief
            Adds a second (or third) transport this layer serves peers over.
          \param[in] netTransport
            An already-opened transport. Takes ownership.
          \return
            false when a transport of that kind is already registered.
          \details
            What makes a listen server possible over Steam. Steam P2P cannot
            connect to its own SteamID, so a host that is also a player cannot
            reach its own server through the Steam transport. Opening a Winsock
            listener alongside the Steam one lets the host's own client half
            arrive over 127.0.0.1 while remote players arrive over the relay.
            The loopback socket never leaves the machine, so this costs no
            exposure and no port forwarding.

            One transport per NetEndpoint::Kind, because that is what routing
            keys on: a peer's endpoint carries its kind, and sends go back out
            through the transport that kind names.
          \note
            Call before Start(). The I/O thread reads the transport list without
            a lock, on the assumption that it is fixed once running.
        *************************************************************************/
        bool AddTransport(std::unique_ptr<INetTransport> netTransport);

        /*************************************************************************
          \brief
            Registers new client connection with assigned identifiers.
          \param[in] id
            Unique PlayerID assigned by server.
          \param[in] endpoint
            Peer address for packet routing (IPv4 or Steam).
          \param[in] sessionID
            Session token for validation.
          \return
            true if connection added, false if PlayerID already exists.
          \details
            Creates ClientConnection instance in connections map. Initializes
            sequence numbers to 1, activity timestamps to current time.
          \note
            Thread-safe via connectionsMutex.
        *************************************************************************/
        bool AddConnection(PlayerID id, NetEndpoint const& endpoint, SessionID sessionID);
        bool RemoveConnection(PlayerID id);

        ClientConnection* GetConnection(PlayerID id);

        struct ReliabilityStats
        {
            uint32_t packetsSent{};
            uint32_t packetsReceived{};
            uint32_t packetsRetransmitted{};
            uint32_t packetsLost{};
            uint32_t duplicatesReceived{};
            double   averageRoundTripTime{};
        };

        /*************************************************************************
          \brief
            Retrieves reliability statistics snapshot.
          \return
            Copy of current stats structure.
          \details
            Thread-safe read. Values may be stale immediately after return.
        *************************************************************************/
        inline ReliabilityStats GetStats() const { return stats; }
        /*************************************************************************
          \brief
            Resets all statistics counters to zero.
          \details
            Not thread-safe. Call only when I/O thread is stopped.
        *************************************************************************/
        void ResetStats() { stats = ReliabilityStats{}; }

        /*************************************************************************
          \brief
            Starts I/O thread for socket processing.
          \return
            true if thread started successfully.
          \details
            Spawns socketIOThread running ReceiveThreadProc. Sets shouldRun flag.
            Must be called before Send/Receive operations.
        *************************************************************************/
        bool Start();
        void Stop();
        mutable std::shared_mutex connectionsMutex;
        mutable std::shared_mutex messagesMutex;
        std::condition_variable_any messageCV;
        std::condition_variable_any ackCV;

        std::unordered_map<PlayerID, std::unique_ptr<ClientConnection>> connections;

        std::queue<std::pair<NetworkMessage, PlayerID>> incomingMessages;
        std::queue<std::pair<NetworkMessage, PlayerID>> coldMessages;

        /*************************************************************************
          \struct FragmentAssembly
          \brief
            Tracks in-progress reassembly of a fragmented message.
        *************************************************************************/
        struct FragmentAssembly
        {
            uint16_t fragmentTotal = 0;
            uint16_t receivedCount = 0;
            std::vector<std::vector<uint8_t>> fragments;
            std::chrono::steady_clock::time_point firstReceived;
        };

        std::unordered_map<uint32_t, FragmentAssembly> fragmentAssembly;

        /*************************************************************************
          \brief
            Transmits packet to specific client.
          \param[in] id
            Target PlayerID.
          \param[in] packet
            Packet to send.
          \return
            true if sendto succeeded.
          \details
            Looks up ClientConnection by id, serializes packet, calls sendto.
            Updates transmission metadata (timeSent, retransmissionCount).
          \note
            Thread-safe via connectionsMutex shared_lock.
        *************************************************************************/
        bool SendPacket(PlayerID id, Packet const& packet);

        /*************************************************************************
          \brief
            Sends packet unreliably (fire-and-forget) to specific client.
          \param[in] id
            Target PlayerID.
          \param[in] packet
            Packet to send (packetType will be set to UNRELIABLE_DATA).
          \return
            true if sendto succeeded.
          \details
            Sends immediately with no sequence number assignment, no ACK tracking,
            and no retransmission. Suitable for real-time data (input, snapshots)
            where latest data supersedes older data.
        *************************************************************************/
        bool SendPacketUnreliable(PlayerID id, Packet const& packet);

        /*************************************************************************
          \brief
            Sends raw bytes unreliably to a specific address (bypasses connection map).
          \param[in] endpoint
            Target peer.
          \param[in] data
            Raw serialized packet bytes.
          \param[in] size
            Byte count.
          \return
            true if the transport accepted the datagram.
          \details
            Used for FRAGMENT packets, and for sending to a peer that has no
            entry in the connection map yet.
        *************************************************************************/
        bool SendUnreliableToEndpoint(NetEndpoint const& endpoint, uint8_t const* data, size_t size);

        /*************************************************************************
          \brief
            Which backend is carrying this session's traffic.
        *************************************************************************/
        TransportKind GetTransportKind() const
        {
            // The most capable one, when several are open: a Steam listen
            // server also holds a Winsock loopback transport, and reporting
            // "Winsock" for it would be misleading in logs and the debug panel.
            for (auto const& candidate : transports)
            {
                if (candidate->GetKind() == TransportKind::Steam) return TransportKind::Steam;
            }

            return transports.empty() ? TransportKind::Winsock : transports.front()->GetKind();
        }

        /*************************************************************************
          \brief
            Non-owning view of the transport, for backend-specific diagnostics.
          \return
            nullptr once Stop() has run.
          \note
            Do not drive Send/Receive/Poll through this - the I/O thread owns
            those. Read-only queries such as Steam's per-connection status are
            what it is for.
        *************************************************************************/
        INetTransport* GetTransport(TransportKind kind) const;

        /*************************************************************************
          \brief
            How many transports this layer is serving peers over.
        *************************************************************************/
        size_t GetTransportCount() const { return transports.size(); }

        /*************************************************************************
          \brief
            Resolves a peer address to its PlayerID.
          \param[in] endpoint
            Peer address, typically from a received datagram.
          \return
            PlayerID, or 0 when the endpoint is unknown.
          \details
            O(1) hash lookup against endpointToPlayer. This was a linear scan
            of the whole connection map, run on every received packet.
          \note
            Thread-safe via connectionsMutex shared_lock.
        *************************************************************************/
        PlayerID FindPlayerByEndpoint(NetEndpoint const& endpoint) const;

        /*************************************************************************
          \brief
            Takes the players the transport reported as disconnected.
          \param[out] out
            PlayerIDs are appended.
          \return
            How many were appended.
          \details
            Filled on the I/O thread, drained on the main thread by
            GameServer::Update / GameClient::Update, which then remove the
            connection and notify Lua - the same two steps the heartbeat-timeout
            path performs, so scripts see one disconnect mechanism rather than
            two.

            Always empty on the Winsock backend, which has no way to know a
            peer is gone until the heartbeat lapses.
          \note
            Thread-safe.
        *************************************************************************/
        size_t DrainTransportDisconnects(std::vector<PlayerID>& out);

    private:
        /*************************************************************************
          \brief
            The transport that serves a given endpoint kind.
          \param[in] kind
            Endpoint kind to route for.
          \return
            nullptr when nothing is registered for that kind.
        *************************************************************************/
        INetTransport* TransportFor(NetEndpoint::Kind kind) const;

        // Usually one. Two on a Steam listen server: Winsock for the host's own
        // client over loopback, Steam for everyone else. Fixed once Start()
        // has spawned the I/O thread, which is why it needs no lock.
        std::vector<std::unique_ptr<INetTransport>> transports;

        // Rotates the transport ReceivePacket reads first, so a chatty one
        // cannot starve the other. I/O thread only.
        size_t nextReceiveTransport{ 0 };

        // Filled by ProcessTransportDisconnects on the I/O thread, drained by
        // the main thread. Its own mutex: taking connectionsMutex here would
        // put an I/O-thread lock inside a main-thread disconnect path for no
        // reason, and this list has nothing to do with the connection map.
        std::vector<PlayerID> transportDisconnects;
        mutable std::mutex    transportDisconnectsMutex;

        // Reverse index over `connections`, maintained by Add/RemoveConnection.
        // Guarded by connectionsMutex alongside the map it mirrors.
        std::unordered_map<NetEndpoint, PlayerID> endpointToPlayer;

        std::queue<PlayerID> availablePlayerIDs;

        std::atomic<bool> shouldRun{ false };
        std::atomic<bool> shouldStopAtomic{ false };

        std::thread socketIOThread;

        ReliabilityStats stats;
        /*************************************************************************
          \brief
            I/O thread main loop for packet processing.
          \details
            Continuously: receives packets via ReceivePacket, checks for timeouts,
            triggers retransmissions, sends heartbeats. Runs until shouldStopAtomic.
        *************************************************************************/
        void ReceiveThreadProc();
        /*************************************************************************
          \brief
            Receives and processes one incoming packet.
          \return
            true if packet received and processed, false on socket error.
          \details
            Calls recvfrom, deserializes packet, validates session, handles based
            on PacketType. Updates sequence tracking, sends ACKs, reassembles fragments.
        *************************************************************************/
        bool ReceivePacket();

        /*************************************************************************
          \brief
            Retires peers the transport itself reported as gone.
          \details
            Steam reports ClosedByPeer / ProblemDetectedLocally within a frame.
            Turning those into the same disconnect the heartbeat timeout would
            eventually produce is what lets a host notice an alt-F4 at once
            rather than ten seconds later. No-op on the Winsock backend, which
            has nothing equivalent to report.
        *************************************************************************/
        void ProcessTransportDisconnects();
    };
}
