--[[************************************************************************
  \file       TextFadeLogo.lua
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

local class = DefineType("TextFade", {
    Serializable = {
        duration = 15.0
    },

    isFading = false,
    elapsedTime = 0,
    hasStarted = false,

    Init = function(self, entity)
        Event.subscribe("SkipIntro", function()
            self.isFading = false

            local tc = ECS.getTextComponent(entity)
            if tc then
                tc.visible = false
                tc.color = { r=1, g=1, b=1, a=0 }
                ECS.setTextComponent(entity, tc)
            end
        end)
    end,

    Update = function(self, entity)
        if Persistent.IntroPlayed then
            local tc = ECS.getTextComponent(entity)
            if tc then
                tc.visible = false
                ECS.setTextComponent(entity, tc)
            end
            return
        end

        --------------------------------------------------
        -- AUTO START
        --------------------------------------------------
        if not self.hasStarted then
            self.hasStarted = true

            local tc = ECS.getTextComponent(entity)
            if not tc then
                Log.error("[TextFade] Missing TextComponent")
                return
            end

            tc.visible = true
            tc.color = { r=1, g=1, b=1, a=1 }
            ECS.setTextComponent(entity, tc)

            self.elapsedTime = 0
            self.isFading = true
        end

        if not self.isFading then return end

        self.elapsedTime = self.elapsedTime + Time.getDeltaTime()
        local t = self.elapsedTime / self.duration

        if t >= 1 then
            t = 1
            self.isFading = false

            local tc = ECS.getTextComponent(entity)
            if tc then
                tc.visible = false
                ECS.setTextComponent(entity, tc)
            end

            return
        end

        local easedT = t * t * (3 - 2 * t)

        local tc = ECS.getTextComponent(entity)
        if not tc then return end

        tc.color = {
            r = 1,
            g = 1,
            b = 1,
            a = 1 - easedT
        }

        ECS.setTextComponent(entity, tc)
    end
})

return class