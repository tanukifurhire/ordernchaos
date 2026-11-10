local class = DefineType("ClientAbility", 
{
    Serializable =
    {
        castDelay = 0.3,
        impactDelay = 0.5,
        recoveryTime = 0.3,
        sfxDelay = 0.0  -- Delay before playing ability SFX (override in derived abilities)
    },

    abilityID = 0,
    name = "Unnamed",
    abilityType = CONSTANTS.ABILITY_OFFENSIVE,
    cost = 1,
    numRed = 0,
    numBlue = 0,
    unitDamage = 0,
    tileDamage = 0,
    ownerFaction = CONSTANTS.ORDER_PLAYER,
    targetsOpponent = true,
    requiresTargeting = false,
    isAutoTarget = false,

    CanUse = function(self, grid, currentAP)
        -- Each ability can only be used once per turn (unless Overclock Trigger)
        local gm = ClientGameManager_instance
        if gm and gm.usedAbilities then
            local castingUnit = gm.pendingCastUnit or gm:GetUnitForAbility(self.abilityID)
            local abilityKey = (castingUnit or 0) .. "_" .. self.abilityID
            if castingUnit and gm.usedAbilities[abilityKey] then
                -- Check if the owning unit has Overclock Trigger passive
                if not gm:HasPassive(castingUnit, CONSTANTS.ABILITIES.PASSIVE_1) then
                    return false
                end
            end
        end

        if currentAP < self.cost then return false end

        local numRed, numBlue = self:CountOccupiedTiles(grid)
        return numRed >= self.numRed and numBlue >= self.numBlue
    end,

    CountOccupiedTiles = function(self, grid)
        local numRed = 0
        local numBlue = 0

        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1
                if grid.Tiles[index].occupied then
                    if grid.Tiles[index].color == CONSTANTS.TILE_RED then
                        numRed = numRed + 1
                    elseif grid.Tiles[index].color == CONSTANTS.TILE_BLUE then
                        numBlue = numBlue + 1
                    end
                end
            end
        end

        return numRed, numBlue
    end,

    GetHighlightColor = function(self)
        if self.abilityType == CONSTANTS.ABILITY_OFFENSIVE then
            return CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
        end
        return CONSTANTS.COLOR_SUPPORT_ABILITY_HIGHLIGHT
    end,

    -- Called when the ability icon/hotkey is selected. Validates and enters targeting mode.
    OnSelect = function(self, grid, currentAP)
        if not self:CanUse(grid, currentAP) then return false end
        return true
    end,

    -- Called every frame while ability is selected. Handles targeting overlay.
    Update = function(self, targetGrid)
        local tilePos = targetGrid:GetHoveredTilePos()
        local gm = ClientGameManager_instance
        if not tilePos then
            if gm then gm:RevertPreviewedTileDamage() end
            return
        end

        local targetTiles = self:GetTargetTiles(tilePos.x, tilePos.y)
        local color = self:GetHighlightColor()

        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1
                local isTarget = false

                for _, tileIdx in ipairs(targetTiles) do
                    if index == tileIdx then
                        isTarget = true
                        break
                    end
                end

                if isTarget then
                    Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, color)
                    Grid.setTileTexture(targetGrid.gridID, row, col, 1, gm.borderTexture)
                else
                    Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                    Grid.setTileTexture(targetGrid.gridID, row, col, 1, gm.emptyTexture)
                end
            end
        end

        Grid.setTileTexture(targetGrid.gridID, tilePos.y, tilePos.x, 1, gm.crosshairTexture)
        gm:PreviewTileDamage(targetGrid, targetTiles, self.tileDamage)
    end,

    -- Clears all targeting visuals (colorAdd + layer 1 textures).
    ClearTargeting = function(self, targetGrid)
        local gm = ClientGameManager_instance
        gm:RevertPreviewedTileDamage()
        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                Grid.setTileTexture(targetGrid.gridID, row, col, 1, gm.emptyTexture)
            end
        end
    end,

    -- Returns array of 1-based tile indices that would be affected. Override per ability.
    GetTargetTiles = function(self, targetX, targetY)
        return {}
    end,

    -- Internal trigger state for Update-loop-based resolution playback
    _triggerState = nil,

    -- VFX standard offset (matches old VFXPlayer defaults)
    vfxOffsetX = 0.01,
    vfxOffsetY = 0.5,

    -- SFX event names for each ability (maps abilityID to SFXManager event)
    ABILITY_SFX_EVENT = {
        [1]  = "PlayVFXOracleA1",
        [2]  = "PlayVFXOracleA2",
        [3]  = "PlayVFXDragonA1",
        [4]  = "PlayVFXDragonA2",
        [5]  = "PlayVFXImpA1",
        [6]  = "PlayVFXImpA2",
        [7]  = "PlayVFXEyeA1",
        [8]  = "PlayVFXEyeA2",
        [9]  = "PlayVFXTirionA1",
        [10] = "PlayVFXTirionA2",
        [11] = "PlayVFXVizomA1",
        [12] = "PlayVFXVizomA2",
        [13] = "PlayVFXGeneric1",
    },

    -- Helper: plays the SFX associated with this ability via SFXManager events
    PlayAbilitySound = function(self)
        local eventName = self.ABILITY_SFX_EVENT[self.abilityID]
        if eventName then
            -- pan to target position if available
            local result = self._currentResult
            if result and result.action then
                local pan = (result.action.targetX / (CONSTANTS.GRID_COLS - 1)) * 2 - 1
                Event.emitLocal("SetStereoPan", { pan = pan })
            end
            Event.emitLocal(eventName, {})
        end
    end,

    -- Helper: spawns floating damage numbers for all hits in _currentResult.
    -- Call this from UpdateTrigger at the moment you want damage to appear.
    ShowDamageNumbers = function(self)
        local result = self._currentResult
        if not result or not result._deferredHits then
            return
        end

        local gm = ClientGameManager_instance
        if not gm then return end

        local hits = result._deferredHits
        local useTotalDamage = result._useTotalDamage
        local displayDamage = result._deferredDamage
        local fallbackDamage = result._fallbackDamage

        -- Trigger hit state on affected units
        Event.emitLocal("OnTilesHit", { hits = hits })

        for _, hit in ipairs(hits) do
            self:_spawnHitNumber(hit, useTotalDamage, displayDamage, fallbackDamage, result._wrathConsumed)
        end

        -- Clear deferred hits so we don't show them again
        result._deferredHits = nil
    end,

    -- Helper: spawns damage number for a specific grid position.
    -- Used by abilities with sequential hits (e.g., Vizom bounces, Imp recasts).
    -- Searches both _deferredHits and _recastHits.
    ShowDamageNumberAt = function(self, gridX, gridY)
        local result = self._currentResult
        if not result then
            return false
        end

        local useTotalDamage = result._useTotalDamage
        local displayDamage = result._deferredDamage
        local fallbackDamage = result._fallbackDamage

        -- Search deferred hits first
        if result._deferredHits then
            for i, hit in ipairs(result._deferredHits) do
                if hit.gridX == gridX and hit.gridY == gridY then
                    Event.emitLocal("OnTilesHit", { hits = { hit } })
                    self:_spawnHitNumber(hit, useTotalDamage, displayDamage, fallbackDamage, result._wrathConsumed)
                    table.remove(result._deferredHits, i)
                    return true
                end
            end
        end

        -- Search recast hits (for multi-hit abilities like Imp_1, Eye_1)
        if result._recastHits then
            for i, hit in ipairs(result._recastHits) do
                if hit.gridX == gridX and hit.gridY == gridY then
                    Event.emitLocal("OnTilesHit", { hits = { hit } })
                    self:_spawnHitNumber(hit, false, 0, fallbackDamage, result._wrathConsumed)
                    table.remove(result._recastHits, i)
                    return true
                end
            end
        end

        return false
    end,

    -- Internal: spawns floating text and modifiers for a single hit
    _spawnHitNumber = function(self, hit, useTotalDamage, displayDamage, fallbackDamage, wrathConsumed)
        local gm = ClientGameManager_instance
        if not gm then return end

        local hitGrid = hit.isOrderGrid and gm.orderGrid or gm.chaosGrid
        if not hitGrid then return end

        local world = Grid.gridToWorld(hitGrid.gridID, hit.gridX, hit.gridY)
        -- Damage numbers offset further right so they don't block modifier icons
        local dmgOffsetX = 0.45
        local dmgOffsetY = 0.5
        if hit.shielded then
            local shieldedDmg = useTotalDamage and displayDamage or (hit.damage or 0)
            Event.emitLocal("SpawnFloatingText", {
                worldX = world.x + dmgOffsetX,
                worldY = world.y + dmgOffsetY,
                damage = shieldedDmg,
                text = shieldedDmg > 0 and nil or "BLOCKED"
            })
        else
            local hitDmg = useTotalDamage and displayDamage or (hit.damage or fallbackDamage)
            Event.emitLocal("SpawnFloatingText", {
                worldX = world.x + dmgOffsetX,
                worldY = world.y + dmgOffsetY,
                damage = hitDmg
            })
        end

        -- Spawn damage modifier indicators (Hunter, Isolation, Burn, Wrath)
        local modifiers = {}
        if hit.hunterMarkBonus then
            table.insert(modifiers, "HunterMarkBonus")
        end
        if hit.isolationBonus then
            table.insert(modifiers, "IsolationBonus")
        end
        if hit.burnBonus then
            table.insert(modifiers, "BurnBonus")
        end
        if wrathConsumed then
            table.insert(modifiers, "WrathBonus")
        end
        if #modifiers > 0 then
            local yOffset = 1.0
            for i, modName in ipairs(modifiers) do
                local prefab = Prefab.spawn(modName, world.x + 0.01, world.y + yOffset + (i - 1) * 0.5)
                if prefab then
                    Log.info("[Ability] Spawned damage indicator: " .. modName)
                end
            end
        end
    end,

    -- Helper: spawns a VFX prefab entity at a grid position with world-space offset.
    -- Returns the spawned entity (or nil on failure).
    SpawnVFX = function(self, prefabName, grid, col, row, offsetX, offsetY)
        offsetX = offsetX or self.vfxOffsetX
        offsetY = offsetY or self.vfxOffsetY
        
        -- Apply tileXCorrection for trap VFX prefabs
        local isTrapVFX = (prefabName == "Tirion_Ability2_VFX" or prefabName == "Vizom_Ability2_VFX")
        if isTrapVFX then
            local tileXCorrection = {
                [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
            }
            local xCorrection = tileXCorrection[row] and tileXCorrection[row][col] or 0
            offsetX = offsetX + xCorrection
        end
        
        local world = Grid.gridToWorld(grid.gridID, col, row)
        local spawnX = world.x + offsetX
        local spawnY = world.y + offsetY
        local spawned = Prefab.spawn(prefabName, spawnX, spawnY)
        if spawned then
            Log.info("[VFX] Spawned " .. prefabName .. " at (" ..
                string.format("%.2f", spawnX) .. ", " .. string.format("%.2f", spawnY) .. ")")
            
            Event.emitLocal("AbilityVFXSpawned", { -- for AbilityQueueUI.lua
                vfxEntity = spawned
            })

        else
            Log.warning("[VFX] Failed to spawn prefab: " .. prefabName)
        end
        return spawned
    end,

    -- Helper: sets up the standard highlight-then-clear trigger pattern.
    -- Used by most offensive abilities. Special abilities override Trigger/UpdateTrigger directly.
    StartStandardTrigger = function(self, grid, targetTiles, color, interval, vfxDuration)
        Log.info("[Ability] StartStandardTrigger: " .. self.name ..
            " tiles=" .. #targetTiles .. " interval=" .. interval .. " vfxDuration=" .. vfxDuration)
        
        -- Show damage numbers immediately for standard triggers
        self:ShowDamageNumbers()

        -- Highlight first tile immediately
        if #targetTiles > 0 then
            local tileIdx = targetTiles[1]
            local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
            local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
            Grid.setTileColorAdd(grid.gridID, row, col, 0, color)
        end

        self._triggerState = {
            phase = "highlight",
            grid = grid,
            tiles = targetTiles,
            color = color,
            currentTile = 1,
            timer = 0,
            interval = interval,
            vfxDuration = vfxDuration
        }
    end,

    -- Default UpdateTrigger: drives the standard highlight→vfx→clear→complete cycle.
    -- Called every frame by ResolutionPhase while this ability is resolving.
    UpdateTrigger = function(self, dt)
        local s = self._triggerState
        if not s then return end

        s.timer = s.timer + dt

        if s.phase == "highlight" then
            if s.currentTile < #s.tiles and s.timer >= s.interval then
                s.currentTile = s.currentTile + 1
                local tileIdx = s.tiles[s.currentTile]
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                Grid.setTileColorAdd(s.grid.gridID, row, col, 0, s.color)
                s.timer = 0
            elseif s.currentTile >= #s.tiles then
                s.phase = "vfx"
                s.timer = 0
            end
        elseif s.phase == "vfx" then
            if s.timer >= s.vfxDuration then
                for _, tileIdx in ipairs(s.tiles) do
                    local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                    local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                    Grid.setTileColorAdd(s.grid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                end
                self._triggerState = nil
                Log.info("[Ability] Trigger complete: " .. self.name .. " (ID=" .. self.abilityID .. ")")
                Event.emitLocal("OnAbilityTriggerComplete", { abilityID = self.abilityID })
            end
        end
    end,

    -- Called during resolution phase to start this ability's visual replay. Override per ability.
    Trigger = function(self, orderGrid, chaosGrid, targetX, targetY, param1, param2)
    end,

    -- special VFX effects for playback in AbilityQueueUI.lua
    TriggerPlayback = function(self, orderGrid, chaosGrid, targetX, targetY, param1, param2, casterFaction)
    end
})

return class
