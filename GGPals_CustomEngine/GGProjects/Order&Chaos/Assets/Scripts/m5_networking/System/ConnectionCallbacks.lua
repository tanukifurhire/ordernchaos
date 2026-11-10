--[[
    Forgets every Lua-side trace of a session.

    This has to be explicit because nothing else does it. RPC.disconnect() maps
    onto GameClient::StopClient(), which tears the reliability layer down on
    the spot - it never calls ClientRPC_OnDisconnectedFromServer, and the
    server's disconnect response can no longer arrive to trigger it either. So
    the flags describing the connection outlived the connection.

    That is what broke "finish a match, start another one". The next scene's
    NetworkedGameClient:Init read Persistent.connected, believed it was still
    connected, and skipped connecting. Clicking Create Room then went straight
    to RPC.call on a transport that no longer existed, which raises
    "No client or server available" from inside OnClick - and the engine
    reports a script error there by pausing, which is what the crash was.

    The faction goes too, and playerFaction with it. That one is the
    pass-and-play hotseat marker, but ClientGameManager reads it when seating
    the local player, so a value carried over from a finished game is how
    "host as Order" produced a Chaos player.

    Session identity is cleared but the player's name is not: that is something
    they typed, not something the connection owned. Server-side room
    bookkeeping is not cleared here either - see NetworkSessionResetServer.
]]
function NetworkSessionResetState()
    Persistent.connected = false
    Persistent.assignedClientID = -1

    Persistent.sessionRoomID = nil
    Persistent.sessionRoomName = nil
    Persistent.sessionFaction = nil
    Persistent.sessionOpponentName = nil
    Persistent.playerFaction = nil

    -- The component may already have gone with its scene, which leaves this
    -- global pointing at freed memory. A nil check cannot see that - only
    -- touching it can - so the writes go through pcall.
    local client = NetworkedGameClient_instance
    if client then
        pcall(function()
            client.connected = false
            client.assignedClientID = -1
            client.isTransitioningToGame = false
            client.transitionGuardUntil = 0
        end)
    end
end

--[[
    Throws away the room list this process was serving.

    Separate from the client reset above because the two have different
    owners and different lifetimes. Losing our own connection says nothing
    about the rooms we host - a host whose client half drops still has peers
    to close out cleanly - so this runs only when the server itself is going
    down.

    It has to run at all because the list lives in Persistent, which is what
    lets it survive the scene changes a listen server walks through. It
    survives the end of the match just as happily: the next time this player
    hosts, ServerRPC_CreateRoom looks them up by client ID, finds them still
    sitting in last match's room, and answers with the faction they held then
    rather than the one they just chose.
]]
function NetworkSessionResetServer()
    Persistent.serverLobbyList = {}
    Persistent.serverConnectedClients = {}
end

--[[
    Tears down everything this process owns of a networked session.

    One helper because there are several ways out of a game - the win banner,
    the settings menu, the lobby back button - and each previously did a
    different subset. On a listen server that matters more than it sounds: the
    host owns a client, a server AND a Steam lobby, and leaving any of them
    running strands remote players in a match nobody is serving, or leaves a
    dead lobby advertised in everyone's browser.

    Order is deliberate:
      1. disconnect the client, so the server sees a clean leave rather than a
         timeout
      2. leave the lobby, so browsers stop listing it before it stops working
      3. stop the server, which closes remaining peers with linger so their
         disconnect notices actually flush
      4. forget the session, so the next one starts from nothing

    Every step is safe when it does not apply - a joiner is not hosting, a LAN
    game has no lobby - so callers need no conditionals.

    Safe from game-loop code only: buttons, Update, event handlers reached from
    those. From inside a ClientRPC_* handler use NetworkSessionTeardownDeferred
    instead, for the reason spelled out there.
]]
function NetworkSessionTeardown(reason)
    Log.info("[Network] Tearing down session" .. (reason and (": " .. reason) or ""))

    RPC.disconnect()

    if Steam.isAvailable() and Steam.getLobbyId() ~= "0" then
        Steam.leaveLobby()
    end

    if RPC.isHosting() then
        RPC.stopServer()
    end

    -- Last, not first: stopping the server closes each peer, and the
    -- disconnect cleanup that runs for them still wants the room list.
    NetworkSessionResetState()
    NetworkSessionResetServer()
end

--[[
    NEVER CALL NetworkSessionTeardown FROM A ClientRPC_* HANDLER. Call this.

    Those handlers are dispatched from inside GameClient::Update, part-way
    through draining the incoming message queue - and the statement after the
    dispatch is `reliabilityLayer->coldMessages.pop()`. RPC.disconnect() maps
    onto StopClient(), which does `reliabilityLayer.reset()`. Tearing down from
    inside a handler therefore frees the queue the loop is standing on, and the
    process dies the moment the handler returns, with nothing in the log after
    the teardown's own success lines.

    The engine guards its own two versions of this (search GameClient.cpp for
    "must exit immediately") by returning straight out of Update. Lua cannot -
    it does not own that stack frame - so the only safe move is to not do it
    yet.

    The Lua-side state is cleared immediately, because that touches nothing the
    network layer owns and the rest of the frame should already see the session
    as over. Only the sockets wait, and only until the next frame.
]]
NetworkSessionPendingTeardown = nil

function NetworkSessionTeardownDeferred(reason)
    Log.info("[Network] Session teardown queued for the next frame"
        .. (reason and (": " .. reason) or ""))

    NetworkSessionResetState()
    NetworkSessionPendingTeardown = reason or "deferred"
end

--[[
    Runs a queued teardown. Called from NetworkedGameClient's Update, which is
    ordinary game-loop code - the message queue is not being iterated there, so
    dropping the transport is safe.
]]
function NetworkSessionPumpTeardown()
    local reason = NetworkSessionPendingTeardown
    if not reason then return end

    -- Cleared first: NetworkSessionTeardown must not be able to re-enter this.
    NetworkSessionPendingTeardown = nil
    NetworkSessionTeardown(reason)
end

-- Callback for when a Client is connected (Only called on Server!)
-- Backend will issue a clientID (int) to the incoming client.
function ServerRPC_OnClientConnected(clientID)
    Log.info("[ServerRPC] Client " .. clientID .. " is connected!")

    -- Track the client here, not in a component subscriber. This function is
    -- dispatched from C++ and exists no matter which scene the host is in; a
    -- component only exists in the scene it was placed in, and on a listen
    -- server the host moves through several while peers are still joining.
    if ServerTrackClient then ServerTrackClient(clientID) end

    Event.emitLocal("OnClientConnected", {
        client = clientID
    })
end

-- Callback for when Client is successfully connected to server 
-- (Only called on Client!)
function ClientRPC_OnConnectedToServer(assignedClientID)
    Log.info("[ClientRPC] Connected to Server successfully!")
    Event.emitLocal("OnConnectedToServer", {
        clientID = assignedClientID
    })
end

-- Callback for when Client is disconnected (Only called on Server!)
-- Backend will inform Lua of the particular clientID that disconnected.
function ServerRPC_OnClientDisconnected(clientID)
    Log.info("[ServerRPC] Client " .. clientID .. " disconnected!")

    -- Same reasoning as the connect side: the cleanup has to run wherever the
    -- host happens to be standing, and the NetworkedGameServer component only
    -- exists in two of the five scenes a host walks through. Doing this in a
    -- component subscriber meant a player who left while the host was sitting
    -- in the lobby room or the arsenal was never removed from the room - so
    -- the room kept a phantom occupant, and never deleted itself when the last
    -- real player left.
    if ServerHandleClientDisconnected then ServerHandleClientDisconnected(clientID) end

    -- Emitted after the authoritative cleanup, purely so scene-side UI can
    -- react. Nothing that matters may depend on a subscriber existing.
    Event.emitLocal("OnClientDisconnected", {
        client = clientID
    })

    if ServerUntrackClient then ServerUntrackClient(clientID) end
end

-- Callback for when Client is disconnected (Only called on Client when
-- Server forcefully disconnects Client - to gracefully handle disconnections)
function ClientRPC_OnDisconnectedFromServer()
    Log.info("[ClientRPC] Disconnected from Server!")
    Event.emitLocal("OnDisconnectedFromServer", {})
end

local class = DefineType("ConnectionCallbacks", {

})

return class