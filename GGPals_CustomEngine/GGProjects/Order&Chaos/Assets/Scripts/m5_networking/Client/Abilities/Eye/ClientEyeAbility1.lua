local class = DefineType("ClientEyeAbility1",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.1,
        recastTriggerDamageInterval = 0.1,
        triggerVFXDuration = 0.5,
        recastDelay = 0.4,
        sfxDelay = 0.001  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.EYE_1
        self.ability.name = "Ruinous Gaze"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 2
        self.ability.numRed = 0
        self.ability.numBlue = 2
        self.ability.unitDamage = 8
        self.ability.tileDamage = 2
        self.ability.splashUnitDamage = 1
        self.ability.ownerFaction = CONSTANTS.CHAOS_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        -- Single tile targeting
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end

        self.ability.Update = function(ab, targetGrid)
            local gm = ClientGameManager_instance
            local tilePos = targetGrid:GetHoveredTilePos()

            -- revert all previously previewed tile damages
            -- Event.emitLocal("RevertPreviewedTileTextures", {})

            if not tilePos then return end

            local targetTiles = ab:GetTargetTiles(tilePos.x, tilePos.y)
            local color = ab:GetHighlightColor()

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
            Grid.setTileTexture(targetGrid.gridID, tilePos.y, tilePos.x, 2, gm.crosshairTexture)
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            local grid = orderGrid
            local color = ab:GetHighlightColor()

            -- Initial lightning VFX on center tile
            local centerIdx = targetY * CONSTANTS.GRID_COLS + targetX + 1
            local centerRow = math.floor((centerIdx - 1) / CONSTANTS.GRID_COLS)
            local centerCol = (centerIdx - 1) % CONSTANTS.GRID_COLS
            Grid.setTileColorAdd(grid.gridID, centerRow, centerCol, 0, color)
            ab:SpawnVFX("Eye_Ability1_VFX", grid, targetX, targetY, 0.16, 1.5)
            ab:PlayAbilitySound()

            -- Read recast tiles from server result (left and right if tile destroyed)
            local recastTiles = {}
            if ab._currentResult and ab._currentResult.recastTiles then
                recastTiles = ab._currentResult.recastTiles
            end

            -- Separate splash tiles into initial (around center) and recast (around recast positions)
            local initialSplashTiles = {}
            local recastSplashTiles = {}
            if ab._currentResult and ab._currentResult.splashTiles then
                for _, st in ipairs(ab._currentResult.splashTiles) do
                    if st.isRecast then
                        table.insert(recastSplashTiles, st)
                    else
                        table.insert(initialSplashTiles, st)
                    end
                end
            end

            -- Show initial splash highlights immediately (the 8 tiles around center)
            for _, st in ipairs(initialSplashTiles) do
                Grid.setTileColorAdd(grid.gridID, st.y, st.x, 0, color)
            end

            ab._triggerState = {
                phase = "initial",
                timer = 0,
                grid = grid,
                color = color,
                centerRow = centerRow,
                centerCol = centerCol,
                recastTiles = recastTiles,
                initialSplashTiles = initialSplashTiles,
                recastSplashTiles = recastSplashTiles,
                recastIdx = 0,
                recastDelay = outer.recastDelay,
                vfxDuration = outer.triggerVFXDuration
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "initial" then
                if s.timer >= self.triggerDamageInterval then
                    -- Show initial damage numbers (non-recast hits only)
                    ab:ShowDamageNumbers()
                end

                if s.timer >= s.recastDelay then
                    if #s.recastTiles > 0 then
                        s.phase = "recast"
                        s.timer = 0
                        -- Show recast VFX and recast splash highlights
                        for _, rt in ipairs(s.recastTiles) do
                            Grid.setTileColorAdd(s.grid.gridID, rt.y, rt.x, 0, s.color)
                            ab:SpawnVFX("Eye_Ability1_VFX", s.grid, rt.x, rt.y, 0.16, 1.5)
                        end
                        for _, st in ipairs(s.recastSplashTiles) do
                            Grid.setTileColorAdd(s.grid.gridID, st.y, st.x, 0, s.color)
                        end
                        ab:PlayAbilitySound()
                    else
                        s.phase = "vfx"
                        s.timer = 0
                    end
                end

            elseif s.phase == "recast" then
                if s.timer >= s.vfxDuration then
                    -- Clear all highlights
                    Grid.setTileColorAdd(s.grid.gridID, s.centerRow, s.centerCol, 0, CONSTANTS.COLOR_NONE)
                    for _, rt in ipairs(s.recastTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, rt.y, rt.x, 0, CONSTANTS.COLOR_NONE)
                    end
                    for _, st in ipairs(s.initialSplashTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, st.y, st.x, 0, CONSTANTS.COLOR_NONE)
                    end
                    for _, st in ipairs(s.recastSplashTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, st.y, st.x, 0, CONSTANTS.COLOR_NONE)
                    end
                end

                if s.timer >= (s.recastDelay + self.recastTriggerDamageInterval) then
                    -- Show recast floating text and apply deferred damage
                    local gm = ClientGameManager_instance
                    if ab._currentResult and ab._currentResult._recastHits then
                        local recastHits = ab._currentResult._recastHits
                        local recastDmg = ab._currentResult._recastDamage or 0
                        if #recastHits > 0 then
                            Event.emitLocal("OnTilesHit", { hits = recastHits })
                            local gridHits = {}
                            Log.info("Triggering " .. #recastHits .. " hits")
                            for i, hit in ipairs(recastHits) do
                                local hitGrid = hit.isOrderGrid and gm.orderGrid or gm.chaosGrid
                                if hitGrid then
                                    local tileIdx = ClientGrid.GetTileIndex(nil, hit.gridX, hit.gridY)
                                    gridHits[tileIdx] = gridHits[tileIdx] and (gridHits[tileIdx] + 1) or 1
                                    local world = Grid.gridToWorld(hitGrid.gridID, hit.gridX, hit.gridY)
                                    local dmgOffsetX = 0.45
                                    if hit.shielded then
                                        local shieldedDmg = hit.damage or 0
                                        Event.emitLocal("SpawnFloatingText", {
                                            worldX = world.x + dmgOffsetX,
                                            worldY = world.y + gridHits[tileIdx] * 0.5,
                                            damage = shieldedDmg,
                                            text = shieldedDmg > 0 and nil or "BLOCKED"
                                        })
                                    else
                                        Event.emitLocal("SpawnFloatingText", {
                                            worldX = world.x + dmgOffsetX,
                                            worldY = world.y + gridHits[tileIdx] * 0.5,
                                            damage = hit.damage or 0
                                        })
                                    end
                                end
                            end
                            -- Camera shake for recast hits
                            local shakeIntensity = math.min(2.0 + (#recastHits * 1.5) + (recastDmg * 0.1), 8.0)
                            Camera.startShake(shakeIntensity, 0.5)
                        end
                        -- Apply deferred recast damage to health bar
                        if recastDmg > 0 and ab._currentResult.damagePlayer and ab._currentResult.damagePlayer > 0 then
                            gm:ApplyDamage(ab._currentResult.damagePlayer, recastDmg)
                        end
                    end

                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end

            elseif s.phase == "vfx" then
                if s.timer >= s.vfxDuration then
                    Grid.setTileColorAdd(s.grid.gridID, s.centerRow, s.centerCol, 0, CONSTANTS.COLOR_NONE)
                    for _, st in ipairs(s.initialSplashTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, st.y, st.x, 0, CONSTANTS.COLOR_NONE)
                    end
                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end
            end
        end
    end
})

return class
