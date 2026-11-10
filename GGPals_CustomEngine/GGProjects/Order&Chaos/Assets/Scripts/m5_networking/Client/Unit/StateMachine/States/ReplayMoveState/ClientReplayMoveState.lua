local class = DefineType("ClientReplayMoveState", 
{
    stateMachine = nil,

    Serializable = 
    {
        moveEndDelay = 0.3,
        trapHitPauseDuration = 0.5
    },

    isEnding = false,
    endTimer = 0,
    trapHitPaused = false,
    trapHitTimer = 0,

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)
        self.isEnding = false
        self.endTimer = 0
        self.trapHitPaused = false
        self.trapHitTimer = 0

        local unit = self.stateMachine.unit
        Log.info("[ReplayMove] Enter: unit=" .. unit.unitType ..
            " pathLength=" .. (unit.movementPath and #unit.movementPath or 0))
        unit:PlayAnimation(entity, "MoveLoop")

        if unit.movementPath and #unit.movementPath > 0 then
            unit.currentPathIndex = 1

            local isOrder = (unit.ownerPlayer == 1)
            local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

            local tile = unit.movementPath[1]
            local worldPos = Grid.gridToWorld(grid.gridID, tile.x, tile.y)
            if worldPos then
                unit.nextPathWorldX = worldPos.x + unit.gridOffsetX
                unit.nextPathWorldY = worldPos.y + unit.gridOffsetY
            end
        end
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
        local unit = self.stateMachine.unit

        -- Handle trap hit pause (unit plays Hit anim, then resumes moving)
        if self.trapHitPaused then
            self.trapHitTimer = self.trapHitTimer - Time.getDeltaTime()
            if self.trapHitTimer <= 0 then
                self.trapHitPaused = false
                unit:PlayAnimation(entity, "MoveLoop")
                -- Advance to next path tile and continue
                unit.currentPathIndex = unit.currentPathIndex + 1
                if unit.currentPathIndex <= unit.totalPathIndex then
                    local tile = unit.movementPath[unit.currentPathIndex]
                    local isOrder = (unit.ownerPlayer == 1)
                    local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid
                    local worldPos = Grid.gridToWorld(grid.gridID, tile.x, tile.y)
                    if worldPos then
                        unit.nextPathWorldX = worldPos.x + unit.gridOffsetX
                        unit.nextPathWorldY = worldPos.y + unit.gridOffsetY
                    end
                else
                    self:FinishMovement(entity)
                end
            end
            return
        end

        if self.isEnding then
            self.endTimer = self.endTimer - Time.getDeltaTime()
            if self.endTimer <= 0 then
                Log.info("[ReplayMove] Emitting OnReplayMoveComplete for unit=" .. unit.unitType)
                Event.emitLocal("OnReplayMoveComplete", { unitType = unit.unitType })
                self.stateMachine:ChangeState(entity, self.stateMachine.states.readyState)
            end
            return
        end

        if not unit.movementPath or unit.currentPathIndex > unit.totalPathIndex then
            self:FinishMovement(entity)
            return
        end

        local transform = ECS.getTransform(entity)
        local dx = unit.nextPathWorldX - transform.x
        local dy = unit.nextPathWorldY - transform.y
        local dist = math.sqrt(dx * dx + dy * dy)

        if dist < CONSTANTS.EPSILON then
            transform.x = unit.nextPathWorldX
            transform.y = unit.nextPathWorldY
            ECS.setTransform(entity, transform)

            -- Check if we arrived at a trap tile
            local arrivedTile = unit.movementPath[unit.currentPathIndex]
            if unit.trapHitTile and not unit.trapHitTriggered
               and arrivedTile and arrivedTile.x == unit.trapHitTile.x and arrivedTile.y == unit.trapHitTile.y then
                unit.trapHitTriggered = true
                self.trapHitPaused = true
                self.trapHitTimer = self.trapHitPauseDuration
                unit:PlayAnimation(entity, "Hit")
                Log.info("[ReplayMove] Unit " .. unit.unitType .. " hit trap at (" .. arrivedTile.x .. "," .. arrivedTile.y .. ")")
                Event.emitLocal("OnUnitTrapHit", { unitType = unit.unitType })
                return
            end

            unit.currentPathIndex = unit.currentPathIndex + 1

            if unit.currentPathIndex <= unit.totalPathIndex then
                local tile = unit.movementPath[unit.currentPathIndex]
                local isOrder = (unit.ownerPlayer == 1)
                local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

                local worldPos = Grid.gridToWorld(grid.gridID, tile.x, tile.y)
                if worldPos then
                    unit.nextPathWorldX = worldPos.x + unit.gridOffsetX
                    unit.nextPathWorldY = worldPos.y + unit.gridOffsetY
                end
            else
                self:FinishMovement(entity)
            end
            return
        end

        dx = dx / dist
        dy = dy / dist

        local step = CONSTANTS.MOVE_SPEED_NORMAL * Time.getDeltaTime()
        transform.x = transform.x + dx * math.min(step, dist)
        transform.y = transform.y + dy * math.min(step, dist)
        ECS.setTransform(entity, transform)
    end,

    FinishMovement = function(self, entity)
        self.isEnding = true
        self.endTimer = self.moveEndDelay

        local unit = self.stateMachine.unit
        Log.info("[ReplayMove] FinishMovement: unit=" .. unit.unitType .. " (will emit OnReplayMoveComplete after " .. self.moveEndDelay .. "s)")
        unit:PlayAnimation(entity, "MoveEnd")
    end
})

return class