/*************************************************************************
  \file       WinsockTransport.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Winsock UDP backend for INetTransport.

  \details
	This is the transport the project shipped with, lifted out of
	ReliabilityLayer, GameServer::Init and GameClient::Init and put behind the
	interface. Behaviour is unchanged on purpose: same non-blocking socket,
	same select() wait, same WSAEWOULDBLOCK / WSAECONNRESET handling, same
	MAX_DATAGRAM_SIZE receive buffer.

	Ownership moved, though. The socket used to be created by GameServer /
	GameClient and merely handed to ReliabilityLayer, which then closed a
	handle it did not own. Now the transport creates it, owns it and closes
	it, and the layers above never see a SOCKET at all.

  \dependencies
	INetTransport.hpp, PlatformWindows.hpp, UDPTypes.hpp

  \see
	INetTransport, SteamTransport, ReliabilityLayer

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <INetTransport.hpp>
#include <PlatformWindows.hpp>
#include <UDPTypes.hpp>

#include <string>

namespace GGNetworking
{
	/*************************************************************************
	  \class WinsockTransport
	  \brief
		Raw UDP datagram transport over a single non-blocking Winsock socket.
	  \details
		One socket serves both directions, exactly as before: the server binds
		it to DEFAULT_SERVER_PORT, the client binds it to an ephemeral port and
		remembers the server address. `reliable` is ignored - UDP has no such
		mode, and ReliabilityLayer above is what provides it.
	  \see
		INetTransport
	*************************************************************************/
	class WinsockTransport final : public INetTransport
	{
	public:
		WinsockTransport() = default;
		~WinsockTransport() override;

		WinsockTransport(WinsockTransport const&)            = delete;
		WinsockTransport& operator=(WinsockTransport const&) = delete;

		TransportKind GetKind() const override { return TransportKind::Winsock; }

		/*************************************************************************
		  \brief
			Creates the socket and binds it to a fixed port (host side).
		  \param[in] port
			Port to bind. INADDR_ANY on every interface.
		  \return
			true when both socket creation and bind succeeded.
		*************************************************************************/
		bool OpenListen(uint16_t port) override;

		/*************************************************************************
		  \brief
			Creates the socket, binds an ephemeral port, remembers the peer.
		  \param[in] to
			Server endpoint. Must be Kind::IPV4 - the caller resolves the
			hostname, because DNS is an application concern, not a transport one.
		  \return
			true when socket creation and bind succeeded. UDP has no connect
			handshake, so this cannot tell you the server is actually there;
			the CLIENT_CONNECT_REQUEST exchange above does that.
		*************************************************************************/
		bool OpenConnect(NetEndpoint const& to) override;

		void Close() override;

		int  Send(NetEndpoint const& to, uint8_t const* data, size_t size, bool reliable) override;
		bool Receive(ReceivedDatagram& out) override;

		/*************************************************************************
		  \brief
			select() on the socket so the I/O thread sleeps instead of spinning.
		  \param[in] timeoutMicros
			Maximum wait.
		*************************************************************************/
		void Poll(uint32_t timeoutMicros) override;

		NetEndpoint GetRemoteEndpoint() const override { return remoteEndpoint; }

		char const* GetLastErrorText() const override { return lastError.c_str(); }

		/*************************************************************************
		  \brief
			The port this socket actually bound to.
		  \return
			Host byte order. Useful after OpenConnect, which requests port 0
			and lets the OS pick.
		*************************************************************************/
		uint16_t GetBoundPort() const;

	private:
		/*************************************************************************
		  \brief
			Shared socket creation: SOCK_DGRAM, non-blocking, bound.
		  \param[in] port
			Port to bind, 0 for OS-assigned.
		  \return
			true on success; records lastError and cleans up on failure.
		*************************************************************************/
		bool CreateAndBind(uint16_t port);

		SOCKET      socketHandle{ INVALID_SOCKET };

		// Client side only: the server address OpenConnect was given. On the
		// Winsock path the resolved address IS the peer identity, so this is
		// simply what was passed in.
		NetEndpoint remoteEndpoint{};

		std::string lastError;
	};
}
