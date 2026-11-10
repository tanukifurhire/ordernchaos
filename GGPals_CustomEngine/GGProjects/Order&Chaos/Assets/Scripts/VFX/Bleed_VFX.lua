-- Bleed_VFX.lua - Visual effect for Bleed debuff (Vizom_2)
-- Plays "Fire" animation once each time Bleed procs, then returns to "Stop"

local class = DefineType("Bleed_VFX", {
    Serializable = {
        unitType = 0,
        applyDelay = 0.2
    },

    isActive = false,
    pendingActivation = false,
    timer = 0.0,

    Init = function(self, entity)
        Bleed_VFX.SetAlpha(self, entity, 0)

        Event.subscribe("ApplyBleed", function(params)
            if params.unitType == self.unitType then
                Log.info("[Bleed_VFX] Applied to unit " .. self.unitType)
                self.isActive = true
                self.pendingActivation = true
                self.timer = 0.0
            end
        end)

        Event.subscribe("OnUnitStatusInflicted", function(params)
            if params.statusID == CONSTANTS.ABILITIES.VIZOM_2 and params.targetUnit == self.unitType then
                Log.info("[Bleed_VFX] Status inflicted on unit " .. self.unitType)
                self.isActive = true
                self.pendingActivation = true
                self.timer = 0.0
            end
        end)

        -- Play "Fire" animation when bleed actually procs (damage tick)
        Event.subscribe("BleedProc", function(params)
            if params.unitType == self.unitType then
                Log.info("[Bleed_VFX] Bleed proc on unit " .. self.unitType)
                Bleed_VFX.SetAlpha(self, entity, 1)
                ECS.setAnimation(entity, "Fire")
            end
        end)

        Event.subscribe("InPlanningPhase", function(params)
            if self.isActive then
                Bleed_VFX.SetAlpha(self, entity, 1)
            else
                Bleed_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("RemoveBleed", function(params)
            if params.unitType == self.unitType then
                Log.info("[Bleed_VFX] Removed from unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Bleed_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("OnUnitStatusRemoved", function(params)
            if params.statusID == CONSTANTS.ABILITIES.VIZOM_2 and params.targetUnit == self.unitType then
                Log.info("[Bleed_VFX] Status removed from unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Bleed_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("InResolutionPhase", function()
            -- Clear VFX for resolution replay to prevent duplicates
            if self.isActive then
                Log.info("[Bleed_VFX] Clearing for resolution replay on unit " .. self.unitType)
                Bleed_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("OnChangePhase", function()
            -- Check if bleed has expired
            local gm = ClientGameManager_instance
            if gm and gm.unitStatuses and gm.unitStatuses[self.unitType] then
                local bleedStatus = gm.unitStatuses[self.unitType][CONSTANTS.ABILITIES.VIZOM_2]
                if not bleedStatus or bleedStatus <= 0 then
                    self.isActive = false
                    self.pendingActivation = false
                    self.timer = 0.0
                    Bleed_VFX.SetAlpha(self, entity, 0)
                end
            end
        end)

        Log.info("[Bleed_VFX] Init complete for unit " .. self.unitType)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
        if not self.pendingActivation then return end

        self.timer = self.timer + Time.getDeltaTime()
        if self.timer >= self.applyDelay then
            self.pendingActivation = false
            Bleed_VFX.SetAlpha(self, entity, 1)
        end
    end,

    OnAnimationTransition = function(self, entity)
        ECS.setAnimation(entity, "Stop")
    end,

    SetAlpha = function(self, entity, alpha)
        local mr = ECS.getMeshRenderer(entity)
        if not mr then return end
        mr.colorMultiply = { r = 1, g = 1, b = 1, a = alpha }
        ECS.setMeshRenderer(entity, mr)
    end
})

return class
