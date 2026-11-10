--[[************************************************************************
  \file       LogoSequence.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.0
  \brief
    EDIT YOUR BRIEF HERE
  \details
    EDIT YOUR DETAILS HERE
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
Persistent = Persistent or {}

local class = DefineType("LogoSequence", {
    Serializable = {
        hiddenX = -10000,
        hiddenY = -10000,

        spawnX = 0,
        spawnY = 0,

        targetX = -492.6,
        targetY = 196.0,

        targetSX = 612.0,
        targetSY = 508.34,

        startScaleMultiplier = 1.3,

        fadeDuration = 3.0,

        pulseDuration = 2.6,   
        pulseAmount = 0.06,

        moveDuration = 2.0,

        breatheDuration = 0.900  
    },

    state = 0,
    elapsed = 0,
    hasStarted = false,

    startX = 0,
    startY = 0,
    startSX = 0,
    startSY = 0,

    isDisabled = false,
    forceShow = false,

    Init = function(self, entity)
        ECS.setTransform(entity, {
            x = self.hiddenX,
            y = self.hiddenY
        })

        Event.subscribe("SkipLogo", function()
            self.isDisabled = true

            -- Immediately hide
            ECS.setTransform(entity, {
                x = self.hiddenX,
                y = self.hiddenY
            })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply = { r=1, g=1, b=1, a=0 }
                ECS.setUIComponent(entity, ui)
            end

            -- Stop everything
            self.state = 0
        end)

        Event.subscribe("ShowLogo", function()
            self.isDisabled = false
            self.forceShow = true

            ECS.setTransform(entity, {
                x = self.targetX,
                y = self.targetY,
                sx = self.targetSX,
                sy = self.targetSY
            })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply = { r=1, g=1, b=1, a=1 }
                ECS.setUIComponent(entity, ui)
            end

            self.state = 4  -- go straight to breathing
        end)
    end,

    Update = function(self, entity)
        if self.isDisabled then return end
        if Persistent.IntroPlayed then
            ECS.setTransform(entity, {
                x = self.targetX,
                y = self.targetY,
                sx = self.targetSX,
                sy = self.targetSY
            })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply = { r=1, g=1, b=1, a=1 }
                ECS.setUIComponent(entity, ui)
            end

            return
        end
        --------------------------------------------------
        -- LEFT CLICK SKIP TO FINAL LOGO STATE
        --------------------------------------------------
        if not self.forceShow and Input.isMouseButtonTriggered(MOUSE_LEFT) then
            self.forceShow = true
            self.elapsed = 0

            Event.emitLocal("SkipIntro", {})

            ECS.setTransform(entity, {
                x = self.targetX,
                y = self.targetY,
                sx = self.targetSX,
                sy = self.targetSY
            })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply = { r=1, g=1, b=1, a=1 }
                ECS.setUIComponent(entity, ui)
            end

            self.state = 4  -- go straight to breathing
            return
        end
        --------------------------------------------------
        -- START
        --------------------------------------------------
        if not self.hasStarted and not self.forceShow then
            self.hasStarted = true

            local ui = ECS.getUIComponent(entity)
            if not ui then return end

            self.startX = self.spawnX
            self.startY = self.spawnY

            self.startSX = self.targetSX * self.startScaleMultiplier
            self.startSY = self.targetSY * self.startScaleMultiplier

            ECS.setTransform(entity, {
                x = self.startX,
                y = self.startY,
                sx = self.startSX,
                sy = self.startSY
            })

            ui.colorMultiply = { r=1, g=1, b=1, a=0 }
            ECS.setUIComponent(entity, ui)

            self.state = 1
            self.elapsed = 0
        end

        if self.state == 0 then return end

        self.elapsed = self.elapsed + Time.getDeltaTime()

        local ui = ECS.getUIComponent(entity)
        if not ui then return end

        --------------------------------------------------
        -- STATE 1: FADE IN
        --------------------------------------------------
        if self.state == 1 then
            local t = self.elapsed / self.fadeDuration

            if t >= 1 then
                t = 1
                self.state = 2
                self.elapsed = 0

                -- SHINE ONCE
                local fx = ECS.getObjectEffectComponent(entity)
                if fx then
                    fx.effectType = ECS.EffectType.SHINE
                    fx.loop = false
                    fx.effectDuration = self.pulseDuration
                    fx.intensity = 0.3
                    ECS.setObjectEffectComponent(entity, fx)
                end
            end

            local easedT = t * t
            ui.colorMultiply = { r=1, g=1, b=1, a=easedT }
            ECS.setUIComponent(entity, ui)
        end

        --------------------------------------------------
        -- STATE 2: SINGLE SLOW PULSE (START)
        --------------------------------------------------
        if self.state == 2 then
            local progress = self.elapsed / self.pulseDuration
            local pulse = math.sin(progress * math.pi) * self.pulseAmount

            ECS.setTransform(entity, {
                x = self.startX,
                y = self.startY,
                sx = self.startSX * (1 + pulse),
                sy = self.startSY * (1 + pulse)
            })

            if progress >= 1 then
                self.state = 3
                self.elapsed = 0

                local tr = ECS.getTransform(entity)
                if tr then
                    self.startX = tr.x
                    self.startY = tr.y
                    self.startSX = tr.sx
                    self.startSY = tr.sy
                end
            end
        end

        --------------------------------------------------
        -- STATE 3: MOVE TO CORNER
        --------------------------------------------------
        if self.state == 3 then
            local t = self.elapsed / self.moveDuration

            if t >= 1 then
                t = 1

                ECS.setTransform(entity, {
                    x = self.targetX,
                    y = self.targetY,
                    sx = self.targetSX,
                    sy = self.targetSY
                })

                self.startSX = self.targetSX
                self.startSY = self.targetSY

                self.state = 4
                self.elapsed = 0
            end

            local easedT = 1 - (1 - t)^3

            local newX = self.startX + (self.targetX - self.startX) * easedT
            local newY = self.startY + (self.targetY - self.startY) * easedT

            local newSX = self.startSX + (self.targetSX - self.startSX) * easedT
            local newSY = self.startSY + (self.targetSY - self.startSY) * easedT

            ECS.setTransform(entity, {
                x = newX,
                y = newY,
                sx = newSX,
                sy = newSY
            })
        end

        --------------------------------------------------
        -- STATE 4: FINAL ONE-TIME STRONG PULSE
        --------------------------------------------------
        if self.state == 4 then
            local progress = self.elapsed / self.breatheDuration

            -- stronger pulse (increase intensity here)
            local pulse = math.sin(progress * math.pi) * (self.pulseAmount * 4)

            ECS.setTransform(entity, {
                x = self.targetX,
                y = self.targetY,
                sx = self.targetSX * (1 + pulse),
                sy = self.targetSY * (1 + pulse)
            })

            if progress >= 1 then
                -- lock to final size (no more breathing)
                ECS.setTransform(entity, {
                    x = self.targetX,
                    y = self.targetY,
                    sx = self.targetSX,
                    sy = self.targetSY
                })

                self.state = 5  -- done, no more updates
                Persistent.IntroPlayed = true            end
        end

        if self.state == 5 then
            return
        end
    end
})

return class