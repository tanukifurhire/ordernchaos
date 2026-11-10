/*************************************************************************
  \file       UDPReliabilityLayer.cpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
	UDP reliability protocol with threading and connection management
  \details
	Implements ReliabilityLayer class providing TCP-like reliability over UDP.
	Manages dedicated I/O thread for asynchronous packet reception, player ID
	allocation from reusable pool, client connection tracking with thread-safe
	maps, and incoming message queue for application-layer consumption.

	Handles CLIENT_CONNECT_REQUEST messages by assigning unique PlayerIDs and
	registering ClientConnection instances. Routes received packets to correct
	players via address-to-ID lookup. Provides statistics tracking for network
	performance monitoring (packets sent/received/lost).
  \dependencies
	UDPReliabilityLayer.hpp, UDP.hpp, UDPMessage.hpp, UDPTypes.hpp,
	PlatformWindows.hpp, <iostream>, <thread>, <chrono>, <shared_mutex>
  \see
	UDPReliabilityLayer.hpp, ReliabilityLayer, ClientConnection, Packet
  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <UDPReliabilityLayer.hpp>

#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace GGNetworking
{
	/*************************************************************************
	  \brief
		Constructs reliability layer over an already-opened transport.
	  \param netTransport
		Transport to drive. OpenListen or OpenConnect must already have
		succeeded on it.
	  \details
		Everything below this class - sequencing, ACK/NAK, adaptive RTO,
		congestion measurement, fragmentation, dedup - is transport-agnostic,
		and now says so. The layer used to be handed a raw SOCKET, which welded
		it to Winsock and left it closing a handle it did not own.

		Does not start the I/O thread; call Start() explicitly.
	  \note
		Takes ownership. The transport is closed in Stop() and destroyed with
		this object.
	  \see
		Start, Stop, INetTransport, GameServer::Init
	*************************************************************************/
	ReliabilityLayer::ReliabilityLayer(std::unique_ptr<INetTransport> netTransport)
	{
		if (netTransport) transports.push_back(std::move(netTransport));
	}

	bool ReliabilityLayer::AddTransport(std::unique_ptr<INetTransport> netTransport)
	{
		if (!netTransport) return false;

		NetEndpoint::Kind const kind = (netTransport->GetKind() == TransportKind::Steam)
			? NetEndpoint::Kind::STEAM
			: NetEndpoint::Kind::IPV4;

		// One per kind. Routing keys on the endpoint's kind, so a second
		// transport of the same kind would be unreachable - and worse, would
		// silently swallow half the sends if the wrong one were picked.
		if (TransportFor(kind) != nullptr)
		{
			std::cerr << "NETWORK: a " << TransportKindName(netTransport->GetKind())
				<< " transport is already registered" << std::endl;
			return false;
		}

		transports.push_back(std::move(netTransport));
		return true;
	}

	INetTransport* ReliabilityLayer::TransportFor(NetEndpoint::Kind kind) const
	{
		for (auto const& candidate : transports)
		{
			bool const isSteam = (candidate->GetKind() == TransportKind::Steam);

			if (kind == NetEndpoint::Kind::STEAM && isSteam)  return candidate.get();
			if (kind == NetEndpoint::Kind::IPV4  && !isSteam) return candidate.get();
		}

		return nullptr;
	}

	INetTransport* ReliabilityLayer::GetTransport(TransportKind kind) const
	{
		for (auto const& candidate : transports)
		{
			if (candidate->GetKind() == kind) return candidate.get();
		}

		return nullptr;
	}

	/*************************************************************************
	  \brief
		Destructor ensuring clean shutdown of I/O thread and socket closure.
	  \details
		Calls Stop() to:
		1. Signal I/O thread to exit via shouldRun atomic flag
		2. Join socketIOThread (blocks until thread terminates)
		3. Close socket if still valid

		Ensures no resource leaks even if Stop() was not explicitly called.
	  \warning
		Blocking destructor - may delay up to CONNECTION_TIMEOUT_MS if thread
		is stuck in recvfrom(). Consider calling Stop() explicitly before
		destruction for deterministic shutdown timing.
	  \see
		Stop, Start
	*************************************************************************/
	ReliabilityLayer::~ReliabilityLayer()
	{
		Stop();
	}

	/*************************************************************************
	  \brief
		Starts reliability layer I/O thread and initializes player ID pool.
	  \return
		true if thread started successfully, false on thread creation failure.
	  \details
		Initialization sequence:
		1. Populates availablePlayerIDs queue with IDs 1 to MAX_PLAYERS-1
		   (PlayerID 0 reserved for invalid/server)
		2. Sets shouldRun atomic flag to true
		3. Spawns socketIOThread running ReceiveThreadProc()

		Must be called before any Send/Receive operations. Logs detailed error
		messages via std::cerr if thread creation fails (resource exhaustion).
	  \note
		Thread-safe to call multiple times (subsequent calls are no-ops if
		already running). Check shouldRun.load() to detect running state.
	  \warning
		Does not validate socket state. Caller must ensure socket is bound
		and set to non-blocking mode before calling Start().
	  \see
		Stop, ReceiveThreadProc, GameServer::Start
	*************************************************************************/
	bool ReliabilityLayer::Start()
	{
		if (transports.empty())
		{
			std::cerr << "Error: ReliabilityLayer::Start with no transport" << std::endl;
			return false;
		}

		for (uint32_t i{ 1 }; i < MAX_PLAYERS; i++) {
			availablePlayerIDs.push(i);
		}

		shouldRun.store(true, std::memory_order_release);

		try
		{
			socketIOThread = std::thread([this]() { this->ReceiveThreadProc(); });
		}
		catch (const std::system_error& e)
		{
			std::cerr << "Error: Failed to create socket IO for thread: " 
				<< e.what() << std::endl;
			return false;
		}

		std::cout << "ReliabilityLayer started" << std::endl;
		return true;
	}

	/*************************************************************************
	  \brief
		Stops I/O thread and closes socket for clean shutdown.
	  \details
		Shutdown sequence:
		1. Sets shouldRun to false (signals thread to exit)
		2. Joins socketIOThread (blocks until ReceiveThreadProc returns)
		3. Closes the transport

		Safe to call multiple times (subsequent calls are no-ops).
		Blocks until the I/O thread finishes its current iteration.
	  \note
		Does not send DISCONNECT packets to clients. Caller should explicitly
		disconnect all clients before calling Stop().
	  \warning
		Blocking call - waits out at most one I/O-thread iteration, which is
		bounded by the 1 ms Poll timeout.
	  \see
		Start, ReceiveThreadProc, ~ReliabilityLayer
	*************************************************************************/
	void ReliabilityLayer::Stop()
	{
		shouldRun.store(false, std::memory_order_release);

		// Join before closing: the I/O thread is the only caller of the
		// transport's Send/Receive/Poll, so closing underneath it would be a
		// use-after-free of whatever the backend holds.
		if (socketIOThread.joinable()) socketIOThread.join();

		for (auto const& openTransport : transports) openTransport->Close();
	}

	/*************************************************************************
	  \brief
		I/O thread main loop for continuous packet reception and retransmission handling.
	  \details
		Thread execution flow:
		1. Loops while shouldRun.load() is true
		2. Calls ReceivePacket() to process incoming datagrams
		3. Checks all connections for packets needing retransmission
		4. Retransmits unacknowledged packets that have timed out
		5. Sleeps 1ms if ReceivePacket returns false (no data available)

		The 1ms sleep prevents busy-waiting when no packets arrive, reducing
		CPU usage while maintaining low latency (~1ms response time).

		Retransmission logic:
		- Checks pendingPackets for each connection
		- If packet not acknowledged and timeout exceeded, retransmit
		- Increments retransmissionCount and stats.packetsRetransmitted
		- Removes packet if MAX_RETRANSMISSIONS exceeded

		Thread exits when shouldRun is set to false by Stop().
	  \note
		Runs in dedicated thread spawned by Start(). Do not call directly.
		All socket operations use non-blocking mode (WSAEWOULDBLOCK expected).
	  \see
		Start, Stop, ReceivePacket
	*************************************************************************/
	void ReliabilityLayer::ReceiveThreadProc()
	{
		while (shouldRun.load())
		{
			if (!ReceivePacket())
			{
				// Wait on the transports instead of a blind sleep. Winsock does
				// this with select(); Steam has no readability handle and just
				// sleeps.
				//
				// The 1 ms budget is split across them rather than spent on
				// each: polling two transports for a full millisecond apiece
				// would double idle latency for every peer on both.
				uint32_t const perTransport =
					1000u / static_cast<uint32_t>(transports.size());

				for (auto const& idleTransport : transports)
				{
					idleTransport->Poll(perTransport);
				}
			}

			// Peers the transport knows are gone (Steam reports these within a
			// frame; Winsock has nothing to report and this is a no-op).
			ProcessTransportDisconnects();

			// Handle retransmissions
			std::unique_lock lock(connectionsMutex);
			for (auto& [playerID, connection] : connections)
			{
				auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now().time_since_epoch());

				bool timeoutOccurred = false;

				for (auto it = connection->pendingPackets.begin(); it != connection->pendingPackets.end(); )
				{
					Packet& packet = it->second;

					if (!packet.acknowledged)
					{
						auto elapsed = (now - packet.timeSent).count();

						// Use per-connection adaptive RTO instead of fixed PACKET_TIMEOUT_MS
						if (elapsed > static_cast<long long>(connection->rto))
						{
							if (packet.retransmissionCount >= MAX_RETRANSMISSIONS)
							{
								// Give up on this packet
								it = connection->pendingPackets.erase(it);
								continue;
							}

							timeoutOccurred = true;

							// Retransmit
							packet.CalculateChecksum();
							std::vector<uint8_t> serializedData = packet.Serialize();

							// Unreliable on purpose even on the Steam backend:
							// this IS the retransmit, so a second reliability
							// layer underneath would only add its own delay.
							INetTransport* out = TransportFor(connection->endpoint.kind);
							int bytesSent = out
								? out->Send(connection->endpoint,
									serializedData.data(), serializedData.size(), false)
								: -1;

							if (bytesSent >= 0)
							{
								packet.timeSent = now;
								packet.retransmissionCount++;
								stats.packetsRetransmitted++;
							}
						}
					}

					++it;
				}

				// Congestion control: multiplicative decrease on timeout (once per connection)
				if (timeoutOccurred)
				{
					connection->ssthresh = (std::max)(connection->cwnd / 2, 2u);
					connection->cwnd = 1;
					connection->cwndAckCount = 0;
					// Karn's algorithm: double RTO on timeout (exponential backoff)
					connection->rto = (std::min)(connection->rto * 2.0, MAX_RTO_MS);
				}
			}
			lock.unlock();
		}
	}

	/*************************************************************************
	  \brief
		Retrieves client connection by player ID.
	  \param id
		PlayerID to lookup (1 to MAX_PLAYERS-1).
	  \return
		Pointer to ClientConnection if found, nullptr if ID not registered.
	  \details
		Thread-safe lookup via std::unordered_map::find. Does not modify
		connection state. Used by SendPacket and application layer for
		connection metadata access (last activity, sequence numbers, etc.).
	  \note
		Returns raw pointer (non-owning). Caller must not delete. Pointer
		remains valid until RemoveConnection is called for this ID.
	  \warning
		Concurrent access safe via connectionsMutex, but caller must not
		hold pointer across lock boundaries (may become dangling if another
		thread calls RemoveConnection).
	  \see
		AddConnection, connections
	*************************************************************************/
	ClientConnection* ReliabilityLayer::GetConnection(PlayerID id)
	{
		auto it = connections.find(id);

		if (it == connections.end()) return nullptr;
		else return it->second.get();
	}

	/*************************************************************************
	  \brief
		Transmits packet to specific client via UDP socket.
	  \param id
		Target PlayerID (must exist in connections map).
	  \param packet
		Packet instance to serialize and send.
	  \return
		true if sendto succeeded, false on socket error or unknown PlayerID.
	  \details
		Transmission process:
		1. Looks up ClientConnection by id (shared_lock on connectionsMutex)
		2. Assigns sequence number from connection's nextSendSequence
		3. Calculates checksum for packet header
		4. Serializes packet via Packet::Serialize()
		5. Calls sendto() with client's sockaddr_in
		6. Stores packet in pendingPackets map for ACK tracking
		7. Updates transmission metadata (timeSent, retransmissionCount)
		8. Increments stats.packetsSent counter

		Handles common sendto errors:
		- WSAEWOULDBLOCK: Socket buffer full (returns false, caller should retry)
		- WSAECONNRESET: Client unreachable (returns false, triggers timeout logic)
	  \note
		Adds packet to pendingPackets for retransmission if not acknowledged.
	  \warning
		Does not validate packet checksum. Calls CalculateChecksum() internally.
	  \see
		ReceivePacket, Packet::Serialize, ClientConnection
	*************************************************************************/
	bool ReliabilityLayer::SendPacketUnreliable(PlayerID id, Packet const& packet)
	{
		std::shared_lock lock(connectionsMutex);

		auto it = connections.find(id);
		if (it == connections.end()) return false;

		ClientConnection* connection = it->second.get();

		// Make a mutable copy - set as unreliable, no sequence tracking
		Packet outgoingPacket = packet;
		outgoingPacket.header.sequenceNum = 0; // No sequencing for unreliable
		outgoingPacket.header.payloadSize = static_cast<uint16_t>(outgoingPacket.payload.size());
		outgoingPacket.header.packetType = PacketType::UNRELIABLE_DATA;
		outgoingPacket.CalculateChecksum();

		std::vector<uint8_t> serializedData = outgoingPacket.Serialize();

		INetTransport* out = TransportFor(connection->endpoint.kind);
		if (!out || out->Send(connection->endpoint,
			serializedData.data(), serializedData.size(), false) < 0)
		{
			return false;
		}

		stats.packetsSent++;
		return true;
	}

	bool ReliabilityLayer::SendUnreliableToEndpoint(NetEndpoint const& endpoint, uint8_t const* data, size_t size)
	{
		INetTransport* out = TransportFor(endpoint.kind);
		if (!out) return false;

		if (out->Send(endpoint, data, size, false) < 0) return false;

		stats.packetsSent++;
		return true;
	}

	bool ReliabilityLayer::SendPacket(PlayerID id, Packet const& packet)
	{
		std::shared_lock lock(connectionsMutex);

		auto it = connections.find(id);
		if (it == connections.end()) return false;

		ClientConnection* connection = it->second.get();

		// Make a mutable copy to assign sequence number
		Packet outgoingPacket = packet;
		outgoingPacket.header.sequenceNum = connection->nextSendSequence;
		outgoingPacket.header.payloadSize = static_cast<uint16_t>(outgoingPacket.payload.size());
		outgoingPacket.CalculateChecksum();

		std::vector<uint8_t> serializedData = outgoingPacket.Serialize();

		// `reliable = true` is a hint the backend may ignore, and by default
		// both do: this layer's own sequencing and RTO are what make the
		// packet reliable, and they must stay in the loop to remain
		// demonstrable. SteamTransport honours the hint only when reliability
		// has been deliberately delegated to Valve.
		INetTransport* out = TransportFor(connection->endpoint.kind);
		if (!out || out->Send(connection->endpoint,
			serializedData.data(), serializedData.size(), true) < 0)
		{
			return false;
		}

		// Store packet for potential retransmission
		outgoingPacket.timeSent = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch());
		outgoingPacket.retransmissionCount = 0;
		outgoingPacket.acknowledged = false;

		connection->pendingPackets[connection->nextSendSequence] = outgoingPacket;
		connection->nextSendSequence++;

		stats.packetsSent++;

		return true;
	}

	/*************************************************************************
	  \brief
		Receives and processes one incoming UDP datagram.
	  \return
		true if packet received and processed successfully, false on socket
		error or malformed packet.
	  \details
		Reception and processing pipeline:
		1. Calls recvfrom() on non-blocking socket
		2. Handles socket errors (WSAEWOULDBLOCK, WSAECONNRESET, etc.)
		3. Validates minimum size (HEADER_SIZE bytes)
		4. Deserializes into Packet via Packet::Deserialize()
		5. Handles packet based on PacketType:
		   - ACK: Marks corresponding packet in pendingPackets as acknowledged
		   - DATA: Validates sequence, sends ACK, extracts NetworkMessage
		   - HEARTBEAT: Updates lastActivity timestamp
		6. Checks for duplicate packets via receivedPackets map
		7. Enqueues message + PlayerID pair into incomingMessages queue

		Statistics tracking:
		- Increments packetsReceived on successful deserialization
		- Increments packetsLost on errors (truncated, invalid header, etc.)
		- Increments duplicatesReceived on repeated sequence numbers

		Error handling:
		- WSAEWOULDBLOCK: No data available (returns false, not an error)
		- WSAEINTR: Interrupted syscall (returns false, retry expected)
		- WSAECONNRESET: ICMP port unreachable (logs, returns false)
		- WSAEINVAL: Invalid socket parameters (logs error, returns false)
	  \note
		Thread-safe via messagesMutex lock when enqueueing messages.
		Called continuously by ReceiveThreadProc in I/O thread.
	  \see
		SendPacket, ReceiveThreadProc, FindPlayerByEndpoint
	*************************************************************************/
	bool ReliabilityLayer::ReceivePacket()
	{
		ReceivedDatagram datagram;

		// Round-robin across transports so a busy one cannot starve the other.
		// nextReceiveTransport carries the rotation between calls; it is only
		// ever touched by the I/O thread.
		bool gotData = false;
		for (size_t attempt = 0; attempt < transports.size(); ++attempt)
		{
			size_t const index = (nextReceiveTransport + attempt) % transports.size();

			if (transports[index]->Receive(datagram))
			{
				nextReceiveTransport = (index + 1) % transports.size();
				gotData = true;
				break;
			}
		}

		if (!gotData) return false;

		NetEndpoint const&  fromAddr      = datagram.from;
		uint8_t const*      buffer        = datagram.data.data();
		int const           bytesReceived = static_cast<int>(datagram.data.size());

		if (bytesReceived < PacketHeader::HEADER_SIZE)
		{
			stats.packetsLost++;
			return false;
		}

		Packet incomingPacket;

		Packet::PacketDeserializeStatus packetStatus = incomingPacket.Deserialize(buffer, bytesReceived);

		switch (packetStatus)
		{
		case Packet::PacketDeserializeStatus::BYTES_NOT_SAME_TOTAL:
		{
			stats.packetsLost++;

			// Size mismatch likely means corrupted payloadSize field.
			// Send NAK so the sender can retransmit promptly instead of
			// waiting for the full RTO timeout.
			if (bytesReceived >= static_cast<int>(PacketHeader::HEADER_SIZE))
			{
				SequenceNum corruptedSeq{};
				std::memcpy(&corruptedSeq, buffer, sizeof(SequenceNum));

				// Only NAK if seq looks plausible (non-zero = reliable packet)
				if (corruptedSeq != 0)
				{
					Packet nakPacket{};
					nakPacket.header.sequenceNum = corruptedSeq;
					nakPacket.header.payloadSize = 0;
					nakPacket.header.packetType = PacketType::NAK;
					nakPacket.CalculateChecksum();

					std::vector<uint8_t> serializedNak = nakPacket.Serialize();
					if (INetTransport* out = TransportFor(fromAddr.kind))
					{
						out->Send(fromAddr, serializedNak.data(), serializedNak.size(), false);
					}

					std::cout << "NETWORK: Sent NAK for size-mismatched packet seq=" << corruptedSeq << std::endl;
				}
			}

			return false;
		}
		case Packet::PacketDeserializeStatus::INVALID_HEADER:
		{
			stats.packetsLost++;

			// Send NAK to request retransmission of the corrupted packet
			// Extract sequence number directly from raw buffer (first 4 bytes)
			if (bytesReceived >= static_cast<int>(PacketHeader::HEADER_SIZE))
			{
				SequenceNum corruptedSeq{};
				std::memcpy(&corruptedSeq, buffer, sizeof(SequenceNum));

				// seq=0 means unreliable packet — no point NAK-ing since
				// unreliable packets aren't tracked for retransmission
				if (corruptedSeq != 0)
				{
					Packet nakPacket{};
					nakPacket.header.sequenceNum = corruptedSeq;
					nakPacket.header.payloadSize = 0;
					nakPacket.header.packetType = PacketType::NAK;
					nakPacket.CalculateChecksum();

					std::vector<uint8_t> serializedNak = nakPacket.Serialize();
					if (INetTransport* out = TransportFor(fromAddr.kind))
					{
						out->Send(fromAddr, serializedNak.data(), serializedNak.size(), false);
					}

					std::cout << "NETWORK: Sent NAK for corrupted packet seq=" << corruptedSeq << std::endl;
				}
			}

			return false;
		}
		case Packet::PacketDeserializeStatus::NO_PAYLOAD:
		{
			// ACK and HEARTBEAT packets have no payload
			break;
		}
		case Packet::PacketDeserializeStatus::PACKET_DESERIALIZE_SUCCESS:
		{
			stats.packetsReceived++;
			break;
		}
		}

		// Handle packet based on type
		if (incomingPacket.header.packetType == PacketType::ACK)
		{
			// Handle acknowledgment
			PlayerID senderID = FindPlayerByEndpoint(fromAddr);
			if (senderID == 0) return false;

			std::unique_lock lock(connectionsMutex);
			auto it = connections.find(senderID);
			if (it == connections.end()) return false;

			ClientConnection* connection = it->second.get();
			SequenceNum ackedSeq = incomingPacket.header.sequenceNum;

			auto pendingIt = connection->pendingPackets.find(ackedSeq);
			if (pendingIt != connection->pendingPackets.end())
			{
				pendingIt->second.acknowledged = true;

				// Karn's algorithm: only use RTT sample from non-retransmitted packets
				if (pendingIt->second.retransmissionCount == 0)
				{
					auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now().time_since_epoch());
					double sampleRTT = static_cast<double>((now - pendingIt->second.timeSent).count());

					// Kurose-style adaptive RTO: EstimatedRTT + 4 * DevRTT
					if (connection->estimatedRTT == 0.0)
					{
						connection->estimatedRTT = sampleRTT;
						connection->devRTT = sampleRTT / 2.0;
					}
					else
					{
						connection->devRTT = 0.75 * connection->devRTT
							+ 0.25 * std::fabs(sampleRTT - connection->estimatedRTT);
						connection->estimatedRTT = 0.875 * connection->estimatedRTT
							+ 0.125 * sampleRTT;
					}

					connection->rto = connection->estimatedRTT + 4.0 * connection->devRTT;
					if (connection->rto < MIN_RTO_MS) connection->rto = MIN_RTO_MS;
					if (connection->rto > MAX_RTO_MS) connection->rto = MAX_RTO_MS;

					stats.averageRoundTripTime = connection->estimatedRTT;
				}

				// Congestion control: grow window
				if (connection->cwnd < connection->ssthresh)
				{
					// Slow start: exponential increase (cwnd++ per ACK)
					connection->cwnd++;
				}
				else
				{
					// Congestion avoidance: additive increase (cwnd++ per cwnd ACKs)
					connection->cwndAckCount++;
					if (connection->cwndAckCount >= connection->cwnd)
					{
						connection->cwnd++;
						connection->cwndAckCount = 0;
					}
				}

				connection->pendingPackets.erase(pendingIt);
			}

			connection->lastActivity = std::chrono::steady_clock::now();
			lock.unlock();
			ackCV.notify_all();
			return true;
		}
		else if (incomingPacket.header.packetType == PacketType::HEARTBEAT)
		{
			// Handle heartbeat
			PlayerID senderID = FindPlayerByEndpoint(fromAddr);
			if (senderID == 0) return false;

			std::unique_lock lock(connectionsMutex);
			auto it = connections.find(senderID);
			if (it == connections.end()) return false;

			it->second->lastActivity = std::chrono::steady_clock::now();
			it->second->lastHeartbeat = std::chrono::steady_clock::now();

			// Send ACK for heartbeat
			Packet ackPacket{};
			ackPacket.header.sequenceNum = incomingPacket.header.sequenceNum;
			ackPacket.header.payloadSize = 0;
			ackPacket.header.packetType = PacketType::ACK;
			ackPacket.CalculateChecksum();

			std::vector<uint8_t> serializedAck = ackPacket.Serialize();
			if (INetTransport* out = TransportFor(fromAddr.kind))
			{
				out->Send(fromAddr, serializedAck.data(), serializedAck.size(), false);
			}

			return true;
		}
		else if (incomingPacket.header.packetType == PacketType::DATA)
		{
			// Send ACK immediately using the fromAddr (works for both client and server)
			Packet ackPacket{};
			ackPacket.header.sequenceNum = incomingPacket.header.sequenceNum;
			ackPacket.header.payloadSize = 0;
			ackPacket.header.packetType = PacketType::ACK;
			ackPacket.CalculateChecksum();

			std::vector<uint8_t> serializedAck = ackPacket.Serialize();
			if (INetTransport* out = TransportFor(fromAddr.kind))
			{
				out->Send(fromAddr, serializedAck.data(), serializedAck.size(), false);
			}

			// Process message payload
			if (incomingPacket.payload.size() > 0)
			{
				NetworkMessage message;
				MessageHeader const* messageHeader = reinterpret_cast<MessageHeader const*>(incomingPacket.payload.data());

				NetworkMessage::MessageDeserializeStatus messageStatus =
					message.Deserialize(incomingPacket.payload.data(), incomingPacket.payload.size());

				if (messageStatus == NetworkMessage::MessageDeserializeStatus::MESSAGE_DESERIALIZE_SUCCESS ||
					messageStatus == NetworkMessage::MessageDeserializeStatus::NO_PAYLOAD)
				{
					// Handle connection request specially to assign PlayerID
					PlayerID senderID = 0;
					
					if (messageHeader->messageType == MessageType::CLIENT_CONNECT_REQUEST)
					{
						// Idempotent connect: if this address already has a connection,
						// reuse the existing PlayerID instead of creating duplicates.
						senderID = FindPlayerByEndpoint(fromAddr);
						if (senderID == 0)
						{
							// availablePlayerIDs is written from the main thread
							// too, by RemoveConnection, and now from this thread
							// by ProcessTransportDisconnects. Take the same lock
							// those use before touching it - an unguarded pop
							// against a concurrent push corrupts the deque.
							PlayerID id = 0;
							{
								std::unique_lock idLock(connectionsMutex);
								if (availablePlayerIDs.empty())
								{
									std::cerr << "NETWORK: No available PlayerIDs for new connection" << std::endl;
									return false;
								}

								id = availablePlayerIDs.front();
								availablePlayerIDs.pop();
							}

							if (!AddConnection(id, fromAddr, 0))
							{
								std::unique_lock idLock(connectionsMutex);
								availablePlayerIDs.push(id);
								return false;
							}
							senderID = id;
						}
					}
					else
					{
						// For other messages, try to find the player by address
						senderID = FindPlayerByEndpoint(fromAddr);
						if (senderID == 0) return false;
					}

					// Drop duplicate reliable DATA packets (retransmits) so RPC calls
					// are not executed multiple times at higher layers.
					{
						SequenceNum const seq = incomingPacket.header.sequenceNum;
						if (senderID != 0 && seq != 0)
						{
							std::unique_lock connLock(connectionsMutex);
							auto connIt = connections.find(senderID);
							if (connIt == connections.end()) return false;

							ClientConnection* senderConn = connIt->second.get();
							auto inserted = senderConn->receivedPackets.emplace(seq, true);
							if (!inserted.second)
							{
								senderConn->lastActivity = std::chrono::steady_clock::now();
								stats.duplicatesReceived++;
								return true;
							}

							if (seq >= senderConn->expectedReceiveSequence)
							{
								senderConn->expectedReceiveSequence = seq + 1;
							}

							// Trim old sequence history to avoid unbounded growth.
							if (senderConn->receivedPackets.size() > 4096)
							{
								SequenceNum cutoff = 0;
								if (senderConn->expectedReceiveSequence > 2048)
								{
									cutoff = senderConn->expectedReceiveSequence - 2048;
								}

								for (auto it = senderConn->receivedPackets.begin(); it != senderConn->receivedPackets.end(); )
								{
									if (it->first < cutoff) it = senderConn->receivedPackets.erase(it);
									else ++it;
								}
							}

							senderConn->lastActivity = std::chrono::steady_clock::now();
						}
					}

					auto messageIDPair = std::make_pair(message, senderID);

					std::unique_lock msgLock(messagesMutex);
					incomingMessages.push(messageIDPair);
					msgLock.unlock();
					messageCV.notify_one();
				}
			}

			return true;
		}
		else if (incomingPacket.header.packetType == PacketType::FRAGMENT)
		{
			std::cout << "[FRAG-RECV] Received FRAGMENT packet, payloadSize="
				<< incomingPacket.payload.size() << std::endl;

			// Send ACK for the fragment
			Packet ackPacket{};
			ackPacket.header.sequenceNum = incomingPacket.header.sequenceNum;
			ackPacket.header.payloadSize = 0;
			ackPacket.header.packetType = PacketType::ACK;
			ackPacket.CalculateChecksum();

			std::vector<uint8_t> serializedAck = ackPacket.Serialize();
			if (INetTransport* out = TransportFor(fromAddr.kind))
			{
				out->Send(fromAddr, serializedAck.data(), serializedAck.size(), false);
			}

			// Validate fragment payload has at least the FragmentHeader
			if (incomingPacket.payload.size() < FragmentHeader::HEADER_SIZE)
			{
				std::cerr << "[FRAG-RECV] Payload too small for FragmentHeader ("
					<< incomingPacket.payload.size() << " < " << FragmentHeader::HEADER_SIZE << ")" << std::endl;
				stats.packetsLost++;
				return false;
			}

			// Extract fragment header
			FragmentHeader fragHeader;
			std::memcpy(&fragHeader, incomingPacket.payload.data(), FragmentHeader::HEADER_SIZE);

			std::cout << "[FRAG-RECV] FragmentHeader: msgID=" << fragHeader.messageID
				<< " index=" << fragHeader.fragmentIndex
				<< " total=" << fragHeader.fragmentTotal
				<< " dataSize=" << (incomingPacket.payload.size() - FragmentHeader::HEADER_SIZE) << std::endl;

			if (fragHeader.fragmentIndex >= fragHeader.fragmentTotal || fragHeader.fragmentTotal == 0)
			{
				std::cerr << "[FRAG-RECV] Invalid fragment header: index=" << fragHeader.fragmentIndex
					<< " total=" << fragHeader.fragmentTotal << std::endl;
				stats.packetsLost++;
				return false;
			}

			// Extract fragment data (everything after FragmentHeader)
			std::vector<uint8_t> fragData(
				incomingPacket.payload.begin() + FragmentHeader::HEADER_SIZE,
				incomingPacket.payload.end());

			// Store fragment in reassembly buffer
			auto& assembly = fragmentAssembly[fragHeader.messageID];
			if (assembly.fragmentTotal == 0)
			{
				assembly.fragmentTotal = fragHeader.fragmentTotal;
				assembly.receivedCount = 0;
				assembly.fragments.resize(fragHeader.fragmentTotal);
				assembly.firstReceived = std::chrono::steady_clock::now();
				std::cout << "[FRAG-RECV] New assembly started for msgID=" << fragHeader.messageID
					<< " expecting " << fragHeader.fragmentTotal << " fragments" << std::endl;
			}

			// Only store if not already received (handle duplicate fragments)
			if (assembly.fragments[fragHeader.fragmentIndex].empty())
			{
				assembly.fragments[fragHeader.fragmentIndex] = std::move(fragData);
				assembly.receivedCount++;
				std::cout << "[FRAG-RECV] Stored fragment " << fragHeader.fragmentIndex
					<< " (" << assembly.receivedCount << "/" << assembly.fragmentTotal << " received)" << std::endl;
			}
			else
			{
				std::cout << "[FRAG-RECV] Duplicate fragment " << fragHeader.fragmentIndex
					<< " for msgID=" << fragHeader.messageID << " (ignored)" << std::endl;
			}

			// Check if all fragments received
			if (assembly.receivedCount == assembly.fragmentTotal)
			{
				// Reassemble the original serialized message
				std::vector<uint8_t> reassembled;
				for (auto const& frag : assembly.fragments)
				{
					reassembled.insert(reassembled.end(), frag.begin(), frag.end());
				}

				std::cout << "[FRAG-RECV] All fragments received for msgID=" << fragHeader.messageID
					<< " reassembledSize=" << reassembled.size() << std::endl;

				// Deserialize as NetworkMessage
				NetworkMessage message;
				auto msgStatus = message.Deserialize(reassembled.data(), reassembled.size());

				if (msgStatus == NetworkMessage::MessageDeserializeStatus::MESSAGE_DESERIALIZE_SUCCESS)
				{
					PlayerID senderID = FindPlayerByEndpoint(fromAddr);

					auto messageIDPair = std::make_pair(message, senderID);

					std::unique_lock msgLock(messagesMutex);
					incomingMessages.push(messageIDPair);
					msgLock.unlock();
					messageCV.notify_one();

					std::cout << "[FRAG-RECV] SUCCESS: Reassembled and enqueued message ID=" << fragHeader.messageID
						<< " from " << fragHeader.fragmentTotal << " fragments ("
						<< reassembled.size() << " bytes), msgType=" 
						<< static_cast<int>(message.GetMessageType()) << std::endl;
				}
				else
				{
					std::cerr << "[FRAG-RECV] FAILED to deserialize reassembled message ID="
						<< fragHeader.messageID << " status=" << static_cast<int>(msgStatus)
						<< " reassembledSize=" << reassembled.size() << std::endl;
				}

				fragmentAssembly.erase(fragHeader.messageID);
			}

			// Clean up stale fragment assemblies (older than FRAGMENT_TIMEOUT_MS)
			auto now = std::chrono::steady_clock::now();
			for (auto it = fragmentAssembly.begin(); it != fragmentAssembly.end(); )
			{
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
					now - it->second.firstReceived).count();
				if (elapsed > FRAGMENT_TIMEOUT_MS)
				{
					std::cerr << "[FRAG-RECV] Timeout: discarding incomplete message ID="
						<< it->first << " (" << it->second.receivedCount
						<< "/" << it->second.fragmentTotal << " fragments)" << std::endl;
					it = fragmentAssembly.erase(it);
				}
				else
				{
					++it;
				}
			}

			stats.packetsReceived++;
			return true;
		}
		else if (incomingPacket.header.packetType == PacketType::UNRELIABLE_DATA)
		{
			// Fire-and-forget: no ACK sent, no sequence tracking, just deliver the message
			if (incomingPacket.payload.size() > 0)
			{
				NetworkMessage message;
				NetworkMessage::MessageDeserializeStatus messageStatus =
					message.Deserialize(incomingPacket.payload.data(), incomingPacket.payload.size());

				if (messageStatus == NetworkMessage::MessageDeserializeStatus::MESSAGE_DESERIALIZE_SUCCESS ||
					messageStatus == NetworkMessage::MessageDeserializeStatus::NO_PAYLOAD)
				{
					PlayerID senderID = FindPlayerByEndpoint(fromAddr);

					auto messageIDPair = std::make_pair(message, senderID);

					std::unique_lock msgLock(messagesMutex);
					incomingMessages.push(messageIDPair);
					msgLock.unlock();
					messageCV.notify_one();
				}
			}

			stats.packetsReceived++;
			return true;
		}
		else if (incomingPacket.header.packetType == PacketType::NAK)
		{
			// Handle negative acknowledgment - retransmit requested packet immediately
			PlayerID senderID = FindPlayerByEndpoint(fromAddr);
			if (senderID == 0) return false;

			std::unique_lock lock(connectionsMutex);
			auto it = connections.find(senderID);
			if (it == connections.end()) return false;

			ClientConnection* connection = it->second.get();
			SequenceNum nakedSeq = incomingPacket.header.sequenceNum;

			auto pendingIt = connection->pendingPackets.find(nakedSeq);
			if (pendingIt != connection->pendingPackets.end())
			{
				Packet& pendingPkt = pendingIt->second;

				// Rate-limit: skip if already retransmitted recently
				auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now().time_since_epoch());
				if ((now - pendingPkt.timeSent).count() < 50)
					return true;

				// Retransmit
				pendingPkt.CalculateChecksum();
				std::vector<uint8_t> serializedData = pendingPkt.Serialize();

				INetTransport* out = TransportFor(connection->endpoint.kind);
				int bytesSent = out
					? out->Send(connection->endpoint,
						serializedData.data(), serializedData.size(), false)
					: -1;

				if (bytesSent >= 0)
				{
					pendingPkt.timeSent = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now().time_since_epoch());
					pendingPkt.retransmissionCount++;
					stats.packetsRetransmitted++;
					std::cout << "NETWORK: Retransmitted packet seq=" << nakedSeq << " due to NAK" << std::endl;
				}
			}

			return true;
		}

		return false;
	}

	/*************************************************************************
	  \brief
		Finds the PlayerID associated with a peer address.
	  \param endpoint
		Peer address from the received datagram.
	  \return
		PlayerID (1 to MAX_PLAYERS-1) if the endpoint matches a known
		connection, 0 if it does not.
	  \details
		Single hash lookup against endpointToPlayer, which AddConnection and
		RemoveConnection keep in step with the connections map under the same
		mutex. This used to be a linear scan of every connection, executed on
		every received packet - wasted work that grew with the player count on
		the hottest path in the layer.

		Used by ReceivePacket to correlate incoming packets with established
		connections, and it is where the server's authoritative notion of "who
		sent this" comes from - the identity later reconciled against the
		clientID an RPC claims.
	  \note
		Thread-safe via shared_lock (allows concurrent reads).
	  \warning
		Does not handle NAT rebinding on the IPv4 path (a client whose IP or
		port changes mid-session becomes a stranger). Session tokens would fix
		it; the Steam path sidesteps it, since identity there is the connection
		handle rather than an address.
	  \see
		ReceivePacket, AddConnection, ClientConnection, NetEndpoint
	*************************************************************************/
	PlayerID ReliabilityLayer::FindPlayerByEndpoint(NetEndpoint const& endpoint) const
	{
		std::shared_lock lock(connectionsMutex);

		auto it = endpointToPlayer.find(endpoint);
		if (it == endpointToPlayer.end()) return 0;

		return it->second;
	}

	/*************************************************************************
	  \brief
		Notes peers the transport reported as gone, for the main thread to act on.
	  \details
		Steam knows within a frame that a peer closed or became unreachable and
		says so; Winsock cannot, and its DrainClosedPeers is a no-op. Surfacing
		those reports means a host sees an alt-F4 immediately instead of waiting
		out the ten-second HEARTBEAT_TIMEOUT_US.

		Deliberately does NOT call RemoveConnection here. This runs on the I/O
		thread, and removing the connection would leave GameServer with nothing
		to notice on its next Update - so Lua's ServerRPC_OnClientDisconnected
		would never fire and the game would keep a phantom player forever.
		Instead the PlayerID is queued and the main thread does the removal
		along with the Lua notification, exactly as the heartbeat-timeout path
		already does.
	  \note
		Called from the I/O thread each iteration.
	  \see
		INetTransport::DrainClosedPeers, DrainTransportDisconnects
	*************************************************************************/
	void ReliabilityLayer::ProcessTransportDisconnects()
	{
		std::vector<NetEndpoint> closedPeers;

		size_t drained = 0;
		for (auto const& openTransport : transports)
		{
			drained += openTransport->DrainClosedPeers(closedPeers);
		}

		if (drained == 0) return;

		for (NetEndpoint const& endpoint : closedPeers)
		{
			PlayerID const playerID = FindPlayerByEndpoint(endpoint);
			if (playerID == 0) continue;   // never completed a handshake

			std::cout << "NETWORK: transport reported peer " << endpoint.ToString()
				<< " (player " << playerID << ") disconnected" << std::endl;

			std::unique_lock lock(transportDisconnectsMutex);
			transportDisconnects.push_back(playerID);
		}
	}

	size_t ReliabilityLayer::DrainTransportDisconnects(std::vector<PlayerID>& out)
	{
		std::unique_lock lock(transportDisconnectsMutex);

		if (transportDisconnects.empty()) return 0;

		size_t const count = transportDisconnects.size();
		out.insert(out.end(), transportDisconnects.begin(), transportDisconnects.end());
		transportDisconnects.clear();

		return count;
	}

	/*************************************************************************
	  \brief
		Registers new client connection with assigned identifiers.
	  \param id
		Unique PlayerID assigned by server (from availablePlayerIDs queue).
	  \param endpoint
		Peer address for packet routing (from the received datagram).
	  \param sessionID
		Session token for reconnection validation (currently unused).
	  \return
		true if connection added successfully, false if PlayerID already exists.
	  \details
		Connection registration process:
		1. Creates ClientConnection instance with provided parameters
		2. Acquires unique_lock on connectionsMutex
		3. Checks if id already exists in connections map (prevents overwrites)
		4. Inserts std::unique_ptr<ClientConnection> into map
		5. Initializes connection state:
		   - nextSendSequence = 1
		   - expectedReceiveSequence = 1
		   - lastHeartbeat/lastActivity = current time
		   - isConnected = false (set to true after handshake)

		Called by ReceivePacket upon receiving CLIENT_CONNECT_REQUEST message.
		PlayerID is allocated from availablePlayerIDs queue (IDs are reused
		after disconnection).
	  \note
		Thread-safe via unique_lock (exclusive write access).
		Does not send CONNECT_RESPONSE packet - caller must do this explicitly.
		Also inserts into endpointToPlayer, the reverse index
		FindPlayerByEndpoint reads; the two are only ever updated together,
		under this mutex.
	  \warning
		Rejects an endpoint that already maps to a different PlayerID rather
		than letting the second registration silently shadow the first, which
		would strand the original connection with no route back to it.
	  \see
		ReceivePacket, FindPlayerByEndpoint, ClientConnection
	*************************************************************************/
	bool ReliabilityLayer::AddConnection(PlayerID id, NetEndpoint const& endpoint, SessionID sessionID)
	{
		std::unique_lock lock(connectionsMutex);

		auto it = connections.find(id);
		if (it != connections.end()) return false;

		// One endpoint, one player. Without this the reverse index and the
		// connection map disagree, and packets from the shadowed peer resolve
		// to the wrong PlayerID - which is a spoofing primitive, not just a bug.
		auto existing = endpointToPlayer.find(endpoint);
		if (existing != endpointToPlayer.end())
		{
			std::cerr << "NETWORK: endpoint " << endpoint.ToString()
				<< " already registered to player " << existing->second
				<< ", refusing to also map it to " << id << std::endl;
			return false;
		}

		ClientConnection client;
		client.playerID = id;
		client.sessionID = sessionID;
		client.endpoint = endpoint;
		client.lastHeartbeat = std::chrono::steady_clock::now();
		client.lastActivity = std::chrono::steady_clock::now();

		connections[id] = std::make_unique<ClientConnection>(client);
		endpointToPlayer[endpoint] = id;

		return true;
	}

	bool ReliabilityLayer::RemoveConnection(PlayerID id)
	{
		std::unique_lock lock(connectionsMutex);
		auto it = connections.find(id);
		if (it == connections.end()) return false;

		endpointToPlayer.erase(it->second->endpoint);
		connections.erase(it);

		if (id != 0)
		{
			availablePlayerIDs.push(id);
		}
		return true;
	}
}
