local class = DefineType("SettingsButton",
{
    TextureAsset = {
        normalSprite = 0,
        hoverSprite = 0
    },

    Init = function(self, entity)
        Log.info("[SettingsButton] Initialized")
    end, -- end of Init

    OnHoverEnter = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.hoverSprite
            })
        end
    end, -- end of OnHoverEnter

    OnHoverExit = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.normalSprite
            })
        end
    end, -- end of OnHoverExit

    OnClick = function(self, entity)
        Log.info("[SettingsButton] OnClick called")
        Event.emitLocal("OnOpenSettingsMenu", {})
    end -- end of OnClick
})

return class