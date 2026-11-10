local SPOTLIGHT_LAYER = 12

local class = DefineType("AbilityButton", 
{
    Serializable = 
    {
        abilityType = 0,
        isTutorial  = false,
        hotkeySlot  = 0
    },

    -- Combined variables
    isDisabled       = false,
    isTutorialLocked = false,
    originalLayer    = 0,
    isSpotlighted    = false,

    tooltipEntity      = nil,
    resolvedAbilityType = 0,
    ownerUnitType       = 0,
    
    isValid = false,
    inPrepPhase = false,
    abilitySelected = false,

    Init = function(self, entity)
        if self.isTutorial then
            self.hotkeySlot = self.abilityType
        end

        Event.subscribe("InPlanningPhase", function()
            self.inPrepPhase = true
        end)

        -- if an ability is selected, darken all other abilities
        Event.subscribe("OnAbilitySelectedValid", function(params)
            -- disable this functionality in other phases
            if not self.inPrepPhase then
                return
            end

            self.abilitySelected = true

            if params.ability ~= self.resolvedAbilityType then
                ECS.setUIComponent(entity, {
                    colorMultiply = { r = 0.2, g = 0.2, b = 0.2, a = 0.8 }
                })
            end
        end)

        Event.subscribe("OnAbilityQueued", function(params)
            -- reset all non disabled abilities to normal first
            self.abilitySelected = false -- reset to false

            if not self.isDisabled then
                ECS.setUIComponent(entity, {
                    colorMultiply = CONSTANTS.COLOR_NONE
                })
            end

            -- Match by unit type AND ability ID so only the specific ability button gets disabled
            if params.unitType and params.unitType == self.ownerUnitType
               and params.abilityID and params.abilityID == self.resolvedAbilityType then

                local gm = ClientGameManager_instance
                if gm and gm:HasPassive(self.ownerUnitType, CONSTANTS.ABILITIES.PASSIVE_1) then
                    return
                end

                self.isDisabled = true
                ECS.setUIComponent(entity, {
                    colorMultiply = { r = 0.2, g = 0.2, b = 0.2, a = 0.8 }
                })
            end
        end)

        Event.subscribe("OnChangePhase", function()
            self.isDisabled = false
            self.abilitySelected = false
            ECS.setUIComponent(entity, {
                colorMultiply = CONSTANTS.COLOR_NONE
            })
        end)

        Event.subscribe("OnPrepPhaseReset", function()
            self.isDisabled = false
            self.abilitySelected = false
            ECS.setUIComponent(entity, {
                colorMultiply = CONSTANTS.COLOR_NONE
            })
        end)

        ----------------------------------------------------------------
        -- Bottom Bar Enable/Disable
        ----------------------------------------------------------------
        Event.subscribe("SetUIBottomBarEnabled", function(params)
            self.isDisabled = not params.enabled

            if params.enabled then
                ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NONE })
            else
                ECS.setUIComponent(entity, { colorMultiply = { r=0.3, g=0.3, b=0.3, a=0.5 } })
            end
        end)

        ----------------------------------------------------------------
        -- Tutorial System
        ----------------------------------------------------------------
        if self.isTutorial then

            -- Save original layer
            local ok = pcall(function()
                local ui = ECS.getUIComponent(entity)
                self.originalLayer = ui.layer
            end)
            if not ok then
                self.originalLayer = 0
            end

            -- Start locked
            self.isTutorialLocked = true
            ECS.setUIComponent(entity, {
                colorMultiply = { r=0.3, g=0.3, b=0.3, a=0.5 }
            })

            local function setSpotlight(shouldSpotlight)
                if shouldSpotlight == self.isSpotlighted then return end

                self.isSpotlighted = shouldSpotlight
                local targetLayer = shouldSpotlight and SPOTLIGHT_LAYER or self.originalLayer

                pcall(function()
                    ECS.setUIComponent(entity, { layer = targetLayer })
                end)
            end

            Event.subscribe("OnTutorialStepChanged", function(params)

                local allowed = params.allowedAbility

                -- Unlock logic
                if allowed == -1 or allowed == self.abilityType then
                    self.isTutorialLocked = false
                    ECS.setUIComponent(entity, {
                        colorMultiply = { r=1.0, g=1.0, b=1.0, a=1.0 }
                    })
                else
                    self.isTutorialLocked = true
                    ECS.setUIComponent(entity, {
                        colorMultiply = { r=0.3, g=0.3, b=0.3, a=0.5 }
                    })
                end

                -- Spotlight logic
                local spA = params.spotlightAbilityA or 0
                local spB = params.spotlightAbilityB or 0

                local shouldSpotlight =
                    (spA ~= 0 and self.abilityType == spA) or
                    (spB ~= 0 and self.abilityType == spB)

                setSpotlight(shouldSpotlight)
            end)
        end

        ----------------------------------------------------------------
        -- Hotkey System
        ----------------------------------------------------------------
        if self.hotkeySlot ~= 0 then
            Event.subscribe("OnAbilityHotkey", function(params)

                if params.slot ~= self.hotkeySlot then return end
                if self.isDisabled or self.isTutorialLocked then return end

                Log.info("[AbilityButton] Activating ability " .. params.slot)

                Event.emitLocal("OnAbilitySelected", {
                    ability = self.resolvedAbilityType,
                    unitType = self.ownerUnitType
                })
            end)
        end

        ----------------------------------------------------------------
        -- Re-highlight Abilities when cancelling
        ----------------------------------------------------------------
        Event.subscribe("OnAbilityCastCancelled", function(params)
            if self.isDisabled or self.isTutorialLocked then return end

            ECS.setUIComponent(entity, {
                colorMultiply = CONSTANTS.COLOR_NONE
            })
        end)
    end,

    ----------------------------------------------------------------
    -- Start
    ----------------------------------------------------------------
    Start = function(self, entity)

        if ClientGameManager_instance then
            self.resolvedAbilityType =
                ClientGameManager_instance:ResolveArsenalAbility(self.abilityType)
        else
            self.resolvedAbilityType = self.abilityType
        end

        self.ownerUnitType =
            CONSTANTS.ABILITY_SLOT_TO_UNIT[self.abilityType] or 0

        if self.resolvedAbilityType ~= self.abilityType then
            Log.info("[AbilityButton] Resolved ability "
                .. self.abilityType .. " -> "
                .. self.resolvedAbilityType)
        end
    end,

    ----------------------------------------------------------------
    -- Hover Enter
    ----------------------------------------------------------------
    OnHoverEnter = function(self, entity)
        if self.isDisabled or self.isTutorialLocked then return end
        Event.emitLocal("PlayMainMenuHoverBling", {})

        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_BUTTON_HOVER_MULTIPLY
        })

        local t = ECS.getWorldPosition(entity)

        if t then
            Event.emitLocal("ShowAbilityTooltip", {
                abilityID = self.resolvedAbilityType,
                x = t.x,
                y = t.y
            })
        end
    end,

    ----------------------------------------------------------------
    -- Hover Exit
    ----------------------------------------------------------------
    OnHoverExit = function(self, entity)
        Event.emitLocal("HideAbilityTooltip", {})
        if self.isDisabled or self.isTutorialLocked then return end

        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_NONE
        })

    end,

    ----------------------------------------------------------------
    -- Click
    ----------------------------------------------------------------
    OnClick = function(self, entity, x, y)
        if self.isDisabled or self.isTutorialLocked then return end
        Event.emitLocal("PlayMainMenuClick", {})

        Event.emitLocal("OnAbilitySelected", {
            ability = self.resolvedAbilityType,
            unitType = self.ownerUnitType
        })
    end,

    Update = function(self, entity)
        if self.isTutorial and self.isTutorialLocked then
            ECS.setUIComponent(entity, {
                colorMultiply = { r=0.3, g=0.3, b=0.3, a=0.5 }
            })
            return
        end
        if not self.inPrepPhase then
            return
        end

        local gm = ClientGameManager_instance
        local ability = gm:GetAbilityByID(self.resolvedAbilityType)

        if not ability then
            -- Log.info("[AbilityButton] GetAbilityByID returned nil for resolvedAbilityType=" .. tostring(self.resolvedAbilityType) .. " (raw abilityType=" .. tostring(self.abilityType) .. ")")
            return
        end

        -- if not invalid, then darken ability
        if not ability:CanUse(gm:GetPlayerGrid(), gm.currentAP) then
            ECS.setUIComponent(entity, {
                colorMultiply = { r = 0.2, g = 0.2, b = 0.2, a = 0.8 }
            })
        else
            if not self.abilitySelected then
                ECS.setUIComponent(entity, {
                    colorMultiply = CONSTANTS.COLOR_NONE
                })
            end
        end
    end
})

return class