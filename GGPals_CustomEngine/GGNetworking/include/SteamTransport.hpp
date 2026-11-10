/*************************************************************************
  \file       SteamTransport.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	ISteamNetworkingSockets P2P backend for INetTransport.

  \details
	Carries the project's own Packets over Steam's peer-to-peer message
	channel. The interesting property is what it does NOT do: it does not
	replace the reliability layer. Packets go out as UNRELIABLE, no-Nagle
	messages, so sequencing, ACK/NAK, adaptive RTO, congestion measurement,
	duplicate suppression and fragmentation all keep running exactly as they
	do over Winsock. Steam supplies what the custom layer genuinely cannot:
	NAT traversal, Valve's SDR relay network, authentication and encryption.

	SetDelegateReliability(true) flips reliable Packets onto Steam's own
	reliable channel instead. That retransmits sooner than a min-20ms RTO
	riding on top of a relay hop, so it is the better choice for a shipped
	build - at the cost of the custom reliability layer no longer executing
	on that path, which matters when the layer is a graded artifact. Off by
	default for that reason; it is a deliberate, reversible switch.

  \threading
	This is the part that goes wrong quietly, so it is spelled out:

	  - SteamAPI_RunCallbacks() runs on the MAIN thread only, once per frame,
		from SteamPlatform::RunCallbacks(). Every connection-state callback in
		this class is delivered from inside it, therefore on the main thread.
	  - Poll() and Receive() run on the I/O thread. Receive() calls only
		ReceiveMessagesOnPollGroup and SendMessageToConnection, both of which
		Valve documents as thread-safe.
	  - The two threads meet at eventMutex, which guards the closed-peer
		queue the callbacks fill and the I/O thread drains.

	Calling RunCallbacks from both threads corrupts SDK state, and the symptom
	is intermittent, plausible-looking packet loss. If loss ever appears
	without a matching cause, suspect this first.

  \dependencies
	INetTransport.hpp, Steamworks SDK, <mutex>, <unordered_map>, <vector>

  \see
	INetTransport, SteamPlatform, SteamLobbyManager

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <INetTransport.hpp>

#ifdef GG_STEAM_ENABLED

#include <steam/steam_api.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace GGNetworking
{
	/*************************************************************************
	  \class SteamTransport
	  \brief
		Steam P2P datagram transport.
	  \see
		INetTransport, WinsockTransport
	*************************************************************************/
	class SteamTransport final : public INetTransport
	{
	public:
		SteamTransport();
		~SteamTransport() override;

		SteamTransport(SteamTransport const&)            = delete;
		SteamTransport& operator=(SteamTransport const&) = delete;

		TransportKind GetKind() const override { return TransportKind::Steam; }

		/*************************************************************************
		  \brief
			Opens a P2P listen socket (host side).
		  \param[in] port
			Ignored. Steam P2P has no UDP port; peers are addressed by SteamID
			on a virtual port, and the relay decides the actual route.
		  \return
			true when the listen socket and poll group were created.
		*************************************************************************/
		bool OpenListen(uint16_t port) override;

		/*************************************************************************
		  \brief
			Starts a P2P connection to the host (client side).
		  \param[in] to
			Must be Kind::STEAM with steamID set to the host's SteamID64,
			typically read from the lobby's host_steamid metadata.
		  \return
			true when the connection attempt started. Completion arrives later
			via the status callback; the CLIENT_CONNECT_REQUEST handshake above
			is what actually confirms an end-to-end path.
		  \warning
			You cannot ConnectP2P to your own SteamID. Two accounts on two
			Steam clients are required for any real P2P test.
		*************************************************************************/
		bool OpenConnect(NetEndpoint const& to) override;

		void Close() override;

		int  Send(NetEndpoint const& to, uint8_t const* data, size_t size, bool reliable) override;
		bool Receive(ReceivedDatagram& out) override;

		/*************************************************************************
		  \brief
			I/O-thread idle step.
		  \param[in] timeoutMicros
			How long to sleep when no message was pending.
		  \details
			Deliberately just a sleep. Steam has no readability primitive to
			select() on, and it must NOT pump callbacks here - see the file's
			threading note.
		*************************************************************************/
		void Poll(uint32_t timeoutMicros) override;

		/*************************************************************************
		  \brief
			Main-thread step. Nothing to do here.
		  \details
			The pump is SteamPlatform::RunCallbacks(), called once per frame by
			the engine whether or not a session is running - lobby callbacks
			need it before any transport exists. One pump, one thread, one
			call site.
		*************************************************************************/
		void ServiceMainThread() override {}

		size_t DrainClosedPeers(std::vector<NetEndpoint>& out) override;

		NetEndpoint GetRemoteEndpoint() const override { return hostEndpoint; }

		char const* GetLastErrorText() const override { return lastError.c_str(); }

		/*************************************************************************
		  \brief
			Routes reliable Packets onto Steam's reliable channel.
		  \param[in] delegate
			true to hand reliability to Valve, false (default) to keep the
			project's own layer doing the work.
		  \details
			Call before Open*. See the class note for the trade-off.
		*************************************************************************/
		void SetDelegateReliability(bool delegate) { delegateReliability = delegate; }

		/*************************************************************************
		  \brief
			Number of peers currently connected through this transport.
		*************************************************************************/
		size_t GetConnectionCount() const;

		/*************************************************************************
		  \brief
			Copies Steam's own view of a connection's health.
		  \param[in] endpoint
			Peer to query. Must be Kind::STEAM.
		  \param[out] pingMs
			Round-trip estimate as Valve measures it.
		  \param[out] connectionQualityLocal
			0..1 fraction of packets Valve believes arrived.
		  \param[out] outPacketsPerSec
			Send rate.
		  \return
			false when the connection is unknown or Steam is unavailable.
		  \details
			Worth showing beside the project's own ReliabilityStats in the
			debug panel. When both sets of numbers are visible it is
			immediately obvious which layer owns a problem, which otherwise
			takes an afternoon to work out.
		*************************************************************************/
		bool GetRealTimeStatus(NetEndpoint const& endpoint,
			int& pingMs, float& connectionQualityLocal, float& outPacketsPerSec) const;

	private:
		/*************************************************************************
		  \brief
			Connection-state callback. Main thread, from RunCallbacks().
		  \param[in] info
			The state change Steam is reporting.
		  \details
			Host: accepts incoming peers, but only after SteamPlatform's peer
			authorizer agrees the SteamID belongs in the lobby.
			Both: turns closure into a queued endpoint the I/O thread drains,
			which is how the layers above see a prompt disconnect instead of
			waiting out the ten-second heartbeat timeout.
		*************************************************************************/
		void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);

		// CCallback rather than the STEAM_CALLBACK macro: that macro locates
		// its owner with offsetof, which is undefined on a polymorphic class,
		// and INetTransport gives this one a vtable. CCallback takes the
		// object pointer directly and sidesteps the whole question.
		CCallback<SteamTransport, SteamNetConnectionStatusChangedCallback_t> connectionStatusCallback;

		/*************************************************************************
		  \brief
			Forgets a connection and records it for DrainClosedPeers.
		  \param[in] connection
			Handle being retired.
		  \param[in] steamID
			Peer identity, for the queued endpoint and the log line.
		  \param[in] reason
			Human-readable cause.
		*************************************************************************/
		void RetireConnection(HSteamNetConnection connection, uint64_t steamID, char const* reason);

		static constexpr int GG_VIRTUAL_PORT = 0;   // our P2P channel on the SteamID

		HSteamListenSocket  listenSocket{ k_HSteamListenSocket_Invalid };
		HSteamNetPollGroup  pollGroup{ k_HSteamNetPollGroup_Invalid };
		HSteamNetConnection hostConnection{ k_HSteamNetConnection_Invalid };

		NetEndpoint hostEndpoint{};     // client side: the host we dialled
		bool        isHost{ false };
		bool        delegateReliability{ false };

		// Live connections, so Send can validate a handle and the debug panel
		// can enumerate peers. Guarded by eventMutex.
		std::unordered_map<uint32_t, uint64_t> connectionToSteamID;

		// Filled by the main-thread callback, drained by the I/O thread.
		std::vector<NetEndpoint> closedPeers;

		mutable std::mutex eventMutex;
		std::string        lastError;
	};
}

#endif  // GG_STEAM_ENABLED
