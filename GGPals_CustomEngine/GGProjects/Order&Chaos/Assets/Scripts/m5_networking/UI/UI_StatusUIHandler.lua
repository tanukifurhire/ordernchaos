local class = DefineType("StatusUIHandler", {
    Serializable = {
        orderUnit = 1,
        chaosUnit = 4,

        isSidePanel = true,

        -- Layout settings for dynamic status positioning
        statusSpacingX = 40.601,  -- horizontal gap between each status icon
        statusSpacingY = 0.001,   -- vertical gap (0 = single row)
        statusStartOffsetX = 35.001, -- X offset of the first status from parent origin
        statusStartOffsetY = 10.001, -- Y offset of the first status from parent origin
        maxPerRow = 6            -- max statuses per row before wrapping
    },

    unit = 0,
    activeStatuses = nil, -- [statusID] = spawned entity

    GetStatusData = function(self, statusID)
        local gm = ClientGameManager_instance
        local ownerUnit = 0
        local stackCount = 1
        local isExpiring = false

        if gm and gm.hunterMarks and statusID == CONSTANTS.ABILITIES.PASSIVE_2 then
            for markerUnit, markedEnemy in pairs(gm.hunterMarks) do
                if markedEnemy == self.unit then
                    ownerUnit = markerUnit
                    break
                end
            end
        end

        if gm and gm.unitStatuses and gm.unitStatuses[self.unit] then
            stackCount = tonumber(gm.unitStatuses[self.unit][statusID]) or stackCount
        end
        if stackCount < 1 then stackCount = 1 end

        if gm and gm.unitStatusExpiring and gm.unitStatusExpiring[self.unit] then
            isExpiring = gm.unitStatusExpiring[self.unit][statusID] and true or false
        end

        return ownerUnit, stackCount, isExpiring
    end,

    GetStatusCount = function(self)
        local count = 0
        for _ in pairs(self.activeStatuses) do count = count + 1 end
        return count
    end,

    GetStatusPosition = function(self, index)
        if self.isSidePanel and index >= 2 then
            local col = (index - 2) % self.maxPerRow
            local x = self.statusStartOffsetX + col * self.statusSpacingX
            local y = self.statusStartOffsetY + self.statusStartOffsetX
            return x, y
        end
        local col = index % self.maxPerRow
        local x = self.statusStartOffsetX + col * self.statusSpacingX
        local y = self.statusStartOffsetY
        return x, y
    end,

    RepositionStatuses = function(self)
        local index = 0
        for _, statusEntity in pairs(self.activeStatuses) do
            local x, y = self:GetStatusPosition(index)
            ECS.setTransform(statusEntity, { x = x, y = y })
            index = index + 1
        end
    end,

    UpdateStatusDisplay = function(self, statusID, ownerUnit, stackCount, isExpiring)
        local statusEntity = self.activeStatuses[statusID]
        if not statusEntity then return end
        local scriptInstance = ECS.getScriptInstance(statusEntity)
        if scriptInstance and scriptInstance.ShowDisplay then
            scriptInstance:ShowDisplay(statusEntity, ownerUnit or 0, self.unit, statusID, stackCount or 1, isExpiring and true or false)
        end
    end,

    SpawnStatus = function(self, entity, statusID, ownerUnit, stackCount, isExpiring)
        local prefabName = CONSTANTS.STATUS_PREFABS[statusID]
        if not prefabName then
            Log.warning("[StatusUIHandler] No prefab for statusID " .. statusID)
            return
        end

        local x, y = self:GetStatusPosition(self:GetStatusCount())
        local statusEntity = Prefab.spawn(prefabName, x, y, entity)
        if not statusEntity then
            Log.warning("[StatusUIHandler] Failed to spawn " .. prefabName)
            return
        end

        self.activeStatuses[statusID] = statusEntity
        local resolvedOwner, resolvedStack, resolvedExpiring = self:GetStatusData(statusID)
        self:UpdateStatusDisplay(
            statusID,
            ownerUnit ~= nil and ownerUnit or resolvedOwner,
            stackCount ~= nil and stackCount or resolvedStack,
            isExpiring ~= nil and isExpiring or resolvedExpiring
        )
        Log.info("[StatusUIHandler] Spawned " .. prefabName .. " on unit " .. self.unit .. " (from unit " .. tostring(ownerUnit or resolvedOwner) .. ")")
    end,

    ClearStatuses = function(self)
        for statusID, statusEntity in pairs(self.activeStatuses) do
            local scriptInstance = ECS.getScriptInstance(statusEntity)
            if scriptInstance and scriptInstance.Destroy then
                scriptInstance:Destroy(statusEntity, nil, statusID)
            else
                ECS.destroyEntityDeferred(statusEntity)
            end
        end
        self.activeStatuses = {}
    end,

    -- Rebuild status display from the global lookup for the current unit
    RebuildStatuses = function(self, entity)
        self:ClearStatuses()

        if not ClientGameManager_instance then return end
        if self.unit < 1 or self.unit > 6 then return end

        local statuses = ClientGameManager_instance.unitStatuses[self.unit]
        if not statuses then return end

        for statusID, stackCount in pairs(statuses) do
            if tonumber(stackCount) and tonumber(stackCount) > 0 then
                self:SpawnStatus(entity, statusID)
            end
        end
    end,

    Init = function(self, entity)
        -- Fix: Dragoon(1) and Oracle(2) status UI handlers have swapped unitType in editor
        -- Oracle should be 1, Dragoon should be 2
        if self.orderUnit == 1 then self.orderUnit = 2
        elseif self.orderUnit == 2 then self.orderUnit = 1 end
        
        if self.chaosUnit == 1 then self.chaosUnit = 2
        elseif self.chaosUnit == 2 then self.chaosUnit = 1 end
        
        self.activeStatuses = {}

        Event.subscribe("OnUnitStatusInflicted", function(params)
            if params.targetUnit ~= self.unit then return end

            local statusID = params.statusID
            if self.activeStatuses[statusID] then
                self:UpdateStatusDisplay(statusID, params.ownerUnit, params.stackCount, params.isExpiring)
                return
            end

            self:SpawnStatus(entity, statusID, params.ownerUnit, params.stackCount, params.isExpiring)
        end)

        Event.subscribe("OnUnitStatusRemoved", function(params)
            if params.targetUnit ~= self.unit then return end

            local statusID = params.statusID
            local statusEntity = self.activeStatuses[statusID]
            if statusEntity then
                local scriptInstance = ECS.getScriptInstance(statusEntity)
                if scriptInstance and scriptInstance.Destroy then
                    scriptInstance:Destroy(statusEntity, params.targetUnit, statusID)
                else
                    ECS.destroyEntityDeferred(statusEntity)
                end
                self.activeStatuses[statusID] = nil
                self:RepositionStatuses()
                Log.info("[StatusUIHandler] Removed status " .. statusID .. " from unit " .. self.unit)
            end
        end)

        Event.subscribe("OnGameplayFactionReady", function(params)
            -- Switch tracked unit based on active faction
            if params.isOrder then
                if self.isSidePanel then self.unit = self.chaosUnit
                else self.unit = self.orderUnit end
            else
                if self.isSidePanel then self.unit = self.orderUnit
                else self.unit = self.chaosUnit end
            end

            -- Rebuild from global lookup so surviving statuses are restored
            self:RebuildStatuses(entity)
        end)
    end,

    Start = function(self, entity)
        -- Resolve initial unit based on player faction and panel type
        -- This mirrors the OnGameplayFactionReady logic to ensure consistency
        local isOrder = false
        if ClientGameManager_instance and ClientGameManager_instance.playerFaction then
            isOrder = (ClientGameManager_instance.playerFaction == CONSTANTS.ORDER_PLAYER)
        end

        if isOrder then
            if self.isSidePanel then
                self.unit = self.chaosUnit  -- Side panel shows opponent (chaos)
            else
                self.unit = self.orderUnit  -- Main panel shows self (order)
            end
        else
            if self.isSidePanel then
                self.unit = self.orderUnit  -- Side panel shows opponent (order)
            else
                self.unit = self.chaosUnit  -- Main panel shows self (chaos)
            end
        end
    end
})

return class
