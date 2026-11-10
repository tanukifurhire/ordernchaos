local class = DefineType("ClientImpAbility1",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.0001,  -- delay between sequential recast VFX spawns
        triggerVFXDuration = 0.0001,     -- hold time after last recast before clearing highlights
        recastDelay = 0.0001,            -- delay after initial hit before recasts begin
        initialHoldDuration = 0.0001,    -- hold time for initial hit VFX before recasts
        sfxDelay = 0.0001               -- delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.IMP_1
        self.ability.name = "Imp-Pact!"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 2
        self.ability.numRed = 1
        self.ability.numBlue = 1
        self.ability.unitDamage = 4
        self.ability.tileDamage = 3
        self.ability.ownerFaction = CONSTANTS.CHAOS_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        -- 2 vertical tiles targeting
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if targetY + 1 < CONSTANTS.GRID_ROWS then
                table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + targetX + 1)
            end
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

            -- Initial hit VFX on both vertical tiles
            local targetTiles = ab:GetTargetTiles(targetX, targetY)
            local hitTiles = {}
            for _, tileIdx in ipairs(targetTiles) do
                local tRow = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local tCol = (tileIdx - 1) % CONSTANTS.GRID_COLS
                Grid.setTileColorAdd(grid.gridID, tRow, tCol, 0, color)
                ab:SpawnVFX("Imp_Ability1_VFX", grid, tCol, tRow)
                table.insert(hitTiles, { row = tRow, col = tCol })
            end
            local centerRow = targetY
            local centerCol = targetX
            ab:PlayAbilitySound()

            -- Show initial (non-recast) damage numbers only
            ab:ShowDamageNumbers()

            -- Read recast tiles from server result (one per unit hit)
            local recastTiles = {}
            if ab._currentResult and ab._currentResult.recastTiles then
                recastTiles = ab._currentResult.recastTiles
            end

            ab._triggerState = {
                phase = "initial",
                timer = 0,
                grid = grid,
                color = color,
                centerRow = centerRow,
                centerCol = centerCol,
                hitTiles = hitTiles,
                recastTiles = recastTiles,
                recastIdx = 0,
                recastDelay = outer.recastDelay,
                initialHoldDuration = outer.initialHoldDuration,
                vfxDuration = outer.triggerVFXDuration
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "initial" then
                if s.timer >= s.recastDelay then
                    if #s.recastTiles > 0 then
                        s.phase = "recast"
                        s.timer = 0
                        s.recastIdx = 0
                    else
                        s.phase = "vfx"
                        s.timer = 0
                    end
                end

            elseif s.phase == "recast" then
                -- Show recast VFX one at a time, with damage numbers per recast
                if s.recastIdx < #s.recastTiles and s.timer >= outer.triggerDamageInterval then
                    s.recastIdx = s.recastIdx + 1
                    local rt = s.recastTiles[s.recastIdx]
                    Grid.setTileColorAdd(s.grid.gridID, rt.y, rt.x, 0, s.color)
                    ab:SpawnVFX("Imp_Ability1_VFX", s.grid, rt.x, rt.y)
                    ab:PlayAbilitySound()

                    -- Show damage number for this recast tile
                    ab:ShowDamageNumberAt(rt.x, rt.y)

                    -- Camera shake per recast hit
                    Camera.startShake(2.0, 0.3)

                    s.timer = 0
                elseif s.recastIdx >= #s.recastTiles and s.timer >= s.vfxDuration then
                    -- Clear all highlights
                    for _, ht in ipairs(s.hitTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, ht.row, ht.col, 0, CONSTANTS.COLOR_NONE)
                    end
                    for _, rt in ipairs(s.recastTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, rt.y, rt.x, 0, CONSTANTS.COLOR_NONE)
                    end
                    -- Apply deferred recast damage to health bar
                    local gm = ClientGameManager_instance
                    if ab._currentResult and ab._currentResult._recastHits then
                        local recastDmg = ab._currentResult._recastDamage or 0
                        if recastDmg > 0 and ab._currentResult.damagePlayer and ab._currentResult.damagePlayer > 0 then
                            gm:ApplyDamage(ab._currentResult.damagePlayer, recastDmg)
                        end
                    end
                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end

            elseif s.phase == "vfx" then
                if s.timer >= s.vfxDuration then
                    for _, ht in ipairs(s.hitTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, ht.row, ht.col, 0, CONSTANTS.COLOR_NONE)
                    end
                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end
            end
        end
    end
})

return class
