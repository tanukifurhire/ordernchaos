NetworkedGameClient_instance = NetworkedGameClient_instance or nil

function ClientRPC_OnReturnDeployUnit(unitType, gridPosX, gridPosY)
    Log.info("[ClientRPC] Deployment acknowledged: unitType " .. unitType .. " at (" .. gridPosX .. ", " .. gridPosY .. ")")
    
    if ClientGameManager_instance then
        local isClientOrder = (ClientGameManager_instance.playerFaction == CONSTANTS.ORDER_PLAYER)
        local isUnitOrder = (unitType > 0) and (unitType < CONSTANTS.UNIT.IMP)

        if isClientOrder == isUnitOrder then
            -- The deploying player
            ClientGameManager_instance.awaitingServer = false
        end
    end
end

function ClientRPC_OnReturnEndDeploymentPhase(success)
    if success then return end

    Log.error("[ClientRPC] Unsuccessful in ending deployment phase")

    if ClientGameManager_instance then
        ClientGameManager_instance.awaitingServer = false
    end
end

function ClientRPC_OnEndDeploymentPhase(
    oracleX, oracleY,
    dragoonX, dragoonY,
    tirionX, tirionY,
    impX, impY,
    eyeX, eyeY,
    vizomX, vizomY)
    
    Log.info("[ClientRPC] Received all unit positions from server")
    
    -- Update unit positions for all players
    if oracleX and oracleY and
        dragoonX and dragoonY and
        tirionX and tirionY and
        impX and impY and
        eyeX and eyeY and
        vizomX and vizomY then
        
        Event.emitLocal("UpdatePosForce", {
            unitType = CONSTANTS.UNIT.ORACLE,
            x = oracleX,
            y = oracleY
        })

        Event.emitLocal("UpdatePosForce", {
            unitType = CONSTANTS.UNIT.DRAGOON,
            x = dragoonX,
            y = dragoonY
        })

        Event.emitLocal("UpdatePosForce", {
            unitType = CONSTANTS.UNIT.TIRION,
            x = tirionX,
            y = tirionY
        })

        Event.emitLocal("UpdatePosForce", {
            unitType = CONSTANTS.UNIT.IMP,
            x = impX,
            y = impY
        })

        Event.emitLocal("UpdatePosForce", {
            unitType = CONSTANTS.UNIT.EYE,
            x = eyeX,
            y = eyeY
        })

        Event.emitLocal("UpdatePosForce", {
            unitType = CONSTANTS.UNIT.VIZOM,
            x = vizomX,
            y = vizomY
        })

        if ClientGameManager_instance then
            ClientGameManager_instance:InitializeHealth()
            ClientGameManager_instance.awaitingServer = false
            if NetworkedGameClient_instance then
                NetworkedGameClient_instance.isTransitioningToGame = false
            end
            ClientGameManager_instance.turnPhases:ChangePhase(ClientGameManager_instance.turnPhases.preparationPhase)  
        end
    end
end

function ClientRPC_OnReturnLobbyJoined(lobbyName)
    Log.info("[ClientRPC] Lobby '" .. lobbyName .. "' joined!")
end

function ClientRPC_OnResolutionData(data)
    if not ClientGameManager_instance then
        Log.error("[Client] ClientRPC_OnResolutionData: no ClientGameManager_instance!")
        return
    end
    if not data then
        Log.error("[Client] Received nil resolution data!")
        return
    end
    if not data.queue then
        Log.error("[Client] Resolution data missing queue!")
        return
    end
    if type(data.queue) ~= "table" then
        Log.error("[Client] Resolution data queue is not a table!")
        return
    end
    if data.activeBuffs ~= nil and type(data.activeBuffs) ~= "table" then
        Log.warning("[Client] Resolution data activeBuffs is malformed; replacing with empty table")
        data.activeBuffs = {}
    end
    if data.syncPositions ~= nil and type(data.syncPositions) ~= "table" then
        Log.warning("[Client] Resolution data syncPositions is malformed; replacing with empty table")
        data.syncPositions = {}
    end
    if data.buffStacks ~= nil and type(data.buffStacks) ~= "table" then
        Log.warning("[Client] Resolution data buffStacks is malformed; ignoring")
        data.buffStacks = nil
    end

    Log.info("[Client] ====== RECEIVED RESOLUTION DATA ======")
    Log.info("[Client] Queue size: " .. #data.queue .. " actions")
    Log.info("[Client] SyncPositions: " .. (data.syncPositions and #data.syncPositions or "nil"))

    -- Log each action summary
    for i, result in ipairs(data.queue) do
        if type(result) == "table" and type(result.action) == "table" then
            local a = result.action
            Log.info("[Client] Action " .. i .. ": faction=" .. tostring(a.faction) ..
                " type=" .. tostring(a.actionType) ..
                " abilityID=" .. tostring(a.abilityID) ..
                " unit=" .. tostring(a.unitType) ..
                " target=(" .. tostring(a.targetX) .. "," .. tostring(a.targetY) .. ")" ..
                " damage=" .. tostring(result.damage) ..
                " hits=" .. (result.hits and #result.hits or 0))
        else
            Log.error("[Client] Action " .. i .. " is malformed")
        end
    end

    ClientGameManager_instance.resolutionData = data
    if data and data.buffStacks then
        ClientGameManager_instance.buffStacks = {
            fervor = data.buffStacks.fervor or 0,
            berserk = data.buffStacks.berserk or 0
        }
    end
    ClientGameManager_instance.awaitingServer = false
    if ClientGameManager_instance.turnPhases and ClientGameManager_instance.turnPhases.resolutionPhase then
        ClientGameManager_instance.turnPhases:ChangePhase(ClientGameManager_instance.turnPhases.resolutionPhase)
    else
        Log.error("[Client] Cannot change to resolution phase: turn phases not initialized")
    end
end

function ClientRPC_RoomCreated(roomID)
    Log.info("[ClientRPC] Room created with ID: " .. roomID)
    if NetworkedGameClient_instance then
        NetworkedGameClient_instance.isTransitioningToGame = false
        NetworkedGameClient_instance.transitionGuardUntil = 0
    end
    
    Event.emitLocal("OnRoomCreated", {
        roomID = roomID
    })
end

function ClientRPC_RoomList(roomList)
    Log.info("[ClientRPC] Received room list with " .. #roomList .. " rooms")
    local rooms = {}
    for _, entry in ipairs(roomList) do
        table.insert(rooms, {
            roomID = entry.roomID,
            name = entry.name,
            orderJoined = (entry.orderJoined == 1),
            chaosJoined = (entry.chaosJoined == 1),
            status = entry.status
        })
    end
    Event.emitLocal("OnRoomListReceived", { rooms = rooms })
end

function ClientRPC_JoinRoomSuccess(roomID, faction, roomName, opponentName)
    Log.info("[ClientRPC] Joined room " .. roomID .. " as faction " .. faction)
    if NetworkedGameClient_instance then
        NetworkedGameClient_instance.isTransitioningToGame = false
        NetworkedGameClient_instance.transitionGuardUntil = 0
    end

    Persistent.sessionRoomID = roomID
    Persistent.sessionFaction = faction
    Persistent.sessionRoomName = roomName or "Room"
    Persistent.sessionOpponentName = opponentName or ""

    Event.emitLocal("OnJoinRoomSuccess", {
        roomID = roomID,
        faction = faction,
        roomName = roomName or "Room",
        opponentName = opponentName or ""
    })
end

function ClientRPC_JoinRoomFailed(reason)
    Log.warning("[ClientRPC] Join room failed: " .. reason)
    if NetworkedGameClient_instance then
        NetworkedGameClient_instance.isTransitioningToGame = false
        NetworkedGameClient_instance.transitionGuardUntil = 0
    end
    Persistent.lastError = "Join room failed: " .. reason

    Event.emitLocal("OnJoinRoomFailed", {
        reason = reason
    })
end

function ClientRPC_RoomState(state)
    -- Update opponent name based on our faction
    if Persistent.sessionFaction == CONSTANTS.ORDER_PLAYER then
        Persistent.sessionOpponentName = state.chaosName or ""
    elseif Persistent.sessionFaction == CONSTANTS.CHAOS_PLAYER then
        Persistent.sessionOpponentName = state.orderName or ""
    end
    
    -- Normalize booleans: RPC may deserialize false as 0, which is truthy in Lua
    Event.emitLocal("OnRoomStateUpdate", {
        roomID = state.roomID,
        orderName = state.orderName,
        chaosName = state.chaosName,
        orderJoined = (state.orderJoined == true or state.orderJoined == 1),
        chaosJoined = (state.chaosJoined == true or state.chaosJoined == 1),
        orderReady  = (state.orderReady == true or state.orderReady == 1),
        chaosReady  = (state.chaosReady == true or state.chaosReady == 1)
    })
end

function ClientRPC_StartGame(roomID)
    Log.info("[ClientRPC] Game starting in room " .. roomID)

    Event.emitLocal("OnGameStarting", {
        roomID = roomID
    })
end

function ClientRPC_OnHunterMarks(hunterMarks)
    Log.info("[ClientRPC] Received Hunter's Marks")
    if ClientGameManager_instance then
        -- Normalize keys/values to numbers (RPC serialization may convert to strings)
        local normalizedMarks = {}
        if hunterMarks then
            for k, v in pairs(hunterMarks) do
                local numKey = tonumber(k) or k
                local numVal = tonumber(v) or v
                if tonumber(numKey) and tonumber(numVal) then
                    normalizedMarks[numKey] = numVal
                    Log.info("[ClientRPC] Hunter's Mark: unit " .. tostring(numKey) .. " marks enemy " .. tostring(numVal))
                else
                    Log.warning("[ClientRPC] Ignoring malformed Hunter's Mark entry key=" .. tostring(k) .. " value=" .. tostring(v))
                end
            end
        end
        ClientGameManager_instance.hunterMarks = normalizedMarks
    end
end

function ClientRPC_ArsenalConfirmed(arsenalMap)
    Log.info("[ClientRPC] Arsenal confirmed by server")
    Event.emitLocal("OnArsenalServerConfirmed", { arsenalMap = arsenalMap })
end

function ClientRPC_ArsenalBothReady(roomID, opponentArsenalMap)
    Log.info("[ClientRPC] Both players' arsenals confirmed for room " .. roomID)
    if NetworkedGameClient_instance then
        NetworkedGameClient_instance.isTransitioningToGame = true
        NetworkedGameClient_instance.transitionGuardUntil = Time.getTotalTime() + 2.0
    end

    -- Persist opponent's arsenal map so ClientGameManager can resolve enemy abilities/passives
    if opponentArsenalMap then
        -- sessionFaction first: this RPC only ever arrives in a networked
        -- match, so the seat the server gave us is the answer. playerFaction
        -- is the pass-and-play hotseat marker and can be left over from a
        -- previous game entirely - trusting it here filed the opponent's
        -- arsenal under our own faction's key, so both sides resolved
        -- abilities from one loadout.
        local faction = Persistent.sessionFaction or Persistent.playerFaction or CONSTANTS.ORDER_PLAYER
        local isOrder = (faction == CONSTANTS.ORDER_PLAYER)
        local oppKey = isOrder and "arsenalMap_chaos" or "arsenalMap_order"
        Persistent[oppKey] = opponentArsenalMap
        Log.info("[ClientRPC] Opponent arsenal persisted as " .. oppKey)
    end

    Event.emitLocal("OnArsenalBothReady", { roomID = roomID })
end

function ClientRPC_ArsenalRejected(reason)
    Log.warning("[ClientRPC] Arsenal rejected by server: " .. reason)
    Event.emitLocal("OnArsenalServerRejected", { reason = reason })
end

function ClientRPC_OpponentDisconnected()
    Log.warning("[ClientRPC] Opponent has disconnected from the match!")

    if NetworkedGameClient_instance and NetworkedGameClient_instance.isTransitioningToGame and
       Time.getTotalTime() < (NetworkedGameClient_instance.transitionGuardUntil or 0) then
        Log.info("[ClientRPC] Ignoring opponent disconnect during game/scene transition")
        return
    end

    -- The match is over for this process either way, so tear the whole session
    -- down rather than only forgetting the room.
    --
    -- This matters most for the host, because the host is also the server and
    -- the owner of the Steam lobby. Dropping them back into the browser while
    -- both were still running left their own dead lobby advertised - to
    -- everyone else, and to themselves: the browser listed it, clicking it
    -- called Steam.joinLobby on a lobby they already owned, and
    -- SteamCB_OnLobbyEntered answered "we are the host, not connecting as a
    -- client". A row that could not be joined and would not go away.
    --
    -- Deferred, NOT immediate. We are inside a ClientRPC_* handler, which runs
    -- part-way through GameClient::Update draining its message queue; calling
    -- RPC.disconnect() here frees that queue underneath the loop that is
    -- reading it, and the process dies as soon as this function returns.
    if NetworkSessionTeardownDeferred then
        NetworkSessionTeardownDeferred("opponent disconnected")
    end

    Persistent.lastError = "Opponent has disconnected from the match"

    Event.emitLocal("RequestSceneTransition", {
        scene = "M5LobbySelectionScene"
    })
end

local class = DefineType("NetworkedGameClient", 
{
    Serializable =
    {
        serverLink = "127.0.0.1",
        
        serverPort = 9081,                  -- Make port configurable
        autoConnect = false,                -- Don't auto-connect (lobby flow needs manual connect)

        lobbyName = "TestLobby",
        password = ""
    },

    connected = false,
    assignedClientID = -1,
    isTransitioningToGame = false,
    transitionGuardUntil = 0,

    Init = function(self, entity)
        NetworkedGameClient_instance = self

        -- Carry the session across the scene change. The component is new but
        -- the connection underneath it is not, so a host walking from the
        -- lobby into the arsenal must not have to reconnect.
        --
        -- This flag is a belief, not a fact - there is no way to ask the
        -- transport - so it is only as good as whoever cleared it last. That
        -- is why every route out of a session now goes through
        -- NetworkSessionResetState, and why Send corrects the belief rather
        -- than letting a stale one reach RPC.call.
        if Persistent.connected then
            Log.info("Already connected to server!")
            self.connected = true
            self.assignedClientID = Persistent.assignedClientID
        end

        -- Connection established
        Event.subscribe("OnConnectedToServer", function(params)
            Log.info("[NetworkedGameClient] Connected to server!")
            self.assignedClientID = params.clientID
            self.connected = true

            Persistent.connected = true
            Persistent.assignedClientID = params.clientID
            self.isTransitioningToGame = false
            self.transitionGuardUntil = 0
            
            Log.info("[NetworkedGameClient] Assigned client ID: " .. params.clientID)
            
            -- Emit event for UI to handle
            Event.emitLocal("OnClientConnected", {
                clientID = params.clientID
            })

            self:RequestRoomList()
        end)

        -- Connection lost
        Event.subscribe("OnDisconnectedFromServer", function(params)
            Log.info("[NetworkedGameClient] Disconnected from server")

            -- One place decides what "no session" means, so the room we were
            -- in and the faction we held go too. Leaving those behind is how a
            -- faction from a finished match turned up in the next lobby.
            NetworkSessionResetState()
            Persistent.lastError = "Disconnected from server"

            Event.emitLocal("OnClientDisconnected", {})
        end)
        
        -- Game starting - go to Arsenal Select first
        Event.subscribe("OnGameStarting", function(params)
            Log.info("[NetworkedGameClient] Game starting! Switching to arsenal select...")
            self.isTransitioningToGame = true
            self.transitionGuardUntil = Time.getTotalTime() + 2.0
            Scene.setActive("M6NetworkGameArsenalScene")
        end)
    end,

    Start = function(self, entity)
        Log.info("[NetworkedGameClient] Ready to connect")
        Log.info("[NetworkedGameClient] Server: " .. self.serverLink .. ":" .. self.serverPort)

        -- There used to be an unconditional `NetworkedGameClient:Connect(...)`
        -- here, and it was wrong twice over.
        --
        -- The name resolves to the TYPE table, not this instance, so `self`
        -- inside Connect was the class - it could not record a connection on
        -- anyone, and wrote its failure into Persistent.lastError, which is
        -- why opening the server browser flashed "Failed to connect to server"
        -- at the player every single time.
        --
        -- And it should not have been dialling at all. Every scene that
        -- carries this component ran it on entry, so a player who was NOT
        -- connected got a speculative connection to serverLink on each scene
        -- change, and RPC.connectToServer stops any existing client before
        -- starting a new one. Connections are opened deliberately: by hosting
        -- (SteamCB_OnLobbyCreated) or by joining a lobby
        -- (SteamCB_OnLobbyEntered), never by walking into a scene.
        if self.autoConnect then
            Log.info("[NetworkedGameClient] Auto-connecting...")
            self:Connect(self.serverLink, self.serverPort)
        end
    end,

    --[[
        Carries out a teardown that an RPC handler asked for.

        This exists so the transport is dropped from ordinary game-loop code
        rather than from inside GameClient::Update's message drain - see
        NetworkSessionTeardownDeferred for what happens otherwise.

        Every scene a session can end in carries a NetworkedGameClient, so the
        queued teardown always finds an Update to run on, and always within a
        frame of being asked for.
    ]]
    Update = function(self, entity)
        if NetworkSessionPumpTeardown then
            NetworkSessionPumpTeardown()
        end
    end,

    Connect = function(self, host, port)
        if self.connected or Persistent.connected then
            Log.warning("[NetworkedGameClient] Already connected")
            return true
        end
        
        host = host or self.serverLink
        port = port or self.serverPort
        
        Log.info("[NetworkedGameClient] Connecting to " .. host .. ":" .. port .. "...")
        
        if RPC.connectToServer(host, port) then
            Log.info("[NetworkedGameClient] Connection request sent")
            return true
        else
            Log.error("[NetworkedGameClient] Failed to initiate connection")
            Persistent.lastError = "Failed to connect to server"
            return false
        end
    end,
    
    Disconnect = function(self)
        if not self.connected then
            Log.warning("[NetworkedGameClient] Not connected")
            return
        end

        Log.info("[NetworkedGameClient] Disconnecting...")

        RPC.disconnect()

        -- Reset immediately rather than waiting to be told. StopClient
        -- destroys the reliability layer on the spot, so the server's
        -- disconnect response - and with it ClientRPC_OnDisconnectedFromServer
        -- - can never arrive.
        NetworkSessionResetState()
    end,

    --[[
        Sends one server-bound call, and never lets it take the game with it.

        self.connected is a belief about the transport, not the transport
        itself - it is restored from Persistent when a scene loads, so anything
        that stops the client without clearing it leaves this component saying
        "connected" about a socket that is gone. RPC.call does not tolerate
        that: with neither a client nor a server running it raises, and every
        one of these methods is reached from OnClick, where the engine treats a
        raised error as a script fault and pauses. A stale flag was therefore
        one click away from taking the process down.

        The pcall is the part that matters - a failed call should cost a click,
        not a session. Correcting the flag is done only for the one error that
        actually means the session is gone; anything else (a value that will
        not serialize, say) is a bug in the caller and must not tear down a
        connection that is working perfectly well.
    ]]
    Send = function(self, handlerName, ...)
        if not self.connected then
            Log.error("[NetworkedGameClient] " .. handlerName .. " needs a connection")
            return false
        end

        local ok, err = pcall(RPC.call, handlerName, self.assignedClientID, ...)

        if ok then return true end

        err = tostring(err)
        Log.error("[NetworkedGameClient] " .. handlerName .. " failed: " .. err)

        if string.find(err, "No client or server", 1, true) then
            Log.error("[NetworkedGameClient] The session is gone; clearing it")
            Persistent.lastError = "Lost connection to the server"

            NetworkSessionResetState()
            Event.emitLocal("OnDisconnectedFromServer", {})
        end

        return false
    end,
    
    CreateRoom = function(self, playerName, roomName, faction)
        self.isTransitioningToGame = false
        self.transitionGuardUntil = 0
        Log.info("[NetworkedGameClient] Creating room: " .. roomName)
        return self:Send("ServerRPC_CreateRoom", playerName, roomName, faction)
    end,
    
    JoinRoom = function(self, playerName, roomID, faction)
        self.isTransitioningToGame = false
        self.transitionGuardUntil = 0
        Log.info("[NetworkedGameClient] Joining room " .. roomID .. " as faction " .. faction)
        return self:Send("ServerRPC_JoinRoom", playerName, roomID, faction)
    end,
    
    LeaveRoom = function(self, roomID)
        self.isTransitioningToGame = false
        self.transitionGuardUntil = 0
        Log.info("[NetworkedGameClient] Leaving room " .. roomID)
        return self:Send("ServerRPC_LeaveRoom", roomID)
    end,
    
    SetReady = function(self, roomID, isReady)
        Log.info("[NetworkedGameClient] Setting ready state: " .. tostring(isReady))
        return self:Send("ServerRPC_SetReady", roomID, isReady and 1 or 0)
    end,
    
    RequestRoomList = function(self)
        Log.info("[NetworkedGameClient] Requesting room list")
        return self:Send("ServerRPC_RequestRoomList")
    end,

    --[[
        Asks for the current state of a room we are already in.

        Room state is only broadcast when it changes, so a scene that loads
        after the last change never hears about it. The lobby room is exactly
        that case: the host creates a room and walks into a scene that missed
        its own join.
    ]]
    RequestRoomState = function(self, roomID)
        if not roomID or roomID < 0 then return false end
        return self:Send("ServerRPC_RequestRoomState", roomID)
    end,

    StartGame = function(self, roomID)
        Log.info("[NetworkedGameClient] Requesting server to start game for room " .. roomID)
        return self:Send("ServerRPC_StartGame", roomID)
    end,
    
    SubmitArsenal = function(self, roomID, arsenalMap)
        Log.info("[NetworkedGameClient] Submitting arsenal to server for room " .. roomID)
        return self:Send("ServerRPC_SubmitArsenal", roomID, arsenalMap)
    end,

    CreateOrJoinLobby = function(self, lobbyName, password, faction)
        Log.info("[NetworkedGameClient] Joining lobby: " .. lobbyName)
        return self:Send("ServerRPC_CreateOrJoinLobby", lobbyName, password, faction)
    end
})

return class
