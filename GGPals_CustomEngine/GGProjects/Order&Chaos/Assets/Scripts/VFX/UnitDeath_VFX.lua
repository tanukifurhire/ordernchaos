-- UnitDeath_VFX.lua - Visual effect for unit death in EndGamePhase
-- Plays "Fire" animation once when the unit's tile is destroyed

local class = DefineType("UnitDeath_VFX", {
    Serializable = {
        unitType = 0
    },

    Init = function(self, entity)
        UnitDeath_VFX.SetAlpha(self, entity, 0)

        Event.subscribe("UnitDeath", function(params)
            if params.unitType == self.unitType then
                Log.info("[UnitDeath_VFX] Death VFX on unit " .. self.unitType)
                UnitDeath_VFX.SetAlpha(self, entity, 1)
                ECS.setAnimation(entity, "Fire")
            end
        end)

        Log.info("[UnitDeath_VFX] Init complete for unit " .. self.unitType)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
    end,

    SetAlpha = function(self, entity, alpha)
        local mr = ECS.getMeshRenderer(entity)
        if not mr then return end
        mr.colorMultiply = { r = 1, g = 1, b = 1, a = alpha }
        ECS.setMeshRenderer(entity, mr)
    end
})

return class
