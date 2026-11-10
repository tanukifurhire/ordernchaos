--[[************************************************************************
  \file       UI_StatusDamageIndicator.lua
  \author     
  \par        Email: 
  \date       03 April 2026
  \brief
    Visual indicator for damage modifiers (Hunter's Mark, Isolation, Burn, Wrath).
    Fades out over time and destroys itself when fully faded.
************************************************************************--]]
local class = DefineType("UI_StatusDamageIndicator",
{
    Serializable =
    {
        fadeSpeed = 1.001,        -- Alpha reduction per second (1.0 = 1 second to fade)
        floatSpeed = 0.5,       -- World units per second to float upward
        indicatorWidth = 0.5,   -- Width of each indicator for spacing calculations
        indicatorSpacing = 0.05 -- Gap between indicators
    },

    currentAlpha = 1.0,
    startY = 0,
    elapsed = 0,

    Init = function(self, entity)
        self.currentAlpha = 1.0
        self.elapsed = 0
        local transform = ECS.getTransform(entity)
        if transform then
            self.startY = transform.y
        end
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
        local dt = Time.getDeltaTime()
        self.elapsed = self.elapsed + dt

        -- Fade out
        self.currentAlpha = self.currentAlpha - (self.fadeSpeed * dt)
        if self.currentAlpha <= 0 then
            self.currentAlpha = 0
            ECS.destroyEntityDeferred(entity)
            return
        end

        -- Float upward
        local transform = ECS.getTransform(entity)
        if transform then
            ECS.setTransform(entity, {
                x = transform.x,
                y = self.startY + (self.floatSpeed * self.elapsed),
                scaleX = transform.scaleX,
                scaleY = transform.scaleY,
                rotation = transform.rotation
            })
        end

        -- Apply fade to mesh renderer
        local mr = ECS.getMeshRenderer(entity)
        if mr then
            ECS.setMeshRenderer(entity, {
                colorMultiply = {
                    r = mr.colorMultiply and mr.colorMultiply.r or 1,
                    g = mr.colorMultiply and mr.colorMultiply.g or 1,
                    b = mr.colorMultiply and mr.colorMultiply.b or 1,
                    a = self.currentAlpha
                }
            })
        end
    end
})

return class
