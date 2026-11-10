/*************************************************************************
  \file       PlatformWindows.cpp
  \author Tai Xiao Xuan, Sherwyn Low Yuen Kai, Teo Pek Lin
  \par    email: xiaoxuan.tai@digipen.edu, sherwynyuenkai.low@digipen.edu, t.peklin@digipen.edu
  \date   17 Mar 2026
  \version    1.0
  \brief
	Windows Sockets (Winsock) initialization and cleanup utilities
  \details
	Provides platform-specific networking initialization for Windows via
	Winsock 2.2 API. Wraps WSAStartup and WSACleanup with error handling and
	version validation. Must be called before any socket operations on Windows.
	Logs detailed diagnostic information via std::cout and std::cerr for
	debugging network stack initialization failures.

	Reference-counted by Winsock - supports multiple init/shutdown pairs safely.
  \dependencies
	PlatformWindows.hpp, <WinSock2.h>, <WS2tcpip.h>, <iostream>
  \see
	PlatformWindows.hpp, WSAStartup, WSACleanup, NetworkUtils
  \copyright
	Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include <PlatformWindows.hpp>

#include <iostream>

namespace GGNetworking
{
	namespace NetworkUtils
	{
		/*************************************************************************
		  \brief
			Initializes Windows Sockets API (Winsock) version 2.2 for network operations.
		  \return
			true if WSAStartup succeeds and correct Winsock version is available,
			false on initialization failure or version mismatch.
		  \details
			Calls WSAStartup to load Winsock DLL and verifies version 2.2 compatibility.
			Must be called before any socket operations (socket creation, bind, connect).
			Logs detailed error messages via std::cerr if initialization fails.
			Multiple calls are reference-counted by Winsock (safe redundant calls).
		  \note
			Each successful call must be paired with ShutdownWindowsNetworking().
			Failure typically indicates missing network drivers or incompatible Winsock.
		  \see
			ShutdownWindowsNetworking, WSAStartup, WSADATA
		*************************************************************************/
		bool InitWindowsNetworking()
		{
			int err;

			WSADATA wsadata;
			err = WSAStartup(MAKEWORD(2, 2), &wsadata);

			if (err != 0) {
				std::cerr << "WSAStartup failed with error " << err << std::endl;
				return false;
			}
			std::cout << "WSAStartup Success!" << std::endl;

			if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2) {
				std::cerr << "Could not find a usable version of Winsock.dll" << std::endl;
				return false;
			}
			std::cout << "The Winsock 2.2 dll was found okay" << std::endl;

			return true;
		}

		/*************************************************************************
		  \brief
			Releases Winsock resources and unloads Winsock DLL.
		  \details
			Calls WSACleanup to decrement Winsock reference count and free resources
			when count reaches zero. Must be called after all sockets are closed and
			network operations complete. Logs errors via std::cerr if cleanup fails.
		  \warning
			Do not call while sockets are still open or network threads are running.
			Unbalanced calls (more Shutdown than Init) may cause premature resource
			release and undefined behavior when accessing Winsock functions.
		  \see
			InitWindowsNetworking, WSACleanup
		*************************************************************************/
		void ShutdownWindowsNetworking()
		{
			int32_t iResult = WSACleanup();
			if (iResult == SOCKET_ERROR)
			{
				std::cerr << "WSACleanup failed with error: " << WSAGetLastError() << std::endl;
			}
		}
	}
}