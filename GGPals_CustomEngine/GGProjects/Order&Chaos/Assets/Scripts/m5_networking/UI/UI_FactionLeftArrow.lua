local class = DefineType("FactionLeftArrow", {
    Serializable = {},
    
    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },

    hovered = false,
    
    Init = function(self, entity)
        
    end,

    Update = function(self, entity)
        if (Persistent.sessionFaction == CONSTANTS.ORDER_PLAYER) then
            ECS.setUIComponent(entity, {
                colorMultiply = CONSTANTS.COLOR_BUTTON_HOVER_MULTIPLY
            })
        else
            if self.hovered then 
                ECS.setUIComponent(entity, {
                    colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY
                })
            else
                ECS.setUIComponent(entity, {
                    colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY
                })
            end
        end
    end,
    
    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
        self.hovered = true

        if self.hoverTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.hoverTexture })
        end
    end,
    
    OnHoverExit = function(self, entity)
        self.hovered = false

        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,
    
    OnClick = function(self, entity, x, y)
        Event.emitLocal("PlayMainMenuClick", {})
        Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER

        Event.emitLocal("OnFactionChanged", { faction = CONSTANTS.ORDER_PLAYER })
    end
})

return class