local class = DefineType("HowToPlayButton",
{
    TextureAsset = {
        normalSprite = 0,
        hoverSprite = 0
    },

    Init = function(self, entity)
        Log.info("[HowToPlayButton] Initialized")
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
        Log.info("[HowToPlayButton] OnClick called")
        Event.emitLocal("OnHowToPlayOpen", {})
    end -- end of OnClick
})

return class