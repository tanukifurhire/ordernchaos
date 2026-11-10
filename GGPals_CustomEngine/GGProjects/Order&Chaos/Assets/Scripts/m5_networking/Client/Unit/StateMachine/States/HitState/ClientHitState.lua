local class = DefineType("ClientHitState", 
{
    stateMachine = nil,

    Serializable = 
    {
        hitStopDuration = 0.5
    },

    timer = 0,
    previousState = nil,

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)
        self.timer = self.hitStopDuration
        local unit = self.stateMachine.unit
        unit:PlayAnimation(entity, "Hit")
        Event.emitLocal("PlayCombatUnitHit", {})
    end,

    Exit = function(self, entity)
    end,

    HandleInput = function(self, entity)
    end,

    OnClick = function(self, entity)
    end,

    OnDrop = function(self, entity)
    end,

    Update = function(self, entity)
        self.timer = self.timer - Time.getDeltaTime()

        if self.timer <= 0 then
            if self.previousState then
                self.stateMachine:ChangeState(entity, self.previousState)
                self.previousState = nil
            else
                self.stateMachine:ChangeState(entity, self.stateMachine.states.readyState)
            end
        end
    end
})

return class
