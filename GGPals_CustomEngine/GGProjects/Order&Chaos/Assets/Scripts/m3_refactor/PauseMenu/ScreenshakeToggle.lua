local class = DefineType("ScreenshakeToggle",
{
    TextureAsset = {
        enabledSprite = 0,
        disabledSprite = 0
    },
    
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    enabled = true,

    Init = function(self, entity)
        Log.info("[ScreenshakeToggle] Initialized")
    end, -- end of Init

    OnHoverEnter = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.hoverSizeX,
            sy = self.hoverSizeY
        })
    end, -- end of OnHoverEnter

    OnHoverExit = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.normalSizeX,
            sy = self.normalSizeY
        })
    end, -- end of OnHoverExit

    OnClick = function(self, entity, x, y)
        Log.info("[ScreenshakeToggle] OnClick called")

        local sprite = ECS.getSprite(entity)
        if not sprite then
            return
        end

        -- if enabled, then disable
        if self.enabled == true then
            ECS.setSprite(entity, {
                textureAssetID = self.disabledSprite
            })
            self.enabled = false
        else -- if disabled, then enable
            ECS.setSprite(entity, {
                textureAssetID = self.enabledSprite
            })
            self.enabled = true
        end
    end -- end of OnClick
})

return class