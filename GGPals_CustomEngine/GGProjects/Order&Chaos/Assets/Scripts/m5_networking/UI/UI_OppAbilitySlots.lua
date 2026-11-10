local class = DefineType("OppAbilitySlots", {
    Serializable = {
        orderSlotID = 0,
        chaosSlotID = 0,

        isOppSlot = true
    },

    resolvedAbilityID = 0,
    currentIconEntity = nil,

    ResolveAndSpawn = function(self, entity, isOrder)
        local slotID = isOrder and self.chaosSlotID or self.orderSlotID
        if ClientGameManager_instance then
            local resolved = ClientGameManager_instance:ResolveArsenalAbility(slotID)
            -- Virtual passive slots (>100) that weren't remapped have no icon;
            -- treat as empty (no passive equipped for this unit)
            if resolved > 100 then
                self.resolvedAbilityID = 0
            else
                self.resolvedAbilityID = resolved
            end
        else
            self.resolvedAbilityID = slotID
        end
        self:SpawnIcon(entity)
    end,

    Init = function(self, entity)
        Event.subscribe("OnGameplayFactionReady", function(params)
            local isOrder = params.isOrder
            if not self.isOppSlot then
                isOrder = not isOrder
            end
            self:ResolveAndSpawn(entity, isOrder)
        end)
    end,

    Start = function(self, entity)
        local isOrder = true
        if ClientGameManager_instance then
            isOrder = (ClientGameManager_instance.playerFaction == CONSTANTS.ORDER_PLAYER)
            if not self.isOppSlot then
                isOrder = not isOrder
            end
        end
        self:ResolveAndSpawn(entity, isOrder)
    end,

    SpawnIcon = function(self, entity)
        if self.currentIconEntity then
            ECS.destroyEntityDeferred(self.currentIconEntity)
            self.currentIconEntity = nil
        end

        if self.resolvedAbilityID == 0 then return end

        local prefabName = CONSTANTS.ARSENAL_ICON_PREFABS[self.resolvedAbilityID]
        if not prefabName then
            Log.warning("[OppAbilitySlot] No icon prefab for ability " .. self.resolvedAbilityID)
            return
        end

        local icon = Prefab.spawn(prefabName, entity)
        if icon then
            self.currentIconEntity = icon
            Log.info("[OppAbilitySlot] Spawned " .. prefabName .. " for ability " .. self.resolvedAbilityID)
            
            local iconSelf = ECS.getScriptInstance(icon)
            if iconSelf then
                iconSelf.abilityIndex = self.resolvedAbilityID
                iconSelf.isPassive = self.isPassiveSlot
                iconSelf.isArsenal = false
            end
        else
            Log.warning("[OppAbilitySlot] Failed to spawn prefab " .. prefabName)
        end
    end
})

return class