--[[************************************************************************
  \file       UI_HealthBarShake.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.0
  \brief
    script to make healthbar shake upon hit
  \details
    EDIT YOUR DETAILS HERE
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("HealthBarShake", {
    Serializable = {
        player = 0,

        -- Hit shake
        hitShakeAmount = 5.0,
        hitShakeDuration = 0.3,

        -- Hit scale punch
        hitScale = 1.1,
    },

    onHealthChanged = 0,
    playHealthBarAnimations = 0,
    originalY = 0,
    originalSX = 1,
    originalSY = 1,

    -- track last known health
    currentHealth = 0,
    lastHealth = -1,
    hitTimer = 0,
    isHit = false,

    Init = function(self, entity)

        if self.onHealthChanged ~= 0 then
            Event.unsubscribe(self.onHealthChanged)
        end

        if self.playHealthBarAnimations ~= 0 then
            Event.unsubscribe(self.playHealthBarAnimations)
        end

        self.onHealthChanged = Event.subscribe("OnHealthChanged", function(params)
            if params.player == self.player then
                if self.lastHealth == -1 then
                    self.lastHealth = params.curr -- initialise to starting health
                end
                self.currentHealth = params.curr
            end
        end)

        self.playHealthBarAnimations = Event.subscribe("PlayHealthBarAnimations", function(params)
            if self.currentHealth < self.lastHealth then
                self.isHit = true
                self.hitTimer = self.hitShakeDuration
            end

            self.lastHealth = self.currentHealth
        end)
    end,

    Start = function(self, entity)
        local transform = ECS.getTransform(entity)

        if transform then
            self.originalY = transform.y
            self.originalSX = transform.sx
            self.originalSY = transform.sy
        end
    end,

    Update = function(self, entity)
        if not self.isHit then return end

        local dt = Time.getDeltaTime()
        local transform = ECS.getTransform(entity)

        if not transform then return end

        self.hitTimer = self.hitTimer - dt

        if self.hitTimer <= 0 then
            -- Shake finished, restore original transform exactly
            self.isHit = false
            self.hitTimer = 0

            ECS.setTransform(entity, {
                y = self.originalY,
                sx = self.originalSX,
                sy = self.originalSY,
            })

            return
        end

        local progress = math.max(0, self.hitTimer / self.hitShakeDuration)

        local offsetY = math.sin(progress * math.pi * 8) * self.hitShakeAmount * progress
        local scaleMultiplier = 1.0 + (self.hitScale - 1.0) * progress

        ECS.setTransform(entity, {
            y = self.originalY + offsetY,
            sx = self.originalSX * scaleMultiplier,
            sy = self.originalSY * scaleMultiplier,
        })
    end
})

return class