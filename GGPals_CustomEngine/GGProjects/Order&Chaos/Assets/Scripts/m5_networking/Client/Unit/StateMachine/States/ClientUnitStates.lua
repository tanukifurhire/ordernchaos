local class = DefineType("ClientUnitStates",
{
    Contains = 
    {
        deployState = "ClientDeployState",
        readyState = "ClientReadyState",
        projectedMoveState = "ClientProjectedMoveState",
        replayMoveState = "ClientReplayMoveState",
        movementStartupState = "ClientMovementStartupState",
        hitState = "ClientHitState",
        supportCastState = "ClientSupportCastState"
    },

    Init = function(self, stateMachine, entity)
        self.deployState:Init(stateMachine, entity)
        self.readyState:Init(stateMachine, entity)
        self.projectedMoveState:Init(stateMachine, entity)
        self.replayMoveState:Init(stateMachine, entity)
        self.movementStartupState:Init(stateMachine, entity)
        self.hitState:Init(stateMachine, entity)
        self.supportCastState:Init(stateMachine, entity)
    end
})

return class