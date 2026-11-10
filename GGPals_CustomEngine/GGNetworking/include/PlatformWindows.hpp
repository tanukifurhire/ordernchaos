/*************************************************************************
  \file       PlatformWindows.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       17 Mar 2026
  \version    1.0
  \brief
    Windows networking platform layer header with Winsock declarations
  \details
    Provides Windows-specific networking setup via Winsock 2.2 API wrappers.
    Disables deprecated Winsock warnings via _WINSOCK_DEPRECATED_NO_WARNINGS.
    Includes necessary Windows headers (WinSock2.h, WS2tcpip.h) and links
    ws2_32.lib for socket functions. Declares InitWindowsNetworking and
    ShutdownWindowsNetworking in NetworkUtils namespace for cross-platform
    compatibility (Linux version would use POSIX sockets).

    Must be included before any UDP networking headers to ensure proper
    Winsock definitions.
  \dependencies
    <WinSock2.h>, <WS2tcpip.h>, <string>
  \see
    PlatformWindows.cpp, NetworkUtils, WSAStartup, WSACleanup
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

// Disabling winsock deprecation warnings
//#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WS2tcpip.h>
#include <WinSock2.h>

#pragma comment (lib, "ws2_32.lib")

#include <string>

namespace GGNetworking
{
	namespace NetworkUtils
	{
        /*************************************************************************
          \brief
            Initializes Winsock2 networking subsystem.
          \return
            true if WSAStartup succeeds, false on failure.
          \details
            Calls WSAStartup with version 2.2 request. Must be called before any
            socket operations. Failure typically indicates missing network drivers
            or incompatible Winsock version. Logs error details via GetLastNetworkError.
          \note
            Multiple calls are reference-counted by Winsock (safe to call redundantly).
            Ensure corresponding ShutdownNetworking call for each successful Init.
          \see
            ShutdownNetworking, WSAStartup
        *************************************************************************/
		bool InitWindowsNetworking();

        /*************************************************************************
          \brief
            Cleans up Winsock2 networking subsystem.
          \details
            Calls WSACleanup to release Winsock resources. Must be called after all
            sockets are closed and networking operations complete. Unbalanced calls
            (more Shutdown than Init) may cause premature resource release.
          \warning
            Do not call while sockets are still open or network threads are running.
            Undefined behavior results from accessing closed Winsock handles.
          \see
            InitializeNetworking, WSACleanup
        *************************************************************************/
        void ShutdownWindowsNetworking();
	}
}