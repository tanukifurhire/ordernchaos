local class = DefineType("ClientOracleAbility1",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.15,
        triggerVFXDuration = 0.5,
        sfxDelay = 0.0  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.ORACLE_1
        self.ability.name = "Crystal Cascade"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 3
        self.ability.numRed = 0
        self.ability.numBlue = 3
        self.ability.unitDamage = 4
        self.ability.tileDamage = 3
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
        self.ability.targetsOpponent = true
        self.ability.isAutoTarget = true

        local outer = self

        -- Server generates random tiles; client reads from result
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            return {}
        end

        self.ability.GetSelfTargetParams = function(ab)
            return { targetX = 0, targetY = 0, param1 = 0, param2 = 0 }
        end

        self.ability.Trigger = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2)
            local grid = chaosGrid
            local color = ab:GetHighlightColor()
            local randomTiles = {}

            -- Read server-generated random tiles from result
            if ab._currentResult and ab._currentResult.randomTiles then
                randomTiles = ab._currentResult.randomTiles
            end

            if #randomTiles == 0 then
                Log.warning("[Oracle1] No random tiles in result, falling back")
                ab._triggerState = nil
                Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                return
            end

            -- Highlight + VFX for first tile immediately
            local firstTile = randomTiles[1]
            local firstRow = firstTile.y
            local firstCol = firstTile.x
            Grid.setTileColorAdd(grid.gridID, firstRow, firstCol, 0, color)
            ab:SpawnVFX("Oracle_Ability1_VFX", grid, firstCol, firstRow)
            ab:PlayAbilitySound()
            
            -- Show damage numbers at impact
            ab:ShowDamageNumbers()

            ab._triggerState = {
                phase = "spawning",
                timer = 0,
                grid = grid,
                color = color,
                randomTiles = randomTiles,
                currentIdx = 1,
                interval = outer.triggerDamageInterval,
                vfxDuration = outer.triggerVFXDuration
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "spawning" then
                if s.currentIdx < #s.randomTiles and s.timer >= s.interval then
                    s.currentIdx = s.currentIdx + 1
                    local tile = s.randomTiles[s.currentIdx]
                    Grid.setTileColorAdd(s.grid.gridID, tile.y, tile.x, 0, s.color)
                    ab:SpawnVFX("Oracle_Ability1_VFX", s.grid, tile.x, tile.y)
                    s.timer = 0
                elseif s.currentIdx >= #s.randomTiles then
                    s.phase = "vfx"
                    s.timer = 0
                end

            elseif s.phase == "vfx" then
                if s.timer >= s.vfxDuration then
                    for _, tile in ipairs(s.randomTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, tile.y, tile.x, 0, CONSTANTS.COLOR_NONE)
                    end
                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end
            end
        end
    end
})

return class
