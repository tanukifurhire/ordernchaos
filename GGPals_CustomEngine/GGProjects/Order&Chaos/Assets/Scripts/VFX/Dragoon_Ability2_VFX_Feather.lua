local class = DefineType("Dragoon_Ability2_VFX_Feather", {
    Serializable = {
        flySpeed = 6.0,
        impactDuration = 0.4    -- seconds to show Impact before destroying
    },

    targetX = 0,
    targetY = 0,
    isFlying = false,
    featherIndex = 0,
    initialized = false,
    hasTriggered = false,
    hasFinished = false,
    impactTimer = -1.0,         -- -1 means not yet counting

    Init = function(self, entity) end,
    Start = function(self, entity) end,

    Update = function(self, entity)
        -- Self-destruct countdown after landing
        if self.impactTimer >= 0.0 then
            self.impactTimer = self.impactTimer + Time.getDeltaTime()
            if self.impactTimer >= self.impactDuration then
                Dragoon_Ability2_VFX_Feather.Finish(self, entity)
            end
            return
        end

        -- Wait for target assignment from VFXPlayer
        if not self.initialized then
            local pending = FeatherTargets_pending
            if pending then
                for i, data in ipairs(pending) do
                    if data.entity == entity then
                        self.targetX = data.targetX
                        self.targetY = data.targetY
                        self.featherIndex = data.featherIndex
                        self.flySpeed = data.flySpeed or self.flySpeed
                        self.isFlying = true
                        self.initialized = true

                        local transform = ECS.getTransform(entity)
                        local dx = self.targetX - transform.x
                        local dy = self.targetY - transform.y
                        local angle = math.atan2(dy, dx)
                        ECS.setTransform(entity, {
                            x = transform.x,
                            y = transform.y,
                            rotation = angle
                        })

                        Event.emitLocal("PlaySFXFeatherLaunch", {})
                        Dragoon_Ability2_VFX_Feather.PlayAnimation(self, entity, "Fly")
                        table.remove(pending, i)
                        Log.info("[Feather " .. self.featherIndex .. "] Target acquired, flying!")
                        break
                    end
                end
            end
            return
        end

        if not self.isFlying then return end

        local transform = ECS.getTransform(entity)
        local dx = self.targetX - transform.x
        local dy = self.targetY - transform.y
        local dist = math.sqrt(dx * dx + dy * dy)

        if dist < 0.05 then
            transform.x = self.targetX
            transform.y = self.targetY
            ECS.setTransform(entity, transform)
            self.isFlying = false

            -- Fire damage/hit event immediately on landing
            if not self.hasTriggered then
                self.hasTriggered = true
                Event.emitLocal("Dragoon_Ability2_VFX_Feather_Transition", {
                    featherIndex = self.featherIndex
                })
            end

            Dragoon_Ability2_VFX_Feather.PlayAnimation(self, entity, "Impact")
            self.impactTimer = 0.0  -- start the destroy countdown
            return
        end

        dx = dx / dist
        dy = dy / dist
        local step = self.flySpeed * Time.getDeltaTime()
        transform.x = transform.x + dx * math.min(step, dist)
        transform.y = transform.y + dy * math.min(step, dist)
        ECS.setTransform(entity, transform)
    end,

    Finish = function(self, entity)
        if self.hasFinished then return end
        self.hasFinished = true
        self.impactTimer = -1.0

        -- Safety: fire transition if it somehow hasn't fired yet
        if not self.hasTriggered then
            self.hasTriggered = true
            Event.emitLocal("Dragoon_Ability2_VFX_Feather_Transition", {
                featherIndex = self.featherIndex
            })
        end

        Event.emitLocal("Dragoon_Ability2_VFX_Feather_Finished", {
            featherIndex = self.featherIndex
        })
        Log.info("[Feather " .. self.featherIndex .. "] Destroyed")
        ECS.destroyEntityDeferred(entity)
    end,

    PlayAnimation = function(self, entity, stateName)
        local animInfo = ECS.getAnimation(entity)
        if not animInfo then return end
        if animInfo.currentState == stateName then return end
        ECS.setAnimation(entity, stateName)
    end,

    -- These still work if your animation system fires them, but are no longer required
    OnVFXAnimationTransition = function(self, entity) end,

    OnVFXAnimationFinished = function(self, entity)
        Dragoon_Ability2_VFX_Feather.Finish(self, entity)
    end
})

return class