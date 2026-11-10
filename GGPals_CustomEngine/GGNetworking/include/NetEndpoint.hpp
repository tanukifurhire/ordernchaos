/*************************************************************************
  \file       NetEndpoint.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Backend-agnostic peer address for the transport abstraction.

  \details
	Every layer above the socket used to identify a peer by sockaddr_in.
	Steam identifies a peer by an HSteamNetConnection handle (a uint32) plus a
	SteamID64; there is no IP or port to speak of, because traffic may be
	relayed through Valve's SDR network. NetEndpoint is the one type both
	backends can express, so ClientConnection, the connection map and the
	send/receive paths stop caring which transport is underneath.

	The type is deliberately trivially copyable and small (24 bytes) so it can
	live by value inside ClientConnection and be used as an unordered_map key
	without allocation.

	Equality is defined per-kind:
	  - IPV4  compares address and port. Two datagrams from the same host and
	          port are the same peer, matching the old FindPlayerByAddress.
	  - STEAM compares the connection handle only. The handle is the identity
	          Steam authenticated; steamID is carried alongside for logging,
	          the lobby-membership gate and future reconnect support, but is
	          not part of identity because one SteamID may legitimately hold
	          more than one connection during a reconnect.

  \dependencies
	PlatformWindows.hpp (sockaddr_in), <cstdint>, <functional>, <string>

  \see
	INetTransport, ClientConnection, ReliabilityLayer

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <PlatformWindows.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace GGNetworking
{
	/*************************************************************************
	  \struct NetEndpoint
	  \brief
		Transport-agnostic identity of a remote peer.
	  \details
		Discriminated by Kind. An INVALID endpoint never compares equal to
		anything, including another INVALID endpoint, so a default-constructed
		value cannot accidentally alias a real connection.
	  \see
		INetTransport, WinsockTransport, SteamTransport
	*************************************************************************/
	struct NetEndpoint
	{
		enum class Kind : uint8_t
		{
			INVALID = 0,
			IPV4    = 1,
			STEAM   = 2
		};

		Kind     kind{ Kind::INVALID };

		// --- IPV4 ---
		uint32_t ipv4{};        // network byte order, as sockaddr_in::sin_addr
		uint16_t port{};        // network byte order, as sockaddr_in::sin_port

		// --- STEAM ---
		uint32_t steamConn{};   // HSteamNetConnection handle
		uint64_t steamID{};     // SteamID64 of the peer, for logging / auth gate

		/*************************************************************************
		  \brief
			Whether this endpoint names a peer at all.
		*************************************************************************/
		bool IsValid() const { return kind != Kind::INVALID; }

		bool operator==(NetEndpoint const& other) const
		{
			if (kind != other.kind) return false;

			switch (kind)
			{
			case Kind::IPV4:  return ipv4 == other.ipv4 && port == other.port;
			case Kind::STEAM: return steamConn == other.steamConn;
			default:          return false;   // INVALID never matches
			}
		}

		bool operator!=(NetEndpoint const& other) const { return !(*this == other); }

		/*************************************************************************
		  \brief
			Builds an IPV4 endpoint from a Winsock address.
		  \param[in] addr
			sockaddr_in as produced by recvfrom or getaddrinfo.
		  \return
			Endpoint of Kind::IPV4.
		*************************************************************************/
		static NetEndpoint FromSockaddr(sockaddr_in const& addr)
		{
			NetEndpoint endpoint;
			endpoint.kind = Kind::IPV4;
			endpoint.ipv4 = addr.sin_addr.S_un.S_addr;
			endpoint.port = addr.sin_port;
			return endpoint;
		}

		/*************************************************************************
		  \brief
			Builds a STEAM endpoint from a connection handle and peer identity.
		  \param[in] connectionHandle
			HSteamNetConnection value, opaque to this layer.
		  \param[in] steamID64
			Peer's SteamID64, or 0 when not yet known.
		*************************************************************************/
		static NetEndpoint FromSteam(uint32_t connectionHandle, uint64_t steamID64)
		{
			NetEndpoint endpoint;
			endpoint.kind      = Kind::STEAM;
			endpoint.steamConn = connectionHandle;
			endpoint.steamID   = steamID64;
			return endpoint;
		}

		/*************************************************************************
		  \brief
			Converts back to a Winsock address for sendto.
		  \return
			sockaddr_in. Zeroed with sin_family AF_INET when this is not an
			IPV4 endpoint - callers on the Winsock path should never reach
			that case, and a zeroed address fails the send loudly rather than
			silently targeting a wrong host.
		*************************************************************************/
		sockaddr_in ToSockaddr() const
		{
			sockaddr_in addr{};
			addr.sin_family = AF_INET;

			if (kind == Kind::IPV4)
			{
				addr.sin_addr.S_un.S_addr = ipv4;
				addr.sin_port             = port;
			}

			return addr;
		}

		/*************************************************************************
		  \brief
			Human-readable form for logs. Never used for identity.
		*************************************************************************/
		std::string ToString() const
		{
			switch (kind)
			{
			case Kind::IPV4:
			{
				char text[INET_ADDRSTRLEN]{};
				in_addr address{};
				address.S_un.S_addr = ipv4;
				inet_ntop(AF_INET, &address, text, INET_ADDRSTRLEN);
				return std::string(text) + ":" + std::to_string(ntohs(port));
			}
			case Kind::STEAM:
				return "steam:conn=" + std::to_string(steamConn)
					+ " id=" + std::to_string(steamID);
			default:
				return "<invalid>";
			}
		}
	};
}

namespace std
{
	/*************************************************************************
	  \brief
		Hash specialisation so NetEndpoint can key an unordered_map.
	  \details
		Only the fields that participate in operator== are hashed, otherwise
		equal endpoints could land in different buckets.
	*************************************************************************/
	template <>
	struct hash<GGNetworking::NetEndpoint>
	{
		size_t operator()(GGNetworking::NetEndpoint const& endpoint) const noexcept
		{
			// 64-bit FNV-1a over the identity fields.
			uint64_t value = 1469598103934665603ULL;

			auto mix = [&value](uint64_t input)
				{
					for (int byteIndex = 0; byteIndex < 8; ++byteIndex)
					{
						value ^= (input >> (byteIndex * 8)) & 0xFFULL;
						value *= 1099511628211ULL;
					}
				};

			mix(static_cast<uint64_t>(endpoint.kind));

			switch (endpoint.kind)
			{
			case GGNetworking::NetEndpoint::Kind::IPV4:
				mix((static_cast<uint64_t>(endpoint.ipv4) << 16)
					| static_cast<uint64_t>(endpoint.port));
				break;
			case GGNetworking::NetEndpoint::Kind::STEAM:
				mix(static_cast<uint64_t>(endpoint.steamConn));
				break;
			default:
				break;
			}

			return static_cast<size_t>(value);
		}
	};
}
