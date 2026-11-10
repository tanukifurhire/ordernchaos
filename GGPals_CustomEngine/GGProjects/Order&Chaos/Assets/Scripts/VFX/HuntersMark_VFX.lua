-- HuntersMark_VFX.lua - Visual effect for Hunter's Mark (PASSIVE_2)
-- Plays "Fire" animation once when mark is applied, then returns to "Stop"

local class = DefineType("HuntersMark_VFX", {
    Serializable = {
        unitType = 0
    },

    Init = function(self, entity)
        HuntersMark_VFX.SetAlpha(self, entity, 0)

        -- Play "Fire" when this unit gets marked
        Event.subscribe("HuntersMarkProc", function(params)
            if params.unitType == self.unitType then
                Log.info("[HuntersMark_VFX] Mark proc on unit " .. self.unitType)
                HuntersMark_VFX.SetAlpha(self, entity, 1)
                ECS.setAnimation(entity, "Fire")
            end
        end)

        -- Hide when mark is removed
        Event.subscribe("OnUnitStatusRemoved", function(params)
            if params.statusID == CONSTANTS.ABILITIES.PASSIVE_2 and params.targetUnit == self.unitType then
                Log.info("[HuntersMark_VFX] Mark removed from unit " .. self.unitType)
                HuntersMark_VFX.SetAlpha(self, entity, 0)
            end
        end)

        Log.info("[HuntersMark_VFX] Init complete for unit " .. self.unitType)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
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
