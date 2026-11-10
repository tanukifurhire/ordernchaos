local class = DefineType("Oracle", {
    Serializable = {
        ownerPlayer = 1,
        unitType = 2,
        maxHealth = 12,
        moveRange = 2,
        gridOffsetX = 0.01,
        gridOffsetY = 0.5
    },

    Oracle_MovementPath = {},

    currentHealth = 12,
    gridX = -1,
    gridY = -1,
    previousGridX = -1,
    previousGridY = -1,

    hasActedThisTurn = false,
    hasMovedThisTurn = false,

    selected = false,
    deployed = false,
    shouldMove = false,
    shouldMoveForce = false,
    isInHitReaction = false,

    pedestalPosX = 0.1,
    pedestalPosY = 0.1,

    currentPathIndex = 1,
    totalPathIndex = 1,
    nextPathWorldX = 0,
    nextPathWorldY = 0,

    -- fade
    fadeT = 0,
    fadeDuration = 0.25,
    fading = false,

    -- Replay
    isReplaying = false,

    Init = function(self, entity)
        local transform = ECS.getTransform(entity)
        self.pedestalPosX = transform.x
        self.pedestalPosY = transform.y

        self.currentHealth = self.maxHealth

        Event.subscribe("TryMoveUnit", function(params)
            if params.unitType == self.unitType and not self.hasMovedThisTurn then
                Event.emitLocal("ShowUnitMovement", {
                    unitType = params.unitType,
                    gridX = params.gridX,
                    gridY = params.gridY,
                    range = self.moveRange
                })
            end
        end)

        Event.subscribe("TryMoveCommand", function(params)
            if params.unitType ~= self.unitType or self.hasMovedThisTurn then return end

            local result = ValidateUnitMovement(-1, params.isOrder,
                self.gridX, self.gridY,
                params.gridX, params.gridY,
                self.moveRange)

            if result and #result.path > 0 then
                local session = SessionManager_instance.sessionID
                self.Oracle_MovementPath = result.path
                self.totalPathIndex = #result.path

                if TrySetTileOccupied(-1, params.isOrder, params.gridX, params.gridY, self.unitType) then
                    SetTileUnoccupied(-1, params.isOrder, self.gridX, self.gridY)

                    self.previousGridX = self.gridX
                    self.previousGridY = self.gridY
                    self.gridX = params.gridX
                    self.gridY = params.gridY

                    AbilityList[CONSTANTS.ABILITIES.MOVEMENT].Queue(self, entity)

                    self.hasMovedThisTurn = true
                    AbilityManager_instance.orderTilesMoved = (AbilityManager_instance.orderTilesMoved or 0) + (#result.path - 1)

                    Oracle.PlayAnimation(self, entity, "MoveStart")
                end
            end

            Event.emitLocal("ClearHighlights", {})
        end)

        Event.subscribe("InPlanningPhase", function()
            if self.previousGridX >= 0 and self.previousGridY >= 0 then
                Oracle.MoveTo(self, entity, self.previousGridX, self.previousGridY)
            end

            -- reset tint + fade
            local mr = ECS.getMeshRenderer(entity)
            if mr then
                mr.colorMultiply = { r = 1, g = 1, b = 1, a = 1 }
                ECS.setMeshRenderer(entity, mr)
            end

            self.fading = false
            self.fadeT = 0
        end)

        Event.subscribe("OnEndPlanningPhase", function()
            self.hasMovedThisTurn = false
            self.hasActedThisTurn = false

            if self.previousGridX >= 0 and self.previousGridY >= 0 then
                Log.info("Oracle resetting pos!")
                Oracle.MoveTo(self, entity, self.previousGridX, self.previousGridY)
            end

            local mr = ECS.getMeshRenderer(entity)
            if mr then
                mr.colorMultiply = { r = 1, g = 1, b = 1, a = 1 }
                ECS.setMeshRenderer(entity, mr)
            end

            self.fadeT = 0
            self.fading = false
        end)

        Event.subscribe("ReplayUnitMovement", function(params)
            if params.unitType == self.unitType then
                self.previousGridX = -1
                self.previousGridY = -1
                self.isReplaying = true
                
                -- Restart movement from beginning
                self.currentPathIndex = 1
                if self.Oracle_MovementPath and #self.Oracle_MovementPath > 0 then
                    local tile = self.Oracle_MovementPath[self.currentPathIndex]
                    Oracle.MoveTowards(self, entity, tile.x, tile.y)
                    self.shouldMove = true
                end

                Oracle.PlayAnimation(self, entity, "MoveStart")
            end
        end)

        Event.subscribe("ReplaySupportAbility", function(params)
            if params.unitType == self.unitType then
                self.isReplaying = true
                Event.emitLocal("TriggerOracle2AnimPreview", {})
                Oracle.PlayAnimation(self, entity, "Ability2")
            end
        end)

        Event.subscribe("OnForceMoveUnit", function(params)
            if params.unitType ~= self.unitType then return end
            Oracle.MoveToForce(self, entity, params)
        end)

        Event.subscribe("TriggerOracle1", function()
            Event.emitLocal("TriggerOracle1AnimPreview", {})
            Oracle.PlayAnimation(self, entity, "Ability1")
        end)

        Event.subscribe("TriggerOracle2", function()
            Event.emitLocal("TriggerOracle2AnimPreview", {})
            Oracle.PlayAnimation(self, entity, "Ability2")
        end)

        -- subscribe to OnTilesHit from Abilities script
        Event.subscribe("OnTilesHit", function(params)
            Oracle.OnTilesHit(self, entity, params)
        end)

        Event.subscribe("OnTrapTriggered", function(params)
            if params.unitType == self.unitType then
                Log.info("[Oracle] Trap damage! Health: " .. self.currentHealth)
                
                Oracle.PlayAnimation(self, entity, "Hit")

                Event.emitLocal("TakeDamage", {
                    player = CONSTANTS.ORDER_PLAYER,
                    damage = params.damage
                })

                Event.emitLocal("PlayHealthBarAnimations", {})
            end
        end)
    end,

    Update = function(self, entity)
        if self.selected and not self.deployed then
            local worldPos = Input.getMouseWorldPosition()
            ECS.setTransform(entity, { x = worldPos.x, y = worldPos.y })
        end

        if self.shouldMove then Oracle.UpdateMovement(self, entity) end

        Oracle.UpdateMovementForced(self, entity)

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
    end,

    -- ================= DEPLOYMENT =================

    OnClick = function(self, entity)
        Event.emitLocal("PlayGridPickup", {})
        self.selected = true

        if not self.deployed then
            Event.emitLocal("PlayUnitOracleCry", {})
            Event.emitLocal("HighlightValidPlacements", {
                playerId = self.ownerPlayer
            })
        end
    end,

    OnDrop = function(self, entity)
        if self.selected and not self.deployed then
            Event.emitLocal("ClearHighlights", {})
            self.selected = false

            local isOrder = (self.ownerPlayer == 1)
            local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
            if not grid then return end

            local worldPos = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, worldPos.x, worldPos.y)

            if tilePos then
                if TrySetTileOccupied(-1, isOrder, tilePos.x, tilePos.y, self.unitType) then
                    Event.emitLocal("PlayGridPlaceGrid", {})

                    self.deployed = true
                    self.gridX = tilePos.x
                    self.gridY = tilePos.y
                    self.previousGridX = tilePos.x
                    self.previousGridY = tilePos.y

                    local gridPos = Grid.gridToWorld(grid.gridID, tilePos.x, tilePos.y)

                    ECS.setTransform(entity, {
                        x = gridPos.x + self.gridOffsetX,
                        y = gridPos.y + self.gridOffsetY
                    })

                    ECS.setMeshRenderer(entity, {
                        perspectiveFactor = CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR
                    })

                    Event.emitLocal("OnUnitDeployed", {
                        isOrder = true,
                        unitType = CONSTANTS.UNIT.ORACLE
                    })

                    return
                end
            end

            ECS.setTransform(entity, {
                x = self.pedestalPosX,
                y = self.pedestalPosY
            })
        end

        self.selected = false
    end,

    ------------------------

    PlayAnimation = function(self, entity, stateName)
        local animInfo = ECS.getAnimation(entity)
        if animInfo and animInfo.currentState ~= stateName then
            ECS.setAnimation(entity, stateName)
        end
    end,

    OnMoveStartFinished = function(self, entity)
        Event.emitLocal("PlayUnitOracleMove", {})
        self.currentPathIndex = 1
        local tile = self.Oracle_MovementPath[1]
        self.shouldMove = true
        Oracle.PlayAnimation(self, entity, "MoveLoop")
        Oracle.MoveTowards(self, entity, tile.x, tile.y)
    end,

    OnMoveEndFinished = function(self, entity)
        if self.isReplaying then
            self.isReplaying = false
            Event.emitLocal("ReplayComplete", {})
        end
        Oracle.PlayAnimation(self, entity, "Idle")
    end,

    OnAbility1Transition = function(self, entity)
        Event.emitLocal("TriggerOracle1Projectile", {})
        Event.emitLocal("UpdateGridTextures", { playerId = self.ownerPlayer })
        Event.emitLocal("PlayHealthBarAnimations", {})
    end,

    OnAbility1Finished = function(self, entity)
        Oracle.PlayAnimation(self, entity, "Idle")
        Event.emitLocal("AbilityAnimationFinished", { player = self.ownerPlayer })
    end,

    OnAbility2Transition = function(self, entity)
        Event.emitLocal("TriggerOracle2VFX", {
            gridX = self.gridX,
            gridY = self.gridY
        })
    end,

    OnAbility2Finished = function(self, entity)
        if self.isReplaying then
            self.isReplaying = false
            Event.emitLocal("ReplayComplete", {})
        end

        Oracle.PlayAnimation(self, entity, "Idle")
    end,

    -- If tile unit is standing on is hit, play hit animation
    OnTilesHit = function(self, entity, params)
        local hits = params.hits
        if not hits then
            Log.info("No hits!")
            return
        end


        for _, hit in ipairs(hits) do
            -- on the same grid
            if hit.unitType == self.unitType and not self.isInHitReaction then
                self.isInHitReaction = true
                Log.info("I'm hit!")
                Oracle.PlayAnimation(self, entity, "Hit")
            end
        end
    end, -- end of OnTilesHit

    -- Once hit is finished, resume idle animation
    OnHitFinished = function(self, entity)
        Log.info("[Oracle] Hit animation finished! Playing Idle animation now")
        self.isInHitReaction = false
        Oracle.PlayAnimation(self, entity, "Idle")
    end, --  end of OnHitFinished

    MoveToForce = function(self, entity, params)
        self.shouldMoveForce = true
        local grid = Grids_instance.OrderGrid
        if not grid then return end

        local worldPos = Grid.gridToWorld(grid.gridID, params.toX, params.toY)
        if worldPos then
            self.nextPathWorldX = worldPos.x + self.gridOffsetX
            self.nextPathWorldY = worldPos.y + self.gridOffsetY
            self.gridX = params.toX
            self.gridY = params.toY
            self.previousGridX = -1
            self.previousGridY = -1
        end
    end,

    MoveTowards = function(self, entity, x, y)
        local grid = Grids_instance.OrderGrid
        if not grid then return end

        local worldPos = Grid.gridToWorld(grid.gridID, x, y)
        if worldPos then
            self.nextPathWorldX = worldPos.x + self.gridOffsetX
            self.nextPathWorldY = worldPos.y + self.gridOffsetY
        end
    end,

    MoveTo = function(self, entity, x, y)
        local grid = Grids_instance.OrderGrid
        if not grid then return end

        local worldPos = Grid.gridToWorld(grid.gridID, x, y)
        if worldPos then
            local transform = ECS.getTransform(entity)

            transform.x = worldPos.x + self.gridOffsetX
            transform.y = worldPos.y + self.gridOffsetY

            ECS.setTransform(entity, transform)
        end
    end,

    UpdateMovement = function(self, entity)
        if self.currentPathIndex <= self.totalPathIndex then
            local transform = ECS.getTransform(entity)

            local dx = self.nextPathWorldX - transform.x
            local dy = self.nextPathWorldY - transform.y
            local dist = math.sqrt(dx*dx + dy*dy)

            if dist < CONSTANTS.EPSILON then
                transform.x = self.nextPathWorldX
                transform.y = self.nextPathWorldY
                ECS.setTransform(entity, transform)

                -- Check for trap on EVERY tile during resolution
                if self.isReplaying then
                    local currentTile = self.Oracle_MovementPath[self.currentPathIndex]
                    if currentTile then
                        local trapData = CheckAndTriggerTrap(true, currentTile.x, currentTile.y)
                        if trapData then
                            Log.info("[Oracle] Stepped on trap at (" .. currentTile.x .. ", " .. currentTile.y .. ")! Taking " .. trapData.damage .. " damage")
                            
                            Event.emitLocal("OnTrapTriggered", {
                                unitType = self.unitType,
                                gridX = currentTile.x,
                                gridY = currentTile.y,
                                damage = trapData.damage,
                                vfxEntity = trapData.vfxEntity
                            })
                        end
                    end
                end

                self.currentPathIndex = self.currentPathIndex + 1

                if self.currentPathIndex <= self.totalPathIndex then
                    local tile = self.Oracle_MovementPath[self.currentPathIndex]
                    Oracle.MoveTowards(self, entity, tile.x, tile.y)
                else
                    self.shouldMove = false
                    Oracle.PlayAnimation(self, entity, "MoveEnd")
                    
                    if not self.isReplaying then
                        self.fadeT = 0
                        self.fading = true
                    end
                end
                return
            end

            dx = dx / dist
            dy = dy / dist

            local step = CONSTANTS.MOVE_SPEED_NORMAL * Time.getDeltaTime()
            transform.x = transform.x + dx * math.min(step, dist)
            transform.y = transform.y + dy * math.min(step, dist)

            ECS.setTransform(entity, transform)
        end
    end,

    UpdateMovementForced = function(self, entity)
        if not self.shouldMoveForce then return end

        local transform = ECS.getTransform(entity)

        local dx = self.nextPathWorldX - transform.x
        local dy = self.nextPathWorldY - transform.y
        local dist = math.sqrt(dx*dx + dy*dy)

        if dist < CONSTANTS.EPSILON then
            transform.x = self.nextPathWorldX
            transform.y = self.nextPathWorldY
            ECS.setTransform(entity, transform)
            self.shouldMoveForce = false
            return
        end

        dx = dx / dist
        dy = dy / dist

        local step = CONSTANTS.MOVE_SPEED_NORMAL * Time.getDeltaTime()
        transform.x = transform.x + dx * math.min(step, dist)
        transform.y = transform.y + dy * math.min(step, dist)

        ECS.setTransform(entity, transform)
    end
})

return class
