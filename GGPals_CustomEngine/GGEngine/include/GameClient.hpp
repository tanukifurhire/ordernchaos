/*************************************************************************
  \file       GameClient.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief      Declares the GameClient class for managing client-side 
              networking in GGEngine.

  \details
    The GameClient handles the client lifecycle for connecting to a game
    server over UDP with a reliability layer. It provides functionality
    for:
      - Establishing and disconnecting from the server.
      - Submitting turn actions and other game commands.
      - Receiving and processing server messages asynchronously.
      - Maintaining client-server time synchronization.
      - Exposing GGEvent callbacks to integrate networked game logic
        without tight coupling to the application layer.

    The GameClient relies on the ReliabilityLayer for guaranteed delivery
    on top of the transport, which is non-blocking so it integrates with
    the editor or game loop without stalling a frame.

    Singleton access is provided via GetGameClient().

  \dependencies
    - UDPReliabilityLayer.hpp
    - GGEvent.hpp
    - INetTransport (Winsock or Steam backend, chosen at runtime)
    - NetworkMessage, PlayerID, SessionID types
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <UDPReliabilityLayer.hpp>

#include <GGEvent.hpp>

namespace GGNetworking
{
    /*************************************************************************
      \class GameClient
      \brief
        Client-side network manager with event-driven message handling.
      \details
        Manages client lifecycle: server connection, turn action submission,
        game state synchronization. Exposes GGEvent callbacks for game logic
        integration without tight coupling. Handles connection handshake,
        message deserialization, and event dispatch to application layer.
      \see
        SystemManagerInterface, ReliabilityLayer, GameServer, GGEvent
    *************************************************************************/
    class GameClient
    {
    public:
        GameClient();
        ~GameClient();

        bool Init();
        /*************************************************************************
          \brief
            Establishes connection to game server.
          \param[in] serverAddr
            Server hostname or IP (default "eisenfaust.ddns.net").
          \param[in] port
            Server port (default DEFAULT_SERVER_PORT=9081).
          \return
            true if CONNECT packet sent successfully.
          \details
            Resolves serverAddr via getaddrinfo, instantiates ReliabilityLayer,
            sends CONNECT packet with CLIENT_CONNECT_REQUEST message. Waits for
            SERVER_CONNECT_RESPONSE in Update() to complete handshake. Connection
            is non-blocking; check IsConnected() to verify completion.
          \note
            Returns immediately after sending CONNECT. Server response processed
            asynchronously in Update().
        *************************************************************************/
        bool ConnectToServer(std::string const& serverAddr = "eisenfaust.ddns.net", uint16_t port = DEFAULT_SERVER_PORT);

        /*************************************************************************
          \brief
            Connects to a host over Steam P2P.
          \param[in] hostSteamID
            Host's SteamID64 as decimal text, normally read from the lobby's
            host_steamid metadata.
          \return
            true if the connection attempt started.
          \details
            The Steam counterpart of ConnectToServer. There is no hostname to
            resolve and no port to reach: the SteamID plus Valve's relay network
            is the whole address. Everything above the transport is identical -
            the same CLIENT_CONNECT_REQUEST handshake runs, the same PlayerID
            is assigned, the same ClientRPC_OnConnectedToServer fires.
          \warning
            Takes the ID as a STRING. A SteamID64 exceeds the 2^53 a LuaJIT
            number holds exactly, and the corruption is silent.
        *************************************************************************/
        bool ConnectToSteamHost(std::string const& hostSteamID);

        void StopClient();
        void DisconnectFromServer();
        
        void Update();
        inline bool IsConnected() const { return isConnected; }
        inline bool IsRunning() const { return isRunning; }

        /*************************************************************************
          \brief
            Which transport backend this client session is running on.
          \return
            The active kind, or the preferred kind when no session is running.
        *************************************************************************/
        TransportKind GetTransportKind() const;

        /*************************************************************************
          \brief
            Reliability statistics for the running session.
          \return
            Zeroed stats when no session is running.
        *************************************************************************/
        ReliabilityLayer::ReliabilityStats GetStats() const;

        /*************************************************************************
          \brief
            Submits turn action to server.
          \param[in] action
            NetworkMessage containing serialized game command.
          \details
            Wraps action in CLIENT_TURN_ACTION message type, sends via SendToServer.
            Server validates and responds with SERVER_TURN_RESULT.
          \note
            Does not block. Response handled asynchronously via onTurnResult event.
        *************************************************************************/
        void SendTurnAction(NetworkMessage const& action);

        /*************************************************************************
          \brief
            Transmits message to server.
          \param[in] message
            NetworkMessage to send.
          \details
            Serializes message to DATA packet, calls reliabilityLayer->SendPacket
            targeting server. Internal helper for SendTurnAction and other
            client-initiated messages.
        *************************************************************************/
        void SendToServer(NetworkMessage const& message);

        /*************************************************************************
          \brief
            Retrieves assigned PlayerID from server handshake.
          \return
            PlayerID assigned during connection, 0 if not connected.
          \details
            Set when SERVER_CONNECT_RESPONSE received. Used for identifying self
            in game state updates.
        *************************************************************************/
        PlayerID GetMyPlayerID() const { return playerID; }

        /*************************************************************************
          \brief
            Gets current Game Client's time since Init
          \return
            Time from GameClient::Init() called. In microseconds
        *************************************************************************/
        inline int64_t GetGameClientTime() const
        {
            return std::chrono::duration_cast<std::chrono::microseconds>
                (std::chrono::steady_clock::now() - startTime).count();
        }

        /*************************************************************************
          \brief
            Gets Game Server's time since Init
          \return
            Time from GameServer::Init() called. In microseconds
        *************************************************************************/
        inline int64_t GetGameServerTime() const
        {
            return GetGameClientTime() + clientServerOffset;
        }

    private:
        /*************************************************************************
          \brief
            Backend-independent half of connecting.
          \param[in] transport
            Transport that has already opened a connection to the host. Taken
            by value because this method takes ownership of it either way.
          \return
            true when the reliability layer started and the connect request
            was sent.
          \details
            Registers the server endpoint, starts the I/O thread and sends
            CLIENT_CONNECT_REQUEST. Both ConnectToServer and ConnectToSteamHost
            funnel through here so the two paths cannot drift apart.
        *************************************************************************/
        bool BeginSession(std::unique_ptr<INetTransport> transport);

        static constexpr PlayerID SERVER_CONNECTION_ID = 1;

        bool isConnected{ false };
        bool isRunning{ false };
        SessionID sessionID{};
        PlayerID playerID{};

        Callback<> onStopPlayModeCallback;

        std::unique_ptr<ReliabilityLayer> reliabilityLayer;

        // The server as the transport identifies it: an address on the Winsock
        // path, a connection handle on the Steam one.
        NetEndpoint serverEndpoint{};

        int64_t clientServerOffset{};
        std::chrono::steady_clock::time_point startTime{};
        std::chrono::steady_clock::time_point lastPingTime{};
        int64_t lastPingSentTime{}; // Client time when last ping was sent
        double roundTripTime{ 0.0 }; // in milliseconds
    };

    inline auto& GetGameClient()
    {
        static std::unique_ptr<GameClient> gameClient = std::make_unique<GameClient>();
        return gameClient;
    }
}
