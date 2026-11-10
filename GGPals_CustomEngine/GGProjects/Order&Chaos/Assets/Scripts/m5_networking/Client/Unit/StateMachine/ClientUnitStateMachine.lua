local class = DefineType("ClientUnitStateMachine",
{
    Contains =
    {
        states = "ClientUnitStates"
    },

    currentState = nil,
    unit = nil,

    InitStateMachine = function(self, unit, entity)
        self.unit = unit
        self.states:Init(self, entity)

        if not self.currentState then
            self.currentState = self.states.deployState
            self.currentState:Enter(entity)
        end
    end,

    ChangeState = function(self, entity, state)
        if self.currentState and state then
            self.currentState:Exit(entity)
            self.currentState = state
            self.currentState:Enter(entity)
        end
    end,

    HandleStateMachineInput = function(self, entity)
        if self.currentState then
            self.currentState:HandleInput(entity)
        end
    end,

    HandleOnClick = function(self, entity)
        if self.currentState then
            self.currentState:OnClick(entity)
        end
    end,

    HandleOnDrop = function(self, entity)
        if self.currentState then
            self.currentState:OnDrop(entity)
        end
    end,

    UpdateStateMachine = function(self, entity)
        if self.currentState then
            self.currentState:Update(entity)
        end
    end
})

return class