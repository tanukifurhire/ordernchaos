local class = DefineType("ClientImpAbility2",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerPullDelay = 0.3,
        triggerVFXDuration = 0.5,
        sfxDelay = 0.0  -- Delay before playing ability SFX
    },

    targetingPhase = 0,
    storedTargetX = 0,
    storedTargetY = 0,

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.IMP_2
        self.ability.name = "Wicked Shift"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 1
        self.ability.numRed = 1
        self.ability.numBlue = 0
        self.ability.unitDamage = 8
        self.ability.tileDamage = 4
        self.ability.ownerFaction = CONSTANTS.CHAOS_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        self.ability.OnSelect = function(ab, grid, currentAP)
            if not ab:CanUse(grid, currentAP) then return false end
            outer.targetingPhase = 0
            outer.storedTargetX = 0
            outer.storedTargetY = 0
            outer._phaseJustChanged = false
            return true
        end

        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end

        -- Returns adjacent empty tiles around a position
        self.ability.GetAdjacentTiles = function(ab, centerX, centerY)
            local tiles = {}
            local offsets = { {0,-1}, {0,1}, {-1,0}, {1,0} }
            for _, off in ipairs(offsets) do
                local x = centerX + off[1]
                local y = centerY + off[2]
                if x >= 0 and x < CONSTANTS.GRID_COLS and y >= 0 and y < CONSTANTS.GRID_ROWS then
                    table.insert(tiles, y * CONSTANTS.GRID_COLS + x + 1)
                end
            end
            return tiles
        end

        self.ability.Update = function(ab, targetGrid)
            -- Clear same-frame guard (set by phase 0 → 1 transition)
            outer._phaseJustChanged = false

            local gm = ClientGameManager_instance
            local tilePos = targetGrid:GetHoveredTilePos()

            -- revert all previously previewed tile damages
            -- Event.emitLocal("RevertPreviewedTileTextures", {})

            if not tilePos then return end

            local color = ab:GetHighlightColor()

            if outer.targetingPhase == 0 then
                -- Phase 0: select the target unit to pull
                local targetTiles = ab:GetTargetTiles(tilePos.x, tilePos.y)

                for row = 0, CONSTANTS.GRID_ROWS - 1 do
                    for col = 0, CONSTANTS.GRID_COLS - 1 do
                        local index = row * CONSTANTS.GRID_COLS + col + 1
                        local isTarget = false
                        for _, tileIdx in ipairs(targetTiles) do
                            if index == tileIdx then isTarget = true; break end
                        end
                        if isTarget then
                            -- preview damage to the tile
                            Event.emitLocal("PreviewTileTexture", {
                                tileIdx = index,
                                damage = ab.tileDamage
                            })
                            -----------------------------
                            Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, color)
                            Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.borderTexture)
                        else
                            Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                            Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.emptyTexture)
                        end
                    end
                end
                -- Crosshair only on occupied tiles (valid pull targets)
                local hoverIdx = tilePos.y * CONSTANTS.GRID_COLS + tilePos.x + 1
                if targetGrid.Tiles[hoverIdx] and targetGrid.Tiles[hoverIdx].occupied then
                    Grid.setTileTexture(targetGrid.gridID, tilePos.y, tilePos.x, 2, gm.crosshairTexture)
                end

            elseif outer.targetingPhase == 1 then
                -- Phase 1: select adjacent tile to pull target into
                local adjTiles = ab:GetAdjacentTiles(outer.storedTargetX, outer.storedTargetY)

                for row = 0, CONSTANTS.GRID_ROWS - 1 do
                    for col = 0, CONSTANTS.GRID_COLS - 1 do
                        local index = row * CONSTANTS.GRID_COLS + col + 1
                        local isAdj = false
                        for _, tileIdx in ipairs(adjTiles) do
                            if index == tileIdx then isAdj = true; break end
                        end

                        if row == outer.storedTargetY and col == outer.storedTargetX then
                            Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, color)
                            Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.borderTexture)
                        elseif isAdj then
                            Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_SUPPORT_ABILITY_HIGHLIGHT)
                            Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.borderTexture)
                        else
                            Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                            Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.emptyTexture)
                        end
                    end
                end
                -- Crosshair only on adjacent tiles (valid destinations)
                local hoverIdx2 = tilePos.y * CONSTANTS.GRID_COLS + tilePos.x + 1
                local isHoverAdj = false
                for _, tileIdx in ipairs(adjTiles) do
                    if tileIdx == hoverIdx2 then isHoverAdj = true; break end
                end
                if isHoverAdj then
                    Grid.setTileTexture(targetGrid.gridID, tilePos.y, tilePos.x, 2, gm.crosshairTexture)
                end
            end

            gm:RevertPreviewedTileDamage()
            if outer.targetingPhase == 0 then
                local targetTiles = ab:GetTargetTiles(tilePos.x, tilePos.y)
                gm:PreviewTileDamage(targetGrid, targetTiles, ab.tileDamage)
            end
        end

        -- Called by prep phase on click. Returns nil to consume click without queuing in phase 0.
        self.ability.ConfirmTarget = function(ab, gridX, gridY)
            if outer.targetingPhase == 0 then
                outer.storedTargetX = gridX
                outer.storedTargetY = gridY
                outer.targetingPhase = 1
                -- Guard: prevent phase 1 from firing on the same click (dual-grid event)
                outer._phaseJustChanged = true
                return nil
            else
                -- Block if phase just changed this frame (same click fired events on both grids)
                if outer._phaseJustChanged then
                    return nil
                end

                -- Validate: clicked tile must be adjacent to stored target
                local adjTiles = ab:GetAdjacentTiles(outer.storedTargetX, outer.storedTargetY)
                local clickIdx = gridY * CONSTANTS.GRID_COLS + gridX + 1
                local isValid = false
                for _, tileIdx in ipairs(adjTiles) do
                    if tileIdx == clickIdx then
                        isValid = true
                        break
                    end
                end

                if not isValid then
                    Log.info("[Imp2] Invalid destination - not adjacent to target")
                    return nil
                end

                local result = {
                    targetX = outer.storedTargetX,
                    targetY = outer.storedTargetY,
                    param1 = gridX,
                    param2 = gridY
                }
                outer.targetingPhase = 0
                return result
            end
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            local grid = orderGrid
            local color = ab:GetHighlightColor()

            local srcRow = targetY
            local srcCol = targetX
            Grid.setTileColorAdd(grid.gridID, srcRow, srcCol, 0, color)

            -- Spawn impact VFX at source tile
            ab:SpawnVFX("Imp_Ability2_VFX", orderGrid, targetX, targetY)
            ab:PlayAbilitySound()
            
            -- Show damage numbers at impact
            ab:ShowDamageNumbers()

            local destRow = param2
            local destCol = param1

            -- Determine hit unit from pre-calculated results
            local hitUnitType = nil
            if ab._currentResult and ab._currentResult.hits then
                for _, hit in ipairs(ab._currentResult.hits) do
                    if hit.unitType then
                        hitUnitType = hit.unitType
                        break
                    end
                end
            end

            -- Subscribe to force move completion if a unit was hit
            ab._forceMoveComplete = false
            ab._forceMoveHandler = 0
            if hitUnitType then
                ab._forceMoveHandler = Event.subscribe("OnReplayMoveComplete", function(params)
                    ab._forceMoveComplete = true
                    if ab._forceMoveHandler ~= 0 then
                        Event.unsubscribe(ab._forceMoveHandler)
                        ab._forceMoveHandler = 0
                    end
                end)
            end

            ab._triggerState = {
                phase = "pull_delay",
                timer = 0,
                pullDelay = outer.triggerPullDelay,
                vfxDuration = outer.triggerVFXDuration,
                grid = grid,
                color = color,
                srcRow = srcRow,
                srcCol = srcCol,
                destRow = destRow,
                destCol = destCol,
                hitUnitType = hitUnitType,
                moveEmitted = false
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "pull_delay" then
                if s.timer >= s.pullDelay then
                    Grid.setTileColorAdd(s.grid.gridID, s.destRow, s.destCol, 0, s.color)

                    local pullBlocked = ab._currentResult and ab._currentResult.pullBlocked
                    if s.hitUnitType and not s.moveEmitted and not pullBlocked then
                        Log.info("[Imp2] Emitting ReplayMoveUnit for unit " .. tostring(s.hitUnitType) ..
                            " from (" .. s.srcCol .. "," .. s.srcRow .. ") to (" .. s.destCol .. "," .. s.destRow .. ")")
                        Event.emitLocal("ReplayMoveUnit", {
                            unitType = s.hitUnitType,
                            isOrder = true,
                            fromX = s.srcCol,
                            fromY = s.srcRow,
                            toX = s.destCol,
                            toY = s.destRow
                        })
                        -- Tell the fist VFX to move to the pull destination
                        Event.emitLocal("OnForceMoveUnit", {
                            ability = CONSTANTS.ABILITIES.IMP_2,
                            toX = s.destCol,
                            toY = s.destRow
                        })
                        s.moveEmitted = true
                        s.phase = "pulling"
                        s.timer = 0
                    elseif pullBlocked then
                        Log.info("[Imp2] Pull blocked: destination tile is destroyed, skipping movement")
                        s.phase = "vfx"
                        s.timer = 0
                    else
                        s.phase = "vfx"
                        s.timer = 0
                    end
                end

            elseif s.phase == "pulling" then
                if ab._forceMoveComplete then
                    s.phase = "vfx"
                    s.timer = 0
                end
                if s.timer > 5.0 then
                    Log.warning("[Imp2] Force move watchdog triggered")
                    s.phase = "vfx"
                    s.timer = 0
                end

            elseif s.phase == "vfx" then
                if s.timer >= s.vfxDuration then
                    Grid.setTileColorAdd(s.grid.gridID, s.srcRow, s.srcCol, 0, CONSTANTS.COLOR_NONE)
                    Grid.setTileColorAdd(s.grid.gridID, s.destRow, s.destCol, 0, CONSTANTS.COLOR_NONE)

                    if ab._forceMoveHandler and ab._forceMoveHandler ~= 0 then
                        Event.unsubscribe(ab._forceMoveHandler)
                        ab._forceMoveHandler = 0
                    end

                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end
            end
        end
    end
})

return class
