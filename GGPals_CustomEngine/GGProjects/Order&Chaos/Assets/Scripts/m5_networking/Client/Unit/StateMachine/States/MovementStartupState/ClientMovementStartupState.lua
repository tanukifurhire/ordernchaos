local class = DefineType("ClientMovementStartupState", 
{
    stateMachine = nil,

    Serializable = 
    {
        startupDelay = 0.3
    },

    timer = 0,
    nextMoveState = nil,

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)
        self.timer = self.startupDelay
        Log.info("[MovementStartup] Enter: delay=" .. self.startupDelay ..
            " nextMoveState=" .. (self.nextMoveState and "REPLAY" or "PROJECTED"))

        local unit = self.stateMachine.unit
        unit:PlayAnimation(entity, "MoveStart")
    end,

    Exit = function(self, entity)
        local unit = self.stateMachine.unit
        if unit.movementPath and #unit.movementPath > 0 then
            unit.currentPathIndex = 1

            local tile = unit.movementPath[1]
            local isOrder = (unit.ownerPlayer == 1)
            local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

            local worldPos = Grid.gridToWorld(grid.gridID, tile.x, tile.y)
            if worldPos then
                unit.nextPathWorldX = worldPos.x + unit.gridOffsetX
                unit.nextPathWorldY = worldPos.y + unit.gridOffsetY
            end
        end
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
            local target = self.nextMoveState or self.stateMachine.states.projectedMoveState
            Log.info("[MovementStartup] Timer done, transitioning to " ..
                (self.nextMoveState and "replayMoveState" or "projectedMoveState"))
            self.nextMoveState = nil
            self.stateMachine:ChangeState(entity, target)
        end
    end
})

return class
