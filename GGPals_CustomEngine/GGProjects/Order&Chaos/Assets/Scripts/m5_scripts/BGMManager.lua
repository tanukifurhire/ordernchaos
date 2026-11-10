local class = DefineType("BGMManager",
{
    AudioAsset = {
        -- BGM audioIDs
        combat  = 0
    },

    volumeIncrement = 0.1,

    -- subscription handles
    _increaseVolume = 0,
    _decreaseVolume = 0,

    Start = function(self, entity)
        local audioConfig = Audio.getAudioConfig()
        Audio.setAudioSourceVolume(self.entity, audioConfig.bgmVolume)
    end,

    Init = function(self, entity)
        self.entity = entity
        Log.info("[BGMManager] Initialized")

        -- unsubscribe any leftover subscriptions
        if self._increaseVolume ~= 0 then
            Event.unsubscribe(self._increaseVolume)
        end
        if self._decreaseVolume ~= 0 then
            Event.unsubscribe(self._decreaseVolume)
        end

        ----------------------------------------------------------------
        -- BGM Event Subscriptions
        -- note: use this later if we have mid game BGM switching 
        ----------------------------------------------------------------
        -- Event.subscribe("PlayBGMCombat", function()
        --     Audio.triggerMusic(self.entity, self.combat, true)
        -- end)

        ----------------------------------------------------------------
        -- BGM Audio Controls
        ----------------------------------------------------------------
        Event.subscribe("GetBGMVolume", function()
            Event.emitLocal("OnReturnBGMVolume", {
                bgmVolume = self.currentVolume or 0.3;
            })
        end)

        self._increaseVolume =
        Event.subscribe("OnIncreaseBGMVolume", function()
            local currentVolume = self.currentVolume or 0.3

            -- skip if already at max volume
            if currentVolume >= 1.0 then
                return
            end

            local newVolume = math.min(1.0, math.floor((currentVolume + self.volumeIncrement) * 10 + 0.5) / 10)
            self.currentVolume = newVolume
            Audio.setAudioSourceVolume(self.entity, newVolume)
            Log.info("[BGMManager] Volume increased to " .. tostring(newVolume))

            -- Persist to audio config
            local audioConfig = Audio.getAudioConfig() or {}
            Audio.setAudioConfig({
                masterVolume = audioConfig.masterVolume or 0.999,
                bgmVolume = newVolume,
                sfxVolume = audioConfig.sfxVolume or 0.7,
            })
        end)

        self._decreaseVolume =
        Event.subscribe("OnDecreaseBGMVolume", function()
            local currentVolume = self.currentVolume or 0.3

            -- skip if already at volume 0
            if currentVolume <= 0.0 then
                return
            end

            local newVolume = math.max(0.0, math.floor((currentVolume - self.volumeIncrement) * 10 + 0.5) / 10)
            self.currentVolume = newVolume
            Audio.setAudioSourceVolume(self.entity, newVolume)
            Log.info("[BGMManager] Volume decreased to " .. tostring(newVolume))

            -- Persist to audio config
            local audioConfig = Audio.getAudioConfig() or {}
            Audio.setAudioConfig({
                masterVolume = audioConfig.masterVolume or 0.999,
                bgmVolume = newVolume,
                sfxVolume = audioConfig.sfxVolume or 0.7,
            })
        end)

    end -- end of Init
})

return class