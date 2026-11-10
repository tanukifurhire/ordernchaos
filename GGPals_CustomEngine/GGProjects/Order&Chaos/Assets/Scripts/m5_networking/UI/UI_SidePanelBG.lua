local class = DefineType("ServerPanelBG", {
    TextureAsset = {
        orderBG = 0,
        chaosBG = 0
    },
    
    Init = function(self, entity)
        Event.subscribe("OnGameplayFactionReady", function(params)
            if params.isOrder then
                ECS.setSprite(entity, { textureAssetID = self.chaosBG })
            else
                ECS.setSprite(entity, { textureAssetID = self.orderBG })
            end
        end)
    end
})

return class