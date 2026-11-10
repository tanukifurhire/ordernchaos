local class = DefineType("SapphireSwirl",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.1,
        triggerVFXDuration = 0.5
    },

    isVertical = false,
    rotateHandler = 0,

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.GENERIC_2
        self.ability.name = "Sapphire Swirl"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 1
        self.ability.numRed = 0
        self.ability.numBlue = 1
        self.ability.unitDamage = 6
        self.ability.tileDamage = 3
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        -- 2 tiles: horizontal (center + right) or vertical (center + down)
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if outer.isVertical then
                if targetY + 1 < CONSTANTS.GRID_ROWS then
                    table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + targetX + 1)
                end
            else
                if targetX + 1 < CONSTANTS.GRID_COLS then
                    table.insert(tiles, targetY * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
                end
            end
            return tiles
        end

        self.ability.OnSelect = function(ab, grid, currentAP)
            if not ab:CanUse(grid, currentAP) then return false end
            outer.isVertical = false
            -- Subscribe to R key for rotation while targeting
            if outer.rotateHandler ~= 0 then
                Event.unsubscribe(outer.rotateHandler)
            end
            outer.rotateHandler = Event.subscribe("OnRotateAbility", function()
                outer.isVertical = not outer.isVertical
                Log.info("[SapphireSwirl] Direction: " .. (outer.isVertical and "vertical" or "horizontal"))
            end)
            return true
        end

        self.ability.Update = function(ab, targetGrid)
            -- Check R key for rotation
            if Input.isKeyTriggered(82) then
                Event.emitLocal("OnRotateAbility", {})
            end

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

        -- ConfirmTarget: store direction in param1 (0=horizontal, 1=vertical)
        self.ability.ConfirmTarget = function(ab, targetX, targetY)
            -- Unsubscribe rotation handler
            if outer.rotateHandler ~= 0 then
                Event.unsubscribe(outer.rotateHandler)
                outer.rotateHandler = 0
            end
            return {
                targetX = targetX,
                targetY = targetY,
                param1 = outer.isVertical and 1 or 0,
                param2 = 0
            }
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            -- Restore direction from param1 for resolution playback
            outer.isVertical = (param1 == 1)
            -- Determine target grid from caster faction
            local casterFaction = ab._currentResult and ab._currentResult.action.faction or CONSTANTS.ORDER_PLAYER
            local targetGrid = (casterFaction == CONSTANTS.ORDER_PLAYER) and chaosGrid or orderGrid
            local targetTiles = ab:GetTargetTiles(targetX, targetY)
            ab:StartStandardTrigger(targetGrid, targetTiles, ab:GetHighlightColor(), outer.triggerDamageInterval, outer.triggerVFXDuration)
            for _, tileIdx in ipairs(targetTiles) do
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                ab:SpawnVFX("SapphireCrescent_VFX", targetGrid, col, row)
            end
            ab:PlayAbilitySound()
        end

        self.ability.TriggerPlayback = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2, casterFaction)
            outer.isVertical = (param1 == 1)
            local targetGrid = (casterFaction == CONSTANTS.ORDER_PLAYER) and chaosGrid or orderGrid
            local targetTiles = ab:GetTargetTiles(targetX, targetY)
            ab:StartStandardTrigger(targetGrid, targetTiles, ab:GetHighlightColor(), outer.triggerDamageInterval, outer.triggerVFXDuration)
            for _, tileIdx in ipairs(targetTiles) do
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                ab:SpawnVFX("SapphireCrescent_VFX", targetGrid, col, row)
            end
            ab:PlayAbilitySound()
        end
    end
})

return class
