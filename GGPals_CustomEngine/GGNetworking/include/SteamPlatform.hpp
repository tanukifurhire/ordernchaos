/*************************************************************************
  \file       SteamPlatform.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Steamworks SDK lifecycle: init, per-frame callback pump, shutdown.

  \details
	Sits beside PlatformWindows in the L1 platform layer. Everything that
	touches SteamAPI_Init / SteamAPI_Shutdown / SteamAPI_RunCallbacks goes
	through here, so there is exactly one owner of the SDK's global state and
	exactly one place the per-frame pump can be forgotten.

	The whole header compiles with or without the SDK present. When
	GG_STEAM_ENABLED is undefined every entry point still exists and returns
	the "Steam is not here" answer, so callers - including the Lua bindings -
	need no #ifdef of their own.

	Two rules that cost days when broken:

	1. RunCallbacks() must be called every frame, on the MAIN thread, and
	   nowhere else. Every asynchronous Steamworks result - lobby created,
	   lobby list returned, connection state changed - is delivered from
	   inside it. Skip it and the API appears to work while silently never
	   completing anything. Call it from a second thread and SDK state
	   corrupts in ways that look like random packet loss.

	2. SteamID64 must cross into Lua as a string. LuaJIT numbers are doubles;
	   a 64-bit account ID does not survive a 53-bit mantissa, and the
	   corruption is silent and intermittent (it only bites on IDs above
	   2^53), which makes it miserable to diagnose after the fact.

  \dependencies
	<cstdint>, <string>

  \see
	SteamTransport, SteamLobby, INetTransport

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace GGNetworking
{
	/*************************************************************************
	  \namespace SteamPlatform
	  \brief
		Process-wide Steamworks SDK lifecycle.
	  \details
		Not a class: there is one Steam API context per process and pretending
		otherwise would only invite a second instance. All functions are
		main-thread only unless documented otherwise.
	*************************************************************************/
	namespace SteamPlatform
	{
		/*************************************************************************
		  \brief
			App ID used during development.
		  \details
			480 is Spacewar, Valve's public test app. It works without a
			partner account, which is why it is here, but its lobby list is
			shared by every Steamworks developer on Earth - see SteamLobby's
			mandatory gg_game filter, without which lobby testing is unusable.
		*************************************************************************/
		static constexpr uint32_t DEVELOPMENT_APP_ID = 480;

		/*************************************************************************
		  \brief
			Whether this binary was built against the Steamworks SDK.
		  \return
			Compile-time answer. True does not mean Steam is usable now - that
			is IsAvailable().
		*************************************************************************/
		bool IsCompiledIn();

		/*************************************************************************
		  \brief
			Relaunches the game through Steam if it was started outside it.
		  \param[in] appId
			Your App ID.
		  \return
			true when the process should exit immediately because Steam is
			relaunching it.
		  \details
			A no-op during development: with steam_appid.txt next to the exe
			the SDK reads the ID from there and this returns false, so running
			straight from the build folder or the debugger keeps working.
			Call it before Init(), and honour a true return by returning from
			main at once.
		*************************************************************************/
		bool RestartAppIfNecessary(uint32_t appId);

		/*************************************************************************
		  \brief
			Initialises the Steamworks SDK.
		  \return
			true when the API came up. false when the SDK is absent from the
			build, the Steam client is not running, no user is logged in, or
			the App ID could not be determined.
		  \details
			Also calls InitRelayNetworkAccess(), which starts the handshake
			with Valve's SDR relay network. Doing it at startup rather than at
			connect time hides several seconds of latency behind the main menu.
			Safe to call more than once; later calls report the existing state.
		  \note
			A false return is normal and not an error - it is the "player is
			running the LAN build" case. Fall back to the Winsock transport.
		*************************************************************************/
		bool Init();

		/*************************************************************************
		  \brief
			Shuts the SDK down. Safe when Init failed or never ran.
		*************************************************************************/
		void Shutdown();

		/*************************************************************************
		  \brief
			Whether Init() succeeded and Shutdown() has not run.
		*************************************************************************/
		bool IsAvailable();

		/*************************************************************************
		  \brief
			Pumps Steam's callback queue. MAIN THREAD, EVERY FRAME.
		  \details
			The single most commonly forgotten Steamworks call. Everything
			asynchronous is delivered from inside it: LobbyCreated_t,
			LobbyMatchList_t, LobbyEnter_t, GameLobbyJoinRequested_t,
			SteamNetConnectionStatusChangedCallback_t. No pump, no callbacks,
			and the failure is silent.
		  \note
			No-op when Steam is unavailable, so the call site needs no guard.
		*************************************************************************/
		void RunCallbacks();

		/*************************************************************************
		  \brief
			Local user's display name.
		  \return
			Persona name, or an empty string when Steam is unavailable.
		*************************************************************************/
		std::string GetPersonaName();

		/*************************************************************************
		  \brief
			Local user's SteamID64 as a number, for C++ use only.
		  \return
			0 when unavailable.
		  \warning
			Never hand this to Lua. See GetSteamIDString.
		*************************************************************************/
		uint64_t GetSteamID64();

		/*************************************************************************
		  \brief
			Local user's SteamID64 rendered as decimal text.
		  \return
			"0" when unavailable.
		  \details
			The form every boundary should use: Lua, lobby metadata, logs.
			A 17-digit SteamID exceeds the 2^53 that a double represents
			exactly, so passing it as a number silently corrupts the low bits.
		*************************************************************************/
		std::string GetSteamIDString();

		/*************************************************************************
		  \brief
			Parses a decimal SteamID64 string.
		  \param[in] text
			Digits, as produced by GetSteamIDString or lobby metadata.
		  \return
			The ID, or 0 when the text is empty, non-numeric or out of range.
		*************************************************************************/
		uint64_t ParseSteamID(std::string const& text);

		/*************************************************************************
		  \brief
			Injects fake loss / lag / reorder / duplication into Steam traffic.
		  \param[in] lossPercent
			Percentage of packets dropped in each direction.
		  \param[in] lagMilliseconds
			Extra delay added in each direction.
		  \param[in] reorderPercent
			Percentage of packets reordered on send.
		  \param[in] duplicatePercent
			Percentage of packets duplicated on send.
		  \return
			true if the values were accepted AND read back unchanged.
		  \details
			The exam for the reliability layer. Pass zeroes to clear.

			The read-back is the point: in some SDK builds these knobs are
			compiled out and the setter succeeds while doing nothing. A false
			return means they are inert and you should shape traffic
			externally (Clumsy on Windows) instead of trusting a clean run.
		  \note
			Global to the process and affects the Steam transport only. The
			Winsock path is unaffected - test that one with Clumsy.
		*************************************************************************/
		bool SetNetworkSimulation(int lossPercent, int lagMilliseconds,
			int reorderPercent, int duplicatePercent);

		/*************************************************************************
		  \brief
			Opens the Steam overlay's invite dialog for a lobby.
		  \param[in] lobbyID
			Lobby to invite into.
		  \details
			Lives here rather than in SteamLobby because it is an ISteamFriends
			call, not a matchmaking one. Silently does nothing when the overlay
			is disabled or Steam is unavailable.
		*************************************************************************/
		void OpenInviteOverlay(uint64_t lobbyID);

		/*************************************************************************
		  \brief
			Predicate deciding whether an inbound Steam peer may connect.
		  \param[in] steamID64
			The remote identity Steam authenticated for the connection.
		  eturn
			true to accept, false to reject.
		*************************************************************************/
		using PeerAuthorizer = std::function<bool(uint64_t steamID64)>;

		/*************************************************************************
		  \brief
			Installs the gate SteamTransport consults before accepting a peer.
		  \param[in] authorizer
			Predicate, or an empty function to remove the gate.
		  \details
			SteamLobbyManager installs one that answers "is this SteamID a
			current member of the lobby I am hosting". Without it, anyone who
			learns the host's SteamID can ConnectP2P straight to the game,
			bypass matchmaking entirely and land directly on the RPC surface -
			which is exactly the audience the RPC registry was hardened
			against.

			Lives in the platform layer rather than on SteamTransport so the
			engine can install it without GGNetworking needing to know that
			lobbies exist, and without the call site needing an #ifdef.
		  
ote
			Main thread only. The predicate is invoked from inside
			RunCallbacks(), which is also main thread.
		*************************************************************************/
		void SetPeerAuthorizer(PeerAuthorizer authorizer);

		/*************************************************************************
		  \brief
			Asks the installed gate about a peer.
		  \param[in] steamID64
			Peer identity.
		  eturn
			The gate's answer, or true when no gate is installed.
		  \details
			Fails OPEN by design: a host that never called SetPeerAuthorizer is
			running direct-connect without lobbies, and refusing every peer
			there would break the LAN path. The gate is installed by whoever
			creates the lobby, which is the only context where membership is a
			meaningful question.
		*************************************************************************/
		bool IsPeerAuthorized(uint64_t steamID64);
	}
}
