local class = DefineType("ClientSupportCastState", 
{
    stateMachine = nil,

    Serializable = 
    {
        castDuration = 0.8
    },

    timer = 0,
    animName = "Ability2",

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)
        local meshRenderer = ECS.getMeshRenderer(entity)
        meshRenderer.layer = 3
        ECS.setMeshRenderer(entity, meshRenderer)

        self.timer = self.castDuration
        local unit = self.stateMachine.unit
        unit:PlayAnimation(entity, self.animName)
    end,

    Exit = function(self, entity)
        local meshRenderer = ECS.getMeshRenderer(entity)
        meshRenderer.layer = 2
        ECS.setMeshRenderer(entity, meshRenderer)
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
            self.stateMachine:ChangeState(entity, self.stateMachine.states.readyState)
        end
    end
})

return class
