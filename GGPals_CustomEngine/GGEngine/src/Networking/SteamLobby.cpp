/*************************************************************************
  \file       SteamLobby.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief      Steam matchmaking implementation.

  \details
	Compiles in two shapes, like SteamPlatform: with the SDK it drives
	ISteamMatchmaking, without it every method is a logged no-op so the Lua
	bindings and the UI above need no conditional code.

  \dependencies
	- SteamLobby.hpp
	- GGEngine.hpp (Lua callback dispatch)
	- GGLogger.hpp

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <SteamLobby.hpp>

#include <GGEngine.hpp>
#include <GGLuaVM.hpp>
#include <GGLogger.hpp>
#include <UDPTypes.hpp>

#include <string>

namespace GGNetworking
{
	namespace
	{
		/*************************************************************************
		  \brief
			The build tag written to gg_build and filtered on when listing.
		*************************************************************************/
		std::string BuildTag()
		{
			return std::to_string(GG_PROTOCOL_VERSION);
		}
	}

	SteamLobbyManager& GetSteamLobby()
	{
		static SteamLobbyManager instance;
		return instance;
	}

#ifdef GG_STEAM_ENABLED

	SteamLobbyManager::SteamLobbyManager()
		// Registering here hooks the SDK dispatcher, so every one of these
		// fires from inside SteamAPI_RunCallbacks - main thread, once a frame.
		: lobbyChatUpdateCallback(this, &SteamLobbyManager::OnLobbyChatUpdate)
		, lobbyDataUpdateCallback(this, &SteamLobbyManager::OnLobbyDataUpdate)
		, joinRequestedCallback(this, &SteamLobbyManager::OnGameLobbyJoinRequested)
		, richPresenceJoinCallback(this, &SteamLobbyManager::OnRichPresenceJoinRequested)
	{
	}

	SteamLobbyManager::~SteamLobbyManager()
	{
		LeaveLobby();
	}

	bool SteamLobbyManager::IsAvailable() const
	{
		return SteamPlatform::IsAvailable() && SteamMatchmaking() != nullptr;
	}

	void SteamLobbyManager::CreateLobby(int maxMembers, std::string const& lobbyName)
	{
		if (!IsAvailable())
		{
			GGLogger::Warning("Steam.createLobby: Steam is not available");
			GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyCreated", std::string("0"), false);
			return;
		}

		if (currentLobbyID != 0)
		{
			GGLogger::Warning("Steam.createLobby: already in a lobby, leaving it first");
			LeaveLobby();
		}

		// Clamp: Steam rejects 0 outright, and 250 is its ceiling.
		if (maxMembers < 2)   maxMembers = 2;
		if (maxMembers > 250) maxMembers = 250;

		// k_ELobbyTypePublic so the lobby appears in RequestLobbyList for
		// anyone running the same build. Friends-only or private would hide it
		// from the browser, which is the feature being built here.
		SteamAPICall_t call = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, maxMembers);
		lobbyCreatedResult.Set(call, this, &SteamLobbyManager::OnLobbyCreated);

		// Stashed so OnLobbyCreated can name the lobby once it exists.
		pendingLobbyName = lobbyName;

		GGLogger::Info("Steam.createLobby: requesting a lobby for " + std::to_string(maxMembers)
			+ " players");
	}

	void SteamLobbyManager::OnLobbyCreated(LobbyCreated_t* result, bool ioFailure)
	{
		if (ioFailure || result == nullptr || result->m_eResult != k_EResultOK)
		{
			int const code = result ? static_cast<int>(result->m_eResult) : -1;
			GGLogger::Error("Steam lobby creation failed, EResult " + std::to_string(code));
			GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyCreated", std::string("0"), false);
			return;
		}

		currentLobby   = CSteamID(result->m_ulSteamIDLobby);
		currentLobbyID = result->m_ulSteamIDLobby;

		// The data contract. Written before anything else can see the lobby,
		// so a client listing lobbies never observes a half-configured one.
		SetLobbyData(KEY_GAME,         GAME_TAG);
		SetLobbyData(KEY_BUILD,        BuildTag());
		SetLobbyData(KEY_HOST_STEAMID, SteamPlatform::GetSteamIDString());
		SetLobbyData(KEY_NAME,         pendingLobbyName.empty()
			? SteamPlatform::GetPersonaName() + "'s game"
			: pendingLobbyName);

		// Install the accept gate now, while we are definitively the host.
		// SteamTransport asks this before accepting any inbound connection;
		// without it, anyone who learns our SteamID bypasses matchmaking and
		// connects straight to the RPC surface.
		SteamPlatform::SetPeerAuthorizer(
			[](uint64_t steamID64) { return GetSteamLobby().IsLobbyMember(steamID64); });

		GGLogger::Info("Steam lobby " + std::to_string(currentLobbyID) + " created");

		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyCreated",
			std::to_string(currentLobbyID), true);
	}

	void SteamLobbyManager::RequestLobbyList()
	{
		if (!IsAvailable())
		{
			GGLogger::Warning("Steam.requestLobbyList: Steam is not available");
			return;
		}

		// Both filters are mandatory, for different reasons:
		//   gg_game  - App ID 480's lobby list is shared by every Steamworks
		//              developer on Earth. Unfiltered, this returns hundreds of
		//              unrelated lobbies and testing is unusable.
		//   gg_build - the only version guard that exists anywhere, since the
		//              wire format carries no version field.
		SteamMatchmaking()->AddRequestLobbyListStringFilter(
			KEY_GAME, GAME_TAG, k_ELobbyComparisonEqual);
		SteamMatchmaking()->AddRequestLobbyListStringFilter(
			KEY_BUILD, BuildTag().c_str(), k_ELobbyComparisonEqual);

		SteamAPICall_t call = SteamMatchmaking()->RequestLobbyList();
		lobbyMatchListResult.Set(call, this, &SteamLobbyManager::OnLobbyMatchList);
	}

	void SteamLobbyManager::OnLobbyMatchList(LobbyMatchList_t* result, bool ioFailure)
	{
		std::vector<SteamLobbyEntry> entries;

		if (!ioFailure && result != nullptr)
		{
			entries.reserve(result->m_nLobbiesMatching);

			for (uint32 i = 0; i < result->m_nLobbiesMatching; ++i)
			{
				CSteamID const lobby = SteamMatchmaking()->GetLobbyByIndex(static_cast<int>(i));

				SteamLobbyEntry entry;
				entry.lobbyID     = lobby.ConvertToUint64();
				entry.hostSteamID = SteamMatchmaking()->GetLobbyData(lobby, KEY_HOST_STEAMID);
				entry.name        = SteamMatchmaking()->GetLobbyData(lobby, KEY_NAME);
				entry.playerCount = SteamMatchmaking()->GetNumLobbyMembers(lobby);
				entry.maxPlayers  = SteamMatchmaking()->GetLobbyMemberLimit(lobby);
				entry.orderTaken  = std::string(SteamMatchmaking()->GetLobbyData(lobby, KEY_ORDER_TAKEN)) == "1";
				entry.chaosTaken  = std::string(SteamMatchmaking()->GetLobbyData(lobby, KEY_CHAOS_TAKEN)) == "1";

				// A lobby whose host_steamid never got written is one we could
				// not connect to anyway - drop it rather than show a dead row.
				if (entry.hostSteamID.empty() || entry.hostSteamID == "0") continue;

				if (entry.name.empty()) entry.name = "Unnamed lobby";

				entries.push_back(std::move(entry));
			}
		}

		GGLogger::Info("Steam lobby list: " + std::to_string(entries.size()) + " match(es)");

		PushLobbyListToLua(entries);
	}

	void SteamLobbyManager::JoinLobby(std::string const& lobbyID)
	{
		if (!IsAvailable())
		{
			GGLogger::Warning("Steam.joinLobby: Steam is not available");
			return;
		}

		uint64_t const parsed = SteamPlatform::ParseSteamID(lobbyID);
		if (parsed == 0)
		{
			GGLogger::Error("Steam.joinLobby: '" + lobbyID + "' is not a valid lobby ID");
			return;
		}

		SteamAPICall_t call = SteamMatchmaking()->JoinLobby(CSteamID(parsed));
		lobbyEnterResult.Set(call, this, &SteamLobbyManager::OnLobbyEntered);

		GGLogger::Info("Steam.joinLobby: joining " + lobbyID + "...");
	}

	void SteamLobbyManager::OnLobbyEntered(LobbyEnter_t* result, bool ioFailure)
	{
		if (ioFailure || result == nullptr
			|| result->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
		{
			int const code = result ? static_cast<int>(result->m_EChatRoomEnterResponse) : -1;
			GGLogger::Error("Steam lobby join failed, response " + std::to_string(code));
			GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyEntered",
				std::string("0"), std::string("0"), false);
			return;
		}

		currentLobby   = CSteamID(result->m_ulSteamIDLobby);
		currentLobbyID = result->m_ulSteamIDLobby;

		std::string const hostSteamID = GetLobbyData(KEY_HOST_STEAMID);

		GGLogger::Info("Entered Steam lobby " + std::to_string(currentLobbyID)
			+ ", host " + hostSteamID);

		// The handoff: Lua reads the host SteamID from here and calls
		// RPC.connectToSteamHost, after which the existing
		// CLIENT_CONNECT_REQUEST handshake runs on top, unchanged.
		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyEntered",
			std::to_string(currentLobbyID), hostSteamID, true);
	}

	void SteamLobbyManager::LeaveLobby()
	{
		if (currentLobbyID == 0) return;

		if (SteamMatchmaking()) SteamMatchmaking()->LeaveLobby(currentLobby);

		GGLogger::Info("Left Steam lobby " + std::to_string(currentLobbyID));

		currentLobby   = CSteamID();
		currentLobbyID = 0;

		// Drop the accept gate with the lobby. Leaving it installed would keep
		// rejecting every peer against an empty member list.
		SteamPlatform::SetPeerAuthorizer(nullptr);
	}

	void SteamLobbyManager::OpenInviteOverlay()
	{
		if (currentLobbyID == 0)
		{
			GGLogger::Warning("Steam.openInviteOverlay: not in a lobby");
			return;
		}

		SteamPlatform::OpenInviteOverlay(currentLobbyID);
	}

	bool SteamLobbyManager::IsHost() const
	{
		if (currentLobbyID == 0 || !SteamMatchmaking()) return false;

		return SteamMatchmaking()->GetLobbyOwner(currentLobby).ConvertToUint64()
			== SteamPlatform::GetSteamID64();
	}

	int SteamLobbyManager::GetMemberCount() const
	{
		if (currentLobbyID == 0 || !SteamMatchmaking()) return 0;
		return SteamMatchmaking()->GetNumLobbyMembers(currentLobby);
	}

	std::string SteamLobbyManager::GetMemberAt(int index) const
	{
		if (currentLobbyID == 0 || !SteamMatchmaking()) return "0";
		if (index < 0 || index >= GetMemberCount())     return "0";

		return std::to_string(
			SteamMatchmaking()->GetLobbyMemberByIndex(currentLobby, index).ConvertToUint64());
	}

	bool SteamLobbyManager::IsLobbyMember(uint64_t steamID64) const
	{
		if (currentLobbyID == 0 || !SteamMatchmaking()) return false;

		int const count = SteamMatchmaking()->GetNumLobbyMembers(currentLobby);

		for (int i = 0; i < count; ++i)
		{
			if (SteamMatchmaking()->GetLobbyMemberByIndex(currentLobby, i).ConvertToUint64() == steamID64)
			{
				return true;
			}
		}

		return false;
	}

	void SteamLobbyManager::SetLobbyData(std::string const& key, std::string const& value)
	{
		if (currentLobbyID == 0 || !SteamMatchmaking()) return;
		SteamMatchmaking()->SetLobbyData(currentLobby, key.c_str(), value.c_str());
	}

	std::string SteamLobbyManager::GetLobbyData(std::string const& key) const
	{
		if (currentLobbyID == 0 || !SteamMatchmaking()) return {};
		return SteamMatchmaking()->GetLobbyData(currentLobby, key.c_str());
	}

	void SteamLobbyManager::OnLobbyChatUpdate(LobbyChatUpdate_t* update)
	{
		if (update == nullptr || update->m_ulSteamIDLobby != currentLobbyID) return;

		bool const joined = (update->m_rgfChatMemberStateChange
			& k_EChatMemberStateChangeEntered) != 0;

		std::string const memberID = std::to_string(update->m_ulSteamIDUserChanged);

		GGLogger::Info("Lobby member " + memberID + (joined ? " joined" : " left"));

		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyMemberChanged",
			memberID, joined, GetMemberCount());
	}

	void SteamLobbyManager::OnLobbyDataUpdate(LobbyDataUpdate_t* update)
	{
		if (update == nullptr || update->m_ulSteamIDLobby != currentLobbyID) return;

		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyDataChanged",
			std::to_string(currentLobbyID));
	}

	void SteamLobbyManager::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* request)
	{
		if (request == nullptr) return;

		std::string const lobbyID = std::to_string(request->m_steamIDLobby.ConvertToUint64());

		GGLogger::Info("Steam invite accepted for lobby " + lobbyID);

		// Tell Lua first so the UI can move to a "joining..." screen, then act.
		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnJoinRequested", lobbyID);

		JoinLobby(lobbyID);
	}

	void SteamLobbyManager::OnRichPresenceJoinRequested(GameRichPresenceJoinRequested_t* request)
	{
		// A genuinely different code path from the invite dialog, even though
		// it ends in the same place. Test both - one working does not imply
		// the other does.
		if (request == nullptr) return;

		std::string const connectString = request->m_rgchConnect;

		GGLogger::Info("Rich-presence join requested: " + connectString);

		// Our rich presence "connect" string is "+connect_lobby <id>", the
		// convention Steam's own examples use.
		std::string const prefix = "+connect_lobby ";
		if (connectString.rfind(prefix, 0) != 0)
		{
			GGLogger::Warning("Unrecognised rich-presence connect string, ignoring");
			return;
		}

		std::string const lobbyID = connectString.substr(prefix.size());

		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnJoinRequested", lobbyID);

		JoinLobby(lobbyID);
	}

#else   // ---------------------------------------------------------------
	// No Steamworks SDK. Every method exists and reports the same "Steam is
	// not here" answer, so the Lua bindings and the UI need no #ifdef.

	SteamLobbyManager::SteamLobbyManager()  = default;
	SteamLobbyManager::~SteamLobbyManager() = default;

	bool SteamLobbyManager::IsAvailable() const { return false; }

	void SteamLobbyManager::CreateLobby(int, std::string const&)
	{
		GGLogger::Warning("Steam.createLobby: this build has no Steamworks SDK");
		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyCreated", std::string("0"), false);
	}

	void SteamLobbyManager::RequestLobbyList()
	{
		GGLogger::Warning("Steam.requestLobbyList: this build has no Steamworks SDK");
		PushLobbyListToLua({});
	}

	void SteamLobbyManager::JoinLobby(std::string const&)
	{
		GGLogger::Warning("Steam.joinLobby: this build has no Steamworks SDK");
		GetEngineInstance().CallLuaFunctionAdvanced("SteamCB_OnLobbyEntered",
			std::string("0"), std::string("0"), false);
	}

	void SteamLobbyManager::LeaveLobby()      {}
	void SteamLobbyManager::OpenInviteOverlay() {}

	bool SteamLobbyManager::IsHost() const    { return false; }
	int  SteamLobbyManager::GetMemberCount() const { return 0; }

	std::string SteamLobbyManager::GetMemberAt(int) const { return "0"; }

	bool SteamLobbyManager::IsLobbyMember(uint64_t) const { return false; }

	void SteamLobbyManager::SetLobbyData(std::string const&, std::string const&) {}

	std::string SteamLobbyManager::GetLobbyData(std::string const&) const { return {}; }

#endif  // GG_STEAM_ENABLED

	// --- Shared between both shapes -------------------------------------

	std::string SteamLobbyManager::GetCurrentLobbyID() const
	{
		return std::to_string(currentLobbyID);
	}

	std::string SteamLobbyManager::GetHostSteamID() const
	{
		std::string const host = GetLobbyData(KEY_HOST_STEAMID);
		return host.empty() ? "0" : host;
	}

	/*************************************************************************
	  \brief
		Hands the server-browser rows to Lua as an array of tables.
	  \param[in] entries
		Rows to pass; may be empty, which Lua sees as an empty table.
	  \details
		Built by hand rather than through CallLuaFunctionAdvanced, which only
		knows how to push scalars.

		Every SteamID and lobby ID goes across as a STRING. They are 64-bit and
		a LuaJIT number is a double: anything above 2^53 loses its low bits, and
		it does so silently, only on some IDs, which makes it a miserable bug
		to track down after the fact.
	*************************************************************************/
	void SteamLobbyManager::PushLobbyListToLua(std::vector<SteamLobbyEntry> const& entries)
	{
		GGLuaVM* vm = GetEngineInstance().GetLuaVM();
		if (!vm) return;

		lua_State* L = vm->GetState();

		lua_getglobal(L, "SteamCB_OnLobbyList");
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 1);   // handler is optional
			return;
		}

		lua_createtable(L, static_cast<int>(entries.size()), 0);

		int index = 1;
		for (SteamLobbyEntry const& entry : entries)
		{
			lua_createtable(L, 0, 5);

			lua_pushstring(L, std::to_string(entry.lobbyID).c_str());
			lua_setfield(L, -2, "id");

			lua_pushstring(L, entry.hostSteamID.c_str());
			lua_setfield(L, -2, "hostSteamId");

			lua_pushstring(L, entry.name.c_str());
			lua_setfield(L, -2, "name");

			lua_pushinteger(L, entry.playerCount);
			lua_setfield(L, -2, "players");

			lua_pushinteger(L, entry.maxPlayers);
			lua_setfield(L, -2, "maxPlayers");

			lua_pushboolean(L, entry.orderTaken);
			lua_setfield(L, -2, "orderTaken");

			lua_pushboolean(L, entry.chaosTaken);
			lua_setfield(L, -2, "chaosTaken");

			lua_rawseti(L, -2, index++);
		}

		if (lua_pcall(L, 1, 0, 0) != 0)
		{
			GGLogger::Error(std::string("[Lua Error] SteamCB_OnLobbyList: ")
				+ lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
}
