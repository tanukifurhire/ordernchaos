--[[
    SteamNetworkCallbacks.lua

    Where every asynchronous Steam answer lands.

    ---------------------------------------------------------------------------
    Attach this to an entity in any scene that can host or join a game.
    ---------------------------------------------------------------------------
    These are plain globals, not methods, because the engine calls them by name
    from C++ (SteamLobby.cpp). They exist as soon as this file is loaded and do
    not need an instance.

    ---------------------------------------------------------------------------
    Naming: SteamCB_, never ServerRPC_ or ClientRPC_
    ---------------------------------------------------------------------------
    These are engine-to-Lua calls, not network RPCs. The RPC registry sweeps
    _G for ServerRPC_* and ClientRPC_* and publishes everything it finds as a
    remotely callable entry point. Naming a lobby callback with either of those
    prefixes would hand a remote peer the ability to invoke it with arguments
    of their choosing. SteamCB_ keeps them off the wire entirely.

    ---------------------------------------------------------------------------
    IDs are STRINGS
    ---------------------------------------------------------------------------
    Every lobbyId and steamId arrives as a string and must stay one. LuaJIT
    numbers are doubles; a SteamID64 needs 64 bits and loses its low bits above
    2^53. tonumber() on one of these is a silent, intermittent corruption bug -
    it only bites on some IDs, which is the worst way to find out.

    ---------------------------------------------------------------------------
    The handoff
    ---------------------------------------------------------------------------
    Lobbies and transport are separate steps, in this order:

      Host:    Steam.createLobby()  ->  SteamCB_OnLobbyCreated
                                        -> RPC.startSteamHost()

      Client:  Steam.joinLobby(id)  ->  SteamCB_OnLobbyEntered
                                        -> RPC.connectToSteamHost(hostSteamId)

    After that the existing CLIENT_CONNECT_REQUEST handshake runs unchanged:
    same PlayerID assignment, same ClientRPC_OnConnectedToServer, same
    everything above the transport.
]]

--[[
    Our lobby was created (or creation failed). We are the host.

    lobbyId  string  the new lobby, "0" on failure
    success  bool
]]
function SteamCB_OnLobbyCreated(lobbyId, success)
    if not success then
        Log.error("[Steam] Lobby creation failed")
        Event.emitLocal("OnSteamLobbyFailed", { reason = "create" })
        return
    end

    Log.info("[Steam] Lobby " .. lobbyId .. " created, starting the host transport")

    -- Only now. The lobby had to exist first: it is what advertises our
    -- SteamID to joiners, and what the transport's accept gate checks
    -- membership against before letting anyone in.
    local server = NetworkedGameServer_instance

    if not server then
        Log.error("[Steam] No NetworkedGameServer in this scene - cannot host. "
            .. "Attach one with autoStart = false to the scene you host from.")
        Steam.leaveLobby()
        Event.emitLocal("OnSteamLobbyFailed", { reason = "noserver" })
        return
    end

    if not server:StartListenServer() then
        Log.error("[Steam] Lobby is up but the host transport failed to start")
        Steam.leaveLobby()
        Event.emitLocal("OnSteamLobbyFailed", { reason = "transport" })
        return
    end

    -- Now join our own game as a player.
    --
    -- Over loopback, not Steam: this process cannot ConnectP2P to its own
    -- SteamID. StartListenServer opened a Winsock listener alongside the Steam
    -- one for exactly this, so the host's client half arrives on 127.0.0.1
    -- while remote players arrive over the relay. Above the transport the two
    -- are indistinguishable - same handshake, same PlayerID assignment, same
    -- ClientRPC_OnConnectedToServer.
    local client = NetworkedGameClient_instance

    if client then
        if not client:Connect("127.0.0.1", server.serverPort) then
            Log.error("[Steam] Host could not connect to its own server")
            Event.emitLocal("OnSteamLobbyFailed", { reason = "loopback" })
            return
        end
    else
        -- Hosting without a local client is legitimate - that is the
        -- dedicated-server arrangement - so this is a warning, not an error.
        Log.warning("[Steam] No NetworkedGameClient in this scene; "
            .. "hosting without playing")
    end

    Event.emitLocal("OnSteamLobbyReady", { lobbyId = lobbyId, isHost = true })
end

--[[
    The lobby list came back.

    lobbies  array of { id, hostSteamId, name, players, maxPlayers }
             All ids are strings. Already filtered to this game and this build
             version by the engine, so every row here is joinable in principle.
]]
function SteamCB_OnLobbyList(lobbies)
    Log.info("[Steam] " .. #lobbies .. " lobby/lobbies found")

    if UI_LobbySelection_instance then
        UI_LobbySelection_instance:PopulateSteamLobbies(lobbies)
    end

    Event.emitLocal("OnSteamLobbyList", { lobbies = lobbies })
end

--[[
    We entered a lobby - by browsing, by invite, or from the friends list.

    lobbyId      string
    hostSteamId  string  who to connect to
    success      bool
]]
function SteamCB_OnLobbyEntered(lobbyId, hostSteamId, success)
    if not success then
        Log.error("[Steam] Failed to enter lobby " .. lobbyId)
        Event.emitLocal("OnSteamLobbyFailed", { reason = "join" })
        return
    end

    Log.info("[Steam] Entered lobby " .. lobbyId .. ", host is " .. hostSteamId)

    -- The host enters its own lobby too, and must not connect to itself:
    -- Steam P2P has no loopback, so the attempt would simply hang.
    if Steam.isHost() then
        Log.info("[Steam] We are the host, not connecting as a client")

        -- Disarm the one-click follow-up. It was set by whichever browser row
        -- was clicked, and if it survives here it stays armed against the next
        -- room list to arrive - joining the player into some unrelated room
        -- they never picked.
        if UI_LobbySelection_instance then
            UI_LobbySelection_instance.pendingAutoJoin = false
        end
        return
    end

    if RPC.connectToSteamHost(hostSteamId) then
        Event.emitLocal("OnSteamLobbyReady", { lobbyId = lobbyId, isHost = false })
    else
        Log.error("[Steam] Could not open a P2P connection to the host")
        Steam.leaveLobby()
        Event.emitLocal("OnSteamLobbyFailed", { reason = "transport" })
    end
end

--[[
    Somebody joined or left the lobby. Lobby membership only - a player is not
    in the game until the connect handshake completes and
    ServerRPC_OnClientConnected fires.

    steamId      string
    joined       bool
    memberCount  number
]]
function SteamCB_OnLobbyMemberChanged(steamId, joined, memberCount)
    Log.info("[Steam] Lobby member " .. steamId .. (joined and " joined" or " left")
        .. " (" .. memberCount .. " in lobby)")

    Event.emitLocal("OnSteamLobbyMembersChanged", {
        steamId = steamId,
        joined = joined,
        memberCount = memberCount
    })
end

--[[
    The host changed lobby metadata.
]]
function SteamCB_OnLobbyDataChanged(lobbyId)
    Event.emitLocal("OnSteamLobbyDataChanged", { lobbyId = lobbyId })
end

--[[
    A friend invited us, or picked "Join Game" from the friends list.

    The engine has already started joining by the time this runs - the point of
    the callback is to let the UI move somewhere sensible, since the player may
    be anywhere in the game when an invite is accepted.
]]
function SteamCB_OnJoinRequested(lobbyId)
    Log.info("[Steam] Invite accepted for lobby " .. lobbyId)

    Event.emitLocal("RequestSceneTransition", {
        scene = "M5LobbySelectionScene"
    })

    Event.emitLocal("OnSteamJoinRequested", { lobbyId = lobbyId })
end

local class = DefineType("SteamNetworkCallbacks", {

    Start = function(self, entity)
        -- Not required for the callbacks above to work - they are globals and
        -- exist as soon as this file loads. This just makes the state visible
        -- in the log when the scene comes up.
        if Steam.isCompiledIn() then
            Log.info("[Steam] transport = " .. Steam.getTransport()
                .. ", available = " .. tostring(Steam.isAvailable()))
        else
            Log.info("[Steam] not compiled into this build; Winsock transport only")
        end
    end
})

return class
