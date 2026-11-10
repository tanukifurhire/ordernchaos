-- UI_ArsenalUnitAbilitySlot.lua - Persistent ability slot tied to a specific unit
-- Unlike ArsenalAbilitySlot, this slot does not reset when switching units.

local class = DefineType("ArsenalUnitAbilitySlot", {
    Serializable = {
        slotIndex = 1,        -- slot within type: 1 or 2 for active, 1 for passive
        isPassiveSlot = false, -- true for the passive slot
        orderUnitID = 1,
        chaosUnitID = 4
    },

    TextureAsset = {
        emptySlotTexture = 0
    },

    unitID = 0,
    currentAbilityIndex = 0,
    currentIconEntity = nil,

    Init = function(self, entity)
        -- Resolve which unit this slot belongs to based on faction
        Event.subscribe("OnArsenalFactionReady", function(params)
            if params.isOrder then
                self.unitID = self.orderUnitID
            else
                self.unitID = self.chaosUnitID
            end
            Log.info("[ArsenalUnitAbilitySlot] Unit " .. self.unitID ..
                " slot " .. self.slotIndex ..
                " (passive=" .. tostring(self.isPassiveSlot) .. ")")
        end)

        -- Only react to updates for this slot's specific unit
        Event.subscribe("OnArsenalSlotUpdated", function(params)
            if self.unitID == 0 then return end
            if params.unitType == self.unitID
                and params.isPassiveSlot == self.isPassiveSlot
                and params.slotIndex == self.slotIndex then
                if params.abilityIndex > 0 then
                    ArsenalUnitAbilitySlot.ShowAbility(self, entity, params.abilityIndex)
                else
                    ArsenalUnitAbilitySlot.ClearSlot(self, entity)
                end
            end
        end)
    end,

    ShowAbility = function(self, entity, abilityIndex)
        self.currentAbilityIndex = abilityIndex

        -- Spawn icon prefab as child (the prefab handles the visual)
        ArsenalUnitAbilitySlot.SpawnIconPrefab(self, entity, abilityIndex)

        Log.info("[ArsenalUnitAbilitySlot] Unit " .. self.unitID ..
            " showing ability " .. abilityIndex ..
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
            Log.info("[ArsenalUnitAbilitySlot] No arsenal icon prefab for ability " .. abilityIndex)
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
                iconSelf.isArsenal = false
            end

            Log.info("[ArsenalUnitAbilitySlot] Spawned icon prefab " .. prefabName)
        else
            Log.warning("[ArsenalUnitAbilitySlot] Failed to spawn prefab " .. prefabName)
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
        ECS.setUIComponent(entity, {
            colorMultiply = {r = 1.2, g = 1.2, b = 0.8, a = 1}
        })
    end,

    OnHoverExit = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = {r = 1, g = 1, b = 1, a = 1}
        })
    end,

    -- No OnClick: the spawned ArsenalSpawnedIcon handles click-to-replace
})

return class
