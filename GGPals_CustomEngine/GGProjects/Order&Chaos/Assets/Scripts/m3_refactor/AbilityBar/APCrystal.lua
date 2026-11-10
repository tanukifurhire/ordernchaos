local SPOTLIGHT_LAYER = 12

local class = DefineType("APCrystal",
{
    Serializable =
    {
        crystalIndex = 1,       -- 1..6 (1 = leftmost / first consumed)
        isTutorial   = false    -- tick TRUE in inspector on all 6 crystal entities
    },

    originalLayer = 0,
    isSpotlighted = false,

    Init = function(self, entity)
        if self.isTutorial then
            -- Save original layer
            local ok = pcall(function()
                local ui = ECS.getUIComponent(entity)
                self.originalLayer = ui.layer
            end)
            if not ok then
                self.originalLayer = 0
            end

            local function setSpotlight(shouldSpotlight)
                if shouldSpotlight == self.isSpotlighted then return end
                self.isSpotlighted = shouldSpotlight
                local targetLayer = shouldSpotlight and SPOTLIGHT_LAYER or self.originalLayer
                pcall(function() ECS.setUIComponent(entity, { layer = targetLayer }) end)
            end

            Event.subscribe("OnTutorialStepChanged", function(params)
                setSpotlight(params.spotlightCrystals or false)
            end)
        end

        Event.subscribe("OnAPUpdate", function(params)
            if not params or not params.value then return end
            APCrystal.Refresh(self, entity, params.value)
        end)

        Event.subscribe("HideAP", function()
            APCrystal.SetDim(self, entity, true)
        end)

        Event.subscribe("ShowAP", function()
            APCrystal.Refresh(self, entity, CONSTANTS.MAX_AP)
        end)

        APCrystal.SetDim(self, entity, true)
    end,

    Refresh = function(self, entity, currentAP)
        if not currentAP then return end
        local active = self.crystalIndex <= currentAP
        APCrystal.SetDim(self, entity, not active)
    end,

    SetDim = function(self, entity, dimmed)
        local ui = ECS.getUIComponent(entity)
        if not ui then return end
        ui.colorMultiply.a = dimmed and 0.6 or 1.0
        ui.colorMultiply.r = dimmed and 0.6 or 1.0
        ui.colorMultiply.g = dimmed and 0.6 or 1.0
        ui.colorMultiply.b = dimmed and 0.6 or 1.0

        ECS.setUIComponent(entity, ui)
    end
})

return class