/*************************************************************************
  \file       SteamLobby.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief      Steam matchmaking: lobby create, list, join, invite.

  \details
	Fills the hole the project never implemented. MessageType 700-711 declares
	a whole lobby protocol - CLIENT_REQUEST_LOBBY_LIST, SERVER_LOBBY_LIST,
	CLIENT_CREATE_LOBBY and the rest - and GameServer::Update handles none of
	it. ISteamMatchmaking supplies all of it, plus the parts that were never
	on the list and matter more: friend invites, "Join Game" from the friends
	list, and the overlay invite dialog.

	Lobby data contract, set on create and filtered on list:

	  gg_game       "ordernchaos_m6"   identifies our lobbies
	  gg_build      protocol version   rejects mismatched builds
	  host_steamid  host's SteamID64   what clients ConnectP2P to
	  gg_name       display name       shown in the browser

	The gg_game filter is not optional while running on App ID 480. Spacewar's
	lobby list is shared by every Steamworks developer testing anything, so an
	unfiltered RequestLobbyList returns hundreds of strangers' lobbies.

	gg_build is the only version guard that exists. Neither PacketHeader nor
	MessageHeader carries a version field, so without this filter a mismatched
	build connects and then misbehaves in ways that look like netcode bugs.

	This module also installs the peer-authorisation gate SteamTransport
	consults before accepting a connection. Without it, anyone who learns the
	host's SteamID can connect directly, skipping matchmaking entirely and
	landing on the RPC surface.

  \dependencies
	- SteamPlatform.hpp
	- Steamworks SDK (optional; every entry point still exists without it)

  \history
	- 1.0 (24 August 2026): Initial version.

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <SteamPlatform.hpp>

#include <cstdint>
#include <string>
#include <vector>

#ifdef GG_STEAM_ENABLED
	#include <steam/steam_api.h>
#endif

namespace GGNetworking
{
	/*************************************************************************
	  \struct SteamLobbyEntry
	  \brief
		One row of the server browser.
	*************************************************************************/
	struct SteamLobbyEntry
	{
		uint64_t    lobbyID{};
		std::string hostSteamID;    // decimal text, never a number - see SteamPlatform
		std::string name;
		int         playerCount{};
		int         maxPlayers{};

		// Faction occupancy, published by the host as lobby metadata. A joiner
		// has no connection to that host yet and so cannot ask its server which
		// slots are taken - lobby data is the only channel that works before
		// connecting.
		bool        orderTaken{ false };
		bool        chaosTaken{ false };
	};

	/*************************************************************************
	  \class SteamLobbyManager
	  \brief
		Owns the local player's relationship with one Steam lobby at a time.
	  \details
		Asynchronous throughout: every request returns immediately and the
		answer arrives on a later frame, from inside
		SteamPlatform::RunCallbacks(), as a SteamCB_* call into Lua. That
		matches how the rest of the engine already talks to scripts via
		ClientRPC_* / ServerRPC_*.

		The SteamCB_ prefix is deliberate. These are engine-to-Lua calls, not
		network RPCs, so they must NOT be named ServerRPC_ or ClientRPC_ - the
		registry's convention sweep would pick those up and publish them as
		remotely callable entry points.
	  \note
		Main thread only. Every method either calls the SDK directly or is
		invoked from inside RunCallbacks.
	  \see
		SteamPlatform, SteamTransport, GameServer
	*************************************************************************/
	class SteamLobbyManager
	{
	public:
		// --- Lobby data keys ---
		static constexpr char const* KEY_GAME         = "gg_game";
		static constexpr char const* KEY_BUILD        = "gg_build";
		static constexpr char const* KEY_HOST_STEAMID = "host_steamid";
		static constexpr char const* KEY_NAME         = "gg_name";
		static constexpr char const* KEY_ORDER_TAKEN  = "gg_order";
		static constexpr char const* KEY_CHAOS_TAKEN  = "gg_chaos";

		/*************************************************************************
		  \brief
			Value of gg_game. Any string, as long as it is ours and unique.
		*************************************************************************/
		static constexpr char const* GAME_TAG = "ordernchaos_m6";

		SteamLobbyManager();
		~SteamLobbyManager();

		SteamLobbyManager(SteamLobbyManager const&)            = delete;
		SteamLobbyManager& operator=(SteamLobbyManager const&) = delete;

		/*************************************************************************
		  \brief
			Whether lobby operations can be attempted at all.
		  \return
			true when Steam is compiled in AND initialised.
		*************************************************************************/
		bool IsAvailable() const;

		/*************************************************************************
		  \brief
			Creates a lobby and becomes its host.
		  \param[in] maxMembers
			Capacity, including the host.
		  \param[in] lobbyName
			Display name for the browser.
		  \details
			Asynchronous. On success the lobby data contract is written, the
			peer-authorisation gate is installed, and Lua receives
			SteamCB_OnLobbyCreated(lobbyId, success) - which is where the game
			calls RPC.startSteamHost() to bring the transport up.
		*************************************************************************/
		void CreateLobby(int maxMembers, std::string const& lobbyName);

		/*************************************************************************
		  \brief
			Asks Steam for lobbies matching this game and build.
		  \details
			Asynchronous; the result reaches Lua as
			SteamCB_OnLobbyList(lobbies). Always filtered on gg_game and
			gg_build - see the file header for why both are mandatory.
		*************************************************************************/
		void RequestLobbyList();

		/*************************************************************************
		  \brief
			Joins a lobby by ID.
		  \param[in] lobbyID
			Lobby to join, as decimal text.
		  \details
			Asynchronous. Success reaches Lua as
			SteamCB_OnLobbyEntered(lobbyId, hostSteamId), which is where the
			game calls RPC.connectToSteamHost(hostSteamId).
		*************************************************************************/
		void JoinLobby(std::string const& lobbyID);

		/*************************************************************************
		  \brief
			Leaves the current lobby and removes the authorisation gate.
		*************************************************************************/
		void LeaveLobby();

		/*************************************************************************
		  \brief
			Opens the Steam overlay's invite dialog for the current lobby.
		  \details
			Does nothing outside a lobby, or when the overlay is unavailable.
		*************************************************************************/
		void OpenInviteOverlay();

		/*************************************************************************
		  \brief
			The lobby this player is in, as decimal text ("0" when none).
		*************************************************************************/
		std::string GetCurrentLobbyID() const;

		/*************************************************************************
		  \brief
			The current lobby's host SteamID, as decimal text ("0" when none).
		*************************************************************************/
		std::string GetHostSteamID() const;

		/*************************************************************************
		  \brief
			Whether the local player owns the current lobby.
		*************************************************************************/
		bool IsHost() const;

		/*************************************************************************
		  \brief
			Number of members in the current lobby.
		*************************************************************************/
		int GetMemberCount() const;

		/*************************************************************************
		  \brief
			SteamID of one member, as decimal text.
		  \param[in] index
			Zero-based, below GetMemberCount().
		  \return
			"0" when out of range.
		*************************************************************************/
		std::string GetMemberAt(int index) const;

		/*************************************************************************
		  \brief
			Whether a SteamID is a current member of the lobby we host.
		  \param[in] steamID64
			Identity to check.
		  \return
			true when present in the member list.
		  \details
			The predicate behind the transport's accept gate. Asked once per
			inbound connection, from inside RunCallbacks.
		*************************************************************************/
		bool IsLobbyMember(uint64_t steamID64) const;

		/*************************************************************************
		  \brief
			Writes one key to the current lobby's metadata.
		  \param[in] key
			Metadata key.
		  \param[in] value
			Metadata value.
		  \note
			Host only; Steam ignores it from a non-owner.
		*************************************************************************/
		void SetLobbyData(std::string const& key, std::string const& value);

		/*************************************************************************
		  \brief
			Reads one key from the current lobby's metadata.
		  \param[in] key
			Metadata key.
		  \return
			Value, or an empty string when absent.
		*************************************************************************/
		std::string GetLobbyData(std::string const& key) const;

	private:
		/*************************************************************************
		  \brief
			Hands server-browser rows to Lua as an array of tables.
		  \param[in] entries
			Rows to pass; an empty vector reaches Lua as an empty table.
		  \details
			Hand-built rather than routed through CallLuaFunctionAdvanced,
			which only knows how to push scalars. Every ID crosses as a string.
		*************************************************************************/
		void PushLobbyListToLua(std::vector<SteamLobbyEntry> const& entries);

		// Name destined for gg_name, held between the CreateLobby request and
		// the callback that finally has a lobby to attach it to.
		std::string pendingLobbyName;

#ifdef GG_STEAM_ENABLED
		// --- Call results (one-shot answers to a specific request) ---
		void OnLobbyCreated(LobbyCreated_t* result, bool ioFailure);
		void OnLobbyMatchList(LobbyMatchList_t* result, bool ioFailure);
		void OnLobbyEntered(LobbyEnter_t* result, bool ioFailure);

		CCallResult<SteamLobbyManager, LobbyCreated_t>   lobbyCreatedResult;
		CCallResult<SteamLobbyManager, LobbyMatchList_t> lobbyMatchListResult;
		CCallResult<SteamLobbyManager, LobbyEnter_t>     lobbyEnterResult;

		// --- Broadcast callbacks (can arrive at any time) ---
		//
		// CCallback rather than the STEAM_CALLBACK macro, matching
		// SteamTransport: the explicit form takes the owning object directly
		// and puts registration in plain sight in the constructor.
		void OnLobbyChatUpdate(LobbyChatUpdate_t* update);
		void OnLobbyDataUpdate(LobbyDataUpdate_t* update);

		/*************************************************************************
		  \brief
			A friend invited us, or picked "Join Game" in the friends list.
		  \details
			Not optional. Without this handler an accepted invite does
			absolutely nothing, with no error anywhere, which is a genuinely
			hard bug to find by inspection.
		*************************************************************************/
		void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* request);

		/*************************************************************************
		  \brief
			Join via rich presence. A different code path from the above -
			test both; one working does not imply the other does.
		*************************************************************************/
		void OnRichPresenceJoinRequested(GameRichPresenceJoinRequested_t* request);

		CCallback<SteamLobbyManager, LobbyChatUpdate_t>               lobbyChatUpdateCallback;
		CCallback<SteamLobbyManager, LobbyDataUpdate_t>               lobbyDataUpdateCallback;
		CCallback<SteamLobbyManager, GameLobbyJoinRequested_t>        joinRequestedCallback;
		CCallback<SteamLobbyManager, GameRichPresenceJoinRequested_t> richPresenceJoinCallback;

		CSteamID currentLobby{};
#endif

		// Mirrors the lobby ID even in a build without the SDK, so the
		// accessors have something consistent to report.
		uint64_t currentLobbyID{ 0 };
	};

	/*************************************************************************
	  \brief
		Process-wide lobby manager.
	  \details
		One Steam user, one lobby at a time; a singleton matches the reality
		and matches GetGameServer / GetGameClient next door.
	*************************************************************************/
	SteamLobbyManager& GetSteamLobby();
}
