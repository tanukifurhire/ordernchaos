-- Burn_VFX.lua - Visual effect for Burn debuff (Dragoon_1)
-- Shows burning effect on unit while Burn status is active

local class = DefineType("Burn_VFX", {
    Serializable = {
        unitType = 0,
        applyDelay = 0.2
    },

    isActive = false,
    pendingActivation = false,
    timer = 0.0,

    Init = function(self, entity)
        Burn_VFX.SetAlpha(self, entity, 0)

        Event.subscribe("ApplyBurn", function(params)
            if params.unitType == self.unitType then
                Log.info("[Burn_VFX] Applied to unit " .. self.unitType)
                self.isActive = true
                self.pendingActivation = true
                self.timer = 0.0
            end
        end)

        Event.subscribe("OnUnitStatusInflicted", function(params)
            if params.statusID == CONSTANTS.ABILITIES.DRAGOON_1 and params.targetUnit == self.unitType then
                Log.info("[Burn_VFX] Status inflicted on unit " .. self.unitType)
                self.isActive = true
                self.pendingActivation = true
                self.timer = 0.0
            end
        end)

        Event.subscribe("InPlanningPhase", function(params)
            if self.isActive then
                Burn_VFX.SetAlpha(self, entity, 1)
            else
                Burn_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("RemoveBurn", function(params)
            if params.unitType == self.unitType then
                Log.info("[Burn_VFX] Removed from unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Burn_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("OnUnitStatusRemoved", function(params)
            if params.statusID == CONSTANTS.ABILITIES.DRAGOON_1 and params.targetUnit == self.unitType then
                Log.info("[Burn_VFX] Status removed from unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Burn_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("InResolutionPhase", function()
            -- Clear VFX for resolution replay to prevent duplicates
            if self.isActive then
                Log.info("[Burn_VFX] Clearing for resolution replay on unit " .. self.unitType)
                Burn_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("OnChangePhase", function()
            -- Check if burn has expired
            local gm = ClientGameManager_instance
            if gm and gm.unitStatuses and gm.unitStatuses[self.unitType] then
                local burnStatus = gm.unitStatuses[self.unitType][CONSTANTS.ABILITIES.DRAGOON_1]
                if not burnStatus or burnStatus <= 0 then
                    self.isActive = false
                    self.pendingActivation = false
                    self.timer = 0.0
                    Burn_VFX.SetAlpha(self, entity, 0)
                end
            end
        end)

        Log.info("[Burn_VFX] Init complete for unit " .. self.unitType)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
        if not self.pendingActivation then return end

        self.timer = self.timer + Time.getDeltaTime()
        if self.timer >= self.applyDelay then
            self.pendingActivation = false
            Burn_VFX.SetAlpha(self, entity, 1)
        end
    end,

    SetAlpha = function(self, entity, alpha)
        local mr = ECS.getMeshRenderer(entity)
        if not mr then return end
        mr.colorMultiply = { r = 1, g = 1, b = 1, a = alpha }
        ECS.setMeshRenderer(entity, mr)
    end
})

return class
