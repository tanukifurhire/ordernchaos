/*************************************************************************
  \file       GameClient.cpp
  \author	  Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Implements the GameClient responsible for managing the client-side
	networking for GGEngine.
  \details
	The GameClient provides the interface for establishing and maintaining
	a UDP connection to the game server. It handles:
	  - Socket creation, binding, and non-blocking configuration.
	  - Server connection requests and disconnect messages.
	  - Integration with the ReliabilityLayer to ensure ordered and
		acknowledged packet delivery on top of UDP.
	  - Serializing and sending NetworkMessage packets to the server.
	  - Receiving and processing incoming messages routed from the
		ReliabilityLayer, including RPC calls and turn-based actions.
	  - Automatic shutdown when the editor stops play mode via
		GlobalEventBus::onStopPlayMode.

	The connection handshake is asynchronous: ConnectToServer() sends a
	CLIENT_CONNECT_REQUEST and finalizes the connection only upon receiving
	SERVER_CONNECT_RESPONSE during Update().

  \dependencies
	- GameClient.hpp
	- RPCMessage.hpp
	- GGLogger.hpp
	- GGEngineEventBus.hpp
	- WinSock2 (socket, bind, sendto, ioctlsocket)
	- ReliabilityLayer (custom reliable UDP wrapper)

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GameClient.hpp>

#include <RPCMessage.hpp>
#include <SteamPlatform.hpp>
#include <GGLogger.hpp>

#include <GGEngine.hpp>

#include <iostream>
#include <cstring>

#include <GGEngineEventBus.hpp>

namespace GGNetworking
{
	GameClient::GameClient() {
		onStopPlayModeCallback =
			[&]()
			{
				StopClient();
			};

		GlobalEventBus::onStopPlayMode += onStopPlayModeCallback;
	}

	GameClient::~GameClient()
	{
		StopClient();

		GlobalEventBus::onStopPlayMode -= onStopPlayModeCallback;
	}

	bool GameClient::Init()
	{
		// Tear down anything left from a previous session. Socket creation,
		// binding and non-blocking setup all moved into the transport, which
		// ConnectToServer / ConnectToSteamHost create once they know which
		// backend and which peer - so Init is purely a reset now.
		if (reliabilityLayer)
		{
			reliabilityLayer->Stop();
			reliabilityLayer.reset();
		}

		serverEndpoint = NetEndpoint{};

		GGLogger::Info("GameClient initialized");

		return true;
	}

	/*************************************************************************
	  \brief
		Brings up the transport and runs the shared connect handshake.
	  \param[in] transport
		Transport with OpenListen/OpenConnect already done.
	  \return
		true when the reliability layer started and CLIENT_CONNECT_REQUEST went
		out.
	  \details
		The half of connecting that does not care which backend is underneath -
		which is all of it above the transport. Both ConnectToServer and
		ConnectToSteamHost end here, so the two paths cannot drift apart.
	*************************************************************************/
	bool GameClient::BeginSession(std::unique_ptr<INetTransport> transport)
	{
		// The endpoint the transport actually ended up with. On Winsock that is
		// the address that was passed in; on Steam it is the connection handle
		// the SDK assigned, which did not exist until ConnectP2P ran.
		serverEndpoint = transport->GetRemoteEndpoint();

		if (!serverEndpoint.IsValid())
		{
			GGLogger::Error("Transport did not report a server endpoint");
			return false;
		}

		reliabilityLayer = std::make_unique<ReliabilityLayer>(std::move(transport));

		if (!reliabilityLayer->Start())
		{
			GGLogger::Error("Failed to start reliability layer");
			reliabilityLayer.reset();
			return false;
		}

		if (!reliabilityLayer->AddConnection(SERVER_CONNECTION_ID, serverEndpoint, 0))
		{
			GGLogger::Error("Failed to register server endpoint in reliability layer");
			reliabilityLayer->Stop();
			reliabilityLayer.reset();
			return false;
		}

		startTime = std::chrono::steady_clock::now();

		NetworkMessage message;
		message.SetMessageType(MessageType::CLIENT_CONNECT_REQUEST);

		NetworkConnectionData instance{};
		instance.timeSent = GetGameClientTime();

		message.WriteValue<NetworkConnectionData>(instance);

		SendToServer(message);

		isRunning = true;
		isConnected = false;

		return true;
	}

	TransportKind GameClient::GetTransportKind() const
	{
		if (reliabilityLayer) return reliabilityLayer->GetTransportKind();
		return GetPreferredTransport();
	}

	ReliabilityLayer::ReliabilityStats GameClient::GetStats() const
	{
		if (!reliabilityLayer) return ReliabilityLayer::ReliabilityStats{};
		return reliabilityLayer->GetStats();
	}

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
	bool GameClient::ConnectToServer(std::string const& hostAddr, uint16_t port)
	{
		if (isRunning)
		{
			GGLogger::Warning("Client already running!");
			return false;
		}

		// Name resolution stays here rather than in the transport: DNS is an
		// application concern, and the Steam path has no hostname at all.
		addrinfo hints{}, * res = nullptr;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		if (getaddrinfo(hostAddr.c_str(), NULL, &hints, &res) != 0) {
			std::cerr << "Failed to resolve hostname" << std::endl;
			return false;
		}

		sockaddr_in resolvedAddr{};
		{
			sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(res->ai_addr);
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);

			std::cout << "Resolved IP Address for " << hostAddr << ": " << ip_str << std::endl;

			resolvedAddr.sin_family = AF_INET;
			resolvedAddr.sin_port = htons(port);
			resolvedAddr.sin_addr = addr_in->sin_addr;
		}

		// getaddrinfo allocates; the old code leaked this on every connect.
		freeaddrinfo(res);

		NetEndpoint const target = NetEndpoint::FromSockaddr(resolvedAddr);

		// Always Winsock, whatever the preferred transport is: being handed a
		// hostname and a port IS the request for the IP path. The Steam route
		// is ConnectToSteamHost.
		std::unique_ptr<INetTransport> transport = CreateTransport(TransportKind::Winsock);
		if (!transport || !transport->OpenConnect(target))
		{
			GGLogger::Error("Failed to open Winsock transport to " + hostAddr
				+ (transport ? std::string(": ") + transport->GetLastErrorText() : std::string()));
			return false;
		}

		return BeginSession(std::move(transport));
	}

	bool GameClient::ConnectToSteamHost(std::string const& hostSteamID)
	{
		if (isRunning)
		{
			GGLogger::Warning("Client already running!");
			return false;
		}

		uint64_t const steamID = SteamPlatform::ParseSteamID(hostSteamID);
		if (steamID == 0)
		{
			GGLogger::Error("ConnectToSteamHost: '" + hostSteamID + "' is not a valid SteamID64");
			return false;
		}

		std::unique_ptr<INetTransport> transport = CreateTransport(TransportKind::Steam);
		if (!transport)
		{
			GGLogger::Error("ConnectToSteamHost: Steam transport unavailable in this build");
			return false;
		}

		if (!transport->OpenConnect(NetEndpoint::FromSteam(0, steamID)))
		{
			GGLogger::Error("ConnectToSteamHost: " + std::string(transport->GetLastErrorText()));
			return false;
		}

		GGLogger::Info("Connecting to Steam host " + hostSteamID + "...");

		return BeginSession(std::move(transport));
	}

	void GameClient::DisconnectFromServer()
	{
		// Send disconnect if we have a valid player ID and socket.
		// Check playerID (not just isConnected) to handle race conditions
		// where isConnected was cleared before StopClient runs, while
		// avoiding sending from an unconnected client (playerID == 0).
		if (playerID != 0 && reliabilityLayer && serverEndpoint.IsValid())
		{
			GGLogger::Info("DisconnectFromServer: Sending disconnect for player " +
				std::to_string(playerID) + " (isConnected=" +
				std::string(isConnected ? "true" : "false") + ")");

			NetworkMessage message;
			message.SetMessageType(MessageType::CLIENT_DISCONNECT_REQUEST);

			NetworkConnectionData instance{};
			instance.timeSent = GetGameClientTime();

			message.WriteValue<NetworkConnectionData>(instance);

			SendToServer(message);
		}
		else
		{
			GGLogger::Warning("DisconnectFromServer: Cannot send (playerID=" +
				std::to_string(playerID) + " endpoint=" +
				serverEndpoint.ToString() + ")");
		}

		isConnected = false;
	}

	void GameClient::StopClient()
	{
		GGLogger::Info("StopClient called: isRunning=" + std::string(isRunning ? "true" : "false") +
			" isConnected=" + std::string(isConnected ? "true" : "false") +
			" endpoint=" + serverEndpoint.ToString() +
			" hasReliability=" + std::string(reliabilityLayer ? "true" : "false"));

		// Allow cleanup even if not running (e.g. Init succeeded but ConnectToServer failed)
		if (!isRunning && !reliabilityLayer) return;

		isRunning = false;

		if (reliabilityLayer)
		{
			DisconnectFromServer();
			reliabilityLayer->Stop();       // Joins the I/O thread, closes the transport
			reliabilityLayer.reset();
		}

		// Reset all connection state so the client is clean for the next session
		isConnected = false;
		playerID = 0;
		sessionID = {};
		serverEndpoint = {};
		clientServerOffset = 0;
		startTime = {};
		lastPingTime = {};
		lastPingSentTime = 0;
		roundTripTime = 0.0;

		GGLogger::Info("GameClient stopped");
	}

	void GameClient::Update() {
		// Update can be reached after StopClient tore the layer down (the
		// engine ticks on IsRunning, which the disconnect path clears a moment
		// earlier), so re-check rather than assume.
		if (!reliabilityLayer) return;

		// Did the transport tell us the host is gone? On the Steam backend a
		// host that quits or crashes shows up here within a frame, which is the
		// difference between "returned to the menu" and "stared at a frozen
		// board until something timed out". The Winsock backend never reports
		// anything, so this is inert there.
		{
			std::vector<PlayerID> droppedPeers;
			if (reliabilityLayer->DrainTransportDisconnects(droppedPeers) > 0)
			{
				for (PlayerID id : droppedPeers)
				{
					if (id != SERVER_CONNECTION_ID) continue;

					GGLogger::Warning("Lost the connection to the host");

					isConnected = false;
					playerID = 0;

					GetEngineInstance().CallLuaFunctionAdvanced("ClientRPC_OnDisconnectedFromServer");
					StopClient();
					return;   // reliabilityLayer is gone; do not touch it again
				}
			}
		}

		// Send periodic ping to keep connection alive and measure RTT

		if (isConnected)
		{
			auto now = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPingTime);

			if (elapsed.count() >= CLIENT_PING_INTERVAL_MS)
			{
				NetworkMessage pingMessage;
				pingMessage.SetMessageType(MessageType::CLIENT_PING);

				NetworkPing pingData{};
				lastPingSentTime = GetGameClientTime();
				pingData.clientTimeSent = lastPingSentTime;

				pingMessage.WriteValue<NetworkPing>(pingData);

				//GGLogger::Info("Sending Ping message to server!");
				SendToServer(pingMessage);

				lastPingTime = now;
			}
		}

		std::unique_lock lock(reliabilityLayer->messagesMutex);
		std::swap(reliabilityLayer->incomingMessages, reliabilityLayer->coldMessages);
		lock.unlock();

		// Process ALL messages
		while (!reliabilityLayer->coldMessages.empty())
		{
			auto const& messageIDPair = reliabilityLayer->coldMessages.front();

			switch (messageIDPair.first.GetMessageType())
			{
			case MessageType::SERVER_CONNECT_RESPONSE:
			{
				NetworkConnectionDataReturn responseData{};
				responseData = messageIDPair.first.ReadValue<NetworkConnectionDataReturn>();

				playerID = responseData.playerID;
				isConnected = true;

				// Calculate client-server time offset
				int64_t currentClientTime = GetGameClientTime();
				clientServerOffset = responseData.serverTimeReceived - currentClientTime;

				GGLogger::Info("Connected to server! Assigned PlayerID: " + std::to_string(playerID));

				GetEngineInstance().CallLuaFunctionAdvanced("ClientRPC_OnConnectedToServer", playerID);

				// Send immediate ping to establish heartbeat
				NetworkMessage pingMessage;
				pingMessage.SetMessageType(MessageType::CLIENT_PING);

				NetworkPing pingData{};
				lastPingSentTime = GetGameClientTime();
				pingData.clientTimeSent = lastPingSentTime;

				pingMessage.WriteValue<NetworkPing>(pingData);

				SendToServer(pingMessage);

				// Initialize ping timer
				lastPingTime = std::chrono::steady_clock::now();
				break;
			}
			case MessageType::CLIENT_DISCONNECT_RESPONSE:
			{
				isConnected = false;
				playerID = 0;
				GGLogger::Info("Disconnected from server.");
				GetEngineInstance().CallLuaFunctionAdvanced("ClientRPC_OnDisconnectedFromServer");
				StopClient();
				return; // reliabilityLayer destroyed by StopClient, must exit immediately
			}
			case MessageType::SERVER_PONG:
			{
				NetworkPingReturn pongData{};
				pongData = messageIDPair.first.ReadValue<NetworkPingReturn>();

				// Calculate RTT: current client time - time when we sent the ping
				int64_t currentTime = GetGameClientTime();
				roundTripTime = static_cast<double>(currentTime - lastPingSentTime) / 1000.0; // Convert microseconds to milliseconds

				// Update client-server offset using server timestamp
				// Server timestamp is when server received our ping, adjust by half RTT
				int64_t halfRTT = static_cast<int64_t>(roundTripTime * 500.0); // half RTT in microseconds
				clientServerOffset = pongData.serverTimeReceived - lastPingSentTime - halfRTT;

				//std::cout << "Ping RTT: " << roundTripTime << " ms, Offset: " << clientServerOffset << " us" << std::endl;
				break;
			}
			case MessageType::CLIENT_TURN_ACTION:
			{
				std::cout << "Process Client Turn Action" << std::endl;
				break;
			}
			case MessageType::RPC_CALL:
			{
				// Client side only ever accepts client-bound handlers, so a
				// peer cannot drive ServerRPC_* logic here. There is no
				// client-asserted identity to reconcile on this direction.
				bool success = GetRPCDeserializer()->Deserialize(
					messageIDPair.first, SERVER_CONNECTION_ID, RPCDirection::ClientBound);

				if (!success)
				{
					GGLogger::Warning("RPC dispatch rejected from server");
				}
				break;
			}
			default:
				GGLogger::Warning(
					"Unknown message type " + std::to_string(
						static_cast<int>(messageIDPair.first.GetMessageType())
					)
				);
				break;
			}

			reliabilityLayer->coldMessages.pop();
		}
	}

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
	void GameClient::SendTurnAction(NetworkMessage const& action)
	{
		SendToServer(action);
	}

	/*************************************************************************
	  \brief
		Transmits message to server.
	  \param[in] message
		NetworkMessage to send.
	  \details
		Serializes message to DATA packet, calls sendto directly to server.
		Internal helper for SendTurnAction and other client-initiated messages.
	*************************************************************************/
	void GameClient::SendToServer(NetworkMessage const& message)
	{
		if (!reliabilityLayer) return;

		auto const serializeMessage = message.Serialize();
		size_t totalWireSize = PacketHeader::HEADER_SIZE + serializeMessage.size();

		if (totalWireSize <= MAX_PACKET_SIZE)
		{
			Packet packet{};
			packet.payload.insert(packet.payload.end(), serializeMessage.begin(), serializeMessage.end());
			packet.header.payloadSize = static_cast<uint16_t>(packet.payload.size());
			packet.header.packetType = PacketType::DATA;
			if (!reliabilityLayer->SendPacket(SERVER_CONNECTION_ID, packet))
			{
				GGLogger::Warning("GameClient::SendToServer failed: no server connection in reliability layer");
			}
		}
		else
		{
			// Large message: fragment
			uint16_t fragmentTotal = static_cast<uint16_t>(
				(serializeMessage.size() + MAX_FRAGMENT_PAYLOAD - 1) / MAX_FRAGMENT_PAYLOAD);

			std::cout << "[FRAG] Client sending " << fragmentTotal << " fragments ("
				<< serializeMessage.size() << " bytes)" << std::endl;

			for (uint16_t i = 0; i < fragmentTotal; ++i)
			{
				size_t offset = static_cast<size_t>(i) * MAX_FRAGMENT_PAYLOAD;
				size_t chunkSize = (std::min)(MAX_FRAGMENT_PAYLOAD, serializeMessage.size() - offset);

				FragmentHeader fragHeader{};
				fragHeader.messageID = message.GetMessageID();
				fragHeader.fragmentIndex = i;
				fragHeader.fragmentTotal = fragmentTotal;

				Packet packet{};
				uint8_t const* fragHeaderBytes = reinterpret_cast<uint8_t const*>(&fragHeader);
				packet.payload.insert(packet.payload.end(),
					fragHeaderBytes, fragHeaderBytes + FragmentHeader::HEADER_SIZE);
				packet.payload.insert(packet.payload.end(),
					serializeMessage.begin() + offset,
					serializeMessage.begin() + offset + chunkSize);

				packet.header = { 0, static_cast<uint16_t>(packet.payload.size()), 0, PacketType::FRAGMENT };
				packet.CalculateChecksum();

				auto serializedPacket = packet.Serialize();

				if (!reliabilityLayer->SendUnreliableToEndpoint(serverEndpoint,
					serializedPacket.data(), serializedPacket.size()))
				{
					std::cerr << "[FRAG] transport rejected fragment " << i << std::endl;
					return;
				}
			}
		}
	}
}
