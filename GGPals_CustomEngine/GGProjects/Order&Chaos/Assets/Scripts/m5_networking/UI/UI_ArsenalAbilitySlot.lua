-- UI_ArsenalAbilitySlot.lua - The 3 slots below the unit sprite in center

local class = DefineType("ArsenalAbilitySlot", {
    Serializable = {
        slotIndex = 1,        -- slot within type: 1 or 2 for active, 1 for passive
        isPassiveSlot = false  -- true for the passive slot
    },

    TextureAsset = {
        emptySlotTexture = 0
    },

    currentAbilityIndex = 0,
    currentIconEntity = nil,
    
    -- Store subscription IDs for cleanup
    subscriptions = {},

    Init = function(self, entity)
        self.subscriptions = {}
        ECS.setSprite(entity, { textureAssetID = self.emptySlotTexture })

        -- Listen for slot updates from ArsenalUI
        self.subscriptions[#self.subscriptions + 1] = Event.subscribe("OnArsenalSlotUpdated", function(params)
            if params.isPassiveSlot == self.isPassiveSlot and params.slotIndex == self.slotIndex then
                if params.abilityIndex > 0 then
                    ArsenalAbilitySlot.ShowAbility(self, entity, params.abilityIndex, params.texture)
                else
                    ArsenalAbilitySlot.ClearSlot(self, entity)
                end
            end
        end)

        Log.info("[ArsenalAbilitySlot] Initialized slot " .. self.slotIndex ..
            " (passive=" .. tostring(self.isPassiveSlot) .. ")")
    end,
    
    OnDestroy = function(self, entity)
        for _, subId in ipairs(self.subscriptions) do
            Event.unsubscribe(subId)
        end
        self.subscriptions = {}
    end,

    ShowAbility = function(self, entity, abilityIndex, texture)
        self.currentAbilityIndex = abilityIndex

        -- Spawn icon prefab as child (the prefab handles the visual)
        ArsenalAbilitySlot.SpawnIconPrefab(self, entity, abilityIndex)

        Log.info("[ArsenalAbilitySlot] Showing ability " .. abilityIndex ..
            " in " .. (self.isPassiveSlot and "passive" or "active") .. " slot " .. self.slotIndex)
    end,

    SpawnIconPrefab = function(self, entity, abilityIndex)
        -- Destroy previous icon if any
        if self.currentIconEntity then
            ECS.destroyEntityDeferred(self.currentIconEntity)
            self.currentIconEntity = nil
        end

        local prefabName = CONSTANTS.ARSENAL_ICON_PREFABS[abilityIndex]
        if not prefabName then
            Log.info("[ArsenalAbilitySlot] No arsenal icon prefab for ability " .. abilityIndex)
            return
        end

        local icon = Prefab.spawn(prefabName, entity)
        if icon then
            self.currentIconEntity = icon

            -- Set ability info on the spawned icon script
            local iconSelf = ECS.getScriptInstance(icon)
            if iconSelf then
                iconSelf.abilityIndex = abilityIndex
                iconSelf.isPassive = self.isPassiveSlot
                iconSelf.isArsenal = true
            end

            Log.info("[ArsenalAbilitySlot] Spawned icon prefab " .. prefabName)
        else
            Log.warning("[ArsenalAbilitySlot] Failed to spawn prefab " .. prefabName)
        end
    end,

    ClearSlot = function(self, entity)
        self.currentAbilityIndex = 0

        -- Destroy spawned icon prefab
        if self.currentIconEntity then
            ECS.destroyEntityDeferred(self.currentIconEntity)
            self.currentIconEntity = nil
        end
    end,

    OnHoverEnter = function(self, entity)
        ECS.setSprite(entity, {
            colorMultiply = {r = 1.2, g = 1.2, b = 0.8, a = 1}
        })
    end,

    OnClick = function(self, entity, x, y)
        if self.currentAbilityIndex == 0 then
            -- Empty slot clicked with a selection pending — place it here
            if ArsenalUI_instance and ArsenalUI_instance.lastSelectedAbility then
                local selected = ArsenalUI_instance.lastSelectedAbility
                if selected.isPassive == self.isPassiveSlot then
                    ArsenalUI.UpdateSlot(ArsenalUI_instance, ArsenalUI_instance.currentUnit,
                        self.isPassiveSlot, self.slotIndex, selected.abilityIndex)
                    ArsenalUI_instance.lastSelectedAbility = nil
                    Event.emitLocal("OnArsenalAbilityHighlighted", { abilityIndex = 0 })
                    ArsenalUI.EmitLoadoutChanged(ArsenalUI_instance)
                end
            end
            return
        end

        -- Filled slot clicked
        if ArsenalUI_instance and ArsenalUI_instance.lastSelectedAbility then
            -- Something already selected — do a swap
            Event.emitLocal("OnArsenalSlotClicked", {
                isPassiveSlot = self.isPassiveSlot,
                slotIndex = self.slotIndex,
                sourceSlotIndex = self.slotIndex
            })
        else
            -- Nothing selected — select this slot's ability
            Event.emitLocal("OnArsenalAbilityDragStart", {
                abilityIndex = self.currentAbilityIndex,
                isPassive = self.isPassiveSlot,
                texture = 0,
                x = x,
                y = y
            })
            Event.emitLocal("OnArsenalAbilityHighlighted", {
                abilityIndex = self.currentAbilityIndex
            })
        end
    end,

    OnHoverExit = function(self, entity)
        ECS.setSprite(entity, {
            colorMultiply = {r = 1, g = 1, b = 1, a = 1}
        })
    end
})

return class
