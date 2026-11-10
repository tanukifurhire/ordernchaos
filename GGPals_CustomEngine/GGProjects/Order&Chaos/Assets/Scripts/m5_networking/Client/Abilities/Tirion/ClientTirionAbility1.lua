local class = DefineType("ClientTirionAbility1",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.1,
        triggerVFXDuration = 0.5,
        dashToDelay = 0.0001,
        hitDelay = 0.4,
        dashBackDelay = 0.0001,
        sfxDelay = 0.0  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.TIRION_1
        self.ability.name = "Luminous Cleave"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 2
        self.ability.numRed = 2
        self.ability.numBlue = 0
        self.ability.unitDamage = 5
        self.ability.tileDamage = 2
        self.ability.ownerFaction = CONSTANTS.ORDER_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        local function GetFervorStacksFromClientState()
            local gm = ClientGameManager_instance
            if not gm then return 0 end

            if gm.buffStacks and gm.buffStacks.fervor ~= nil then
                return gm.buffStacks.fervor
            end

            -- Prefer persistent stack snapshot from server for planning-phase targeting.
            local data = gm.resolutionData
            if data and data.buffStacks and data.buffStacks.fervor ~= nil then
                return data.buffStacks.fervor
            end

            -- PnP fallback: read authoritative server state directly.
            if PassAndPlayServer_instance
                and PassAndPlayServer_instance.gameData
                and PassAndPlayServer_instance.gameData.buffs
                and PassAndPlayServer_instance.gameData.buffs.fervor then
                return PassAndPlayServer_instance.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] or 0
            end

            -- Final fallback to tracked status icon presence (not stacked).
            if gm.unitStatuses
                and gm.unitStatuses[CONSTANTS.UNIT.TIRION]
                and gm.unitStatuses[CONSTANTS.UNIT.TIRION][CONSTANTS.ABILITIES.TIRION_1] then
                return 1
            end

            return 0
        end

        -- Horizontal line: center + right, upgraded to 2x2 at 3+ Fervor
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            local fervorStacks = GetFervorStacksFromClientState()
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if fervorStacks >= 3 then
                if targetX + 1 < CONSTANTS.GRID_COLS then
                    table.insert(tiles, targetY * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
                end
                if targetY + 1 < CONSTANTS.GRID_ROWS then
                    table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + targetX + 1)
                end
                if targetX + 1 < CONSTANTS.GRID_COLS and targetY + 1 < CONSTANTS.GRID_ROWS then
                    table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
                end
            elseif targetX + 1 < CONSTANTS.GRID_COLS then
                table.insert(tiles, targetY * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
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
            local gm = ClientGameManager_instance
            local targetGrid = chaosGrid  -- Tirion targets Chaos grid
            local ownGrid = orderGrid     -- Tirion's own grid (Order)
            local casterUnit = CONSTANTS.UNIT.TIRION

            -- Get target tiles for the attack
            local targetTiles = ab:GetTargetTiles(targetX, targetY)

            -- Define all Tirion instances: main (1) and copies (4, 5, 6)
            local allTirions = { 1, 4, 5, 6 }
            local activeTirions = {}

            -- Assign each Tirion to a tile (up to the number of target tiles)
            for i = 1, math.min(#allTirions, #targetTiles) do
                local tileIdx = targetTiles[i]
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                local targetWorld = Grid.gridToWorld(targetGrid.gridID, col, row)
                
                table.insert(activeTirions, {
                    ownerPlayer = allTirions[i],
                    tileIdx = tileIdx,
                    targetWorldX = targetWorld.x + 0.01,
                    targetWorldY = targetWorld.y + 0.5,
                    arrived = false
                })
            end

            -- Get main Tirion's home position (for all copies to return to same spot)
            local fromX, fromY = 0, 0
            for i, tile in ipairs(ownGrid.Tiles) do
                if tile.unit == casterUnit then
                    fromY = math.floor((i - 1) / CONSTANTS.GRID_COLS)
                    fromX = (i - 1) % CONSTANTS.GRID_COLS
                    break
                end
            end
            local homeWorld = Grid.gridToWorld(ownGrid.gridID, fromX, fromY)

            -- Track how many Tirions have completed each phase
            ab._arrivedCount = 0
            ab._returnedCount = 0

            -- Subscribe to dash completion for all Tirions
            ab._dashPhase = nil
            ab._dashHandler = Event.subscribe("OnDashComplete", function(params)
                if params.unitType == casterUnit then
                    -- Find the Tirion that completed
                    for _, t in ipairs(activeTirions) do
                        if params.ownerPlayer == t.ownerPlayer then
                            if ab._dashPhase == "dash_to" and not t.arrived then
                                t.arrived = true
                                ab._arrivedCount = ab._arrivedCount + 1
                            elseif ab._dashPhase == "dash_back" and not t.returned then
                                t.returned = true
                                ab._returnedCount = ab._returnedCount + 1
                            end
                            break
                        end
                    end
                end
            end)

            ab._triggerState = {
                phase = "pre_dash",
                timer = 0,
                grid = targetGrid,
                ownGrid = ownGrid,
                color = ab:GetHighlightColor(),
                targetX = targetX,
                targetY = targetY,
                tiles = targetTiles,
                activeTirions = activeTirions,
                vfxDuration = outer.triggerVFXDuration,
                damageInterval = outer.triggerDamageInterval,
                dashToDelay = outer.dashToDelay,
                hitDelay = outer.hitDelay,
                dashBackDelay = outer.dashBackDelay,
                homeWorldX = homeWorld.x + 0.01,
                homeWorldY = homeWorld.y + 0.5,
                casterUnit = casterUnit
            }
        end

        self.ability.TriggerPlayback = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2, casterFaction)
            -- trigger tirion attack animation
            Event.emitLocal("OnUnitSupportCast", {
                unitType = CONSTANTS.UNIT.TIRION,
                animName = "Ability1"
            })
            ab:Trigger(orderGrid, chaosGrid, targetX, targetY, param1, param2)
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "pre_dash" then
                -- Wait for dashToDelay, then dash ALL Tirions simultaneously
                if s.timer >= s.dashToDelay then
                    s.phase = "dash_to"
                    s.timer = 0
                    ab._dashPhase = "dash_to"
                    ab._arrivedCount = 0

                    -- Dash each Tirion to their assigned tile
                    for _, t in ipairs(s.activeTirions) do
                        Event.emitLocal("DashUnitToWorld", {
                            unitType = s.casterUnit,
                            ownerPlayer = t.ownerPlayer,
                            worldX = t.targetWorldX,
                            worldY = t.targetWorldY
                        })
                    end
                end

            elseif s.phase == "dash_to" then
                -- Wait for ALL Tirions to arrive
                if ab._arrivedCount >= #s.activeTirions or s.timer >= 3.0 then
                    s.phase = "vfx_wait"
                    s.timer = 0
                end

            elseif s.phase == "vfx_wait" then
                -- Wait for hitDelay before showing attack effects
                if s.timer >= s.hitDelay then
                    s.phase = "highlight"
                    s.timer = 0

                    -- Highlight ALL tiles simultaneously
                    for _, tileIdx in ipairs(s.tiles) do
                        local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                        local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                        Grid.setTileColorAdd(s.grid.gridID, row, col, 0, s.color)
                    end

                    -- Show damage numbers at the hit moment
                    ab:ShowDamageNumbers()
                    ab:PlayAbilitySound()
                end

            elseif s.phase == "highlight" then
                -- Brief pause for visual effect
                if s.timer >= s.vfxDuration then
                    s.phase = "vfx"
                    s.timer = 0
                end

            elseif s.phase == "vfx" then
                -- Clear highlights and dash ALL Tirions back
                if s.timer >= 0 then
                    -- Clear all tile highlights
                    for _, tileIdx in ipairs(s.tiles) do
                        local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                        local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                        Grid.setTileColorAdd(s.grid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                    end

                    s.phase = "dash_back"
                    s.timer = 0
                    ab._dashPhase = "dash_back"
                    ab._returnedCount = 0

                    -- Dash Tirions back: main to home, copies to offscreen (500, 0)
                    for _, t in ipairs(s.activeTirions) do
                        t.returned = false
                        local returnX, returnY
                        if t.ownerPlayer == 1 then
                            -- Main Tirion returns to home position
                            returnX = s.homeWorldX
                            returnY = s.homeWorldY
                        else
                            -- Copies return to offscreen position
                            returnX = 500
                            returnY = 0
                        end
                        Event.emitLocal("DashUnitToWorld", {
                            unitType = s.casterUnit,
                            ownerPlayer = t.ownerPlayer,
                            worldX = returnX,
                            worldY = returnY
                        })
                    end
                end

            elseif s.phase == "dash_back" then
                -- Wait for ALL Tirions to return
                if (ab._returnedCount >= #s.activeTirions or s.timer >= 3.0) and s.timer >= s.dashBackDelay then
                    ab._triggerState = nil
                    if ab._dashHandler and ab._dashHandler ~= 0 then
                        Event.unsubscribe(ab._dashHandler)
                        ab._dashHandler = 0
                    end
                    Event.emitLocal("OnAbilityTriggerComplete", { abilityID = ab.abilityID })
                end
            end
        end
    end
})

return class
