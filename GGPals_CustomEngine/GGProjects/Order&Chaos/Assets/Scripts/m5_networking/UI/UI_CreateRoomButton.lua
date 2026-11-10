local class = DefineType("CreateRoomButton", {
    Serializable = {
        textInputName = "MyTextInput",
        playerInputName = "MyTextInput"
    },
    
    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },
    
    roomName = "",

    -- Set when the player asks for a room before a connection exists. Hosting
    -- has to happen first, and it is asynchronous, so the request is parked
    -- here and replayed once we are connected.
    pendingRoom = nil,

    Init = function(self, entity)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end

        Event.subscribe("TextInputSubmit", function(params)
            if params and params.target == self.textInputName then
                self.roomName = params.text
            elseif params and params.target == self.playerInputName then
                Persistent.sessionPlayerName = params.text
                Log.info("Player Name set to: " .. Persistent.sessionPlayerName)
            end
        end)

        -- Replay a parked room request once the connection is live.
        --
        -- OnClientConnected rather than OnConnectedToServer: the former is
        -- emitted by NetworkedGameClient *after* it sets connected = true, and
        -- CreateRoom refuses to run before that. Note the host receives this
        -- event twice - once from its client half, once from its server half
        -- seeing a client arrive - so pendingRoom is cleared before use and the
        -- second one finds nothing to do.
        Event.subscribe("OnClientConnected", function(params)
            local request = self.pendingRoom
            if not request then return end

            local client = NetworkedGameClient_instance
            if not client or not client.connected then return end

            self.pendingRoom = nil

            Log.info("[CreateRoomButton] Connected - creating the parked room "
                .. request.roomName)
            client:CreateRoom(request.playerName, request.roomName, request.faction)
        end)

        Event.subscribe("OnSteamLobbyFailed", function(params)
            if self.pendingRoom then
                Log.error("[CreateRoomButton] Hosting failed ("
                    .. tostring(params and params.reason) .. "), room not created")
                self.pendingRoom = nil
            end
        end)
    end,
    
    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY
        })

        if self.hoverTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.hoverTexture })
        end
    end,
    
    OnHoverExit = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY
        })
        
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,
    
    OnClick = function(self, entity, x, y)
        Event.emitLocal("PlayMainMenuClick", {})
        Log.info("[CreateRoomButton] Creating room " .. self.roomName)
        
        if not NetworkedGameClient_instance then
            Log.error("[CreateRoomButton] No client instance")
            return
        end
        
        local playerName = Persistent.sessionPlayerName or "Player"
        if playerName == "" then
            playerName = "Player"
        end

        local roomName = self.roomName
        if roomName == "" then
            roomName = "Room_" .. math.random(1000, 9999)
        end

        -- Already in a session: this is purely the game's own room layer.
        if NetworkedGameClient_instance.connected then
            NetworkedGameClient_instance:CreateRoom(playerName, roomName, Persistent.sessionFaction)
            return
        end

        -- Not connected. A "room" lives on top of a connection, so there is
        -- nothing to create one on yet - somebody has to be hosting first.
        --
        -- These are two different layers and it is worth keeping them apart:
        -- a Steam lobby is matchmaking (it is how a joiner discovers the host
        -- and opens a P2P connection), while a room is the game's own concept
        -- holding factions, readiness and arsenals. Hosting produces the
        -- connection; the room is then created over it.
        local lobby = UI_LobbySelection_instance

        if lobby and lobby.useSteam then
            if not Steam.isAvailable() then
                Log.error("[CreateRoomButton] Steam is not available - cannot host. "
                    .. "Is the Steam client running and logged in?")
                return
            end

            -- Park the request; SteamCB_OnLobbyCreated brings the server up and
            -- connects this process to it, and the OnClientConnected handler
            -- above replays this.
            self.pendingRoom = {
                playerName = playerName,
                roomName = roomName,
                faction = Persistent.sessionFaction
            }

            Log.info("[CreateRoomButton] Not connected - hosting a Steam lobby first")
            lobby:HostSteamLobby()
            return
        end

        -- Direct path, and no connection. Previously this fell out of the
        -- bottom of an if with no else, so the click logged "Creating room" and
        -- then silently did nothing at all.
        Log.error("[CreateRoomButton] Not connected to a server, so there is nothing "
            .. "to create a room on. Either tick useSteam on the LobbySelection "
            .. "component to host over Steam, or connect to a server first.")
    end
})

return class