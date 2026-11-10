local class = DefineType("MainMenuButtonv2", {

    Serializable = {
        targetSceneName = "",

        -- Hover background state
        menuState = 1, -- 1=story,2=order,3=chaos,4=howto,5=settings,6=quit

        -- Roles
        isStoryButton = false,
        isHowToPlayButton = false,
        isSettingsButton = false,
        isQuitButton = false,

        playAsOrder = false,
        playAsChaos = false
    },

    hoverTime = 0.0,
    hoverSoundPlayed = true,
    isHovering = false,
    hoverSoundDelay = 0.5,

    settingsOpen = false,
    bgmVolume = 0,
    sfxVolume = 0,

    Init = function(self, entity)
        Event.subscribe("OnReturnBGMVolume", function(params)
            self.bgmVolume = params.bgmVolume
        end)

        Event.subscribe("OnReturnSFXVolume", function(params)
            self.sfxVolume = params.sfxVolume
        end)
    end,

    Update = function(self, entity)
        if not self.isHovering or self.hoverSoundPlayed then 
            return 
        end

        self.hoverTime = self.hoverTime + Time.getDeltaTime()

        if self.hoverTime >= self.hoverSoundDelay then
            if self.playAsOrder then
                Event.emitLocal("PlayMainMenuHoverOrder", {})
            elseif self.playAsChaos then
                Event.emitLocal("PlayMainMenuHoverChaos", {})
            end
            self.hoverSoundPlayed = true
        end
    end,

    ---------------------------------------------------
    -- HOVER
    ---------------------------------------------------
    OnHoverEnter = function(self, entity)
        Event.emitLocal("MainMenuHover", { state = self.menuState })

        Event.emitLocal("PlayMainMenuHoverBling", {})

        -- fire panel switch when hovering order or chaos buttons
        if self.playAsOrder then
            Event.emitLocal("OnPlayModeSwitch", { playAsOrder = true })
        elseif self.playAsChaos then
            Event.emitLocal("OnPlayModeSwitch", { playAsOrder = false })
        end
        self.isHovering = true
        self.hoverTime = 0.0
        self.hoverSoundPlayed = not (self.playAsOrder or self.playAsChaos)
    end,

    OnHoverExit = function(self, entity)
        Event.emitLocal("MainMenuHover", { state = 0 })

        self.isHovering = false
        self.hoverTime = 0.0
        self.hoverSoundPlayed = true
    end,

    ---------------------------------------------------
    -- CLICK
    ---------------------------------------------------
    OnClick = function(self, entity, x, y)
        if self.menuState == 1 then
            Event.emitLocal("SkipLogo", {})
        end

        Event.emitLocal("PlayMainMenuClick", {})

        -- STORY (same as StoryButton)
        if self.isStoryButton then
            Log.info("[MainMenuButton] Opening Story")
            Event.emitLocal("OnCutsceneOpen", {})
            return
        end

        -- HOW TO PLAY (same as HowToPlayButton)
        if self.isHowToPlayButton then
            Log.info("[MainMenuButton] Opening How To Play")
            -- Event.emitLocal("OnHowToPlayOpen", {})
            -- return
        end

        -- SETTINGS
        if self.isSettingsButton then
            -- toggle open/close settings menu
            if not self.settingsOpen then
                Log.info("[MainMenuButton] Opening Settings")
                Event.emitLocal("OnOpenSettingsMenu", {})
                self.settingsOpen = true

            elseif self.settingsOpen then
                Log.info("[MainMenuButton] Closing Settings")
                Event.emitLocal("OnCloseSettingsMenu", {})
                self.settingsOpen = false
            end
            
            return
        end

        -- QUIT
        if self.isQuitButton then
            
            -- reset to audio config defaults
            Audio.setAudioConfig({
                masterVolume = 1.0,
                bgmVolume = 1.0,
                sfxVolume = 1.0
            })
            ---------------------------------

            System.shutdown()
            return
        end

        -- PLAY MODES
        if self.playAsOrder then
            Event.emitLocal("OnClickPlayAsOrder", {})
        elseif self.playAsChaos then
            Event.emitLocal("PlayAsChaos", {})
        end

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

        if self.targetSceneName ~= "" then
            Event.emitLocal("RequestSceneTransition", {
                scene = self.targetSceneName
            })
            
            -- Event.emitLocal("OnClickPlayAsChaos", {})
        end
    
        -- Scene change (Order / Chaos)
        -- if self.targetSceneName ~= "" then
        --     Scene.setActive(self.targetSceneName)
        -- end
    end
})

return class
