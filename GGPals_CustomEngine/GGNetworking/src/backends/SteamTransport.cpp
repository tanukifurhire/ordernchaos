/*************************************************************************
  \file       SteamTransport.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	ISteamNetworkingSockets P2P backend for INetTransport.

  \details
	Compiles to nothing without GG_STEAM_ENABLED, so a checkout with no
	Steamworks SDK still builds and still plays over the Winsock path.

  \dependencies
	SteamTransport.hpp, SteamPlatform.hpp, Steamworks SDK

  \see
	SteamTransport.hpp, SteamPlatform, INetTransport

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <SteamTransport.hpp>

#ifdef GG_STEAM_ENABLED

#include <SteamPlatform.hpp>
#include <UDPTypes.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace GGNetworking
{
	SteamTransport::SteamTransport()
		// Registering here hooks the SDK dispatcher, so from this point on
		// OnConnectionStatusChanged fires from inside SteamAPI_RunCallbacks -
		// main thread, once a frame, and nowhere else.
		: connectionStatusCallback(this, &SteamTransport::OnConnectionStatusChanged)
	{
	}

	SteamTransport::~SteamTransport()
	{
		Close();
	}

	bool SteamTransport::OpenListen(uint16_t /*port*/)
	{
		// The UDP port is meaningless here. Steam P2P addresses a peer by
		// SteamID on a virtual port and lets the relay pick the route, which
		// is the entire reason this backend exists - no port forwarding, no
		// DDNS name, no open inbound firewall rule.
		if (!SteamPlatform::IsAvailable() || !SteamNetworkingSockets())
		{
			lastError = "Steam is not initialised; call Steam.init() first";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			return false;
		}

		SteamNetworkingUtils()->InitRelayNetworkAccess();

		pollGroup = SteamNetworkingSockets()->CreatePollGroup();
		if (pollGroup == k_HSteamNetPollGroup_Invalid)
		{
			lastError = "CreatePollGroup failed";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			return false;
		}

		listenSocket = SteamNetworkingSockets()->CreateListenSocketP2P(GG_VIRTUAL_PORT, 0, nullptr);
		if (listenSocket == k_HSteamListenSocket_Invalid)
		{
			lastError = "CreateListenSocketP2P failed";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			SteamNetworkingSockets()->DestroyPollGroup(pollGroup);
			pollGroup = k_HSteamNetPollGroup_Invalid;
			return false;
		}

		isHost = true;
		lastError.clear();

		std::cout << "SteamTransport: listening for P2P peers as "
			<< SteamPlatform::GetSteamIDString() << std::endl;
		return true;
	}

	bool SteamTransport::OpenConnect(NetEndpoint const& to)
	{
		if (!SteamPlatform::IsAvailable() || !SteamNetworkingSockets())
		{
			lastError = "Steam is not initialised; call Steam.init() first";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			return false;
		}

		if (to.kind != NetEndpoint::Kind::STEAM || to.steamID == 0)
		{
			lastError = "OpenConnect requires a STEAM endpoint carrying the host SteamID64";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			return false;
		}

		// Connecting to yourself does not work over Steam P2P, and the failure
		// mode is a connection that simply never completes. Say so plainly
		// instead of letting it look like a network problem.
		if (to.steamID == SteamPlatform::GetSteamID64())
		{
			lastError = "Cannot connect to your own SteamID - Steam P2P has no loopback. "
				"Use two accounts, or switch to the Winsock transport for local testing.";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			return false;
		}

		SteamNetworkingUtils()->InitRelayNetworkAccess();

		pollGroup = SteamNetworkingSockets()->CreatePollGroup();
		if (pollGroup == k_HSteamNetPollGroup_Invalid)
		{
			lastError = "CreatePollGroup failed";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			return false;
		}

		SteamNetworkingIdentity hostIdentity{};
		hostIdentity.SetSteamID64(to.steamID);

		hostConnection = SteamNetworkingSockets()->ConnectP2P(hostIdentity, GG_VIRTUAL_PORT, 0, nullptr);

		if (hostConnection == k_HSteamNetConnection_Invalid)
		{
			lastError = "ConnectP2P to " + std::to_string(to.steamID) + " failed";
			std::cerr << "SteamTransport: " << lastError << std::endl;
			SteamNetworkingSockets()->DestroyPollGroup(pollGroup);
			pollGroup = k_HSteamNetPollGroup_Invalid;
			return false;
		}

		SteamNetworkingSockets()->SetConnectionPollGroup(hostConnection, pollGroup);

		hostEndpoint = NetEndpoint::FromSteam(static_cast<uint32_t>(hostConnection), to.steamID);

		{
			std::lock_guard<std::mutex> guard(eventMutex);
			connectionToSteamID[static_cast<uint32_t>(hostConnection)] = to.steamID;
		}

		isHost = false;
		lastError.clear();

		std::cout << "SteamTransport: connecting to host " << to.steamID
			<< " (conn " << hostConnection << ")" << std::endl;
		return true;
	}

	void SteamTransport::Close()
	{
		if (!SteamNetworkingSockets())
		{
			// Steam already shut down under us; the handles are stale but
			// there is nothing left to release them with.
			listenSocket   = k_HSteamListenSocket_Invalid;
			pollGroup      = k_HSteamNetPollGroup_Invalid;
			hostConnection = k_HSteamNetConnection_Invalid;
			return;
		}

		{
			std::lock_guard<std::mutex> guard(eventMutex);

			for (auto const& [connectionHandle, steamID] : connectionToSteamID)
			{
				(void)steamID;
				// Linger so anything already queued has a chance to flush -
				// notably the client's CLIENT_DISCONNECT_REQUEST, which is what
				// gives the far side a clean "player left" instead of a timeout.
				SteamNetworkingSockets()->CloseConnection(
					static_cast<HSteamNetConnection>(connectionHandle),
					k_ESteamNetConnectionEnd_App_Generic, "Session ended", true);
			}

			connectionToSteamID.clear();
			closedPeers.clear();
		}

		if (listenSocket != k_HSteamListenSocket_Invalid)
		{
			SteamNetworkingSockets()->CloseListenSocket(listenSocket);
			listenSocket = k_HSteamListenSocket_Invalid;
		}

		if (pollGroup != k_HSteamNetPollGroup_Invalid)
		{
			SteamNetworkingSockets()->DestroyPollGroup(pollGroup);
			pollGroup = k_HSteamNetPollGroup_Invalid;
		}

		hostConnection = k_HSteamNetConnection_Invalid;
		hostEndpoint   = NetEndpoint{};
		isHost         = false;
	}

	int SteamTransport::Send(NetEndpoint const& to, uint8_t const* data, size_t size, bool reliable)
	{
		if (!SteamNetworkingSockets())               return -1;
		if (to.kind != NetEndpoint::Kind::STEAM)     return -1;
		if (size == 0)                               return -1;

		// A Packet is at most MAX_DATAGRAM_SIZE, comfortably inside Steam's
		// per-message ceiling. The check is here so that if MAX_PACKET_SIZE is
		// ever raised, it fails loudly at the boundary rather than producing a
		// truncated message somebody has to bisect for.
		if (size > static_cast<size_t>(k_cbMaxSteamNetworkingSocketsMessageSizeSend))
		{
			std::cerr << "SteamTransport: datagram of " << size
				<< " bytes exceeds Steam's per-message limit" << std::endl;
			return -1;
		}

		// Default is unreliable + NoNagle: the project's own reliability layer
		// is doing sequencing, ACK/NAK and RTO, so Steam must not also queue,
		// coalesce or retransmit underneath it. Flip delegateReliability only
		// when handing that job to Valve on purpose.
		int flags = (reliable && delegateReliability)
			? (k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoNagle)
			: (k_nSteamNetworkingSend_Unreliable | k_nSteamNetworkingSend_NoNagle);

		EResult result = SteamNetworkingSockets()->SendMessageToConnection(
			static_cast<HSteamNetConnection>(to.steamConn),
			data, static_cast<uint32>(size), flags, nullptr);

		if (result != k_EResultOK)
		{
			// k_EResultNoConnection is routine while a peer is tearing down;
			// the status callback will retire it a frame or two later.
			if (result != k_EResultNoConnection && result != k_EResultInvalidParam)
			{
				std::cerr << "SteamTransport: SendMessageToConnection failed, EResult "
					<< static_cast<int>(result) << std::endl;
			}
			return -1;
		}

		return static_cast<int>(size);
	}

	bool SteamTransport::Receive(ReceivedDatagram& out)
	{
		// I/O thread. ReceiveMessagesOnPollGroup is documented thread-safe;
		// nothing else in this function touches the SDK.
		if (!SteamNetworkingSockets())               return false;
		if (pollGroup == k_HSteamNetPollGroup_Invalid) return false;

		SteamNetworkingMessage_t* message = nullptr;

		int received = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(pollGroup, &message, 1);
		if (received <= 0 || message == nullptr) return false;

		out.from = NetEndpoint::FromSteam(
			static_cast<uint32_t>(message->m_conn),
			message->m_identityPeer.GetSteamID64());

		uint8_t const* begin = static_cast<uint8_t const*>(message->m_pData);
		out.data.assign(begin, begin + message->m_cbSize);

		message->Release();
		return true;
	}

	void SteamTransport::Poll(uint32_t timeoutMicros)
	{
		// Deliberately just a sleep. There is no readability handle to wait on,
		// and pumping callbacks here would be the second RunCallbacks caller -
		// the exact mistake this backend's threading note warns about.
		std::this_thread::sleep_for(std::chrono::microseconds(timeoutMicros));
	}

	size_t SteamTransport::DrainClosedPeers(std::vector<NetEndpoint>& out)
	{
		std::lock_guard<std::mutex> guard(eventMutex);

		if (closedPeers.empty()) return 0;

		size_t const count = closedPeers.size();
		out.insert(out.end(), closedPeers.begin(), closedPeers.end());
		closedPeers.clear();

		return count;
	}

	size_t SteamTransport::GetConnectionCount() const
	{
		std::lock_guard<std::mutex> guard(eventMutex);
		return connectionToSteamID.size();
	}

	bool SteamTransport::GetRealTimeStatus(NetEndpoint const& endpoint,
		int& pingMs, float& connectionQualityLocal, float& outPacketsPerSec) const
	{
		if (!SteamNetworkingSockets())           return false;
		if (endpoint.kind != NetEndpoint::Kind::STEAM) return false;

		// GetConnectionRealTimeStatus / SteamNetConnectionRealTimeStatus_t are
		// the SDK 1.58+ spelling. On an older SDK these are called
		// GetQuickConnectionStatus / SteamNetworkingQuickConnectionStatus_t
		// with the same fields - if this is the only thing that fails to
		// compile, that is why, and renaming both is the whole fix.
		SteamNetConnectionRealTimeStatus_t status{};

		if (SteamNetworkingSockets()->GetConnectionRealTimeStatus(
			static_cast<HSteamNetConnection>(endpoint.steamConn),
			&status, 0, nullptr) != k_EResultOK)
		{
			return false;
		}

		pingMs                 = status.m_nPing;
		connectionQualityLocal = status.m_flConnectionQualityLocal;
		outPacketsPerSec       = status.m_flOutPacketsPerSec;

		return true;
	}

	void SteamTransport::RetireConnection(HSteamNetConnection connection, uint64_t steamID, char const* reason)
	{
		{
			std::lock_guard<std::mutex> guard(eventMutex);

			auto it = connectionToSteamID.find(static_cast<uint32_t>(connection));
			if (it == connectionToSteamID.end()) return;   // already retired

			if (steamID == 0) steamID = it->second;
			connectionToSteamID.erase(it);

			// Queue for the I/O thread. This is what lets the layers above see
			// a disconnect within a frame instead of waiting out the
			// ten-second heartbeat timeout.
			closedPeers.push_back(NetEndpoint::FromSteam(
				static_cast<uint32_t>(connection), steamID));
		}

		std::cout << "SteamTransport: connection " << connection
			<< " (steamID " << steamID << ") closed - " << reason << std::endl;

		if (connection == hostConnection) hostConnection = k_HSteamNetConnection_Invalid;
	}

	void SteamTransport::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
	{
		// MAIN THREAD. Delivered from inside SteamAPI_RunCallbacks.
		if (info == nullptr) return;

		HSteamNetConnection const connection = info->m_hConn;
		uint64_t const            peerSteamID = info->m_info.m_identityRemote.GetSteamID64();

		switch (info->m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_Connecting:
		{
			// Only the host fields inbound attempts. A client sees this state
			// for its own outbound connection and must not "accept" it.
			if (!isHost || info->m_info.m_hListenSocket != listenSocket) break;

			// Authorisation gate. Without it, anyone who learns the host's
			// SteamID can dial in directly, skip matchmaking entirely and land
			// straight on the RPC surface.
			if (!SteamPlatform::IsPeerAuthorized(peerSteamID))
			{
				std::cout << "SteamTransport: rejecting " << peerSteamID
					<< " - not a member of the current lobby" << std::endl;

				SteamNetworkingSockets()->CloseConnection(connection,
					k_ESteamNetConnectionEnd_App_Generic, "Not a lobby member", false);
				break;
			}

			EResult accepted = SteamNetworkingSockets()->AcceptConnection(connection);
			if (accepted != k_EResultOK)
			{
				std::cerr << "SteamTransport: AcceptConnection failed, EResult "
					<< static_cast<int>(accepted) << std::endl;
				SteamNetworkingSockets()->CloseConnection(connection,
					k_ESteamNetConnectionEnd_AppException_Generic, "Accept failed", false);
				break;
			}

			SteamNetworkingSockets()->SetConnectionPollGroup(connection, pollGroup);

			{
				std::lock_guard<std::mutex> guard(eventMutex);
				connectionToSteamID[static_cast<uint32_t>(connection)] = peerSteamID;
			}

			std::cout << "SteamTransport: accepted peer " << peerSteamID
				<< " (conn " << connection << ")" << std::endl;
			break;
		}

		case k_ESteamNetworkingConnectionState_Connected:
		{
			// Transport is up. Nothing to do: the application-level
			// CLIENT_CONNECT_REQUEST handshake runs on top of it unchanged,
			// which is what assigns a PlayerID and tells Lua.
			std::cout << "SteamTransport: connection " << connection
				<< " established with " << peerSteamID << std::endl;
			break;
		}

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		{
			RetireConnection(connection, peerSteamID, "closed by peer");
			SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
			break;
		}

		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		{
			// Covers relay failure, timeout and unroutable peers. The
			// end-debug string is the most useful diagnostic Steam gives you,
			// so it goes in the log verbatim.
			RetireConnection(connection, peerSteamID,
				info->m_info.m_szEndDebug[0] != '\0'
					? info->m_info.m_szEndDebug
					: "problem detected locally");

			SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
			break;
		}

		default:
			break;
		}
	}
}

#endif  // GG_STEAM_ENABLED
