/*************************************************************************
  \file       GGGameCoordinator.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       11 November 2025
  \version    1.1
  \brief
    Game coordinator for networked turn-based tactical game
  \details
    The GameCoordinator ensures proper synchronization between OS interrupts,
    network events, and game state updates. Critical for maintaining
    deterministic lockstep networking in Order N Chaos.
  \dependencies
    GGSystemInterface.hpp, GGInterruptHandler.hpp
  \see
    GGInterruptHandler.hpp, GameStateManager.lua
  \history
    - 1.0 (11 November 2025): Initial version.
    - 1.1 (11 November 2025): Added network coordination for multiplayer.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include "GGSystemInterface.hpp"
#include <string>
#include <queue>

/*************************************************************************
  \brief
    Coordinator for networked game - ensures proper synchronization
  \details
    CRITICAL for networked games: Coordinates OS interrupts, network
    events, and game logic in deterministic order to prevent desync.
*************************************************************************/
class GGGameCoordinator : public SystemManagerInterface
{
public:
    /*************************************************************************
      \brief
        Network connection states for coordination
    *************************************************************************/
    enum class NetworkState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        HOST_WAITING,     // Host waiting for client
        CLIENT_WAITING    // Client waiting for host
    };

    /*************************************************************************
      \brief
        Game synchronization states
    *************************************************************************/
    enum class SyncState
    {
        SYNCHRONIZED,     // Both players in sync
        WAITING_FOR_PEER, // Waiting for other player
        RESYNCING,        // Recovering from interrupt
        DESYNCED          // Critical error state
    };

private:
    // Basic game status
    bool m_isGameInitialized;
    bool m_isGameRunning;
    std::string m_currentLuaState;

    // Network coordination
    NetworkState m_networkState;
    SyncState m_syncState;
    bool m_isHost;

    // Buffering for interrupts
    bool m_shouldBufferPackets;
    std::queue<std::string> m_bufferedNetworkEvents;

public:
    /*************************************************************************
      \brief
        Constructor
    *************************************************************************/
    GGGameCoordinator();

    /*************************************************************************
      \brief
        Initializes the game coordinator
      \return
        True if initialization successful
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief
        Updates coordinator - handles priority order
      \details
        Priority order:
        1. OS Interrupts (highest)
        2. Network state changes
        3. Game logic updates (lowest)
    *************************************************************************/
    void Update() override;

    /*************************************************************************
      \brief
        Called by network system when connection state changes
      \param[in] newState
        New network connection state
    *************************************************************************/
    void OnNetworkStateChange(NetworkState newState);

    /*************************************************************************
      \brief
        Called by network system when packet received
      \param[in] packet
        Network packet data
      \return
        True if packet should be processed, false to buffer
    *************************************************************************/
    bool ShouldProcessNetworkPacket(const std::string& packet);

    /*************************************************************************
      \brief
        Called when both players need to sync state
      \details
        Used after interrupts or reconnections to ensure both
        clients have identical game state
    *************************************************************************/
    void RequestStateSync();

    /*************************************************************************
      \brief
        Called by Lua when game states change
      \param[in] newState
        The new Lua game state
    *************************************************************************/
    void NotifyStateChange(const std::string& newState);

    /*************************************************************************
      \brief
        Starts the game (both players ready)
    *************************************************************************/
    void StartGame();

    /*************************************************************************
      \brief
        Stops the game (returns to lobby/menu)
    *************************************************************************/
    void StopGame();

    /*************************************************************************
      \brief
        Gets whether game is currently running
      \return
        True if game is running (not in menus)
    *************************************************************************/
    bool IsGameRunning() const { return m_isGameRunning; }

    /*************************************************************************
      \brief
        Gets current network state
      \return
        Current NetworkState enum value
    *************************************************************************/
    NetworkState GetNetworkState() const { return m_networkState; }

    /*************************************************************************
      \brief
        Gets current sync state
      \return
        Current SyncState enum value
    *************************************************************************/
    SyncState GetSyncState() const { return m_syncState; }

    /*************************************************************************
      \brief
        Checks if we're the host
      \return
        True if host, false if client
    *************************************************************************/
    bool IsHost() const { return m_isHost; }

    /*************************************************************************
      \brief
        Sets host/client status
      \param[in] isHost
        True if this instance is host
    *************************************************************************/
    void SetHostStatus(bool isHost) { m_isHost = isHost; }

private:
    /*************************************************************************
      \brief
        Processes buffered network events after interrupt
    *************************************************************************/
    void ProcessBufferedNetworkEvents();

    /*************************************************************************
      \brief
        Validates game state consistency
      \return
        True if state is valid and consistent
    *************************************************************************/
    bool ValidateGameState();
};