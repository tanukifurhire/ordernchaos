local class = DefineType("FactionIconDisplay", {
    Serializable = {},
    
    TextureAsset = {
        orderIconTexture = 0,
        chaosIconTexture = 0
    },
    
    currentFaction = 1,
    
    Init = function(self, entity)
        Event.subscribe("OnFactionChanged", function(params)
            self.currentFaction = params.faction
            self:UpdateIcon(entity)
        end)
        
        self.currentFaction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
        
        self:UpdateIcon(entity)
    end,
    
    UpdateIcon = function(self, entity)
        if self.currentFaction == CONSTANTS.ORDER_PLAYER then
            if self.orderIconTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.orderIconTexture })
            end
        else
            if self.chaosIconTexture > 0 then
                ECS.setSprite(entity, { textureAssetID = self.chaosIconTexture })
            end
        end
    end
})

return class