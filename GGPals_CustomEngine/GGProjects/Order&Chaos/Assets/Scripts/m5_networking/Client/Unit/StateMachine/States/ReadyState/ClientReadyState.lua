local class = DefineType("ClientReadyState", 
{
    stateMachine = nil,

    Serializable =
    {
        fadeDuration = 0.25
    },

    fadeT = 0,
    fading = false,

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)
        local unit = self.stateMachine.unit
        unit:PlayAnimation(entity, "Idle")

        if unit.hasMovedThisTurn then
            self.fadeT = 0
            self.fading = true
        end
    end,

    Exit = function(self, entity)
        self.fading = false
    end,

    HandleInput = function(self, entity)
    end,
    
    OnClick = function(self, entity)
    end,

    OnDrop = function(self, entity)
    end,

    Update = function(self, entity)
        if self.fading then
            self.fadeT = math.min(self.fadeT + Time.getDeltaTime(), self.fadeDuration)
            local t = self.fadeT / self.fadeDuration
            local darkness = 1.0 - 0.4 * t

            local mr = ECS.getMeshRenderer(entity)
            if mr then
                mr.colorMultiply = { r = darkness, g = darkness, b = darkness, a = 1 }
                ECS.setMeshRenderer(entity, mr)
            end

            if self.fadeT >= self.fadeDuration then
                self.fading = false
            end
        end
    end
})

return class