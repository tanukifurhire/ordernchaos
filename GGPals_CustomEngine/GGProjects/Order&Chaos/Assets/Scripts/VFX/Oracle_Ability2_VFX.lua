local class = DefineType("Oracle_Ability2_VFX", {
    Serializable = {
        offsetX = 0.0,
        offsetY = -0.2,
        shieldDelay = 1.0
    },

    timer = 0.0,
    hasTriggered = false,

    Init = function(self, entity)
        local transform = ECS.getTransform(entity)
        if transform then
            transform.x = transform.x + self.offsetX
            transform.y = transform.y + self.offsetY
            ECS.setTransform(entity, transform)
        end
    end,

    Start = function(self, entity)
        Event.emitLocal("PlayVFXOracleA2", {})
    end,

    Update = function(self, entity)
        if self.hasTriggered then return end

        self.timer = self.timer + Time.getDeltaTime()
        if self.timer >= self.shieldDelay then
            self.hasTriggered = true
            Event.emitLocal("Oracle_Ability2_VFX_Transition", {})
        end
    end,

    OnVFXAnimationFinished = function(self, entity)
        Event.emitLocal("TriggerVFXFinished", {})
        ECS.destroyEntityDeferred(entity)
    end
})

return class