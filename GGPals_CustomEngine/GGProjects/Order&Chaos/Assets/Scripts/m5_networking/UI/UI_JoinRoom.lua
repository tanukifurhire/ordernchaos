local class = DefineType("JoinRoomButton", {
    Serializable = {},

    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },

    selectedRoomID = -1,
    selectedRoomName = "",
    orderJoined = false,
    chaosJoined = false,

    -- Steam lobby ID of the selected entry, or "" when it is an ordinary room.
    -- Empty rather than nil so the field exists on the type - DefineType
    -- refuses to set a field it does not know about.
    selectedSteamLobbyId = "",

    Init = function(self, entity)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,

    UpdateDisplay = function(self, entity, roomID, roomName, orderJoined, chaosJoined, roomStatus, steamLobbyId)
        self.selectedRoomID = roomID
        self.selectedRoomName = roomName
        self.orderJoined = orderJoined
        self.chaosJoined = chaosJoined
        self.selectedSteamLobbyId = steamLobbyId or ""
    end,

    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
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
        Event.emitLocal("PlayMainMenuClick", {})
        if self.selectedRoomID < 0 then
            Log.warning("[JoinRoomButton] No room selected")
            return
        end

        -- A Steam lobby, not a room: we are not connected to this host yet, so
        -- there is nothing to join a room ON. Join the LOBBY, which connects
        -- us; the host's real room list arrives afterwards and replaces this
        -- list, and Join then means what it normally means.
        --
        -- Without this the button called JoinRoom on a host we had no
        -- connection to, which failed the "not connected" guard inside
        -- NetworkedGameClient and looked, from the outside, like the button
        -- doing nothing at all.
        if self.selectedSteamLobbyId ~= "" then
            Log.info("[JoinRoomButton] Joining Steam lobby " .. self.selectedSteamLobbyId)

            -- Arm the follow-up so this is one click, not two: entering the
            -- lobby only connects us to the host, and LobbySelection joins the
            -- room for us as soon as the host's room list arrives.
            if UI_LobbySelection_instance then
                UI_LobbySelection_instance.pendingAutoJoin = true
            end

            Steam.joinLobby(self.selectedSteamLobbyId)
            return
        end

        if not NetworkedGameClient_instance then
            Log.error("[JoinRoomButton] No client")
            return
        end

        if not NetworkedGameClient_instance.connected then
            Log.error("[JoinRoomButton] Not connected to a host, so there is no room to join. "
                .. "Pick a lobby from the list first.")
            return
        end

        if self.orderJoined and self.chaosJoined then
            Log.error("[JoinRoomButton] Room " .. self.selectedRoomName .. " is full!")
            Event.emitLocal("SpawnFloatingText", {
                worldX = 0.0, worldY = 0.3,
                damage = 0, text = "Room is full!"
            })
            return
        end

        local playerName = Persistent.sessionPlayerName or "Player"
        if playerName == "" then
            playerName = "Player"
        end
        
        local faction    = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER

        if (faction == CONSTANTS.ORDER_PLAYER) and (self.orderJoined) then
            Event.emitLocal("SpawnFloatingText", {
                worldX = 0.0, worldY = 0.3,
                damage = 0, text = "Order slot full, joining as Chaos"
            })
            faction = CONSTANTS.CHAOS_PLAYER
        end

        if (faction == CONSTANTS.CHAOS_PLAYER) and (self.chaosJoined) then
            Event.emitLocal("SpawnFloatingText", {
                worldX = 0.0, worldY = 0.3,
                damage = 0, text = "Chaos slot full, joining as Order"
            })
            faction = CONSTANTS.ORDER_PLAYER
        end

        Log.info("[JoinRoomButton] Joining room " .. self.selectedRoomID)
        NetworkedGameClient_instance:JoinRoom(playerName, self.selectedRoomID, faction)
    end
})

return class