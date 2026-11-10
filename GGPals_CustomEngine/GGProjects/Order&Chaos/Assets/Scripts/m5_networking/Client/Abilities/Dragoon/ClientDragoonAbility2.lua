local class = DefineType("ClientDragoonAbility2",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.1,
        triggerVFXDuration = 0.5,
        featherDelay = 0.3,
        featherFlySpeed = 4.0,
        sfxLaunchDelay = 0.0,  -- Delay before playing feather launch SFX
        sfxHitDelay = 0.0      -- Delay before playing feather hit SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.DRAGOON_2
        self.ability.name = "Feather Cyclone"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 2
        self.ability.numRed = 1
        self.ability.numBlue = 1
        self.ability.unitDamage = 6
        self.ability.tileDamage = 4
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        -- Only center tile for targeting preview
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
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
            local gm = ClientGameManager_instance
            local grid = chaosGrid
            local color = ab:GetHighlightColor()

            -- Center tile highlight + VFX
            local centerIdx = targetY * CONSTANTS.GRID_COLS + targetX + 1
            local centerRow = math.floor((centerIdx - 1) / CONSTANTS.GRID_COLS)
            local centerCol = (centerIdx - 1) % CONSTANTS.GRID_COLS
            Grid.setTileColorAdd(grid.gridID, centerRow, centerCol, 0, color)
            ab:SpawnVFX("Dragoon_Ability2_VFX", grid, targetX, targetY)
            ab:PlayAbilitySound()

            -- Read feather tiles from server result
            local featherTiles = {}
            if ab._currentResult and ab._currentResult.featherTiles then
                featherTiles = ab._currentResult.featherTiles
            end

            -- Read per-feather hit data from server result
            local featherHitData = {}
            if ab._currentResult and ab._currentResult.featherHitData then
                featherHitData = ab._currentResult.featherHitData
            end

            -- Read center hit data and apply immediately
            local centerHitData = nil
            if ab._currentResult and ab._currentResult.centerHitData then
                centerHitData = ab._currentResult.centerHitData
            end
            if centerHitData and #centerHitData.hits > 0 then
                Event.emitLocal("OnTilesHit", { hits = centerHitData.hits })
                for _, hit in ipairs(centerHitData.hits) do
                    local hitGrid = hit.isOrderGrid and orderGrid or chaosGrid
                    if hitGrid then
                        local world = Grid.gridToWorld(hitGrid.gridID, hit.gridX, hit.gridY)
                        Event.emitLocal("SpawnFloatingText", {
                            worldX = world.x + 0.01,
                            worldY = world.y + 0.5,
                            damage = hit.damage or 0
                        })
                    end
                end
                if centerHitData.damage > 0 and gm then
                    local damagePlayer = ab._currentResult.damagePlayer or 0
                    if damagePlayer > 0 then
                        gm:ApplyDamage(damagePlayer, centerHitData.damage)
                    end
                end
                Camera.startShake(2.0, 0.3)
            end

            -- Cache center world position for spawning feathers from center
            local centerWorld = Grid.gridToWorld(grid.gridID, targetX, targetY)

            -- Subscribe to feather landing events for per-feather damage
            ab._featherLandedHandler = Event.subscribe("Dragoon_Ability2_VFX_Feather_Transition", function(params)
                -- Play feather hit SFX
                Event.emitLocal("PlaySFXFeatherHit", {})
                
                local fIdx = params.featherIndex
                local fData = featherHitData[fIdx]
                if fData and #fData.hits > 0 then
                    Event.emitLocal("OnTilesHit", { hits = fData.hits })
                    for _, hit in ipairs(fData.hits) do
                        local hitGrid = hit.isOrderGrid and orderGrid or chaosGrid
                        if hitGrid then
                            local world = Grid.gridToWorld(hitGrid.gridID, hit.gridX, hit.gridY)
                            Event.emitLocal("SpawnFloatingText", {
                                worldX = world.x + 0.01,
                                worldY = world.y + 0.5,
                                damage = hit.damage or 0
                            })
                        end
                    end
                    if fData.damage > 0 and gm then
                        local damagePlayer = ab._currentResult.damagePlayer or 0
                        if damagePlayer > 0 then
                            gm:ApplyDamage(damagePlayer, fData.damage)
                        end
                    end
                    Camera.startShake(1.0, 0.15)
                end
            end)

            ab._triggerState = {
                phase = "center",
                timer = 0,
                grid = grid,
                color = color,
                centerRow = centerRow,
                centerCol = centerCol,
                centerWorldX = centerWorld.x,
                centerWorldY = centerWorld.y,
                featherTiles = featherTiles,
                featherIdx = 0,
                featherDelay = outer.featherDelay,
                vfxDuration = outer.triggerVFXDuration
            }
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "center" then
                if s.timer >= s.featherDelay then
                    s.phase = "feathers"
                    s.timer = 0
                    s.featherIdx = 0
                end

            elseif s.phase == "feathers" then
                -- Spawn feather VFX one at a time from center, flying outward to target
                if s.featherIdx < #s.featherTiles and s.timer >= outer.triggerDamageInterval then
                    s.featherIdx = s.featherIdx + 1
                    local ft = s.featherTiles[s.featherIdx]
                    local fRow = ft.y
                    local fCol = ft.x
                    Grid.setTileColorAdd(s.grid.gridID, fRow, fCol, 0, s.color)

                    -- Spawn feather at center tile position
                    local featherEntity = Prefab.spawn("Dragoon_Ability2_VFX_Feather",
                        s.centerWorldX, s.centerWorldY)
                    
                    -- Play feather launch SFX
                    Event.emitLocal("PlaySFXFeatherLaunch", {})

                    -- Register flight target so feather flies outward with correct rotation
                    local targetWorld = Grid.gridToWorld(s.grid.gridID, fCol, fRow)
                    FeatherTargets_pending = FeatherTargets_pending or {}
                    table.insert(FeatherTargets_pending, {
                        entity = featherEntity,
                        targetX = targetWorld.x,
                        targetY = targetWorld.y,
                        featherIndex = s.featherIdx,
                        flySpeed = outer.featherFlySpeed
                    })

                    s.timer = 0
                elseif s.featherIdx >= #s.featherTiles then
                    s.phase = "vfx"
                    s.timer = 0
                end

            elseif s.phase == "vfx" then
                if s.timer >= s.vfxDuration then
                    -- Clear all highlights
                    Grid.setTileColorAdd(s.grid.gridID, s.centerRow, s.centerCol, 0, CONSTANTS.COLOR_NONE)
                    for _, ft in ipairs(s.featherTiles) do
                        Grid.setTileColorAdd(s.grid.gridID, ft.y, ft.x, 0, CONSTANTS.COLOR_NONE)
                    end

                    -- Unsubscribe feather landing handler
                    if ab._featherLandedHandler and ab._featherLandedHandler ~= 0 then
                        Event.unsubscribe(ab._featherLandedHandler)
                        ab._featherLandedHandler = 0
                    end

                    ab._triggerState = nil
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end
            end
        end
    end
})

return class
