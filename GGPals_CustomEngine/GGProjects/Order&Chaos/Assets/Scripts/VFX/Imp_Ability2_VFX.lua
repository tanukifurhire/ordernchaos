local class = DefineType("Imp_Ability2_VFX", {
    Serializable = {
        gridOffsetX = 0.5,
        gridOffsetY = 0.15
    },
    
    onMove = false,
    targetGrid = {
        x = -1,
        y = -1
    },

    forceMoveHandler = 0,

    Init = function(self, entity)
        self.forceMoveHandler = Event.subscribe("OnForceMoveUnit", function(params)
            if params.ability == CONSTANTS.ABILITIES.IMP_2 then
                Log.info("Force unit movement")
                if params.toX >= 0 and params.toY >= 0 then
                    self.onMove = true
                    self.targetGrid.x = params.toX
                    self.targetGrid.y = params.toY
                    Log.info("Moving to grid pos " .. params.toX .. ", " .. params.toY)

                    -- Rotate fist to face the direction of the pull
                    local grid = Grids_instance.OrderGrid
                    if not grid then grid = ClientGameManager_instance.orderGrid end

                    local transform = ECS.getTransform(entity)
                    local targetWorldPos = Grid.gridToWorld(grid.gridID, params.toX, params.toY)
                    local dx = (targetWorldPos.x + self.gridOffsetX) - transform.x
                    local dy = (targetWorldPos.y + self.gridOffsetY) - transform.y
                    local angle = math.atan2(dy, dx)
                    ECS.setTransform(entity, {
                        x = transform.x,
                        y = transform.y,
                        rotation = angle
                    })
                else
                    Log.info("Invalid coordinates " .. params.toX .. ", " .. params.toY)
                end
            end
        end)
    end,

    Start = function(self, entity)
        Imp_Ability2_VFX.PlayAnimation(self, entity, "Fire")
    end,

    Update = function(self, entity)
        if not self.onMove then return end

        local grid = ClientGameManager_instance.orderGrid

        local transform = ECS.getTransform(entity)

        local targetWorldPos = Grid.gridToWorld(grid.gridID, self.targetGrid.x, self.targetGrid.y)
        targetWorldPos.x = targetWorldPos.x + self.gridOffsetX
        targetWorldPos.y = targetWorldPos.y + self.gridOffsetY

        local dx = targetWorldPos.x - transform.x
        local dy = targetWorldPos.y - transform.y
        local dist = math.sqrt(dx * dx + dy * dy)

        if dist < CONSTANTS.EPSILON then
            transform.x = targetWorldPos.x
            transform.y = targetWorldPos.y

            ECS.setTransform(entity, transform)
            self.onMove = false
            Event.emitLocal("TriggerVFXFinished", {})
        end

        dx = dx / dist
        dy = dy / dist

        local step = CONSTANTS.MOVE_SPEED_NORMAL * Time.getDeltaTime()
        transform.x = transform.x + dx * math.min(step, dist)
        transform.y = transform.y + dy * math.min(step, dist)

        ECS.setTransform(entity, transform)
    end,

    PlayAnimation = function(self, entity, stateName)
        -- Check if entity has animator component
        local animInfo = ECS.getAnimation(entity)
        if not animInfo then
            -- No animation component, skip silently
            return
        end
        
        -- Don't switch if already playing this state
        if animInfo.currentState == stateName then
            return
        end
        
        -- Set the new animation state
        ECS.setAnimation(entity, stateName)
    end,

    OnVFXAnimationTransition = function(self, entity)
        Event.emitLocal("Imp_Ability2_VFX_Transition", {})
        Event.emitLocal("UpdateGridTextures", {
            playerId = CONSTANTS.CHAOS_PLAYER
        })
        Event.emitLocal("PlayHealthBarAnimations", {})
        Imp_Ability2_VFX.PlayAnimation(self, entity, "Loop")
    end,

    OnVFXAnimationFinished = function(self, entity)
        if not self.onMove then
            Event.emitLocal("PreviewVFXFinished", {})
            Event.emitLocal("TriggerVFXFinished", {})
            Event.unsubscribe(self.forceMoveHandler)
            ECS.destroyEntityDeferred(entity)
        end
    end
})

return class