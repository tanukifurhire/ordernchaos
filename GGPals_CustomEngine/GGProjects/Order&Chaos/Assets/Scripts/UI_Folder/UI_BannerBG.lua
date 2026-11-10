-- UI_BannerBG.lua - Dark transparent overlay that fades in when showing the winner banner

local class = DefineType("UI_BannerBG", {
    Serializable = {
        fadeDuration = 0.5,   -- How long the fade-in takes (seconds)
        targetAlpha = 0.6     -- Final opacity of the dark filter
    },

    currentAlpha = 0.0,
    fading = false,

    -- True once a win/draw banner is up. Until then clicks are ignored, so a
    -- stray click mid-match cannot dump anyone out of the game.
    bannerShown = false,

    -- Small grace period after the banner appears. Players are often still
    -- clicking from the last action, and dropping them straight to the menu
    -- before they have read who won is worse than making them wait a moment.
    dismissDelay = 0.75,
    shownFor = 0.0,

    Init = function(self, entity)
        -- Start fully transparent
        UI_BannerBG.SetAlpha(self, entity, 0)

        -- Listen for winner banner events
        Event.subscribe("ShowPhaseOverlay", function(params)
            if params.phase == "OrderWin" or params.phase == "ChaosWin" or params.phase == "Draw" then
                Log.info("[UI_BannerBG] Fading in for " .. params.phase)
                self.fading = true
                self.currentAlpha = 0.0
                self.bannerShown = true
                self.shownFor = 0.0
            end
        end)
    end,

    Start = function(self, entity)
    end,

    --[[
        Dismisses the banner and leaves the match.

        Shared by the click handler and the input fallback so both do exactly
        the same thing, and guarded so it can only ever fire once - the
        teardown is not something you want to run twice.
    ]]
    Dismiss = function(self, entity)
        if not self.bannerShown then return end
        if self.shownFor < self.dismissDelay then return end

        self.bannerShown = false

        Event.emitLocal("PlayMainMenuClick", {})
        Log.info("[UI_BannerBG] Banner dismissed, leaving the match")

        -- Full teardown: client, lobby and server. The winner is usually the
        -- host, so skipping the server here would leave a match running with
        -- nobody in it and a lobby still advertised.
        if NetworkSessionTeardown then
            NetworkSessionTeardown("match ended")
        else
            RPC.disconnect()
        end

        Event.emitLocal("RequestSceneTransition", {
            scene = "M5MainMenuScene"
        })
    end,

    OnClick = function(self, entity, x, y)
        self:Dismiss(entity)
    end,

    Update = function(self, entity)
        if self.bannerShown then
            self.shownFor = self.shownFor + Time.getDeltaTime()

            -- Input polling as well as OnClick: the overlay only receives
            -- OnClick if it is set up as an interactable UI element, and
            -- "click anywhere" should not depend on that. Either route reaches
            -- the same guarded Dismiss.
            -- MOUSE_LEFT is an engine global; 32 is GLFW's space, matching the
            -- raw-keycode style used elsewhere in these scripts.
            if Input.isMouseButtonTriggered(MOUSE_LEFT) or Input.isKeyTriggered(32) then
                self:Dismiss(entity)
            end
        end

        if not self.fading then return end

        local dt = Time.getDeltaTime()
        self.currentAlpha = self.currentAlpha + (self.targetAlpha / self.fadeDuration) * dt

        if self.currentAlpha >= self.targetAlpha then
            self.currentAlpha = self.targetAlpha
            self.fading = false
        end

        UI_BannerBG.SetAlpha(self, entity, self.currentAlpha)
    end,

    SetAlpha = function(self, entity, alpha)
        local mr = ECS.getUIComponent(entity)
        if not mr then return end
        mr.colorMultiply = { r = 0, g = 0, b = 0, a = alpha }
        ECS.setUIComponent(entity, mr)
    end
})

return class
