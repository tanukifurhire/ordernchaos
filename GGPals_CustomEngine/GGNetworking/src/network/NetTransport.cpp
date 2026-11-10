/*************************************************************************
  \file       NetTransport.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Transport factory, runtime backend selection, and the Steam peer gate.

  \details
	The one place that knows which transports exist. Everything else asks for
	a TransportKind and gets an INetTransport back, so adding a backend later
	touches this file and nothing else.

	Selection is runtime rather than compile-time on purpose. It preserves the
	fast no-Steam loopback loop for daily iteration, and it is the kill-switch:
	if Steam misbehaves near a deadline, force Winsock and ship the LAN path
	without reverting a line of code.

	The peer-authorisation gate lives here too. SteamTransport must ask "is
	this SteamID allowed in" before accepting a connection, but the answer
	comes from the lobby manager up in GGEngine. A function object owned by the
	platform layer lets the answer travel down without GGNetworking learning
	what a lobby is.

  \dependencies
	INetTransport.hpp, WinsockTransport.hpp, SteamTransport.hpp,
	SteamPlatform.hpp

  \see
	INetTransport, WinsockTransport, SteamTransport

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <INetTransport.hpp>
#include <SteamPlatform.hpp>
#include <WinsockTransport.hpp>

#ifdef GG_STEAM_ENABLED
	#include <SteamTransport.hpp>
#endif

#include <iostream>

namespace GGNetworking
{
	namespace
	{
		// Winsock by default: it needs no Steam client, no login and no
		// network beyond loopback, which is what daily iteration wants.
		TransportKind s_preferredTransport = TransportKind::Winsock;

		SteamPlatform::PeerAuthorizer s_peerAuthorizer;
	}

	char const* TransportKindName(TransportKind kind)
	{
		switch (kind)
		{
		case TransportKind::Winsock: return "Winsock";
		case TransportKind::Steam:   return "Steam";
		default:                     return "unknown";
		}
	}

	bool IsSteamTransportCompiledIn()
	{
#ifdef GG_STEAM_ENABLED
		return true;
#else
		return false;
#endif
	}

	TransportKind GetPreferredTransport()
	{
		return s_preferredTransport;
	}

	TransportKind SetPreferredTransport(TransportKind kind)
	{
		if (kind == TransportKind::Steam)
		{
			if (!IsSteamTransportCompiledIn())
			{
				std::cout << "Transport: Steam requested but this build has no Steamworks SDK; "
					"staying on Winsock" << std::endl;
				s_preferredTransport = TransportKind::Winsock;
				return s_preferredTransport;
			}

			if (!SteamPlatform::IsAvailable())
			{
				// Refuse now, with a reason, rather than at connect time with
				// an opaque failure three layers down.
				std::cout << "Transport: Steam requested but SteamAPI is not initialised "
					"(client closed, not logged in, or Steam.init() not called); "
					"staying on Winsock" << std::endl;
				s_preferredTransport = TransportKind::Winsock;
				return s_preferredTransport;
			}
		}

		s_preferredTransport = kind;

		std::cout << "Transport: selected " << TransportKindName(kind) << std::endl;
		return s_preferredTransport;
	}

	std::unique_ptr<INetTransport> CreateTransport(TransportKind kind)
	{
		switch (kind)
		{
		case TransportKind::Winsock:
			return std::make_unique<WinsockTransport>();

		case TransportKind::Steam:
#ifdef GG_STEAM_ENABLED
			return std::make_unique<SteamTransport>();
#else
			std::cerr << "Transport: Steam backend is not present in this build" << std::endl;
			return nullptr;
#endif

		default:
			return nullptr;
		}
	}

	namespace SteamPlatform
	{
		void SetPeerAuthorizer(PeerAuthorizer authorizer)
		{
			s_peerAuthorizer = std::move(authorizer);
		}

		bool IsPeerAuthorized(uint64_t steamID64)
		{
			// Fails OPEN with no gate installed. A host running direct-connect
			// without lobbies has no membership list to check against, and
			// rejecting everyone there would break the LAN path for no gain.
			if (!s_peerAuthorizer) return true;

			return s_peerAuthorizer(steamID64);
		}
	}
}
