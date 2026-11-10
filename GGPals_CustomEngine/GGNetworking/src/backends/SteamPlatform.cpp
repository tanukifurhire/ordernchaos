/*************************************************************************
  \file       SteamPlatform.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       24 August 2026
  \version    1.0
  \brief
	Steamworks SDK lifecycle implementation.

  \details
	The file compiles in two shapes. With GG_STEAM_ENABLED it talks to the
	SDK; without it, every entry point still links and returns the "no Steam
	here" answer. Keeping both shapes in one file means the stub cannot drift
	out of sync with the real signatures - a mismatch is a compile error, not
	a link error discovered by whoever next builds without the SDK.

  \dependencies
	SteamPlatform.hpp, Steamworks SDK (optional), <iostream>

  \see
	SteamPlatform.hpp, SteamTransport

  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <SteamPlatform.hpp>

// For GetModuleFileNameW / SetEnvironmentVariableA when locating the App ID
// beside the executable. Goes through PlatformWindows.hpp so the
// winsock2-before-windows.h include order stays correct.
#include <PlatformWindows.hpp>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef GG_STEAM_ENABLED
	#include <steam/steam_api.h>
	#include <steam/isteamnetworkingutils.h>
	#include <steam/steamnetworkingtypes.h>
#endif

namespace GGNetworking
{
	namespace SteamPlatform
	{
#ifdef GG_STEAM_ENABLED

		namespace
		{
			bool s_initialised = false;

			/*************************************************************************
			  \brief
				Routes Steam's own transport logging into stdout.
			  \details
				Registered at Init time. Verbosity is Msg by default; raise it to
				k_ESteamNetworkingSocketsDebugOutputType_Verbose while chasing a
				relay or NAT problem, but not otherwise - Verbose is loud enough
				to cost frame time.
			*************************************************************************/
			void SteamNetDebugOutput(ESteamNetworkingSocketsDebugOutputType type, char const* message)
			{
				std::cout << "[SteamNet:" << static_cast<int>(type) << "] " << message << std::endl;
			}
		}

		bool IsCompiledIn() { return true; }

		bool RestartAppIfNecessary(uint32_t appId)
		{
			return SteamAPI_RestartAppIfNecessary(appId);
		}

		namespace
		{
			/*************************************************************************
			  \brief
				Makes the App ID discoverable no matter what the working directory is.
			  \details
				SteamAPI_Init reads steam_appid.txt from the CURRENT WORKING
				DIRECTORY, not from beside the executable. Launch the exe from its
				own folder and it works; launch it from Visual Studio, a shortcut,
				or a batch file that cd's elsewhere, and Init fails with the same
				"is Steam running?" message you would get with the client closed -
				which sends you looking in entirely the wrong place.

				So: find the steam_appid.txt the build copied next to the exe, and
				publish its contents through the SteamAppId / SteamGameId
				environment variables, which the SDK also honours. Nothing is
				hardcoded - if the file is absent (a shipped build launched through
				Steam, where the client sets these itself) this does nothing at all
				and the normal path runs.

				Both the CRT and Win32 environments are set, because the SDK lives
				in its own DLL with its own CRT copy and which one it reads is not
				contractual.
			*************************************************************************/
			void PublishAppIdFromExeDirectory()
			{
				wchar_t modulePath[MAX_PATH]{};
				if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) return;

				std::filesystem::path appIdPath =
					std::filesystem::path(modulePath).parent_path() / "steam_appid.txt";

				std::error_code existsError;
				if (!std::filesystem::exists(appIdPath, existsError)) return;

				std::ifstream file(appIdPath);
				if (!file) return;

				std::string appId;
				file >> appId;               // stops at whitespace, so a trailing newline is fine

				if (appId.empty()) return;

				for (char character : appId)
				{
					if (character < '0' || character > '9') return;
				}

				_putenv_s("SteamAppId", appId.c_str());
				_putenv_s("SteamGameId", appId.c_str());
				SetEnvironmentVariableA("SteamAppId", appId.c_str());
				SetEnvironmentVariableA("SteamGameId", appId.c_str());

				std::cout << "SteamPlatform: using App ID " << appId
					<< " from " << appIdPath.string() << std::endl;
			}
		}

		bool Init()
		{
			if (s_initialised) return true;

			// Before SteamAPI_Init, so it is in place when the SDK looks.
			PublishAppIdFromExeDirectory();

			if (!SteamAPI_Init())
			{
				// Expected whenever the Steam client is closed or nobody is
				// logged in. Not fatal: the caller falls back to Winsock.
				std::cout << "SteamPlatform: SteamAPI_Init failed "
					"(client not running, not logged in, or steam_appid.txt missing)"
					<< std::endl;
				return false;
			}

			s_initialised = true;

			// Start the SDR relay handshake now rather than at connect time.
			// It takes a few seconds and this hides them behind the menu.
			if (SteamNetworkingUtils())
			{
				SteamNetworkingUtils()->InitRelayNetworkAccess();
				SteamNetworkingUtils()->SetDebugOutputFunction(
					k_ESteamNetworkingSocketsDebugOutputType_Msg, SteamNetDebugOutput);
			}

			std::cout << "SteamPlatform: initialised as " << GetPersonaName()
				<< " (" << GetSteamIDString() << ")" << std::endl;

			return true;
		}

		void Shutdown()
		{
			if (!s_initialised) return;

			SteamAPI_Shutdown();
			s_initialised = false;

			std::cout << "SteamPlatform: shut down" << std::endl;
		}

		bool IsAvailable() { return s_initialised; }

		void RunCallbacks()
		{
			if (!s_initialised) return;
			SteamAPI_RunCallbacks();
		}

		std::string GetPersonaName()
		{
			if (!s_initialised || !SteamFriends()) return {};
			return SteamFriends()->GetPersonaName();
		}

		uint64_t GetSteamID64()
		{
			if (!s_initialised || !SteamUser()) return 0;
			return SteamUser()->GetSteamID().ConvertToUint64();
		}

		bool SetNetworkSimulation(int lossPercent, int lagMilliseconds,
			int reorderPercent, int duplicatePercent)
		{
			if (!s_initialised || !SteamNetworkingUtils()) return false;

			auto* utils = SteamNetworkingUtils();

			utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send,
				static_cast<float>(lossPercent));
			utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Recv,
				static_cast<float>(lossPercent));
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send,
				lagMilliseconds);
			utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Recv,
				lagMilliseconds);
			utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketReorder_Send,
				static_cast<float>(reorderPercent));
			utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketDup_Send,
				static_cast<float>(duplicatePercent));

			// Read one value back. Several SDK builds compile these knobs out
			// and let the setter succeed anyway; a silently inert simulator
			// turns "we tested at 5% loss" into a false claim.
			float                          readBack = -1.0f;
			size_t                         size     = sizeof(readBack);
			ESteamNetworkingConfigDataType dataType = k_ESteamNetworkingConfig_Float;

			ESteamNetworkingGetConfigValueResult result =
				utils->GetConfigValue(k_ESteamNetworkingConfig_FakePacketLoss_Send,
					k_ESteamNetworkingConfig_Global, 0,
					&dataType, &readBack, &size);

			bool const applied = (result == k_ESteamNetworkingGetConfigValue_OK
				|| result == k_ESteamNetworkingGetConfigValue_OKInherited)
				&& static_cast<int>(readBack + 0.5f) == lossPercent;

			if (!applied)
			{
				std::cout << "SteamPlatform: network simulation appears INERT in this SDK build "
					"(requested loss " << lossPercent << "%, read back " << readBack
					<< "). Shape traffic with Clumsy instead." << std::endl;
			}

			return applied;
		}

		void OpenInviteOverlay(uint64_t lobbyID)
		{
			if (!s_initialised || !SteamFriends()) return;

			CSteamID lobby(static_cast<uint64>(lobbyID));
			SteamFriends()->ActivateGameOverlayInviteDialog(lobby);
		}

#else   // ---------------------------------------------------------------
		// No Steamworks SDK in this build. Every entry point still exists so
		// callers - including the Lua bindings - need no conditional code.

		bool IsCompiledIn() { return false; }

		bool RestartAppIfNecessary(uint32_t) { return false; }

		bool Init()
		{
			std::cout << "SteamPlatform: built without the Steamworks SDK "
				"(GG_STEAM_ENABLED undefined); Steam features are unavailable"
				<< std::endl;
			return false;
		}

		void Shutdown() {}

		bool IsAvailable() { return false; }

		void RunCallbacks() {}

		std::string GetPersonaName() { return {}; }

		uint64_t GetSteamID64() { return 0; }

		bool SetNetworkSimulation(int, int, int, int) { return false; }

		void OpenInviteOverlay(uint64_t) {}

#endif  // GG_STEAM_ENABLED

		// --- Shared between both shapes ---------------------------------

		std::string GetSteamIDString()
		{
			return std::to_string(GetSteamID64());
		}

		uint64_t ParseSteamID(std::string const& text)
		{
			if (text.empty()) return 0;

			// Reject anything that is not pure digits before calling strtoull,
			// so a lobby-metadata string written by another process cannot
			// smuggle a sign, whitespace or a hex prefix past us.
			for (char character : text)
			{
				if (character < '0' || character > '9') return 0;
			}

			errno = 0;
			char*              end   = nullptr;
			unsigned long long value = std::strtoull(text.c_str(), &end, 10);

			if (errno != 0 || end == text.c_str() || *end != '\0') return 0;

			return static_cast<uint64_t>(value);
		}
	}
}
