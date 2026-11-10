local class = DefineType("RefreshRoomsButton", {
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
        Log.info("[RefreshRoomsButton] Refreshing room list")
        
        if UI_LobbySelection_instance then
            UI_LobbySelection_instance:RefreshRoomList()
        end
    end
})

return class