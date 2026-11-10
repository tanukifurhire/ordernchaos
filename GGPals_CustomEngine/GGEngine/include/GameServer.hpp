/*************************************************************************
  \file       GameServer.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief      Declaration of GameServer class for server-side networking.


  \details
    Provides server-side networking support with UDP reliability layer.
    Handles client connections, turn actions, RPC calls, message broadcasting,
    and heartbeat-based client session management. Integrates with GGEngine
    event bus and SystemManagerInterface for updates.

  \dependencies
    - UDPReliabilityLayer.hpp
    - GGEvent.hpp

  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <UDPReliabilityLayer.hpp>

#include <GGEvent.hpp>

#include <memory>

namespace GGNetworking
{
    /*************************************************************************
      \class GameServer
      \brief
        Server-side network manager integrating with ECS via SystemManagerInterface.
      \details
        Manages game server lifecycle: socket binding, client connections,
        message broadcasting, reliability layer coordination. Integrates with
        GGEngine systems via SystemManagerInterface for Update() callbacks.
        Handles connection handshakes, player management, and turn-based game flow.
      \see
        SystemManagerInterface, ReliabilityLayer, GameClient
    *************************************************************************/
    class GameServer
    {
    public:
        GameServer(uint16_t port = DEFAULT_SERVER_PORT);
        ~GameServer();

        bool StartServer(uint16_t port = DEFAULT_SERVER_PORT);
        void StopServer();

        bool Init();
        void Update();

        inline bool IsRunning() const { return isRunning; }

        inline void SetMaxPlayers(uint32_t max) { maxPlayers = max; }

        inline int64_t GetServerTime() const
        {
            return std::chrono::duration_cast<std::chrono::microseconds>
                (std::chrono::steady_clock::now() - startTime).count();
        }

        /*************************************************************************
          \brief
            Broadcasts message to all connected clients except one.
          \param[in] message
            NetworkMessage to send.
          \param[in] excludePlayer
            PlayerID to skip (0 = send to all).
          \details
            Iterates connections, serializes message to DATA packet, calls
            reliabilityLayer->SendPacket for each target. Used for game state
            updates, player notifications, and turn results.
          \note
            Thread-safe via reliabilityLayer's internal locking.
        *************************************************************************/
        void BroadcastMessage(NetworkMessage const& message, PlayerID excludePlayer = 0);

        /*************************************************************************
          \brief
            Sends message to a specific connected client.
          \param[in] message
            NetworkMessage to send.
          \param[in] player
            PlayerID of the target client.
          \details
            Looks up the connection by PlayerID, serializes message to DATA packet,
            calls sendto for the target. Logs error if PlayerID not found.
          \note
            Thread-safe via reliabilityLayer's internal locking.
        *************************************************************************/
        void SendToClient(NetworkMessage const& message, PlayerID player);

        /*************************************************************************
          \brief
            Which transport backend this server session is running on.
          \return
            The active kind, or the preferred kind when no session is running.
        *************************************************************************/
        TransportKind GetTransportKind() const;

        /*************************************************************************
          \brief
            Reliability statistics for the running session.
          \return
            Zeroed stats when no session is running.
          \details
            Exposed so the debug panel can show these beside Steam's own
            per-connection numbers. Seeing both makes it obvious which layer
            owns a problem.
        *************************************************************************/
        ReliabilityLayer::ReliabilityStats GetStats() const;

    private:
        /*************************************************************************
          \brief
            Spends one RPC token from a connection's rate-limit bucket.
          \param[in] player
            PlayerID whose bucket to charge.
          \param[out] outOverBudget
            Set to true when this connection has now exceeded
            RPC_MAX_VIOLATIONS and should be disconnected.
          \return
            true if a token was available and the RPC may proceed; false if the
            call must be dropped.
          \details
            Lazily refills the bucket based on elapsed time since the last
            charge, so no background timer is needed. Tokens accumulate to at
            most RPC_TOKEN_BURST.
          \note
            Thread-safe via connectionsMutex. Called from Update() while
            messagesMutex is already held - this preserves the existing
            messagesMutex-then-connectionsMutex lock order used elsewhere in
            Update(), so do not invert it.
        *************************************************************************/
        bool ConsumeRPCToken(PlayerID player, bool& outOverBudget);

        // The socket is gone from this layer entirely: the transport owns it
        // now, and on the Steam backend there is no socket to own. See
        // INetTransport.
        bool isRunning{ false };
        uint16_t port;
        uint32_t maxPlayers{ MAX_PLAYERS };
        SessionID currentSessionID{ 1 };

        std::unique_ptr<ReliabilityLayer> reliabilityLayer;

        std::chrono::steady_clock::time_point startTime{};

        Callback<> onStopPlayModeCallback;

        struct GameSession
        {

        };

    };

    inline auto& GetGameServer()
    {
        static std::unique_ptr<GameServer> gameServer = std::make_unique<GameServer>();
        return gameServer;
    }
}