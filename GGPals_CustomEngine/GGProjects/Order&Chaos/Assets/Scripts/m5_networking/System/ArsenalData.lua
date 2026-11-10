ArsenalData_instance = nil

local class = DefineType("ArsenalData", {
    Serializable = {},
    
    selectedAbilities = {
        [CONSTANTS.UNIT.DRAGOON] = {1, 2},
        [CONSTANTS.UNIT.ORACLE] = {1, 2},
        [CONSTANTS.UNIT.TIRION] = {1, 2},
        [CONSTANTS.UNIT.IMP] = {1, 2},
        [CONSTANTS.UNIT.EYE] = {1, 2},
        [CONSTANTS.UNIT.VIZOM] = {1, 2}
    },
    
    abilityOptions = {
        [CONSTANTS.UNIT.DRAGOON] = {"dragoonAbility1", "dragoonAbility2", "generic"},
        [CONSTANTS.UNIT.ORACLE] = {"oracleAbility1", "oracleAbility2", "generic"},
        [CONSTANTS.UNIT.TIRION] = {"tirionAbility1", "tirionAbility2", "generic"},
        [CONSTANTS.UNIT.IMP] = {"impAbility1", "impAbility2", "generic"},
        [CONSTANTS.UNIT.EYE] = {"eyeAbility1", "eyeAbility2", "generic"},
        [CONSTANTS.UNIT.VIZOM] = {"vizomAbility1", "vizomAbility2", "generic"}
    },
    
    Init = function(self, entity)
        ArsenalData_instance = self
        
        if Persistent.arsenalAbilities then
            self.selectedAbilities = Persistent.arsenalAbilities
        else
            Persistent.arsenalAbilities = self.selectedAbilities
        end
        
        Log.info("[ArsenalData] Initialized")
    end,
    
    SetUnitAbilities = function(self, unitType, ability1Index, ability2Index)
        if not self.selectedAbilities[unitType] then
            Log.error("[ArsenalData] Invalid unit type: " .. unitType)
            return false
        end
        
        self.selectedAbilities[unitType] = {ability1Index, ability2Index}
        
        local persistentAbilities = Persistent.arsenalAbilities or {}
        persistentAbilities[unitType] = {ability1Index, ability2Index}
        Persistent.arsenalAbilities = persistentAbilities
        
        Log.info("[ArsenalData] Unit " .. unitType .. " abilities set to: " .. ability1Index .. ", " .. ability2Index)
        return true
    end,
    
    GetUnitAbilities = function(self, unitType)
        return self.selectedAbilities[unitType] or {1, 2}
    end,

    GetAbilityOptions = function(self, unitType)
        return self.abilityOptions[unitType] or {}
    end,
    
    IsArsenalComplete = function(self, faction)
        local units = faction == CONSTANTS.ORDER_PLAYER 
            and {CONSTANTS.UNIT.DRAGOON, CONSTANTS.UNIT.ORACLE, CONSTANTS.UNIT.TIRION}
            or {CONSTANTS.UNIT.IMP, CONSTANTS.UNIT.EYE, CONSTANTS.UNIT.VIZOM}
        
        for _, unitType in ipairs(units) do
            local abilities = self.selectedAbilities[unitType]
            if not abilities or #abilities ~= 2 then
                return false
            end
        end
        
        return true
    end
})

return class