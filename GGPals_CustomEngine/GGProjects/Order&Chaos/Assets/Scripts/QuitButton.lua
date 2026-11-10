local class = DefineType("QuitButton", {
    TextureAsset =
    {
        -- Sprite asset IDs
        normalSprite    = 0,
        hoverSprite     = 0
    },

    Init = function(self, entity)
    end,

    Start = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.normalSprite
            })
        else
            Log.warning("[Quit] No Sprite component on entity")
        end
    end,

    Update = function(self, entity)
    end,

    OnHoverEnter = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.hoverSprite
            })
        end
    end,

    OnHoverExit = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.normalSprite
            })
        end
    end,

    OnClick = function(self, entity, x, y)
        
        -- reset to audio config defaults
        Audio.setAudioConfig({
            masterVolume = 1.0,
            bgmVolume = 1.0,
            sfxVolume = 1.0
        })
        ---------------------------------

        System.shutdown()
    end,
})

return class
