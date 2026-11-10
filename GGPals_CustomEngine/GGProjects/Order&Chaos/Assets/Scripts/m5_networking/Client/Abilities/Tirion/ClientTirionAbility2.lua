local class = DefineType("ClientTirionAbility2", 
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
        self.ability.abilityID = CONSTANTS.ABILITIES.TIRION_2
        self.ability.name = "Sigil of Consecration"
        self.ability.abilityType = CONSTANTS.ABILITY_SUPPORT
        self.ability.cost = 2
        self.ability.numRed = 1
        self.ability.numBlue = 1
        self.ability.unitDamage = 8
        self.ability.tileDamage = 0
        self.ability.sideUnitDamage = 1
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
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

            local color = ab:GetHighlightColor()
            -- Dimmer color for the 3x3 AoE radius preview
            local aoEColor = { r = color.r * 0.5, g = color.g * 0.5, b = color.b * 0.5, a = color.a * 0.5 }

            -- Build set of 3x3 AoE tiles around the center (the trap's blast radius)
            local aoeTiles = {}
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local ax = tilePos.x + dx
                    local ay = tilePos.y + dy
                    if ax >= 0 and ax < CONSTANTS.GRID_COLS and ay >= 0 and ay < CONSTANTS.GRID_ROWS then
                        aoeTiles[ay * CONSTANTS.GRID_COLS + ax + 1] = true
                    end
                end
            end

            local centerIdx = tilePos.y * CONSTANTS.GRID_COLS + tilePos.x + 1

            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    if index == centerIdx then
                        -- Center tile: full color
                        Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, color)
                        Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.borderTexture)
                    elseif aoeTiles[index] then
                        -- AoE radius: dimmer color
                        Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, aoEColor)
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

            gm:PreviewTileDamage(targetGrid, { centerIdx }, ab.tileDamage)
        end

        self.ability.ConfirmTarget = function(ab, gridX, gridY)
            local gm = ClientGameManager_instance
            local grid = gm:GetOpponentGrid()
            local tileIdx = gridY * CONSTANTS.GRID_COLS + gridX + 1
            if not grid.Tiles[tileIdx] or not grid.Tiles[tileIdx].health or grid.Tiles[tileIdx].health <= 0 then
                Log.info("[Tirion2] Cannot place trap: tile at (" .. gridX .. "," .. gridY .. ") is destroyed")
                return nil
            end
            if grid.Tiles[tileIdx].trap then
                Log.info("[Tirion2] Cannot place trap: tile at (" .. gridX .. "," .. gridY .. ") already has a trap")
                return nil
            end
            return { targetX = gridX, targetY = gridY, param1 = 0, param2 = 0 }
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            local grid = chaosGrid
            local color = ab:GetHighlightColor()
            local tileIdx = targetY * CONSTANTS.GRID_COLS + targetX + 1
            local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
            local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
            -- Only show placement highlight for local player's traps
            local gm = ClientGameManager_instance
            if gm and gm.playerFaction == CONSTANTS.ORDER_PLAYER then
                Grid.setTileColorAdd(grid.gridID, row, col, 0, color)
            end
            ab._spawnedTrapVFX = ab:SpawnVFX("Tirion_Ability2_VFX", chaosGrid, targetX, targetY, 0.01, 0.0)
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
