-- UI_GameAbilitySlot.lua - Ability icon slot in the gameplay ability bar
-- Resolves the editor-assigned slot ID through the arsenal map to spawn the correct icon prefab.

local class = DefineType("GameAbilitySlot", {
    Serializable = {
        slotID = 0  -- Original editor ability ID (e.g. DRAGOON_1 = 3)
    },

    resolvedAbilityID = 0,
    currentIconEntity = nil,

    Start = function(self, entity)
        -- Resolve through arsenal map
        if ClientGameManager_instance then
            self.resolvedAbilityID = ClientGameManager_instance:ResolveArsenalAbility(self.slotID)
        else
            self.resolvedAbilityID = self.slotID
        end

        if self.resolvedAbilityID ~= self.slotID then
            Log.info("[GameAbilitySlot] Resolved slot " .. self.slotID .. " -> " .. self.resolvedAbilityID)
        end

        -- Spawn the icon prefab
        GameAbilitySlot.SpawnIcon(self, entity)
    end,

    SpawnIcon = function(self, entity)
        if self.currentIconEntity then
            ECS.destroyEntityDeferred(self.currentIconEntity)
            self.currentIconEntity = nil
        end

        if self.resolvedAbilityID == 0 then return end

        local prefabName = CONSTANTS.ARSENAL_ICON_PREFABS[self.resolvedAbilityID]
        if not prefabName then
            Log.warning("[GameAbilitySlot] No icon prefab for ability " .. self.resolvedAbilityID)
            return
        end

        local icon = Prefab.spawn(prefabName, entity)
        if icon then
            self.currentIconEntity = icon
            Log.info("[GameAbilitySlot] Spawned " .. prefabName .. " for slot " .. self.slotID)
        else
            Log.warning("[GameAbilitySlot] Failed to spawn prefab " .. prefabName)
        end
    end
})

return class
