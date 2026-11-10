local SPOTLIGHT_LAYER = 12

local class = DefineType("AutoHidePedestals", {
    Serializable = {
        unitType   = 0,
        isTutorial = false   -- tick TRUE in inspector on Oracle, Dragoon, Tirion pedestal entities
    },

    originalLayer  = 0,
    componentKind  = "",   -- "ui" or "mesh"
    isSpotlighted  = false,

    Init = function(self, entity)
        -- DARKEN (GLOBAL)
        Event.subscribe("Darken_All", function()
            if not self.isTutorial then return end

            local renderer = ECS.getMeshRenderer(entity)
            if renderer then
                if not self.originalColor then
                    self.originalColor = renderer.colorMultiply
                end

                renderer.colorMultiply = { r = 0.2, g = 0.2, b = 0.2, a = 1.0 }
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (GLOBAL)
        Event.subscribe("Lighten_All", function()
            if not self.isTutorial then return end

            local renderer = ECS.getMeshRenderer(entity)
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (UNIQUE PER FILE)
        Event.subscribe("Lighten_UI_OraclePedestal", function()
            if self.unitType ~= CONSTANTS.UNIT.ORACLE then return end

            local renderer = ECS.getMeshRenderer(entity)
            
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (UNIQUE PER FILE)
        Event.subscribe("Lighten_UI_DragoonPedestal", function()
            if self.unitType ~= CONSTANTS.UNIT.DRAGOON then return end
            local renderer = ECS.getMeshRenderer(entity)
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (UNIQUE PER FILE)
        Event.subscribe("Lighten_UI_TirionPedestal", function()
            if self.unitType ~= CONSTANTS.UNIT.TIRION then return end
            local renderer = ECS.getMeshRenderer(entity)
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (UNIQUE PER FILE)
        Event.subscribe("Lighten_UI_EyePedestal", function()
            if self.unitType ~= CONSTANTS.UNIT.EYE then return end
            local renderer = ECS.getMeshRenderer(entity)
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (UNIQUE PER FILE)
        Event.subscribe("Lighten_UI_VizomPedestal", function()
            if self.unitType ~= CONSTANTS.UNIT.VIZOM then return end
            local renderer = ECS.getMeshRenderer(entity)
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        -- LIGHTEN (UNIQUE PER FILE)
        Event.subscribe("Lighten_UI_ImpPedestal", function()
            if self.unitType ~= CONSTANTS.UNIT.IMP then return end
            local renderer = ECS.getMeshRenderer(entity)
            if renderer and self.originalColor then
                renderer.colorMultiply = self.originalColor
                ECS.setMeshRenderer(entity, renderer)
            end
        end)

        Event.subscribe("OnUnitDeployed", function(params)
            if params.unitType == self.unitType then
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_NONE
                })
            end
        end)
    end
})

return class