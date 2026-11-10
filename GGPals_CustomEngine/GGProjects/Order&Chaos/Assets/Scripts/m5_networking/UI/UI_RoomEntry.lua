local class = DefineType("RoomEntry", {
    Serializable = {
        entryNumber = 1
    },
    
    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0,
        orderGemActive = 0,
        orderGemInactive = 0,
        chaosGemActive = 0,
        chaosGemInactive = 0
    },
    
    roomID = -1,
    roomName = "",
    orderJoined = false,
    chaosJoined = false,
    roomStatus = "",

    -- Set (as a STRING) when this entry is a Steam lobby - a host we have not
    -- connected to - rather than a room on a host we already reached. Empty
    -- for ordinary rooms. Clicking the two does entirely different things.
    --
    -- Empty string, NOT nil: a table constructor entry with a nil value does
    -- not create the key, so the field would not exist in the type at all -
    -- and DefineType's __newindex raises "Cannot set unknown field" rather
    -- than quietly adding it.
    steamLobbyId = "",

    Init = function(self, entity)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,

    UpdateDisplay = function(self, entity)
        local children = ECS.getChildren(entity)
        for _, e in ipairs(children) do
            local scriptComponent = ECS.getScriptInstance(e)

            if scriptComponent then
                scriptComponent:UpdateDisplay(e, self.roomID, self.roomName, self.orderJoined, self.chaosJoined, self.roomStatus, self.steamLobbyId)
            end
        end
    end,
    
    OnHoverEnter = function(self, entity)
        Event.emitLocal("OnRoomEntrySelected", {
            roomID = self.roomID,
            name   = self.roomName
        })
        if self.hoverTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.hoverTexture })
        end
    end,
    
    OnHoverExit = function(self, entity)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,
    
    OnClick = function(self, entity, x, y)
        if self.roomID < 0 then
            Log.warning("[RoomEntry] No room data")
            return
        end

        -- A Steam lobby: join the LOBBY, not a room. That gets us connected to
        -- the host, and SteamCB_OnLobbyEntered opens the P2P connection. The
        -- host's room list arrives afterwards and replaces this list, at which
        -- point these entries become real rooms.
        if self.steamLobbyId ~= "" then
            Log.info("[RoomEntry] Joining Steam lobby " .. self.steamLobbyId)

            -- Same one-click follow-up as the Join button.
            if UI_LobbySelection_instance then
                UI_LobbySelection_instance.pendingAutoJoin = true
            end

            Steam.joinLobby(self.steamLobbyId)
            return
        end

        Log.info("[RoomEntry] Joining room " .. self.roomID)
        
        if not NetworkedGameClient_instance then
            Log.error("[RoomEntry] No client instance")
            return
        end
        
        local playerName = "Player"
        local faction = CONSTANTS.ORDER_PLAYER
        
        playerName = Persistent.sessionPlayerName or "Player"
        faction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
        Persistent.sessionRoomName = self.roomName
        
        NetworkedGameClient_instance:JoinRoom(playerName, self.roomID, faction)
    end
})

return class