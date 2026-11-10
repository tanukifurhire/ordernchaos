local class = DefineType("OppUnitDisplay", {
    Serializable = {
        orderUnitID = 1,
        chaosUnitID = 4
    },

    TextureAsset = {
        orderUnitBar = 0,
        chaosUnitBar = 0
    },

    isOrder = false,
    unitID = 1,

    Init = function(self, entity)
        Event.subscribe("OnGameplayFactionReady", function(params)
            self.isOrder = params.isOrder
            if self.isOrder then
                self.unitID = self.chaosUnitID
                ECS.setSprite(entity, { textureAssetID = self.chaosUnitBar })
            else
                self.unitID = self.orderUnitID
                ECS.setSprite(entity, { textureAssetID = self.orderUnitBar })
            end
        end)
    end
})

return class
