local class = DefineType("ClientDragoonAbility1",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.1,
        triggerVFXDuration = 0.5,
        sfxDelay = 0.0  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.DRAGOON_1
        self.ability.name = "Azure Blaze"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 1
        self.ability.numRed = 1
        self.ability.numBlue = 0
        self.ability.unitDamage = 4
        self.ability.tileDamage = 3
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        -- Vertical line: center + up + down
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if targetY + 1 < CONSTANTS.GRID_ROWS then
                table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + targetX + 1)
            end
            if targetY - 1 >= 0 then
                table.insert(tiles, (targetY - 1) * CONSTANTS.GRID_COLS + targetX + 1)
            end
            return tiles
        end

        self.ability.Update = function(ab, targetGrid)
            local gm = ClientGameManager_instance
            local tilePos = targetGrid:GetHoveredTilePos()

            -- -- revert all previously previewed tile damages
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
            local targetTiles = ab:GetTargetTiles(targetX, targetY)
            ab:StartStandardTrigger(chaosGrid, targetTiles, ab:GetHighlightColor(), outer.triggerDamageInterval, outer.triggerVFXDuration)
            for _, tileIdx in ipairs(targetTiles) do
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                ab:SpawnVFX("Dragoon_Ability1_VFX", chaosGrid, col, row)
            end
            ab:PlayAbilitySound()
        end
    end
})

return class
