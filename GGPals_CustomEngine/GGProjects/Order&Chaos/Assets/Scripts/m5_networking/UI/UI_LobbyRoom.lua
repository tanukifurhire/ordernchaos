UI_LobbyRoom_instance = nil

local class = DefineType("LobbyRoom", {
    Serializable = {
        roomNameTextEntity = 0
    },
    
    localFaction = 1,
    
    Init = function(self, entity)
        UI_LobbyRoom_instance = self

        Event.subscribe("OnJoinRoomSuccess", function(params)
            Persistent.sessionRoomID = params.roomID
            self.localFaction = params.faction
            Event.emitLocal("OnLobbyRoomStateChanged", {
                orderName    = "",
                chaosName    = "",
                orderJoined  = (params.faction == CONSTANTS.ORDER_PLAYER),
                chaosJoined  = (params.faction == CONSTANTS.CHAOS_PLAYER),
                orderReady   = false,
                chaosReady   = false,
                localFaction = params.faction
            })
        end)
        
        Event.subscribe("OnRoomStateUpdate", function(params)
            self:UpdateRoomDisplay(params)
        end)
        
        Event.subscribe("OnGameStarting", function(params)
            Log.info("[LobbyRoom] Game starting!")
            -- Scene transition handled by NetworkedGameClient
        end)
        
        Event.subscribe("OnDisconnectedFromServer", function()
            Log.warning("[LobbyRoom] Disconnected")
            Event.emitLocal("RequestSceneTransition", {
                scene = "M5LobbySelectionScene"
            })
        end)
        
        Log.info("[LobbyRoom] Initialized")
    end,
    
    Start = function(self, entity)
        self.currentRoomID = Persistent.sessionRoomID or -1
        self.localFaction = Persistent.sessionFaction

        -- Ask for the room's current state rather than waiting for the next
        -- thing to change it.
        --
        -- Room state is only ever broadcast on a change, and this scene loads
        -- *after* the change that brought us here - so a host who had just
        -- created a room walked in to a lobby that showed nothing at all, and
        -- stayed that way until the other player joined or readied. Anyone
        -- arriving to a room already half-full saw the same blank.
        if NetworkedGameClient_instance and self.currentRoomID >= 0 then
            NetworkedGameClient_instance:RequestRoomState(self.currentRoomID)
        end

        Log.info("[LobbyRoom] Started")
    end,
    
    UpdateRoomDisplay = function(self, params)
        Event.emitLocal("OnLobbyRoomStateChanged", {
            orderName    = params.orderName,
            chaosName    = params.chaosName,
            orderJoined  = params.orderJoined,
            chaosJoined  = params.chaosJoined,
            orderReady   = params.orderReady,
            chaosReady   = params.chaosReady,
            localFaction = self.localFaction
        })
    end
})

return class