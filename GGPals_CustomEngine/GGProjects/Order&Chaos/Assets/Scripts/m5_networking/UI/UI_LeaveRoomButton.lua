local class = DefineType("LeaveRoomButton", {
    Serializable = {},
    
    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },
    
    Init = function(self, entity)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
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
        Log.info("[LeaveRoomButton] Leaving room")
        
        if NetworkedGameClient_instance then
            local roomID = Persistent.sessionRoomID or -1
            NetworkedGameClient_instance:LeaveRoom(roomID)
            Persistent.sessionRoomID = nil
        end

        Event.emitLocal("RequestSceneTransition", {
            scene = "M5LobbySelectionScene"
        })
    end
})

return class