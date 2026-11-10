--[[************************************************************************
  \file       ScreenFade.lua
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

local class = DefineType("ScreenFade", {
    Serializable = {
        hiddenX = -10000,
        hiddenY = -10000,

        spawnX = 0,
        spawnY = 0,

        duration = 10.0
    },

    isFading = false,
    elapsedTime = 0,
    hasStarted = false,

    Init = function(self, entity)
        ECS.setTransform(entity, {
            x = self.hiddenX,
            y = self.hiddenY
        })

        Event.subscribe("SkipIntro", function()
            self.isFading = false

            ECS.setTransform(entity, {
                x = self.hiddenX,
                y = self.hiddenY
            })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply = { r=1, g=1, b=1, a=0 }
                ECS.setUIComponent(entity, ui)
            end
        end)
    end,

    Update = function(self, entity)
        -- PLAY ONLY ONCE
        if Persistent.IntroPlayed then
            ECS.setTransform(entity, {
                x = self.hiddenX,
                y = self.hiddenY
            })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply = { r=1, g=1, b=1, a=0 }
                ECS.setUIComponent(entity, ui)
            end

            return
        end
        --------------------------------------------------
        -- AUTO START
        --------------------------------------------------
        if not self.hasStarted then
            self.hasStarted = true

            local ui = ECS.getUIComponent(entity)
            if not ui then
                Log.error("[ScreenFade] Missing UIComponent")
                return
            end

            ECS.setTransform(entity, {
                x = self.spawnX,
                y = self.spawnY
            })

            ui.colorMultiply = { r=1,g=1,b=1,a=1 }
            ECS.setUIComponent(entity, ui)

            self.elapsedTime = 0
            self.isFading = true
        end

        if not self.isFading then return end

        self.elapsedTime = self.elapsedTime + Time.getDeltaTime()
        local t = self.elapsedTime / self.duration

        if t >= 1 then
            t = 1
            self.isFading = false

            ECS.setTransform(entity, {
                x = self.hiddenX,
                y = self.hiddenY
            })
            Event.emitLocal("ScreenFadeFinished", {})
        end

        local ui = ECS.getUIComponent(entity)
        if not ui then return end

        local easedT = t * t * (3 - 2 * t)
        
        ui.colorMultiply = {
            r = 1,
            g = 1,
            b = 1,
            a = 1 - t
        }

        ECS.setUIComponent(entity, ui)
    end
})

return class