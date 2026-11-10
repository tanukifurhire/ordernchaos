/*************************************************************************
  \file       GGGameCoordinator.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       11 November 2025
  \version    2.0
  \brief
    Simplified coordinator - OS interrupts handled in C++ only
  \details
    Coordinates network events and game updates. OS interrupts are
    handled transparently - Lua doesn't need to know about them.
  \dependencies
    GGGameCoordinator.hpp, GGInterruptHandler.hpp, GGEngine.hpp
  \see
    GGGameCoordinator.hpp
  \history
    - 1.0 (11 November 2025): Initial implementation
    - 2.0 (11 November 2025): Removed unnecessary Lua OS callbacks
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGGameCoordinator.hpp"
#include "GGInterruptHandler.hpp"
#include "GGEngine.hpp"
#include "GGSystemManagers.hpp"
#include "GameClient.hpp"
#include "GameServer.hpp"
#include <iostream>

/*************************************************************************
  \brief
    Constructor
*************************************************************************/
GGGameCoordinator::GGGameCoordinator()
    : m_isGameInitialized(false)
    , m_isGameRunning(false)
    , m_currentLuaState("uninitialized")
    , m_networkState(NetworkState::DISCONNECTED)
    , m_syncState(SyncState::SYNCHRONIZED)
    , m_isHost(false)
    , m_shouldBufferPackets(false)
{
}

/*************************************************************************
  \brief
    Initializes the game coordinator
  \return
    True if initialization successful
*************************************************************************/
bool GGGameCoordinator::Init()
{
    std::cout << "[GameCoordinator] Initializing" << std::endl;

    // Check if we're host or client
    auto& gameServer = GGNetworking::GetGameServer();
    m_isHost = (gameServer && gameServer->IsRunning());

    // Initialize Lua game state manager
    auto* luaVM = GGEngine::GetEngine().GetLuaVM();
    if (luaVM)
    {
        GGEngine::GetEngine().CallLuaFunction("InitializeGameStateManager", {});
        m_isGameInitialized = true;
        return true;
    }

    std::cerr << "[GameCoordinator] No Lua VM available" << std::endl;
    return false;
}

/*************************************************************************
  \brief
    Updates coordinator with priority handling
*************************************************************************/
void GGGameCoordinator::Update()
{
    // ========================================================
    // PRIORITY 1: OS Interrupts - Just skip if paused
    // ========================================================
    if (GGInterruptHandler::GetInstance()->IsSystemPaused())
    {
        // Don't update ANYTHING during OS interrupt
        // Lua doesn't need to know - just stop calling it
        return;
    }

    // ========================================================
    // PRIORITY 2: Network State Check
    // ========================================================
    if (!m_isHost)
    {
        auto& client = GGNetworking::GetGameClient();
        if (client && !client->IsConnected())
        {
            if (m_isGameRunning)
            {
                StopGame();
                // Only notify Lua about game-relevant events
                GGEngine::GetEngine().CallLuaFunction("OnConnectionLost", {});
            }
            return;
        }
    }

    // ========================================================
    // PRIORITY 3: Game Logic Update (if everything OK)
    // ========================================================
    if (m_isGameInitialized && m_isGameRunning)
    {
        auto& timeManager = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
        float deltaTime = static_cast<float>(timeManager.GetDeltaTime());

        // Normal game update - only happens when focused
        GGEngine::GetEngine().CallLuaFunction("UpdateGameState",
            { std::to_string(deltaTime) });
    }
}

/*************************************************************************
  \brief
    Handles network state changes
*************************************************************************/
void GGGameCoordinator::OnNetworkStateChange(NetworkState newState)
{
    [[maybe_unused]] NetworkState oldState = m_networkState;
    m_networkState = newState;

    // Only notify Lua about game-relevant network events
    if (newState == NetworkState::DISCONNECTED && m_isGameRunning)
    {
        GGEngine::GetEngine().CallLuaFunction("OnConnectionLost", {});
    }
    else if (newState == NetworkState::CONNECTED && !m_isGameRunning)
    {
        GGEngine::GetEngine().CallLuaFunction("OnConnected", {});
    }
}

/*************************************************************************
  \brief
    Checks if network packet should be processed now
*************************************************************************/
bool GGGameCoordinator::ShouldProcessNetworkPacket(const std::string& packet)
{
    // Buffer packets during OS interrupts
    if (GGInterruptHandler::GetInstance()->IsSystemPaused())
    {
        m_bufferedNetworkEvents.push(packet);
        return false;
    }
    return true;
}

void GGGameCoordinator::RequestStateSync()
{
}

void GGGameCoordinator::NotifyStateChange(const std::string& newState [[maybe_unused]])
{
}

/*************************************************************************
  \brief
    Starts the game
*************************************************************************/
void GGGameCoordinator::StartGame()
{
    if (!m_isGameRunning && m_networkState == NetworkState::CONNECTED)
    {
        m_isGameRunning = true;
        GGEngine::GetEngine().CallLuaFunction("StartGame", {});
    }
}

/*************************************************************************
  \brief
    Stops the game
*************************************************************************/
void GGGameCoordinator::StopGame()
{
    if (m_isGameRunning)
    {
        m_isGameRunning = false;
        GGEngine::GetEngine().CallLuaFunction("ReturnToMainMenu", {});
    }
}

void GGGameCoordinator::ProcessBufferedNetworkEvents()
{
}

bool GGGameCoordinator::ValidateGameState()
{
    return false;
}
