-- UI_ArsenalSpawnedIcon.lua - Icon spawned inside ability slots via Prefab.spawn
-- In Arsenal Select: clickable to replace with a selected ability
-- In Game: no interaction

local COLOR_DARKENED = { r = 0.4, g = 0.4, b = 0.4, a = 1.0 }
local COLOR_BLINK_BRIGHT = { r = 1.3, g = 1.3, b = 1.0, a = 1.0 }

local class = DefineType("ArsenalSpawnedIcon", {
    Serializable = {},

    -- Set by the slot that spawns this icon via ECS.getScriptInstance
    abilityIndex = 0,
    isPassive = false,
    isArsenal = false,
    
    isSelected = false,
    isDarkened = false,
    isBlinking = false,
    blinkTimer = 0,
    blinkDuration = 0.8,
    
    -- Store subscription IDs for cleanup
    subscriptions = {},

    Init = function(self, entity)
        self.subscriptions = {}
        
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalSpawnedIconHighlighted", function(params)
            if params.abilityIndex == self.abilityIndex and params.isPassive == self.isPassive then
                self.isSelected = true
            else
                self.isSelected = false
            end
            ArsenalSpawnedIcon.UpdateVisual(self, entity)
        end)
        
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalVisualStateUpdate", function(params)
            if not self.isArsenal then return end
            
            local shouldDarken = false
            local shouldBlink = false
            
            -- Get current unit from ArsenalUI
            local currentUnit = params.currentUnit or 0
            local myUnit = ArsenalUI_instance and ArsenalUI_instance.currentUnit or 0
            
            if params.selectedAbilityIcon then
                -- An ability icon is selected - only blink if this spawned icon:
                -- 1. Belongs to the current unit
                -- 2. Matches the type (passive/active)
                if currentUnit == myUnit and params.selectedAbilityIcon.isPassive == self.isPassive then
                    shouldBlink = true
                else
                    shouldDarken = true
                end
            elseif params.selectedSpawnedIcon then
                if params.selectedSpawnedIcon.abilityIndex == self.abilityIndex and 
                   params.selectedSpawnedIcon.isPassive == self.isPassive then
                    -- This is the selected spawned icon, don't change it
                else
                    shouldDarken = true
                end
            end
            
            self.isDarkened = shouldDarken
            self.isBlinking = shouldBlink
            ArsenalSpawnedIcon.UpdateVisual(self, entity)
        end)
        
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalClearVisualEffects", function()
            self.isDarkened = false
            self.isBlinking = false
            self.isSelected = false
            ArsenalSpawnedIcon.UpdateVisual(self, entity)
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
            ArsenalSpawnedIcon.UpdateVisual(self, entity)
        end
    end,
    
    UpdateVisual = function(self, entity)
        if not self.isArsenal then return end
        
        if self.isSelected then
            ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_SELECTED_MULTIPLY or { r = 0.6, g = 0.9, b = 1.0, a = 1.0 } })
        elseif self.isDarkened then
            ECS.setUIComponent(entity, { colorMultiply = COLOR_DARKENED })
        elseif self.isBlinking then
            local phase = self.blinkTimer / self.blinkDuration
            local brightness = 1.0 + 0.3 * math.sin(phase * 2 * math.pi)
            ECS.setUIComponent(entity, { 
                colorMultiply = { r = brightness, g = brightness, b = brightness * 0.9, a = 1.0 } 
            })
        else
            ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY })
        end
    end,

    OnHoverEnter = function(self, entity)
        local t = ECS.getWorldPosition(entity)
        
        if t then
            Event.emitLocal("ShowAbilityTooltip", {
                abilityID = self.abilityIndex,
                x = t.x,
                y = t.y
            })
        end

        if not self.isArsenal then return end
        Event.emitLocal("PlayMainMenuHoverBling", {})

        -- Don't change visual if darkened or blinking
        if not self.isDarkened and not self.isBlinking and not self.isSelected then
            ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY })
        end
    end,

    OnHoverExit = function(self, entity)
        Event.emitLocal("HideAbilityTooltip", {})

        if not self.isArsenal then return end
        
        -- Only reset visual if not in a special state - let UpdateVisual handle it
        if not self.isDarkened and not self.isBlinking and not self.isSelected then
            ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY })
        end
    end,

    OnClick = function(self, entity, x, y)
        if not self.isArsenal then return end
        Event.emitLocal("PlayMainMenuClick", {})

        Log.info("[ArsenalSpawnedIcon] Clicked spawned icon " .. self.abilityIndex)
        
        -- If this icon is already selected, cancel the selection
        if self.isSelected then
            if ArsenalUI_instance then
                ArsenalUI.ClearSelection(ArsenalUI_instance)
            end
            return
        end
        
        if ArsenalUI_instance and ArsenalUI_instance.lastSelectedAbility then
            -- An ability icon is already selected, perform replacement
            Event.emitLocal("OnArsenalRemoveAbility", {
                abilityIndex = self.abilityIndex,
                isPassive = self.isPassive
            })
        else
            -- No ability selected, select this spawned icon
            Event.emitLocal("OnArsenalSpawnedIconSelected", {
                abilityIndex = self.abilityIndex,
                isPassive = self.isPassive,
                x = x,
                y = y
            })
        end
    end
})

return class
