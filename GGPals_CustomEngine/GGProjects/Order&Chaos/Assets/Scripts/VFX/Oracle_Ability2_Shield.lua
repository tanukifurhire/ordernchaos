local class = DefineType("Oracle_Ability2_Shield", {
    Serializable = {
        unitType = 0,
        applyDelay = 0.2
    },

    isActive = false,
    pendingActivation = false,
    timer = 0.0,

    Init = function(self, entity)
        -- Fix: Dragoon(1) and Oracle(2) shield VFX entities have swapped unitType in editor
        if self.unitType == 1 then self.unitType = 2
        elseif self.unitType == 2 then self.unitType = 1 end

        Oracle_Ability2_Shield.SetAlpha(self, entity, 0)

        Event.subscribe("ApplyShield", function(params)
            if params.unitType == self.unitType then
                Log.info("[Shield] Applied to unit " .. self.unitType)
                self.isActive = true
                self.pendingActivation = true
                self.timer = 0.0
                -- Register with AbilityManager so damage reduction works
                if AbilityManager_instance then
                    AbilityManager_instance.shieldedUnit = self.unitType
                end
            end
        end)

        Event.subscribe("ActivateShieldAfterVFX", function(params)
            if params.unitType == self.unitType then
                Log.info("[Shield] Activated after VFX for unit " .. self.unitType)
                self.isActive = true
                self.pendingActivation = true
                self.timer = 0.0
                if AbilityManager_instance then
                    AbilityManager_instance.shieldedUnit = self.unitType
                end
            end
        end)

        Event.subscribe("InPlanningPhase", function(params)
            if params.player == CONSTANTS.ORDER_PLAYER then
                if self.isActive then
                    Oracle_Ability2_Shield.SetAlpha(self, entity, 1)
                end
            else
                Oracle_Ability2_Shield.SetAlpha(self, entity, 0)
            end
        end)

        Event.subscribe("RemoveShield", function(params)
            if params.unitType == self.unitType then
                Log.info("[Shield] Depleted on unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Oracle_Ability2_Shield.SetAlpha(self, entity, 0)
                if AbilityManager_instance then
                    AbilityManager_instance.shieldedUnit = nil
                end
            end
        end)

        Event.subscribe("InResolutionPhase", function()
            -- Clear shield VFX so resolution replay can apply it fresh
            -- Prevents duplicate shield visuals in networked mode
            if self.isActive then
                Log.info("[Shield] Clearing for resolution replay on unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Oracle_Ability2_Shield.SetAlpha(self, entity, 0)
                if AbilityManager_instance then
                    AbilityManager_instance.shieldedUnit = nil
                end
            end
        end)

        Event.subscribe("OnChangePhase", function()
            if self.isActive then
                Log.info("[Shield] Removed from unit " .. self.unitType)
                self.isActive = false
                self.pendingActivation = false
                self.timer = 0.0
                Oracle_Ability2_Shield.SetAlpha(self, entity, 0)
                if AbilityManager_instance then
                    AbilityManager_instance.shieldedUnit = nil
                end
            end
        end)

        Log.info("[Shield] Init complete for unit " .. self.unitType)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
        if not self.pendingActivation then return end

        self.timer = self.timer + Time.getDeltaTime()
        if self.timer >= self.applyDelay then
            self.pendingActivation = false
            Oracle_Ability2_Shield.SetAlpha(self, entity, 1)
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