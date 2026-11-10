local class = DefineType("ArsenalUnitSelection", {
    Serializable = {
        orderUnitID = 1,
        chaosUnitID = 4,
        darkenAmount = 0.5,
        lerpSpeed = 10.0
    },

    TextureAsset = {
        orderUnitBar = 0,
        chaosUnitBar = 0
    },

    isOrder = false,
    unitID = 1,

    currentBrightness = 1.0,
    targetBrightness = 1.0,

    Init = function(self, entity)
        Event.subscribe("OnArsenalFactionReady", function(params)
            self.isOrder = params.isOrder
            if self.isOrder then
                self.unitID = self.orderUnitID
                ECS.setSprite(entity, { textureAssetID = self.orderUnitBar })
            else
                self.unitID = self.chaosUnitID
                ECS.setSprite(entity, { textureAssetID = self.chaosUnitBar })
            end
        end)

        Event.subscribe("OnArsenalUnitSelected", function(params)
            if params.unitType == self.unitID then
                self.targetBrightness = 1.0
            else
                self.targetBrightness = self.darkenAmount
            end
        end)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
        if math.abs(self.currentBrightness - self.targetBrightness) > 0.01 then
            self.currentBrightness = self.currentBrightness +
                (self.targetBrightness - self.currentBrightness) * self.lerpSpeed * Time.getDeltaTime()
            ECS.setUIComponent(entity, {
                colorMultiply = {
                    r = self.currentBrightness,
                    g = self.currentBrightness,
                    b = self.currentBrightness,
                    a = 1
                }
            })
        elseif self.currentBrightness ~= self.targetBrightness then
            self.currentBrightness = self.targetBrightness
            ECS.setUIComponent(entity, {
                colorMultiply = {
                    r = self.currentBrightness,
                    g = self.currentBrightness,
                    b = self.currentBrightness,
                    a = 1
                }
            })
        end
    end,

    OnHoverEnter = function(self, entity)
        ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY })
    end,

    OnHoverExit = function(self, entity)
        -- Restore to current brightness rather than always full white
        ECS.setUIComponent(entity, {
            colorMultiply = {
                r = self.currentBrightness,
                g = self.currentBrightness,
                b = self.currentBrightness,
                a = 1
            }
        })
    end,

    OnClick = function(self, entity, x, y)
        Log.info("[ArsenalUnitSelection] Selected unit " .. self.unitID)
        Event.emitLocal("OnArsenalUnitSelected", {
            unitType = self.unitID
        })
    end
})

return class