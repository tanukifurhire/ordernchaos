local COLOR_DARKENED = { r = 0.25, g = 0.25, b = 0.25, a = 1.0 }
local COLOR_SELECTED = { r = 0.6, g = 0.9, b = 1.0, a = 1.0 }
local COLOR_BLINK_BRIGHT = { r = 1.3, g = 1.3, b = 1.0, a = 1.0 }

local class = DefineType("ArsenalAbilityIcon", {
    Serializable = {
        abilityIndex = 1,
        isPassive = false
    },

    TextureAsset = {
        iconTexture = 0,
        hoverTexture = 0
    },

    currentUnit = 0,
    isValid = true,
    isSelected = false,
    isInLoadout = false,
    isDarkened = false,
    isBlinking = false,
    blinkTimer = 0,
    blinkDuration = 0.8,
    
    -- Store subscription IDs for cleanup
    subscriptions = {},

    Init = function(self, entity)
        self.subscriptions = {}
        
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalFactionReady", function(params)
            -- Default to first unit of the faction
            if params.isOrder then
                self.currentUnit = CONSTANTS.UNIT.DRAGOON
            else
                self.currentUnit = CONSTANTS.UNIT.IMP
            end
            ArsenalAbilityIcon.UpdateValidity(self, entity)
        end)

        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalUnitSelected", function(params)
            self.currentUnit = params.unitType
            ArsenalAbilityIcon.UpdateValidity(self, entity)
        end)

        -- Highlight when selected by ArsenalUI, unhighlight when another is selected
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalAbilityHighlighted", function(params)
            self.isSelected = (params.abilityIndex == self.abilityIndex and params.abilityIndex > 0)
            ArsenalAbilityIcon.UpdateVisual(self, entity)
        end)

        -- Darken when this ability is already in the current unit's loadout
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalLoadoutChanged", function(params)
            ArsenalAbilityIcon.CheckInLoadout(self, entity, params)
        end)
        
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalVisualStateUpdate", function(params)
            local shouldDarken = false
            local shouldBlink = false
            
            if params.selectedSpawnedIcon then
                -- A spawned icon is selected - blink valid ability icons for replacement
                if params.selectedSpawnedIcon.isPassive == self.isPassive and self.isValid and not self.isInLoadout then
                    shouldBlink = true
                else
                    shouldDarken = true
                end
            elseif params.selectedAbilityIcon then
                -- An ability icon is selected
                if params.selectedAbilityIcon.abilityIndex == self.abilityIndex and 
                   params.selectedAbilityIcon.isPassive == self.isPassive then
                    -- This is the selected ability icon, don't darken
                else
                    -- Darken all other ability icons
                    shouldDarken = true
                end
            end
            
            self.isDarkened = shouldDarken
            self.isBlinking = shouldBlink
            ArsenalAbilityIcon.UpdateVisual(self, entity)
        end)
        
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalClearVisualEffects", function()
            self.isDarkened = false
            self.isBlinking = false
            ArsenalAbilityIcon.UpdateVisual(self, entity)
        end)
    end,
    
    OnDestroy = function(self, entity)
        for _, subId in ipairs(self.subscriptions) do
            Event.unsubscribe(subId)
        end
        self.subscriptions = {}
    end,
    
    Update = function(self, entity)
        local dt = Time.getDeltaTime()
        
        if self.isBlinking then
            self.blinkTimer = self.blinkTimer + dt
            if self.blinkTimer >= self.blinkDuration then
                self.blinkTimer = 0
            end
            ArsenalAbilityIcon.UpdateVisual(self, entity)
        end
    end,

    UpdateValidity = function(self, entity)
        -- Passives are always valid for any unit
        if self.isPassive then
            self.isValid = true
        else
            self.isValid = ArsenalAbilityIcon.IsValidForUnit(self, self.currentUnit)
        end
        ArsenalAbilityIcon.UpdateVisual(self, entity)
    end,

    CheckInLoadout = function(self, entity, params)
        self.isInLoadout = false

        if self.abilityIndex == params.active[1] or self.abilityIndex == params.active[2] then
            self.isInLoadout = true
        end
        if self.abilityIndex == params.passive then
            self.isInLoadout = true
        end

        ArsenalAbilityIcon.UpdateVisual(self, entity)
    end,

    -- Central visual update: priority is inLoadout > selected > isDarkened > isBlinking > invalid > normal
    UpdateVisual = function(self, entity)
        if self.isInLoadout then
            ECS.setUIComponent(entity, { colorMultiply = COLOR_DARKENED })
        elseif self.isSelected then
            ECS.setUIComponent(entity, { colorMultiply = COLOR_SELECTED })
        elseif self.isDarkened then
            ECS.setUIComponent(entity, { colorMultiply = COLOR_DARKENED })
        elseif self.isBlinking then
            local phase = self.blinkTimer / self.blinkDuration
            local brightness = 1.0 + 0.3 * math.sin(phase * 2 * math.pi)
            ECS.setUIComponent(entity, { 
                colorMultiply = { r = brightness, g = brightness, b = brightness * 0.9, a = 1.0 } 
            })
        elseif not self.isValid then
            ECS.setUIComponent(entity, { colorMultiply = COLOR_DARKENED })
        else
            ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY })
        end
    end,

    IsValidForUnit = function(self, unitType)
        if not ArsenalUI_instance then return true end

        -- Check unit's own abilities
        local own = ArsenalUI_instance.unitOwnAbilities[unitType]
        if own then
            for _, id in ipairs(own) do
                if id == self.abilityIndex then return true end
            end
        end

        -- Check generic abilities
        for _, id in ipairs(ArsenalUI_instance.genericAbilities) do
            if id == self.abilityIndex then return true end
        end

        return false
    end,

    OnHoverEnter = function(self, entity)
        if self.isInLoadout or not self.isValid or self.isDarkened then return end
        Event.emitLocal("PlayMainMenuHoverBling", {})

        local t = ECS.getWorldPosition(entity)
        
        if t then
            Event.emitLocal("ShowAbilityTooltip", {
                abilityID = self.abilityIndex,
                x = t.x,
                y = t.y
            })
        end

        if not self.isSelected and not self.isBlinking then
            ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY })
        end
    end,

    OnHoverExit = function(self, entity)
        Event.emitLocal("HideAbilityTooltip", {})
        
        -- Only reset visual if not in a special state
        if not self.isDarkened and not self.isBlinking and not self.isSelected then
            ArsenalAbilityIcon.UpdateVisual(self, entity)
        end
    end,

    OnClick = function(self, entity, x, y)
        if self.isInLoadout or not self.isValid then return end
        Event.emitLocal("PlayMainMenuClick", {})

        Log.info("[ArsenalAbilityIcon] Clicked ability " .. self.abilityIndex ..
            " (passive=" .. tostring(self.isPassive) .. ")")

        -- If this icon is already selected, cancel the selection
        if self.isSelected then
            if ArsenalUI_instance then
                ArsenalUI.ClearSelection(ArsenalUI_instance)
            end
            return
        end

        if ArsenalUI_instance and ArsenalUI_instance.lastSelectedSpawnedIcon then
            -- A spawned icon is already selected, perform replacement
            Event.emitLocal("OnArsenalAbilityIconToSpawnedIcon", {
                abilityIndex = self.abilityIndex,
                isPassive = self.isPassive,
                texture = self.iconTexture
            })
        else
            -- No spawned icon selected, select this ability icon
            Event.emitLocal("OnArsenalAbilityDragStart", {
                abilityIndex = self.abilityIndex,
                isPassive = self.isPassive,
                texture = self.iconTexture,
                x = x,
                y = y
            })
        end
    end,
})

return class
