/*************************************************************************
  \file       INetTransport.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Datagram transport interface separating the reliability layer from the
	socket implementation underneath it.

  \details
	ReliabilityLayer implements sequencing, ACK/NAK, adaptive RTO, congestion
	measurement, fragmentation and duplicate suppression. None of that cares
	whether the bytes travel over a Winsock UDP socket or over Steam's P2P
	relay network - but the code used to call sendto/recvfrom directly, which
	welded it to Winsock.

	INetTransport is the seam. It is deliberately datagram-shaped, not
	stream-shaped: one Send is one Receive on the far side, out-of-order
	delivery and loss are both permitted, and there is no framing. That is
	exactly the contract raw UDP already provides, and exactly the contract
	Steam's unreliable message channel provides, so the reliability layer runs
	unmodified on both.

	Two backends exist:
	  - WinsockTransport  a mechanical extraction of the previous socket code
	  - SteamTransport    ISteamNetworkingSockets P2P, built only when the
	                      Steamworks SDK is present (GG_STEAM_ENABLED)

	The choice is made at RUNTIME, not compile time. That keeps the no-Steam
	loopback path available for day-to-day iteration (no Steam client, no
	login, instant restart) and doubles as the deadline kill-switch: force
	TransportKind::Winsock and the LAN path ships.

  \dependencies
	NetEndpoint.hpp, <cstdint>, <memory>, <vector>

  \see
	WinsockTransport, SteamTransport, ReliabilityLayer

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <NetEndpoint.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace GGNetworking
{
	/*************************************************************************
	  \struct ReceivedDatagram
	  \brief
		One datagram lifted off the wire, with the peer that sent it.
	*************************************************************************/
	struct ReceivedDatagram
	{
		NetEndpoint          from{};
		std::vector<uint8_t> data;
	};

	/*************************************************************************
	  \brief
		Which backend a transport instance wraps.
	*************************************************************************/
	enum class TransportKind : uint8_t
	{
		Winsock = 0,    // raw UDP over Winsock; LAN / DDNS / port forwarding
		Steam   = 1     // ISteamNetworkingSockets P2P with SDR relay
	};

	/*************************************************************************
	  \class INetTransport
	  \brief
		Abstract datagram transport used by ReliabilityLayer.
	  \details
		Threading contract, and it is not optional:
		  - OpenListen / OpenConnect / Close are called from the MAIN thread,
			before Start() spawns the I/O thread or after Stop() joins it.
		  - Send, Receive and Poll are called from the I/O thread only.
		  - A backend whose SDK requires main-thread servicing (Steam does)
			must do that servicing in ServiceMainThread(), never in Poll().
	  \see
		WinsockTransport, SteamTransport
	*************************************************************************/
	class INetTransport
	{
	public:
		virtual ~INetTransport() = default;

		/*************************************************************************
		  \brief
			Which backend this instance is.
		*************************************************************************/
		virtual TransportKind GetKind() const = 0;

		/*************************************************************************
		  \brief
			Prepares the transport to accept inbound peers (host / server side).
		  \param[in] port
			UDP port for the Winsock backend. Ignored by SteamTransport, which
			listens on a virtual port over the relay network.
		  \return
			true on success.
		*************************************************************************/
		virtual bool OpenListen(uint16_t port) = 0;

		/*************************************************************************
		  \brief
			Prepares the transport to talk to one remote host (client side).
		  \param[in] to
			The host to reach. IPV4 endpoint for Winsock, STEAM endpoint
			carrying the host's SteamID64 for Steam.
		  \return
			true if the connection attempt was started. Completion is
			asynchronous on both backends; the application-level
			CLIENT_CONNECT_REQUEST handshake is what actually confirms it.
		*************************************************************************/
		virtual bool OpenConnect(NetEndpoint const& to) = 0;

		/*************************************************************************
		  \brief
			Releases every resource the transport owns.
		  \details
			Must be idempotent - ReliabilityLayer::Stop may run twice.
		*************************************************************************/
		virtual void Close() = 0;

		/*************************************************************************
		  \brief
			Sends one datagram.
		  \param[in] to
			Destination peer.
		  \param[in] data
			Bytes to send (an already-serialized Packet).
		  \param[in] size
			Byte count. Must not exceed MAX_DATAGRAM_SIZE.
		  \param[in] reliable
			A HINT the backend may ignore. Winsock always ignores it: UDP has
			no reliable mode, and the reliability layer above is the one doing
			that work. Steam honours it only when reliability has been
			delegated (see SteamTransport::SetDelegateReliability); by default
			it sends unreliable + no-Nagle so the existing ACK/RTO machinery
			keeps running and stays demonstrable.
		  \return
			Bytes sent, or -1 on failure.
		*************************************************************************/
		virtual int Send(NetEndpoint const& to,
			uint8_t const* data, size_t size,
			bool reliable) = 0;

		/*************************************************************************
		  \brief
			Non-blocking receive of one datagram.
		  \param[out] out
			Filled in when a datagram was pending.
		  \return
			false when nothing is pending. Never blocks.
		*************************************************************************/
		virtual bool Receive(ReceivedDatagram& out) = 0;

		/*************************************************************************
		  \brief
			Called once per I/O-thread iteration when Receive came up empty.
		  \param[in] timeoutMicros
			How long the transport may block waiting for readability.
		  \details
			Winsock: select() with this timeout, so the thread sleeps until a
			datagram lands instead of spinning.
			Steam: a short sleep. It must NOT call SteamAPI_RunCallbacks -
			that belongs on the main thread. See ServiceMainThread.
		*************************************************************************/
		virtual void Poll(uint32_t timeoutMicros) = 0;

		/*************************************************************************
		  \brief
			Main-thread servicing hook, called once per frame.
		  \details
			Winsock: no-op.
			Steam: also a no-op, because its pump is process-wide rather than
			per-transport. SteamPlatform::RunCallbacks() is called once a frame
			by the engine whether or not a session exists, since lobby
			callbacks have to arrive before any transport is created. One pump,
			one thread, one call site. The hook stays for backends that do need
			main-thread servicing of their own.
		*************************************************************************/
		virtual void ServiceMainThread() {}

		/*************************************************************************
		  \brief
			Drains peers the transport itself decided are gone.
		  \param[out] out
			Endpoints whose underlying connection closed.
		  \return
			Number appended to out.
		  \details
			Winsock has no such notion - UDP peers just stop answering, and the
			existing heartbeat timeout handles them - so it always returns 0.
			Steam reports ClosedByPeer / ProblemDetectedLocally promptly, which
			is how a host learns a client alt-F4'd without waiting ten seconds
			for the heartbeat to lapse.
		*************************************************************************/
		virtual size_t DrainClosedPeers(std::vector<NetEndpoint>& out) { (void)out; return 0; }

		/*************************************************************************
		  \brief
			The peer OpenConnect actually established, client side.
		  \return
			An INVALID endpoint on a listening transport, or before
			OpenConnect has run.
		  \details
			Needed because the caller does not know the endpoint it will end up
			with. Winsock returns what was passed in - the resolved address is
			the identity. Steam cannot: OpenConnect is handed a SteamID, and
			the identity that matters afterwards is the HSteamNetConnection
			handle the SDK assigned, which only exists once ConnectP2P has run.
			GameClient registers whatever comes back here as its server
			connection.
		*************************************************************************/
		virtual NetEndpoint GetRemoteEndpoint() const { return NetEndpoint{}; }

		/*************************************************************************
		  \brief
			Last human-readable failure, for surfacing to the UI and logs.
		*************************************************************************/
		virtual char const* GetLastErrorText() const { return ""; }
	};

	/*************************************************************************
	  \brief
		Creates a transport of the requested kind.
	  \param[in] kind
		Backend to instantiate.
	  \return
		Owning pointer, or nullptr when the backend is unavailable in this
		build (asking for Steam without GG_STEAM_ENABLED, for instance).
	  \note
		Does not open anything. Call OpenListen or OpenConnect next.
	*************************************************************************/
	std::unique_ptr<INetTransport> CreateTransport(TransportKind kind);

	/*************************************************************************
	  \brief
		Whether the Steam backend was compiled into this build at all.
	  \details
		Compile-time answer only. Whether Steam is USABLE right now - client
		running, user logged in, SteamAPI_Init succeeded - is a separate
		runtime question answered by SteamPlatform::IsAvailable().
	*************************************************************************/
	bool IsSteamTransportCompiledIn();

	/*************************************************************************
	  \brief
		Reads the transport every new GameServer / GameClient will use.
	*************************************************************************/
	TransportKind GetPreferredTransport();

	/*************************************************************************
	  \brief
		Chooses the transport for subsequently created sessions.
	  \param[in] kind
		Backend to use.
	  \return
		The kind actually in effect. Requesting Steam in a build without it,
		or before SteamPlatform::Init() succeeded, falls back to Winsock and
		says so in the log rather than failing later at connect time.
	  \note
		Has no effect on a session that is already running. Selection is read
		once, when the transport is created.
	*************************************************************************/
	TransportKind SetPreferredTransport(TransportKind kind);

	/*************************************************************************
	  \brief
		Human-readable name of a transport kind, for logs and Lua.
	*************************************************************************/
	char const* TransportKindName(TransportKind kind);
}
