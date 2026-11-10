-- UI_ArsenalUnitDisplay.lua - Displays the selected unit's idle animation

local class = DefineType("ArsenalUnitDisplay", {
    Serializable = {},

    -- Map unitID to idle animation state name
    -- DRAGOON=1, ORACLE=2, TIRION=3, IMP=4, EYE=5, VIZOM=6
    unitIdleAnims = {
        [1] = "Oracle_Idle",
        [2] = "Dragoon_Idle",
        [3] = "Tirion_Idle",
        [4] = "Imp_Idle",
        [5] = "Eye_Idle",
        [6] = "Vizom_Idle",
    },

    -- First unit per faction for the initial animation
    orderDefaultUnit = 1,   -- DRAGOON
    chaosDefaultUnit = 4,   -- IMP

    Init = function(self, entity)
        ECS.setAnimation(entity, "None_Idle")

        Event.subscribe("OnArsenalFactionReady", function(params)
            local defaultUnit = params.isOrder and self.orderDefaultUnit or self.chaosDefaultUnit
            ArsenalUnitDisplay.SetUnitAnimation(self, entity, defaultUnit)
        end)

        Event.subscribe("OnArsenalUnitSelected", function(params)
            ArsenalUnitDisplay.SetUnitAnimation(self, entity, params.unitType)
        end)
    end,

    SetUnitAnimation = function(self, entity, unitType)
        local stateName = self.unitIdleAnims[unitType]
        if stateName then
            ECS.setAnimation(entity, stateName)
            Log.info("[ArsenalUnitDisplay] Set animation to " .. stateName)
        else
            Log.warning("[ArsenalUnitDisplay] No idle animation for unit " .. tostring(unitType))
        end
    end
})

return class
