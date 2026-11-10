-- NOTE: this file is not referenced by any scene or prefab.
--
-- It used to declare itself as "LobbyBook", which is also what
-- UI_LobbyPlayerDisplay.lua - the one M5LobbyRoomScene actually uses -
-- declares. Script metadata is registered by TYPE NAME, not by file, so the
-- two collided: this file loads later (alphabetically) and overwrote the live
-- one, and every LobbyBook entity in the lobby room silently ran THIS
-- definition instead. The tell was "Start not found for entity 16" in the log,
-- because this definition has no Start.
--
-- Renamed so an unused file cannot take a live one's name again.
local class = DefineType("OrderPlayerDisplay", {
    Serializable = {
        isOrderSide = true
    },
    
    TextureAsset = {
        normalTexture = 0,
        glowTexture = 0,
        readyTexture = 0
    },
    
    Init = function(self, entity)
        local eventName = self.isOrderSide and "OrderReadyButtonHover" or "ChaosReadyButtonHover"
        
        Event.subscribe(eventName, function(params)
            if params.isReady and self.readyTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.readyTexture })
            elseif params.isHovering and self.glowTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.glowTexture })
            elseif self.normalTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.normalTexture })
            end
        end)
        
        Event.subscribe("OnLobbyRoomStateChanged", function(params)
            local isJoined, isReady
            if self.isOrderSide then
                isJoined = params.orderJoined
                isReady  = params.orderReady
            else
                isJoined = params.chaosJoined
                isReady  = params.chaosReady
            end

            if isReady and self.readyTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.readyTexture })
            elseif isJoined and self.glowTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.glowTexture })
            elseif self.normalTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.normalTexture })
            end
        end)
    end
})

return class