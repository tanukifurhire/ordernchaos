local class = DefineType("ReturnMainMenuButton", {
    
    Serializable =
    {
        mainMenuScene = ""
    },
    
    TextureAsset =
    {
        -- Sprite asset IDs
        normalSprite    = 0,
        hoverSprite     = 0
    },

    Init = function(self, entity)
        Event.subscribe("OnReturnBGMVolume", function(params)
            self.bgmVolume = params.bgmVolume
        end)

        Event.subscribe("OnReturnSFXVolume", function(params)
            self.sfxVolume = params.sfxVolume
        end)
    end,

    Start = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.normalSprite
            })
        else
            Log.warning("[ReturnMainMenu] No Sprite component on entity")
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
        -- Scene.setActive(self.mainMenuScene)
        
        -- save audio settings to persist in the next scene
        local audioConfig = Audio.getAudioConfig()
        audioConfig.masterVolume = Audio.getMasterVolume()
        Event.emitLocal("GetBGMVolume", {})
        Event.emitLocal("GetSFXVolume", {})
        audioConfig.bgmVolume = self.bgmVolume
        audioConfig.sfxVolume = self.sfxVolume
        Audio.setAudioConfig({
            masterVolume = audioConfig.masterVolume,
            bgmVolume = audioConfig.bgmVolume,
            sfxVolume = audioConfig.sfxVolume
        })
        ---------------------------------------------------

        Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER -- reset for the next replay
        if self.mainMenuScene ~= "" then
            Event.emitLocal("OnReturnMainMenu", {})
        end

        Log.info("[ReturnMainMenuButton] Requesting scene switch to: " .. self.mainMenuScene)
    end,
})

return class
