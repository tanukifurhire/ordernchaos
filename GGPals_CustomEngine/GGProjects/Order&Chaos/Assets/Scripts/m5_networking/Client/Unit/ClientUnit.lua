TutorialAllowedTiles = TutorialAllowedTiles or {}

local class = DefineType("ClientUnit", 
{
    Contains =
    {
        stateMachine = "ClientUnitStateMachine"
    },

    Serializable = 
    {
        maxHealth = 15,
        ownerPlayer = 1,
        unitType = 1,
        gridOffsetX = 0.01,
        gridOffsetY = 0.5,
        moveRange = 2
    },

    gridPos =
    {
        x = -1,
        y = -1,
    },

    isPlayerUnit = false,

    -- Movement data
    movementPath = nil,
    currentPathIndex = 1,
    totalPathIndex = 0,
    nextPathWorldX = 0,
    nextPathWorldY = 0,
    hasMovedThisTurn = false,
    manhattanTilesMoved = 0,

    rowOffsetY = { [0] = 0.04, [1] = 0.1, [2] = 0.08, [3] = 0.00 },
    
    tileXCorrection = {
        [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
        [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
        [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
        [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
    },

    Init = function(self, entity)
        Log.info("[ClientUnit] Initializing stateMachine")
        self.stateMachine:InitStateMachine(self, entity)

        self.gridPos = { x = -1, y = -1 }
        self.movementPath = {}

        Event.subscribe("UpdatePosForce", function(params)
            if self.ownerPlayer > 2 then return end
            if (params.unitType == self.unitType) then
                local grid
                if params.unitType >= 1 and params.unitType <= 3 then
                    grid = ClientGameManager_instance.orderGrid
                elseif params.unitType >= 4 and params.unitType <= 6 then
                    grid = ClientGameManager_instance.chaosGrid
                else
                    Log.error("[UpdatePosForce] Invalid unitType: " .. params.unitType)
                    return
                end
                
                if (self.gridPos.x > -1) and (self.gridPos.y > -1) then
                    grid:SetTileUnoccupied(self.gridPos.x, self.gridPos.y)
                end

                if grid:TrySetTileOccupied(params.x, params.y, params.unitType) then
                    self.gridPos.x = params.x
                    self.gridPos.y = params.y

                    Log.info("Force deploy unit " .. params.unitType .. " on x " .. params.x .. " y " .. params.y)

                    local gridPos = Grid.gridToWorld(grid.gridID, params.x, params.y)

                    ECS.setTransform(entity, {
                        x = gridPos.x + self.gridOffsetX,
                        y = gridPos.y + self.gridOffsetY
                    })

                    local bit = require("bit")

                    grid.unitFlags = bit.bor(grid.unitFlags or 0, bit.lshift(1, params.unitType))
                    
                    Log.info("[DEBUG] unitFlags = " .. tostring(grid.unitFlags))


                    Event.emitLocal("OnUnitDeployed", {
                        unitType = params.unitType,
                        isOrder = (params.unitType >= 1 and params.unitType <= 3)
                    })
                    
                    self.stateMachine:ChangeState(entity, self.stateMachine.states.readyState)
                    return
                else
                    Log.error("[UpdatePosForce] Failed to place unitType " .. params.unitType .. " at " .. params.x .. ", " .. params.y)
                end
            end
        end)

        Event.subscribe("TryMoveCommand", function(params)
            if self.ownerPlayer > 2 then return end
            if params.unitType ~= self.unitType then return end
            if not self.isPlayerUnit then return end
            if self.hasMovedThisTurn then
                Log.info("[ClientUnit] Unit " .. self.unitType .. " already moved this turn")
                return
            end

            -- Only accept movement when in readyState
            if self.stateMachine.currentState ~= self.stateMachine.states.readyState then
                Log.info("[ClientUnit] Unit " .. self.unitType .. " not in readyState, ignoring move")
                return
            end

            local isOrder = (self.ownerPlayer == 1)
            local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid
            local fromX = self.gridPos.x
            local fromY = self.gridPos.y
            local toX = params.gridX
            local toY = params.gridY

            local result = Grid.validateMovement(grid.gridID, fromX, fromY, toX, toY, self.moveRange)

            if not result or not result.canMove or not result.path or #result.path == 0 then
                return
            end

            grid:SetTileUnoccupied(fromX, fromY)

            if grid:TrySetTileOccupied(toX, toY, self.unitType) then
                Log.info("[ClientUnit] Move accepted: unit " .. self.unitType ..
                    " from (" .. fromX .. "," .. fromY .. ") to (" .. toX .. "," .. toY ..
                    ") pathLen=" .. #result.path)
                self.movementPath = result.path
                self.totalPathIndex = #result.path
                self.currentPathIndex = 1
                self.manhattanTilesMoved = math.abs(toX - fromX) + math.abs(toY - fromY)

                self.gridPos.x = toX
                self.gridPos.y = toY
                self.hasMovedThisTurn = true

                Event.emitLocal("OnMoveCommandSuccess", {
                    unitType = self.unitType,
                    fromX = fromX,
                    fromY = fromY,
                    toX = toX,
                    toY = toY
                })

                if self.unitType == CONSTANTS.UNIT.DRAGOON then
                    Event.emitLocal("PlayUnitWyvernMove", {})
                elseif self.unitType == CONSTANTS.UNIT.ORACLE then
                    Event.emitLocal("PlayUnitOracleMove", {})
                elseif self.unitType == CONSTANTS.UNIT.TIRION then
                    Event.emitLocal("PlayUnitTirionMove", {})
                elseif self.unitType == CONSTANTS.UNIT.IMP then
                    Event.emitLocal("PlayUnitImpMove", {})
                elseif self.unitType == CONSTANTS.UNIT.EYE then
                    Event.emitLocal("PlayUnitEyeMove", {})
                elseif self.unitType == CONSTANTS.UNIT.VIZOM then
                    Event.emitLocal("PlayUnitVizomMove", {})
                end

                self.stateMachine:ChangeState(entity, self.stateMachine.states.movementStartupState)
            end
        end)


        Event.subscribe("OnPrepPhaseReset", function(params)
            if self.ownerPlayer > 2 then return end
            self.hasMovedThisTurn = false
            self.movementPath = {}
            self.currentPathIndex = 1
            self.totalPathIndex = 0
            self.manhattanTilesMoved = 0

            -- Stop readyState darkening fade so color reset sticks
            local readyState = self.stateMachine.states.readyState
            if readyState then
                readyState.fading = false
            end

            local mr = ECS.getMeshRenderer(entity)
            if mr then
                mr.colorMultiply = { r = 1, g = 1, b = 1, a = 1 }
                ECS.setMeshRenderer(entity, mr)
            end
        end)

        Event.subscribe("OnTilesHit", function(params)
            if self.ownerPlayer > 2 then return end
            if not params.hits then return end

            -- Skip if in replayMoveState (handles hits internally)
            local currentState = self.stateMachine.currentState
            if currentState == self.stateMachine.states.replayMoveState then return end

            local isOrder = (self.ownerPlayer == 1)
            for _, hit in ipairs(params.hits) do
                if hit.unitType == self.unitType and hit.isOrderGrid == isOrder then
                    local hitState = self.stateMachine.states.hitState
                    if currentState == hitState then
                        -- Already in hitState: reset timer to replay hit animation
                        hitState.timer = hitState.hitStopDuration
                        self:PlayAnimation(entity, "Hit")
                    else
                        hitState.previousState = currentState
                        self.stateMachine:ChangeState(entity, hitState)
                    end
                    return
                end
            end
        end)

        Event.subscribe("OnUnitSupportCast", function(params)
            if params.unitType ~= self.unitType then return end

            local castState = self.stateMachine.states.supportCastState
            castState.animName = params.animName or "Ability2"
            self.stateMachine:ChangeState(entity, castState)
        end)

        -- Cross-grid teleport: instantly moves unit to a world position without tile occupancy changes
        Event.subscribe("DashUnitToWorld", function(params)
            if params.unitType ~= self.unitType then return end
            -- If ownerPlayer is specified, only respond if it matches
            if params.ownerPlayer ~= nil and params.ownerPlayer ~= self.ownerPlayer then return end

            Log.info("[ClientUnit] DashUnitToWorld (teleport): unit " .. self.unitType ..
                " (owner " .. self.ownerPlayer .. ") to world (" .. 
                string.format("%.2f", params.worldX) .. "," .. string.format("%.2f", params.worldY) .. ")")

            -- Teleport instantly
            ECS.setTransform(entity, { x = params.worldX, y = params.worldY })
            Event.emitLocal("OnDashComplete", { unitType = self.unitType, ownerPlayer = self.ownerPlayer })
        end)

        Event.subscribe("ReplayMoveUnit", function(params)
            if self.ownerPlayer > 2 then return end
            if params.unitType ~= self.unitType then return end

            local isOrder = (self.ownerPlayer == 1)
            if params.isOrder ~= isOrder then return end

            Log.info("[ClientUnit] ReplayMoveUnit: unit " .. self.unitType ..
                " from (" .. tostring(params.fromX) .. "," .. tostring(params.fromY) ..
                ") to (" .. tostring(params.toX) .. "," .. tostring(params.toY) .. ")")

            -- Store trap hit tile for mid-movement pause
            self.trapHitTile = params.trapHitTile
            self.trapHitTriggered = false

            local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

            grid:SetTileUnoccupied(self.gridPos.x, self.gridPos.y)

            if grid:TrySetTileOccupied(params.toX, params.toY, self.unitType) then
                local fromWorld = Grid.gridToWorld(grid.gridID, self.gridPos.x, self.gridPos.y)
                local toWorld = Grid.gridToWorld(grid.gridID, params.toX, params.toY)

                self.gridPos.x = params.toX
                self.gridPos.y = params.toY

                local result = Grid.validateMovement(grid.gridID, params.fromX, params.fromY, params.toX, params.toY, self.moveRange)
                if result and result.path and #result.path > 0 then
                    self.movementPath = result.path
                else
                    self.movementPath = { { x = params.toX, y = params.toY } }
                end
                self.totalPathIndex = #self.movementPath
                self.currentPathIndex = 1

                -- Tell startup state to transition to replayMoveState (not projectedMoveState)
                local startup = self.stateMachine.states.movementStartupState
                startup.nextMoveState = self.stateMachine.states.replayMoveState
                self.stateMachine:ChangeState(entity, startup)
            else
                Log.warning("[ClientUnit] ReplayMoveUnit failed: tile occupied at " .. params.toX .. "," .. params.toY)
                grid:TrySetTileOccupied(self.gridPos.x, self.gridPos.y, self.unitType)
                Event.emitLocal("OnReplayMoveComplete", { unitType = self.unitType })
            end
        end)

        Event.subscribe("Darken_Unit", function(params)
            if params.unitType == self.unitType then
                local mr = ECS.getMeshRenderer(entity)
                if mr then
                    mr.colorMultiply = { r = 0.3, g = 0.3, b = 0.3, a = 0.5 }
                    ECS.setMeshRenderer(entity, mr)
                end
            end
        end)

        Event.subscribe("Lighten_Unit", function(params)
            if params.unitType == self.unitType then
                local mr = ECS.getMeshRenderer(entity)
                if mr then
                    mr.colorMultiply = { r = 1, g = 1, b = 1, a = 1 }
                    ECS.setMeshRenderer(entity, mr)
                end
            end
        end)

        -- Unit death fade: quickly fade out mesh renderer alpha
        Event.subscribe("UnitDeath", function(params)
            if params.unitType == self.unitType then
                Log.info("[ClientUnit] UnitDeath: fading out unit " .. self.unitType)
                self._deathFading = true
                self._deathAlpha = 1.0
            end
        end)
    end,

    Start = function(self, entity)
        if PassAndPlayServer_instance or ClientGameManager_instance.playerFaction == self.ownerPlayer then
            self.isPlayerUnit = true
        end
    end,

    PlayAnimation = function(self, entity, stateName)
        local animInfo = ECS.getAnimation(entity)
        if animInfo then
            ECS.setAnimation(entity, stateName)
        end
    end,

    OnClick = function(self, entity)
        if self.isPlayerUnit and not ClientGameManager_instance.awaitingServer then self.stateMachine:HandleOnClick(entity) end
    end,

    OnDrop = function(self, entity)
        if self.isPlayerUnit and not ClientGameManager_instance.awaitingServer then self.stateMachine:HandleOnDrop(entity) end
    end,

    Update = function(self, entity)
        -- Death fade
        if self._deathFading then
            self._deathAlpha = self._deathAlpha - Time.getDeltaTime() * 4.0  -- ~0.25s fade
            if self._deathAlpha <= 0 then
                self._deathAlpha = 0
                self._deathFading = false
            end
            local mr = ECS.getMeshRenderer(entity)
            if mr then
                mr.colorMultiply = { r = 1, g = 1, b = 1, a = self._deathAlpha }
                ECS.setMeshRenderer(entity, mr)
            end
            return
        end

        if (self.gridPos.x > -1) and (self.gridPos.y > -1) then
            self.gridOffsetX = self.tileXCorrection[self.gridPos.y] and (self.tileXCorrection[self.gridPos.y][self.gridPos.x] or 0) or 0
        end

        if self.isPlayerUnit then
            self.stateMachine:HandleStateMachineInput(entity)
        end

        self.stateMachine:UpdateStateMachine(entity)
    end,

    RecalculateGridOffset = function(self)
        if (self.gridPos.x > -1) and (self.gridPos.y > -1) then
            self.gridOffsetX = self.tileXCorrection[self.gridPos.y] and (self.tileXCorrection[self.gridPos.y][self.gridPos.x] or 0) or 0
        end
    end
})

return class