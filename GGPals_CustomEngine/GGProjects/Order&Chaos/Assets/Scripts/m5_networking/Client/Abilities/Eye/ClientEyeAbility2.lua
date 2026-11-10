local class = DefineType("ClientEyeAbility2",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerEffectDelay = 0.3,
        sfxDelay = 0.0  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.EYE_2
        self.ability.name = "Fractured Earth"
        self.ability.abilityType = CONSTANTS.ABILITY_SUPPORT
        self.ability.cost = 1
        self.ability.numRed = 0
        self.ability.numBlue = 1
        self.ability.unitDamage = 0
        self.ability.tileDamage = 1
        self.ability.ownerFaction = CONSTANTS.CHAOS_PLAYER
        self.ability.targetsOpponent = false

        local outer = self

        -- Find the Eye unit (type 5) position on the player's grid
        local function findEyePosition()
            local gm = ClientGameManager_instance
            if not gm then return nil end
            local grid = gm:GetPlayerGrid()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    if grid.Tiles[index].unit == 5 then
                        return { x = col, y = row, health = grid.Tiles[index].health or CONSTANTS.TILE_MAX_HEALTH }
                    end
                end
            end
            return nil
        end

        self.ability.OnSelect = function(ab, grid, currentAP)
            if not ab:CanUse(grid, currentAP) then return false end
            local pos = findEyePosition()
            if not pos then
                Log.info("[Eye2] Cannot use: Eye unit not found on grid")
                return false
            end
            if pos.health <= 1 then
                Log.info("[Eye2] Cannot use: tile HP too low (" .. tostring(pos.health) .. ")")
                return false
            end
            return true
        end

        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end

        -- Provide Eye unit position for self-target auto-queue
        self.ability.GetSelfTargetParams = function(ab)
            local pos = findEyePosition()
            if pos then
                return { targetX = pos.x, targetY = pos.y, param1 = pos.x, param2 = pos.y }
            end
            return nil
        end

        self.ability.Update = function(ab, targetGrid)
            -- Self-target ability, no targeting UI needed
            local gm = ClientGameManager_instance
            if gm then gm:RevertPreviewedTileDamage() end
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            local grid = chaosGrid
            local color = ab:GetHighlightColor()
            local tileIdx = param2 * CONSTANTS.GRID_COLS + param1 + 1
            local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
            local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
            Grid.setTileColorAdd(grid.gridID, row, col, 0, color)
            ab:PlayAbilitySound()

            ab._triggerState = {
                phase = "wait",
                timer = 0,
                duration = outer.triggerEffectDelay,
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
