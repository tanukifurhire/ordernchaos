--[[************************************************************************
  \file       FadeInOutBG.lua
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu 
  \date       01 February 2026
  \version    1.0
  \brief
    Handles a fade-in/fade-out overlay effect for screen transitions,
    such as during the "Pass and Play" screen or other UI overlays.
  \details
    This script defines the FadeInTransitionOverlay class, which manages
    a UI overlay that smoothly fades in and out based on events.
    The overlay supports configurable scale, position, color, maximum alpha,
    and fade speed. The overlay reacts to game events like:
      - "InResolutionPhase": Forces fade out during resolution phase
      - "OnPassAndPlayDisplayed": Shows the overlay when passing turns
      - "OnPassAndPlayContinue": Fades out when continuing the game

    The script uses linear interpolation (lerp) to smoothly adjust alpha values
    over time and automatically hides the overlay when fade out is complete.

  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("FadeInTransitionOverlay",
{
   
    currentAlpha   = 0,
    targetAlpha    = 0,
    isActive       = false,
    isFadingIn     = false,
    isFadingOut    = false,
    inResolution   = false,
    alphaThreshold = 0.01,
  
    Serializable =
    {
        scaleX = 1600,
        scaleY = 900,
        x = 0,
        y = 0,
        colorR = 0,
        colorG = 0,
        colorB = 0,
        maxAlpha = 0.6,
        fadeSpeed = 3,
        uiLayer = 3,
    },

    Init = function(self, entity)
        Log.info("[FadeInTransitionOverlay] Initialized")

        self.currentAlpha = 0
        self.targetAlpha  = 0

        -- Start hidden
        local t = ECS.getTransform(entity)
        t.x = self.x
        t.y = self.y
        t.sx = 0
        t.sy = 0
        ECS.setTransform(entity, t)

        local ui = ECS.getUIComponent(entity)
        if ui then
            ui.layer = self.uiLayer
            ui.ySort = 1
            ui.colorMultiply = { r = 1, g = 1, b = 1, a = 0 }
            ECS.setUIComponent(entity, ui)
        end

       
        function self:ShowOverlay()
        
            self.isActive   = true
            self.isFadingIn = true
            self.isFadingOut = false
            self.targetAlpha = self.maxAlpha

            local t = ECS.getTransform(entity)
            t.sx = self.scaleX
            t.sy = self.scaleY
            ECS.setTransform(entity, t)

            Log.info("[FadeInTransitionOverlay] Fade IN started")
        end

        function self:FadeOutOverlay()
            if not self.isActive then return end
          
            self.isFadingIn  = false
            self.isFadingOut = true
            self.targetAlpha = 0

            Log.info("[FadeInTransitionOverlay] Fade OUT started")
        end

        function self:FullyHide()
            self.isActive    = false
            self.isFadingIn  = false
            self.isFadingOut = false
            self.currentAlpha = 0
            self.targetAlpha  = 0

            local t = ECS.getTransform(entity)
            t.sx = 0
            t.sy = 0
            ECS.setTransform(entity, t)

            Log.info("[FadeInTransitionOverlay] Overlay fully hidden")
        end

        Event.subscribe("InResolutionPhase", function()
        self.inResolution = true
        self:FadeOutOverlay()  -- Force hide during resolution
        end)

        Event.subscribe("OnPassAndPlayDisplayed", function()
        if self.inResolution then return end  -- Don't show if in resolution
        self:ShowOverlay()
        end)

        Event.subscribe("OnPassAndPlayDisplayed", function()
        self:ShowOverlay()
        end)

        Event.subscribe("OnPassAndPlayContinue", function()
        self:FadeOutOverlay()
        end)

    end,
    
    Update = function(self, entity)
        if not self.isActive then return end

        local dt = Time.getDeltaTime()

        -- Lerp alpha
        local delta = self.targetAlpha - self.currentAlpha
        self.currentAlpha = self.currentAlpha + delta * self.fadeSpeed * dt

        -- Clamp & completion checks
        if self.isFadingIn and math.abs(self.currentAlpha - self.maxAlpha) < self.alphaThreshold then
            self.currentAlpha = self.maxAlpha
            self.isFadingIn = false
            Log.info("[FadeInTransitionOverlay] Fade IN complete")
        end

        if self.isFadingOut and self.currentAlpha < self.alphaThreshold then
            self:FullyHide()
            return
        end

        -- Apply alpha
        local ui = ECS.getUIComponent(entity)
        if ui then
            ui.colorMultiply.a = self.currentAlpha
            ECS.setUIComponent(entity, ui)
        end
    end
})

return class