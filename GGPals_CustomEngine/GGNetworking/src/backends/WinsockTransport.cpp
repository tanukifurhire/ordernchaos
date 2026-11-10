/*************************************************************************
  \file       WinsockTransport.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Winsock UDP backend for INetTransport.

  \details
	Mechanical extraction of the socket code that used to live inline in
	ReliabilityLayer, GameServer::Init and GameClient::Init. No behavioural
	change is intended: if this file does something the old code did not, it
	is a bug in the extraction, not a design decision.

  \dependencies
	WinsockTransport.hpp, <iostream>

  \see
	WinsockTransport.hpp, ReliabilityLayer

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <WinsockTransport.hpp>

#include <iostream>

namespace GGNetworking
{
	WinsockTransport::~WinsockTransport()
	{
		Close();
	}

	bool WinsockTransport::CreateAndBind(uint16_t port)
	{
		if (socketHandle != INVALID_SOCKET)
		{
			lastError = "Transport already open";
			return false;
		}

		socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (socketHandle == INVALID_SOCKET)
		{
			lastError = "socket() failed, WSA error "
				+ std::to_string(WSAGetLastError());
			std::cerr << "WinsockTransport: " << lastError << std::endl;
			return false;
		}

		// Non-blocking: the I/O thread must never park inside recvfrom, or
		// Stop() cannot join it promptly.
		u_long nonBlocking = 1;
		if (ioctlsocket(socketHandle, FIONBIO, &nonBlocking) != 0)
		{
			lastError = "ioctlsocket(FIONBIO) failed, WSA error "
				+ std::to_string(WSAGetLastError());
			std::cerr << "WinsockTransport: " << lastError << std::endl;
			closesocket(socketHandle);
			socketHandle = INVALID_SOCKET;
			return false;
		}

		sockaddr_in bindAddr{};
		bindAddr.sin_family      = AF_INET;
		bindAddr.sin_addr.s_addr = INADDR_ANY;
		bindAddr.sin_port        = htons(port);

		if (bind(socketHandle, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
		{
			lastError = "bind() to port " + std::to_string(port)
				+ " failed, WSA error " + std::to_string(WSAGetLastError());
			std::cerr << "WinsockTransport: " << lastError << std::endl;
			closesocket(socketHandle);
			socketHandle = INVALID_SOCKET;
			return false;
		}

		lastError.clear();
		return true;
	}

	bool WinsockTransport::OpenListen(uint16_t port)
	{
		if (!CreateAndBind(port)) return false;

		std::cout << "WinsockTransport: listening on UDP port "
			<< GetBoundPort() << std::endl;
		return true;
	}

	bool WinsockTransport::OpenConnect(NetEndpoint const& to)
	{
		if (to.kind != NetEndpoint::Kind::IPV4)
		{
			lastError = "OpenConnect requires an IPV4 endpoint; got "
				+ to.ToString();
			std::cerr << "WinsockTransport: " << lastError << std::endl;
			return false;
		}

		// Port 0: the OS picks an ephemeral source port, matching the old
		// GameClient::Init behaviour.
		if (!CreateAndBind(0)) return false;

		remoteEndpoint = to;

		std::cout << "WinsockTransport: bound to local port " << GetBoundPort()
			<< ", target " << to.ToString() << std::endl;
		return true;
	}

	void WinsockTransport::Close()
	{
		if (socketHandle != INVALID_SOCKET)
		{
			closesocket(socketHandle);
			socketHandle = INVALID_SOCKET;
		}

		remoteEndpoint = NetEndpoint{};
	}

	int WinsockTransport::Send(NetEndpoint const& to, uint8_t const* data, size_t size, bool /*reliable*/)
	{
		// `reliable` is deliberately unused. UDP has no reliable mode, and
		// ReliabilityLayer above this is precisely what supplies one.
		if (socketHandle == INVALID_SOCKET) return -1;

		if (to.kind != NetEndpoint::Kind::IPV4) return -1;

		sockaddr_in destination = to.ToSockaddr();

		int bytesSent = sendto(socketHandle,
			reinterpret_cast<char const*>(data),
			static_cast<int>(size),
			0,
			reinterpret_cast<sockaddr const*>(&destination),
			sizeof(destination));

		if (bytesSent == SOCKET_ERROR)
		{
			int const error = WSAGetLastError();

			// WSAEWOULDBLOCK: send buffer momentarily full - the caller
			// retransmits anyway. WSAECONNRESET: a prior datagram drew an ICMP
			// port-unreachable; on UDP that is routine and not fatal.
			if (error != WSAEWOULDBLOCK && error != WSAECONNRESET)
			{
				std::cerr << "WinsockTransport: sendto error " << error << std::endl;
			}
			return -1;
		}

		return bytesSent;
	}

	bool WinsockTransport::Receive(ReceivedDatagram& out)
	{
		if (socketHandle == INVALID_SOCKET) return false;

		char        buffer[MAX_DATAGRAM_SIZE];
		sockaddr_in fromAddr{};
		int         fromLen = sizeof(fromAddr);

		int bytesReceived = recvfrom(socketHandle, buffer,
			MAX_DATAGRAM_SIZE,
			0,
			reinterpret_cast<sockaddr*>(&fromAddr),
			&fromLen);

		if (bytesReceived == SOCKET_ERROR)
		{
			int const error = WSAGetLastError();

			if (error == WSAEWOULDBLOCK || error == WSAEINTR)  return false;  // no data
			if (error == WSAECONNRESET)                        return false;  // stale ICMP

			if (error == WSAEINVAL)
			{
				std::cerr << "WinsockTransport: recvfrom WSAEINVAL - socket params invalid!" << std::endl;
			}

			std::cerr << "WinsockTransport: recvfrom error " << error << std::endl;
			return false;
		}

		out.from = NetEndpoint::FromSockaddr(fromAddr);
		out.data.assign(reinterpret_cast<uint8_t const*>(buffer),
			reinterpret_cast<uint8_t const*>(buffer) + bytesReceived);

		return true;
	}

	void WinsockTransport::Poll(uint32_t timeoutMicros)
	{
		if (socketHandle == INVALID_SOCKET) return;

		fd_set readSet;
		FD_ZERO(&readSet);

		// MSVC warns about the C4127 constant-condition inside the FD_SET
		// macro expansion at /W4; the pragma keeps the extraction warning-clean.
	#pragma warning(push)
	#pragma warning(disable: 4127)
		FD_SET(socketHandle, &readSet);
	#pragma warning(pop)

		timeval timeout{};
		timeout.tv_sec  = static_cast<long>(timeoutMicros / 1000000u);
		timeout.tv_usec = static_cast<long>(timeoutMicros % 1000000u);

		// First argument is ignored on Winsock.
		select(0, &readSet, nullptr, nullptr, &timeout);
	}

	uint16_t WinsockTransport::GetBoundPort() const
	{
		if (socketHandle == INVALID_SOCKET) return 0;

		sockaddr_in boundAddr{};
		int         addrLen = sizeof(boundAddr);

		if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&boundAddr), &addrLen) != 0)
		{
			return 0;
		}

		return ntohs(boundAddr.sin_port);
	}
}
