local class = DefineType("ClientVizomAbility2",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerPlacementDelay = 0.2,
        sfxDelay = 0.0  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.VIZOM_2
        self.ability.name = "Sigil of Malevolence"
        self.ability.abilityType = CONSTANTS.ABILITY_SUPPORT
        self.ability.cost = 2
        self.ability.numRed = 1
        self.ability.numBlue = 1
        self.ability.unitDamage = 5
        self.ability.tileDamage = 0
        self.ability.bleedPerTileMoved = 1
        self.ability.ownerFaction = CONSTANTS.CHAOS_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end

        self.ability.Update = function(ab, targetGrid)
            local gm = ClientGameManager_instance
            local tilePos = targetGrid:GetHoveredTilePos()
            if not tilePos then
                gm:RevertPreviewedTileDamage()
                return
            end

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
                        Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, color)
                        Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.borderTexture)
                    else
                        Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                        Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.emptyTexture)
                    end
                end
            end
            -- Only show crosshair on tiles with health > 0
            local hoverIdx = tilePos.y * CONSTANTS.GRID_COLS + tilePos.x + 1
            if targetGrid.Tiles[hoverIdx] and targetGrid.Tiles[hoverIdx].health and targetGrid.Tiles[hoverIdx].health > 0 then
                Grid.setTileTexture(targetGrid.gridID, tilePos.y, tilePos.x, 2, gm.crosshairTexture)
            end

            gm:PreviewTileDamage(targetGrid, targetTiles, ab.tileDamage)
        end

        self.ability.ConfirmTarget = function(ab, gridX, gridY)
            local gm = ClientGameManager_instance
            local grid = gm:GetOpponentGrid()
            local tileIdx = gridY * CONSTANTS.GRID_COLS + gridX + 1
            if not grid.Tiles[tileIdx] or not grid.Tiles[tileIdx].health or grid.Tiles[tileIdx].health <= 0 then
                Log.info("[Vizom2] Cannot place trap: tile at (" .. gridX .. "," .. gridY .. ") is destroyed")
                return nil
            end
            if grid.Tiles[tileIdx].trap then
                Log.info("[Vizom2] Cannot place trap: tile at (" .. gridX .. "," .. gridY .. ") already has a trap")
                return nil
            end
            return { targetX = gridX, targetY = gridY, param1 = 0, param2 = 0 }
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            local grid = orderGrid
            local color = ab:GetHighlightColor()
            local tileIdx = targetY * CONSTANTS.GRID_COLS + targetX + 1
            local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
            local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
            -- Only show placement highlight for local player's traps
            local gm = ClientGameManager_instance
            if gm and gm.playerFaction == CONSTANTS.CHAOS_PLAYER then
                Grid.setTileColorAdd(grid.gridID, row, col, 0, color)
            end
            ab._spawnedTrapVFX = ab:SpawnVFX("Vizom_Ability2_VFX", orderGrid, targetX, targetY, 0.01, 0.0)
            ab:PlayAbilitySound()

            ab._triggerState = {
                phase = "wait",
                timer = 0,
                duration = outer.triggerPlacementDelay,
                grid = grid,
                row = row,
                col = col
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt
            if s.timer >= s.duration then
                Grid.setTileColorAdd(s.grid.gridID, s.row, s.col, 0, CONSTANTS.COLOR_NONE)
                ab._triggerState = nil
                Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
            end
        end
    end
})

return class
