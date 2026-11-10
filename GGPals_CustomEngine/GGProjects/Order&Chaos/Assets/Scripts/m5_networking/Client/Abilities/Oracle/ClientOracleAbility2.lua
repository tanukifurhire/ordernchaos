local class = DefineType("ClientOracleAbility2",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerEffectDelay = 0.5,
        sfxDelay = 0.0,  -- Delay before playing ability SFX
        previewSoundCooldown = 0.5  -- Minimum time between preview sounds
    },

    _lastPreviewSoundTime = 0,

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.ORACLE_2
        self.ability.name = "Fiberglass Finesse"
        self.ability.abilityType = CONSTANTS.ABILITY_SUPPORT
        self.ability.cost = 1
        self.ability.numRed = 0
        self.ability.numBlue = 1
        self.ability.unitDamage = 0
        self.ability.tileDamage = 0
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
        self.ability.targetsOpponent = false
        self.ability.requiresTargeting = true

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

            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    if targetGrid.Tiles[index].occupied then
                        Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, color)
                        Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.borderTexture)
                    else
                        Grid.setTileColorAdd(targetGrid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                        Grid.setTileTexture(targetGrid.gridID, row, col, 2, gm.emptyTexture)
                    end
                end
            end

            -- Crosshair only on hovered tile if it's occupied (valid shield target)
            local hoverIdx = tilePos.y * CONSTANTS.GRID_COLS + tilePos.x + 1
            if targetGrid.Tiles[hoverIdx] and targetGrid.Tiles[hoverIdx].occupied then
                Grid.setTileTexture(targetGrid.gridID, tilePos.y, tilePos.x, 2, gm.crosshairTexture)
                
                -- Play preview sound when hovering over a valid target (with cooldown)
                local currentTime = Time.getTotalTime()
                if currentTime - outer._lastPreviewSoundTime >= outer.previewSoundCooldown then
                    Event.emitLocal("PlayVFXOracleA2", {})
                    outer._lastPreviewSoundTime = currentTime
                end
            end

            gm:PreviewTileDamage(targetGrid, ab:GetTargetTiles(tilePos.x, tilePos.y), ab.tileDamage)
        end

        self.ability.ConfirmTarget = function(ab, gridX, gridY)
            local gm = ClientGameManager_instance
            local grid = gm:GetPlayerGrid()
            local tileIdx = gridY * CONSTANTS.GRID_COLS + gridX + 1
            if grid.Tiles[tileIdx] and grid.Tiles[tileIdx].occupied then
                local shieldedUnit = grid.Tiles[tileIdx].unit or 0
                Log.info("[Oracle2] Shield target confirmed: unit " .. tostring(shieldedUnit) .. " at (" .. gridX .. "," .. gridY .. ")")
                return {
                    targetX = gridX,
                    targetY = gridY,
                    param1 = shieldedUnit,
                    param2 = 0
                }
            else
                Log.info("[Oracle2] No allied unit at (" .. gridX .. "," .. gridY .. ") - cannot shield")
                return nil
            end
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            -- Spawn shield VFX on the shielded unit
            ab:PlayAbilitySound()

            -- Activate the shield VFX that follows the unit around
            if param1 and param1 > 0 then
                Event.emitLocal("ActivateShieldAfterVFX", { unitType = param1 })
            end

            ab._triggerState = {
                phase = "wait",
                timer = 0,
                duration = outer.triggerEffectDelay
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt
            if s.timer >= s.duration then
                ab._triggerState = nil
                Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
            end
        end
    end
})

return class
