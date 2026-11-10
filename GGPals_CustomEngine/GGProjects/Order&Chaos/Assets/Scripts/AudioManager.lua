-- Note: can essentially treat this as a master volume controller
-- Global audio configuration
AudioConfig = {
    sfx = {
        -- Grid / UI interactions
        gridConfirmTile    = "Audio/SFX/Grid_Confirm_Tile.wav",
        gridHoverTile      = "Audio/SFX/Grid_Hover_Tile.wav",
        gridPickup         = "Audio/SFX/Grid_Pickup.wav",
        gridPlaceGrid      = "Audio/SFX/Grid_PlaceGrid.wav",

        uiAbilityInvalid   = "Audio/SFX/UI_Ability_Invalid.wav",
        uiAbilityValid     = "Audio/SFX/UI_Ability_Valid.wav",
        uiEndTurn          = "Audio/SFX/UI_EndTurn.wav",

        mainMenuClick      = "Audio/SFX/MainMenu_Click.wav",
        mainMenuHoverBling = "Audio/SFX/MainMenu_Hover_Bling.wav",
        mainMenuHoverChaos = "Audio/SFX/MainMenu_Hover_Chaos.wav",
        mainMenuHoverOrder = "Audio/SFX/MainMenu_Hover_Order.wav",

        -- Units
        unitEyeCry         = "Audio/SFX/Unit_Eye_Cry.wav",
        unitEyeMove        = "Audio/SFX/Unit_Eye_Move.wav",
        unitImpCry         = "Audio/SFX/Unit_Imp_Cry.wav",
        unitImpMove        = "Audio/SFX/Unit_Imp_Move.wav",
        unitOracleCry      = "Audio/SFX/Unit_Oracle_Cry.wav",
        unitOracleMove     = "Audio/SFX/Unit_Oracle_Move.wav",
        unitWyvernCry      = "Audio/SFX/Unit_Wyvern_Cry.wav",
        unitWyvernMove     = "Audio/SFX/Unit_Wyvern_Move.wav",

        -- VFX
        vfxDragonA1        = "Audio/SFX/VFX_Wyvern_A1.wav",
        vfxDragonA2        = "Audio/SFX/VFX_Wyvern_A2.wav",
        vfxImpA1           = "Audio/SFX/VFX_Imp_A1.wav",
        vfxImpA2           = "Audio/SFX/VFX_Imp_A2.wav",
        vfxEyeA1              = "Audio/SFX/VFX_Eye_A1.wav",
        vfxEyeA2              = "Audio/SFX/VFX_Eye_A2.wav",
        vfxOracleA1           = "Audio/SFX/VFX_Oracle_A1.wav",
        vfxOracleA2           = "Audio/SFX/VFX_Oracle_A2.wav",
        vfxTirionVizomA1      = "Audio/SFX/VFX_TirionVizom_A1.wav",
        vfxTirionVizomA2      = "Audio/SFX/VFX_TirionVizom_A2.wav",
    },

    bgm = {
        combat   = "Audio/Music/BGM_Combat.wav",
        mainMenu = "Audio/Music/BGM_MainMenu.wav",
    },

    volume = {
        sfx = 0.7,
        bgm = 0.3,
    }
}

local class = DefineType("AudioManager", {
    Serializable = {
        masterVolume = 0.999
    },

    volumeIncrement = 0.1,

    -- subscription handles
    _increaseVolume = 0,
    _decreaseVolume = 0,

    -- for fade in/out effect --
    fadeState = "idle", -- idle, fadeOut, fadeIn
    fadeTimer = 0.0,
    fadeDuration = 1.0,
    
    fadeStartVolume = 0.0,
    fadeTargetVolume = 0.0,
    pendingScene = "",
    ----------------------------

    Init = function(self, entity)
        AudioManager_instance = self
        Log.info("[AudioManager] Initialized")

        -- unsubscribe any leftover subscriptions
        if self._increaseVolume ~= 0 then
            Event.unsubscribe(self._increaseVolume)
        end
        if self._decreaseVolume ~= 0 then
            Event.unsubscribe(self._decreaseVolume)
        end

        ----------------------------------------------------------------
        -- BGM events
        ----------------------------------------------------------------
        -- Event.subscribe("PlayBGMCombat", function()
        --     Audio.playMusic(AudioConfig.bgm.combat, AudioConfig.volume.bgm, true)
        -- end)

        -- Event.subscribe("PlayBGMMainMenu", function()
        --     Audio.playMusic(AudioConfig.bgm.mainMenu, AudioConfig.volume.bgm, true)
        -- end)

        ----------------------------------------------------------------
        -- Grid / UI interactions
        ----------------------------------------------------------------
        -- Event.subscribe("PlayGridConfirmTile", function()
        --     Audio.playSound(AudioConfig.sfx.gridConfirmTile, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayGridHoverTile", function()
        --     Audio.playSound(AudioConfig.sfx.gridHoverTile, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayGridPickup", function()
        --     Audio.playSound(AudioConfig.sfx.gridPickup, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayGridPlaceGrid", function()
        --     Audio.playSound(AudioConfig.sfx.gridPlaceGrid, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUIAbilityInvalid", function()
        --     Audio.playSound(AudioConfig.sfx.uiAbilityInvalid, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUIAbilityValid", function()
        --     Audio.playSound(AudioConfig.sfx.uiAbilityValid, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUIEndTurn", function()
        --     Audio.playSound(AudioConfig.sfx.uiEndTurn, AudioConfig.volume.sfx)
        -- end)

        ----------------------------------------------------------------
        -- Main menu
        ----------------------------------------------------------------
        -- Event.subscribe("PlayMainMenuClick", function()
        --     Audio.playSound(AudioConfig.sfx.mainMenuClick, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayMainMenuHoverBling", function()
        --     Audio.playSound(AudioConfig.sfx.mainMenuHoverBling, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayMainMenuHoverChaos", function()
        --     Audio.playSound(AudioConfig.sfx.mainMenuHoverChaos, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayMainMenuHoverOrder", function()
        --     Audio.playSound(AudioConfig.sfx.mainMenuHoverOrder, AudioConfig.volume.sfx)
        -- end)

        ----------------------------------------------------------------
        -- Units
        ----------------------------------------------------------------
        -- Event.subscribe("PlayUnitEyeCry", function()
        --     Audio.playSound(AudioConfig.sfx.unitEyeCry, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitEyeMove", function()
        --     Audio.playSound(AudioConfig.sfx.unitEyeMove, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitImpCry", function()
        --     Audio.playSound(AudioConfig.sfx.unitImpCry, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitImpMove", function()
        --     Audio.playSound(AudioConfig.sfx.unitImpMove, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitOracleCry", function()
        --     Audio.playSound(AudioConfig.sfx.unitOracleCry, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitOracleMove", function()
        --     Audio.playSound(AudioConfig.sfx.unitOracleMove, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitWyvernCry", function()
        --     Audio.playSound(AudioConfig.sfx.unitWyvernCry, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayUnitWyvernMove", function()
        --     Audio.playSound(AudioConfig.sfx.unitWyvernMove, AudioConfig.volume.sfx)
        -- end)

        ----------------------------------------------------------------
        -- VFX
        ----------------------------------------------------------------
        -- Event.subscribe("PlayVFXDragonA1", function()
        --     Audio.playSound(AudioConfig.sfx.vfxDragonA1, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXDragonA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxDragonA2, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXImpA1", function()
        --     Audio.playSound(AudioConfig.sfx.vfxImpA1, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXImpA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxImpA2, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXEyeA1", function()
        --     Audio.playSound(AudioConfig.sfx.vfxEyeA1, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXEyeA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxEyeA2, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXImpA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxImpA2, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXOracleA1", function()
        --     Audio.playSound(AudioConfig.sfx.vfxOracleA1, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXOracleA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxOracleA2, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXTirionVizomA1", function()
        --     Audio.playSound(AudioConfig.sfx.vfxTirionVizomA1, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXTirionVizomA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxTirionVizomA2, AudioConfig.volume.sfx)
        -- end)

        -- Event.subscribe("PlayVFXWyvernA2", function()
        --     Audio.playSound(AudioConfig.sfx.vfxWyvernA2, AudioConfig.volume.sfx)
        -- end)

        ----------------------------------------------------------------
        -- Adjust in-game volume
        ----------------------------------------------------------------
        self._increaseVolume =
        Event.subscribe("OnIncreaseMasterVolume", function()
            local currentVolume = Audio.getMasterVolume()

            -- skip if already at max volume
            if currentVolume >= 1.0 then
                return
            end

            local newVolume = currentVolume + self.volumeIncrement
            Audio.setMasterVolume(newVolume)
        end)

        self._decreaseVolume =
        Event.subscribe("OnDecreaseMasterVolume", function()
            local currentVolume = Audio.getMasterVolume()

            -- skip if already at volume 0
            if currentVolume <= 0.0 then
                return
            end

            local newVolume = currentVolume - self.volumeIncrement
            Audio.setMasterVolume(newVolume)
        end)

        Event.subscribe("RequestSceneTransition", function(params)
            Log.info("[AudioManager] Pending scene switch: " .. params.scene)

            if self.fadeState == "fadeOut" then
                return
            end

            -- Persist current volume settings so the next scene can restore them
            self:SaveAudioConfig()

            System.resume() -- for pause menu transition

            self.fadeState = "fadeOut"
            self.fadeTimer = 0
            self.fadeDuration = 1.0

            self.fadeStartVolume = Audio.getMasterVolume()
            self.fadeTargetVolume = 0.0

            self.pendingScene = params.scene
        end)

        Event.subscribe("Transitionfade", function()
            Log.info("[AudioManager] Transitionfade received, fading audio out")

            if self.fadeState == "fadeOut" then
                return
            end

            -- Persist current volume settings so the next scene can restore them
            self:SaveAudioConfig()

            self.fadeState = "fadeOut"
            self.fadeTimer = 0
            self.fadeDuration = 1.0

            self.fadeStartVolume = Audio.getMasterVolume()
            self.fadeTargetVolume = 0.0

            self.pendingScene = ""  -- no scene to load, caller handles it
        end)

        Event.subscribe("FadeInAudio", function()

        end)

        Event.subscribe("OnMuted", function()
            Audio.setMasterVolume(0.0)
        end)

        Event.subscribe("OnUnmuted", function(params)
            Audio.setMasterVolume(params.newVolume)
        end)
    end,

    Start = function(self, entity)
        Log.info("[AudioManager] ========================================")
        Log.info("[AudioManager] Audio System Ready")
        Log.info("[AudioManager] ========================================")

        -- Read persisted master volume from audio config; fall back to serialized default
        local audioConfig = Audio.getAudioConfig()
        local targetVolume = self.masterVolume
        if audioConfig and audioConfig.masterVolume and audioConfig.masterVolume > 0 then
            targetVolume = audioConfig.masterVolume
        end

        Log.info("[AudioManager] All audio events subscribed, targetVolume=" .. tostring(targetVolume))

        -- for fade in/out effect --
        Audio.setMasterVolume(0.0)

        self.fadeState = "fadeIn"
        self.fadeTimer = 0
        self.fadeDuration = 1.0

        self.fadeStartVolume = 0.0
        self.fadeTargetVolume = targetVolume
        ----------------------------
    end,

    -- Persist current master volume to audio config so next scene can restore it
    SaveAudioConfig = function(self)
        local currentMaster = Audio.getMasterVolume()
        local audioConfig = Audio.getAudioConfig() or {}
        Log.info("[AudioManager] Saving audio config: masterVolume=" .. tostring(currentMaster))
        Audio.setAudioConfig({
            masterVolume = currentMaster,
            bgmVolume = audioConfig.bgmVolume or 0.3,
            sfxVolume = audioConfig.sfxVolume or 0.7,
        })
    end,

    -- for scene switch fade in/out
    Update = function(self, entity)

        local dt = Time.getDeltaTime()

        if self.fadeState == "idle" then
            return
        end

        if self.fadeState == "fadeOut" then
            self.fadeTimer = self.fadeTimer + dt
            local t = math.min(self.fadeTimer / self.fadeDuration, 1.0)

            local volume = self.fadeStartVolume + (self.fadeTargetVolume - self.fadeStartVolume) * t
            Audio.setMasterVolume(volume)

            if t >= 1.0 then
                self.fadeState = "idle"
            end
        elseif self.fadeState == "fadeIn" then
            self.fadeTimer = self.fadeTimer + dt
            local t = math.min(self.fadeTimer / self.fadeDuration, 1.0)

            local volume = self.fadeStartVolume + (self.fadeTargetVolume - self.fadeStartVolume) * t
            Audio.setMasterVolume(volume)

            if t >= 1.0 then
                self.fadeState = "idle"
            end
        end

    end
})

return class
