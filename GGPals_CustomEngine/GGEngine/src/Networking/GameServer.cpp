/*************************************************************************
  \file		  GameServer.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief      Implements the GameServer class for server-side networking.

  \details
	Provides functionality for initializing and managing a UDP server with
	a reliability layer, processing client messages, broadcasting updates,
	handling RPC calls, managing player sessions and heartbeats.

  \dependencies
	- GameServer.hpp
	- GGEngineEventBus.hpp
	- UDPTypes.hpp
	- RPCMessage.hpp
	- GGLogger.hpp

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GameServer.hpp>

#include <GGEngineEventBus.hpp>
#include <GGEngine.hpp>

#include <UDPTypes.hpp>
#include <RPCMessage.hpp>
#include <GGLogger.hpp>

#include <algorithm>
#include <iostream>
#include <cstring>
#include <utility>
#include <vector>

namespace GGNetworking
{
	// Sends a serialized message as multiple FRAGMENT packets.
	//
	// Goes through the reliability layer's transport rather than a raw socket:
	// on the Steam backend there is no socket to send through, and the
	// destination is a connection handle rather than an address.
	static void SendFragmented(ReliabilityLayer& reliability, NetEndpoint const& endpoint,
		std::vector<uint8_t> const& serializedMessage, uint32_t messageID)
	{
		uint16_t fragmentTotal = static_cast<uint16_t>(
			(serializedMessage.size() + MAX_FRAGMENT_PAYLOAD - 1) / MAX_FRAGMENT_PAYLOAD);

		std::cout << "[FRAG-SEND] Fragmenting message ID=" << messageID
			<< " totalBytes=" << serializedMessage.size()
			<< " into " << fragmentTotal << " fragments"
			<< " (MAX_FRAGMENT_PAYLOAD=" << MAX_FRAGMENT_PAYLOAD << ")" << std::endl;

		for (uint16_t i = 0; i < fragmentTotal; ++i)
		{
			size_t offset = static_cast<size_t>(i) * MAX_FRAGMENT_PAYLOAD;
			size_t chunkSize = (std::min)(MAX_FRAGMENT_PAYLOAD, serializedMessage.size() - offset);

			FragmentHeader fragHeader{};
			fragHeader.messageID = messageID;
			fragHeader.fragmentIndex = i;
			fragHeader.fragmentTotal = fragmentTotal;

			Packet packet{};
			// Write fragment header
			uint8_t const* fragHeaderBytes = reinterpret_cast<uint8_t const*>(&fragHeader);
			packet.payload.insert(packet.payload.end(),
				fragHeaderBytes, fragHeaderBytes + FragmentHeader::HEADER_SIZE);
			// Write fragment data chunk
			packet.payload.insert(packet.payload.end(),
				serializedMessage.begin() + offset,
				serializedMessage.begin() + offset + chunkSize);

			packet.header = { 0, static_cast<uint16_t>(packet.payload.size()), 0, PacketType::FRAGMENT };
			packet.CalculateChecksum();

			auto serializedPacket = packet.Serialize();

			std::cout << "[FRAG-SEND] Fragment " << i << "/" << fragmentTotal
				<< " chunkSize=" << chunkSize
				<< " packetPayload=" << packet.payload.size()
				<< " wireSize=" << serializedPacket.size() << std::endl;

			if (!reliability.SendUnreliableToEndpoint(endpoint,
				serializedPacket.data(), serializedPacket.size()))
			{
				std::cerr << "[FRAG-SEND] transport rejected fragment " << i << std::endl;
				return;
			}

			std::cout << "[FRAG-SEND] Fragment " << i << " sent OK ("
				<< serializedPacket.size() << " bytes)" << std::endl;
		}
	}

	GameServer::GameServer(uint16_t portVal)
	{
		port = portVal;

		onStopPlayModeCallback =
			[&]()
			{
				StopServer();
			};

		GlobalEventBus::onStopPlayMode += onStopPlayModeCallback;
	}

	GameServer::~GameServer()
	{
		StopServer();

		GlobalEventBus::onStopPlayMode -= onStopPlayModeCallback;
	}

	bool GameServer::StartServer(uint16_t portVal)
	{
		if (isRunning)
		{
			GGLogger::Warning("Server already running!");
			return false;
		}

		// Socket creation, binding and non-blocking setup all moved into the
		// transport. Which transport that is comes from the runtime selection,
		// so hosting over Steam and hosting over LAN differ by exactly this
		// line and nothing else in the server.
		TransportKind const kind = GetPreferredTransport();

		std::unique_ptr<INetTransport> transport = CreateTransport(kind);
		if (!transport)
		{
			GGLogger::Error("Failed to create " + std::string(TransportKindName(kind)) + " transport");
			return false;
		}

		if (!transport->OpenListen(portVal))
		{
			GGLogger::Error("Failed to open " + std::string(TransportKindName(kind))
				+ " listen transport: " + transport->GetLastErrorText());
			return false;
		}

		reliabilityLayer = std::make_unique<ReliabilityLayer>(std::move(transport));

		// Listen server over Steam: the host is a player, so its own client
		// half has to reach its own server half - and Steam P2P cannot connect
		// to its own SteamID, so it cannot come in over the Steam transport.
		//
		// A second Winsock listener solves it. The host's client connects to
		// 127.0.0.1 while remote players arrive over the relay, and the two
		// kinds of peer coexist because a NetEndpoint carries which transport
		// it belongs to. The loopback socket is bound on INADDR_ANY like any
		// other, but nothing outside needs to reach it and nothing is
		// forwarded, so it costs no exposure.
		if (kind == TransportKind::Steam)
		{
			std::unique_ptr<INetTransport> loopback = CreateTransport(TransportKind::Winsock);

			if (loopback && loopback->OpenListen(portVal))
			{
				reliabilityLayer->AddTransport(std::move(loopback));
				GGLogger::Info("GameServer: loopback listener open on port "
					+ std::to_string(portVal) + " for the local player");
			}
			else
			{
				// Not fatal - remote players still connect over Steam. Only the
				// host's own client is affected, and saying so plainly beats a
				// mystery failure when they try to join their own game.
				GGLogger::Warning("GameServer: could not open the loopback listener on port "
					+ std::to_string(portVal)
					+ "; remote players can still join, but this instance cannot play in its own game");
			}
		}

		if (!reliabilityLayer->Start())
		{
			GGLogger::Error("Failed to start reliability layer!");
			reliabilityLayer.reset();
			return false;
		}

		isRunning = true;
		startTime = std::chrono::steady_clock::now();

		GGLogger::Info("GameServer listening over " + std::string(TransportKindName(kind))
			+ " (" + std::to_string(reliabilityLayer->GetTransportCount()) + " transport(s))");

		return true;
	}

	void GameServer::StopServer()
	{
		// Allow cleanup even if not running (e.g. Init succeeded but StartServer failed)
		if (!isRunning && !reliabilityLayer) return;

		isRunning = false;

		if (reliabilityLayer)
		{
			reliabilityLayer->Stop();       // Joins the I/O thread, closes the transport
			reliabilityLayer.reset();
		}

		GGLogger::Info("GameServer stopped");
	}

	bool GameServer::Init()
	{
		// Tear down anything left from a previous session. Socket creation used
		// to happen here; it now belongs to the transport, which StartServer
		// creates, so Init is purely a reset.
		if (reliabilityLayer)
		{
			reliabilityLayer->Stop();
			reliabilityLayer.reset();
		}

		std::cout << "GameServer Initialized" << std::endl;
		return true;
	}

	TransportKind GameServer::GetTransportKind() const
	{
		if (reliabilityLayer) return reliabilityLayer->GetTransportKind();
		return GetPreferredTransport();
	}

	ReliabilityLayer::ReliabilityStats GameServer::GetStats() const
	{
		if (!reliabilityLayer) return ReliabilityLayer::ReliabilityStats{};
		return reliabilityLayer->GetStats();
	}

	bool GameServer::ConsumeRPCToken(PlayerID player, bool& outOverBudget)
	{
		outOverBudget = false;

		std::unique_lock connectionsLock(reliabilityLayer->connectionsMutex);

		ClientConnection* connection = reliabilityLayer->GetConnection(player);
		if (!connection) return false;

		auto const now = std::chrono::steady_clock::now();

		// Lazy refill: charge for the time that has passed since the last spend
		// rather than running a timer.
		double const elapsedSec = std::chrono::duration<double>(
			now - connection->lastRPCRefill).count();

		connection->lastRPCRefill = now;
		connection->rpcTokens = (std::min)(
			connection->rpcTokens + elapsedSec * RPC_TOKEN_REFILL_PER_SEC,
			static_cast<double>(RPC_TOKEN_BURST));

		if (connection->rpcTokens < 1.0)
		{
			++connection->rpcViolations;

			// Log the first violation and then every 25th, so a sustained
			// flood cannot itself become a log-spam amplifier.
			if (connection->rpcViolations == 1 || (connection->rpcViolations % 25) == 0)
			{
				GGLogger::Warning("Player " + std::to_string(player)
					+ " exceeded RPC rate limit (" + std::to_string(connection->rpcViolations)
					+ " violations)");
			}

			outOverBudget = (connection->rpcViolations >= RPC_MAX_VIOLATIONS);
			return false;
		}

		connection->rpcTokens -= 1.0;
		return true;
	}

	// TODO: Sessions handling
	void GameServer::Update() {
		// Players whose RPC rate-limit violations exceeded the threshold this
		// tick. Disconnected after the message lock is released, matching how
		// heartbeat timeouts are handled below.
		std::vector<PlayerID> abusivePlayers;

		std::unique_lock lock(reliabilityLayer->messagesMutex);

		// Bounded drain: an unbounded "process ALL messages" loop lets a single
		// peer monopolise the frame. Anything left over stays queued for the
		// next tick.
		uint32_t messagesThisTick = 0;

		while (!reliabilityLayer->incomingMessages.empty()
			&& messagesThisTick < MAX_MESSAGES_PER_TICK)
		{
			++messagesThisTick;

			auto& messageIDPair = reliabilityLayer->incomingMessages.front();
			NetworkMessage& incomingMessage = messageIDPair.first;
			PlayerID playerID = messageIDPair.second;

			switch (messageIDPair.first.GetMessageType())
			{
			case MessageType::CLIENT_TURN_ACTION:
			{
				BroadcastMessage(incomingMessage, playerID);
				break;
			}
			case MessageType::CLIENT_CONNECT_REQUEST:
			{
				std::cout << "Player " << playerID << " Connected" << std::endl;
				bool isFirstHandshake = false;
				
				{
					std::unique_lock connectionsLock(reliabilityLayer->connectionsMutex);
					ClientConnection* connection = reliabilityLayer->GetConnection(playerID);
					if (connection)
					{
						connection->lastActivity = std::chrono::steady_clock::now();
						connection->lastHeartbeat = std::chrono::steady_clock::now();
						if (!connection->isConnected)
						{
							connection->isConnected = true;
							isFirstHandshake = true;
						}
					}
				}

				if (isFirstHandshake)
				{
					GetEngineInstance().CallLuaFunctionAdvanced("ServerRPC_OnClientConnected", playerID);
				}

				NetworkMessage message;
				message.SetMessageType(MessageType::SERVER_CONNECT_RESPONSE);

				NetworkConnectionDataReturn returnData{};
				returnData.serverTimeReceived = GetServerTime();
				returnData.connectionStatus = 1;
				returnData.playerID = playerID;

				message.WriteValue<NetworkConnectionDataReturn>(returnData);

				std::cout << "Returning Player " << playerID << " Connection Request Success!" << std::endl;
				SendToClient(message, playerID);
				break;
			}
			case MessageType::CLIENT_DISCONNECT_REQUEST:
			{
				std::cout << "Player " << playerID << " Disconnected" << std::endl;
				NetworkMessage message;
				message.SetMessageType(MessageType::CLIENT_DISCONNECT_RESPONSE);
				SendToClient(message, playerID);

				reliabilityLayer->RemoveConnection(playerID);
				GetEngineInstance().CallLuaFunctionAdvanced("ServerRPC_OnClientDisconnected", playerID);
				break;
			}
			case MessageType::CLIENT_PING:
			{
				//std::cout << "Player " << playerID << " Ping!" << std::endl;

				{
					std::unique_lock connectionsLock(reliabilityLayer->connectionsMutex);
					ClientConnection* connection = reliabilityLayer->GetConnection(playerID);
					if (connection)
					{
						connection->lastHeartbeat = std::chrono::steady_clock::now();
					}
				}

				NetworkMessage message;
				message.SetMessageType(MessageType::SERVER_PONG);

				NetworkPingReturn returnData{};
				returnData.serverTimeReceived = GetServerTime();
				
				message.WriteValue<NetworkPingReturn>(returnData);
				
				//std::cout << "Returning Player " << playerID << " Ping Request Success!" << std::endl;
				SendToClient(message, playerID);
				break;
			}
			case MessageType::RPC_CALL:
			{
				// Rate limit before doing any deserialization work: dispatch
				// runs on the main thread, so an unthrottled flood of
				// well-formed RPCs is a frame-rate attack.
				bool overBudget = false;

				if (!ConsumeRPCToken(playerID, overBudget))
				{
					if (overBudget) abusivePlayers.push_back(playerID);
					break;
				}

				// playerID here is the transport-authenticated identity resolved
				// by ReliabilityLayer::FindPlayerByEndpoint - not whatever the
				// sender claims in its arguments. Deserialize reconciles the two.
				bool success = GetRPCDeserializer()->Deserialize(
					messageIDPair.first, playerID, RPCDirection::ServerBound);

				if (!success)
				{
					GGLogger::Warning("RPC dispatch rejected from player "
						+ std::to_string(playerID));
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

			reliabilityLayer->incomingMessages.pop();
		}

		lock.unlock();

		// Drop peers that blew through the RPC budget repeatedly. Reuses the
		// normal disconnect path so Lua sees a clean OnClientDisconnected.
		for (PlayerID id : abusivePlayers)
		{
			// Guard on the removal succeeding: the same player can reach this
			// loop twice in one tick (several rate-limit violations), and
			// telling Lua a player disconnected twice makes the game clean up
			// a second time against state that is already gone.
			if (!reliabilityLayer->RemoveConnection(id)) continue;

			GGLogger::Warning("Player " + std::to_string(id)
				+ " exceeded the RPC rate limit repeatedly, disconnecting");
			GetEngineInstance().CallLuaFunctionAdvanced("ServerRPC_OnClientDisconnected", id);
		}

		// Peers the transport itself reported as gone. Steam surfaces these
		// within a frame, so a client that alt-F4s is noticed immediately
		// instead of ten seconds later when the heartbeat lapses. Handled with
		// exactly the same two steps as a timeout, so Lua sees one disconnect
		// mechanism rather than two.
		std::vector<PlayerID> transportDroppedPlayers;
		reliabilityLayer->DrainTransportDisconnects(transportDroppedPlayers);

		for (PlayerID id : transportDroppedPlayers)
		{
			// RemoveConnection returns false when this player was already gone
			// (a clean CLIENT_DISCONNECT_REQUEST beat the transport's report),
			// in which case Lua has already been told and must not be told twice.
			if (!reliabilityLayer->RemoveConnection(id)) continue;

			GGLogger::Info("Player " + std::to_string(id) + " dropped by the transport");
			GetEngineInstance().CallLuaFunctionAdvanced("ServerRPC_OnClientDisconnected", id);
		}

		std::vector<PlayerID> timedOutPlayers;
		{
			std::shared_lock connectionsLock(reliabilityLayer->connectionsMutex);
			for (auto const& [id, clientConnection] : reliabilityLayer->connections)
			{
				int64_t duration = std::chrono::duration_cast<std::chrono::microseconds>
					(std::chrono::steady_clock::now() - clientConnection->lastHeartbeat).count();

				if (duration > HEARTBEAT_TIMEOUT_US)
				{
					GGLogger::Info("Player " + std::to_string(id) + " timed out (heartbeat)");
					timedOutPlayers.push_back(id);
				}
			}
		}

		// Notify Lua about timed-out clients (after releasing connection lock).
		// Same guard as above: a peer the transport already reported gone this
		// tick must not be announced twice.
		for (PlayerID id : timedOutPlayers)
		{
			if (!reliabilityLayer->RemoveConnection(id)) continue;

			GetEngineInstance().CallLuaFunctionAdvanced("ServerRPC_OnClientDisconnected", id);
		}
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
	void GameServer::BroadcastMessage(NetworkMessage const& message, PlayerID excludePlayer)
	{
		if (!reliabilityLayer) return;

		std::vector<std::pair<PlayerID, NetEndpoint>> targets;
		{
			std::shared_lock lock(reliabilityLayer->connectionsMutex);
			targets.reserve(reliabilityLayer->connections.size());
			for (auto const& [id, clientConnection] : reliabilityLayer->connections)
			{
				if (id == excludePlayer) continue;
				targets.emplace_back(id, clientConnection->endpoint);
			}
		}

		auto const serializeMessage = message.Serialize();

		size_t totalWireSize = PacketHeader::HEADER_SIZE + serializeMessage.size();

		std::cout << "[NET] BroadcastMessage: msgPayload=" << message.GetPayloadSize()
			<< " serialized=" << serializeMessage.size()
			<< " wireSize=" << totalWireSize
			<< " MAX_PACKET=" << MAX_PACKET_SIZE
			<< " needsFrag=" << (totalWireSize > MAX_PACKET_SIZE ? "YES" : "NO") << std::endl;

		if (totalWireSize <= MAX_PACKET_SIZE)
		{
			for (auto const& [id, _] : targets)
			{
				Packet packet{};
				packet.payload.insert(packet.payload.end(), serializeMessage.begin(), serializeMessage.end());
				packet.header.payloadSize = static_cast<uint16_t>(packet.payload.size());
				packet.header.packetType = PacketType::DATA;
				reliabilityLayer->SendPacket(id, packet);
			}
		}
		else
		{
			// Large message: fragment for each client
			for (auto const& [id, endpoint] : targets)
			{
				(void)id;
				SendFragmented(*reliabilityLayer, endpoint,
					serializeMessage, message.GetMessageID());
			}
		}
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
	void GameServer::SendToClient(NetworkMessage const& message, PlayerID player)
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
			if (!reliabilityLayer->SendPacket(player, packet))
			{
				std::cout << "ERROR: No Player ID of " << player << " found!" << std::endl;
			}
		}
		else
		{
			NetEndpoint targetEndpoint{};
			{
				std::shared_lock lock(reliabilityLayer->connectionsMutex);
				auto it = reliabilityLayer->connections.find(player);
				if (it == reliabilityLayer->connections.end())
				{
					std::cout << "ERROR: No Player ID of " << player << " found!" << std::endl;
					return;
				}
				targetEndpoint = it->second->endpoint;
			}

			// Large message: fragment
			SendFragmented(*reliabilityLayer, targetEndpoint,
				serializeMessage, message.GetMessageID());
		}
	}
}
