local class = DefineType("FadeManager", {

    fadeInDuration = 1.0,
    fadeOutDuration = 1.0,

    isFadingIn = false,
    isFadingOut = false,
    fadeTimer = 0.0,
    fadeAlpha = 1.0,
    fadeOverlayEntity = nil,
    pendingSceneName = "",

    Init = function(self, entity)
        -- Create the overlay immediately as fully black
        self:CreateOverlay(entity)

        -- If the global flag says we should fade in, start immediately
        if _G.__doFadeIn then
            _G.__doFadeIn = false
            Log.info("[FadeManager] Fade in triggered from previous scene")
            self.isFadingIn = true
            self.fadeTimer = 0.0
            self.fadeAlpha = 1.0
        else
            -- No fade in needed, make overlay invisible
            self:SetOverlayAlpha(0.0)
        end

        -- Listen for fade out requests from other scripts
        Event.subscribe("RequestSceneTransition", function(params)
            if params.scene and params.scene ~= "" then
                Log.info("[FadeManager] Fade out requested to scene: " .. params.scene)
                self.pendingSceneName = params.scene
                self.isFadingOut = true
                self.isFadingIn = false
                self.fadeTimer = 0.0
                self.fadeAlpha = 0.0
                _G.__menuFading = true
            end
        end)

        Event.subscribe("Transitionfade", function(params)
        Log.info("[FadeManager] Transitionfade received!") 
        self.isFadingOut = true
        self.isFadingIn = false
        self.fadeTimer = 0.0
        self.fadeAlpha = 0.0
        _G.__menuFading = true
    end)

    end,

    Update = function(self, entity)
        --Log.info("[FadeManager] isFadingOut=" .. tostring(self.isFadingOut) .. " isFadingIn=" .. tostring(self.isFadingIn))
        -- Fade in (black -> transparent)
        if self.isFadingIn then
            self.fadeTimer = self.fadeTimer + Time.getDeltaTime()
            local t = self.fadeTimer / self.fadeInDuration
            if t > 1.0 then t = 1.0 end

            self.fadeAlpha = 1.0 - t
            self:SetOverlayAlpha(self.fadeAlpha)

            Log.info("[FadeManager] Fading in t=" .. string.format("%.3f", t))

            if t >= 1.0 then
                Log.info("[FadeManager] Fade in complete")
                self.isFadingIn = false
                self:SetOverlayAlpha(0.0)
                _G.__menuFading = false
                
            end
            return
        end

        -- Fade out (transparent -> black)
        if self.isFadingOut then
            self.fadeTimer = self.fadeTimer + Time.getDeltaTime()
            local t = self.fadeTimer / self.fadeOutDuration
            if t > 1.0 then t = 1.0 end

            self.fadeAlpha = t
            self:SetOverlayAlpha(self.fadeAlpha)

            Log.info("[FadeManager] Fading out t=" .. string.format("%.3f", t))

            if t >= 1.0 then
                Log.info("[FadeManager] Fade out complete, transitioning to: " .. self.pendingSceneName)
                self.isFadingOut = false
                _G.__menuFading = false
                _G.__doFadeIn = true
                Event.emitLocal("TransitionComplete", {})

                -- Load the pending scene if one was set via RequestSceneTransition
                if self.pendingSceneName ~= "" then
                    Scene.setActive(self.pendingSceneName)
                    self.pendingSceneName = ""
                end
            end
            return
        end
    end,

    ---------------------------------------------------
    -- HELPERS
    ---------------------------------------------------
    CreateOverlay = function(self, entity)
        local overlay = ECS.createEntity()
        ECS.addTransform(overlay, 0.0, 0.0, 1600.0, 900.0, 0.0)
        ECS.addUIComponent(overlay, 1)

        ECS.setUIComponent(overlay, {
            colorAdd            = { r = 0.0, g = 0.0, b = 0.0, a = 1.0 },
            colorMultiply       = { r = 0.0, g = 0.0, b = 0.0, a = 1.0 },
            layer               = 9999,
            pickableObject      = false,
            pixelPerfectPicking = false,
        })
        self.fadeOverlayEntity = overlay
        Log.info("[FadeManager] Overlay created")
    end,

    SetOverlayAlpha = function(self, alpha)
        if self.fadeOverlayEntity == nil then return end
        local ui = ECS.getUIComponent(self.fadeOverlayEntity)
        ui.colorAdd.a = alpha
        ui.colorMultiply.a = alpha
        ECS.setUIComponent(self.fadeOverlayEntity, ui)
    end,
})

return class