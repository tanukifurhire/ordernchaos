local class = DefineType("ClientVizomAbility1",
{
    Contains =
    {
        ability = "ClientAbility"
    },

    Serializable = {
        triggerDamageInterval = 0.1,
        triggerVFXDuration = 0.5,
        bounceDelay = 0.3,
        dashToDelay = 0.0001,
        hitDelay = 0.4,
        dashBackDelay = 0.0001,
        sfxDelay = 0.0001  -- Delay before playing ability SFX
    },

    Init = function(self, entity)
        self.ability.abilityID = CONSTANTS.ABILITIES.VIZOM_1
        self.ability.name = "Umbral Cleave"
        self.ability.abilityType = CONSTANTS.ABILITY_OFFENSIVE
        self.ability.cost = 2
        self.ability.numRed = 2
        self.ability.numBlue = 0
        self.ability.unitDamage = 4
        self.ability.tileDamage = 2
        self.ability.ownerFaction = CONSTANTS.CHAOS_PLAYER
        self.ability.targetsOpponent = true

        local outer = self

        -- Single tile (Berserk bounces are resolved server-side)
        self.ability.GetTargetTiles = function(ab, targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
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
            local targetGrid = orderGrid  -- Vizom targets Order grid
            local ownGrid = chaosGrid     -- Vizom's own grid (Chaos)
            local color = ab:GetHighlightColor()
            local casterUnit = CONSTANTS.UNIT.VIZOM

            -- Get berserk bounces from server result
            local bounces = {}
            if ab._currentResult and ab._currentResult.berserkBounces then
                for _, b in ipairs(ab._currentResult.berserkBounces) do
                    table.insert(bounces, { x = b.x, y = b.y })
                end
            end

            -- Get caster's CURRENT position on own grid (scan tile occupancy, not syncPositions
            -- which are start-of-resolution and stale if the unit already moved)
            local fromX, fromY = 0, 0
            for i, tile in ipairs(ownGrid.Tiles) do
                if tile.unit == casterUnit then
                    fromY = math.floor((i - 1) / CONSTANTS.GRID_COLS)
                    fromX = (i - 1) % CONSTANTS.GRID_COLS
                    break
                end
            end

            -- Calculate world positions
            local targetWorld = Grid.gridToWorld(targetGrid.gridID, targetX, targetY)
            local homeWorld = Grid.gridToWorld(ownGrid.gridID, fromX, fromY)

            -- Subscribe to dash completion
            ab._dashPhase = "dash_to"
            ab._dashHandler = Event.subscribe("OnDashComplete", function(params)
                if params.unitType == casterUnit and (params.ownerPlayer == nil or params.ownerPlayer == CONSTANTS.CHAOS_PLAYER) then
                    if ab._dashPhase == "dash_to" or ab._dashPhase == "dash_bounce" then
                        ab._dashPhase = "arrived"
                    elseif ab._dashPhase == "dash_back" then
                        if ab._dashHandler and ab._dashHandler ~= 0 then
                            Event.unsubscribe(ab._dashHandler)
                            ab._dashHandler = 0
                        end
                        ab._dashPhase = nil
                    end
                end
            end)

            ab._triggerState = {
                phase = "pre_dash",
                targetWorldX = targetWorld.x + 0.01,
                targetWorldY = targetWorld.y + 0.5,
                timer = 0,
                grid = targetGrid,
                ownGrid = ownGrid,
                color = color,
                targetX = targetX,
                targetY = targetY,
                bounces = bounces,
                currentBounce = 0,
                highlightedTiles = {},
                bounceDelay = outer.bounceDelay,
                vfxDuration = outer.triggerVFXDuration,
                dashToDelay = outer.dashToDelay,
                hitDelay = outer.hitDelay,
                dashBackDelay = outer.dashBackDelay,
                homeWorldX = homeWorld.x + 0.01,
                homeWorldY = homeWorld.y + 0.5,
                casterUnit = casterUnit,
                ownerPlayer = CONSTANTS.CHAOS_PLAYER
            }
        end

        self.ability.TriggerPlayback = function(ab, orderGrid, chaosGrid, targetX, targetY, param1, param2, casterFaction)
            -- trigger vizom attack animation
            Event.emitLocal("OnUnitSupportCast", {
                unitType = CONSTANTS.UNIT.VIZOM,
                animName = "Ability1"
            })
            ab:Trigger(orderGrid, chaosGrid, targetX, targetY, param1, param2)
        end

        self.ability.UpdateTrigger = function(ab, dt)
            local s = ab._triggerState
            if not s then return end

            s.timer = s.timer + dt

            if s.phase == "pre_dash" then
                -- Wait for dashToDelay (time between animation start and teleport)
                if s.timer >= s.dashToDelay then
                    s.phase = "dash_to"
                    s.timer = 0
                    ab._dashPhase = "dash_to"
                    Event.emitLocal("DashUnitToWorld", {
                        unitType = s.casterUnit,
                        ownerPlayer = s.ownerPlayer,
                        worldX = s.targetWorldX,
                        worldY = s.targetWorldY
                    })
                end

            elseif s.phase == "dash_to" then
                -- Wait for teleport to arrive
                if ab._dashPhase == "arrived" or s.timer >= 3.0 then
                    s.phase = "vfx_wait"
                    s.timer = 0
                    s._pendingX = s.targetX
                    s._pendingY = s.targetY
                end

            elseif s.phase == "vfx_wait" then
                -- Delay before showing hit after arriving at a tile
                if s.timer >= s.hitDelay then
                    s.phase = "strike"
                    s.timer = 0

                    -- Strike the tile Vizom is standing on
                    local tileIdx = s._pendingY * CONSTANTS.GRID_COLS + s._pendingX + 1
                    local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                    local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                    Grid.setTileColorAdd(s.grid.gridID, row, col, 0, s.color)
                    table.insert(s.highlightedTiles, tileIdx)

                    -- Show damage number for this specific tile
                    ab:ShowDamageNumberAt(s._pendingX, s._pendingY)
                    ab:PlayAbilitySound()
                end

            elseif s.phase == "strike" then
                -- Wait after striking, then start next bounce or finish
                if s.timer >= s.bounceDelay then
                    s.currentBounce = s.currentBounce + 1
                    if s.currentBounce <= #s.bounces then
                        -- Transition to bounce_anim which handles Idle → Ability1
                        s.phase = "bounce_anim"
                        s.timer = 0
                    else
                        -- No more bounces, hold VFX briefly
                        s.phase = "vfx"
                        s.timer = 0
                    end
                end

            elseif s.phase == "bounce_anim" then
                -- First transition to Idle so the animation controller can reset
                if not s._idleSet then
                    s._idleSet = true
                    Event.emitLocal("OnUnitSupportCast", {
                        unitType = s.casterUnit,
                        animName = "Idle"
                    })
                end
                -- Wait bounceDelay for animation controller to process Idle, then trigger Ability1
                if s.timer >= s.bounceDelay and not s._attackSet then
                    s._attackSet = true
                    Event.emitLocal("OnUnitSupportCast", {
                        unitType = s.casterUnit,
                        animName = "Ability1"
                    })
                end
                -- Wait dashToDelay after attack anim starts, then dash
                if s._attackSet and s.timer >= s.bounceDelay + s.dashToDelay then
                    s.phase = "dash_bounce"
                    s.timer = 0
                    s._idleSet = nil
                    s._attackSet = nil
                    local bounce = s.bounces[s.currentBounce]
                    local bounceWorld = Grid.gridToWorld(s.grid.gridID, bounce.x, bounce.y)
                    ab._dashPhase = "dash_bounce"
                    Event.emitLocal("DashUnitToWorld", {
                        unitType = s.casterUnit,
                        ownerPlayer = s.ownerPlayer,
                        worldX = bounceWorld.x + 0.01,
                        worldY = bounceWorld.y + 0.5
                    })
                end

            elseif s.phase == "dash_bounce" then
                -- Wait for dash to bounce tile to complete
                if ab._dashPhase == "arrived" or s.timer >= 3.0 then
                    s.phase = "vfx_wait"
                    s.timer = 0
                    local bounce = s.bounces[s.currentBounce]
                    s._pendingX = bounce.x
                    s._pendingY = bounce.y
                end

            elseif s.phase == "vfx" then
                if s.timer >= s.vfxDuration then
                    -- Clear all highlighted tiles
                    for _, tileIdx in ipairs(s.highlightedTiles) do
                        local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                        local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                        Grid.setTileColorAdd(s.grid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                    end
                    -- Dash back to own grid
                    s.phase = "dash_back"
                    s.timer = 0
                    ab._dashPhase = "dash_back"
                    Event.emitLocal("DashUnitToWorld", {
                        unitType = s.casterUnit,
                        ownerPlayer = s.ownerPlayer,
                        worldX = s.homeWorldX,
                        worldY = s.homeWorldY
                    })
                end

            elseif s.phase == "dash_back" then
                -- Wait for dash back to complete + optional delay
                if (ab._dashPhase == nil or s.timer >= 3.0) and s.timer >= s.dashBackDelay then
                    -- Force Vizom back to home position as safety net
                    Event.emitLocal("DashUnitToWorld", {
                        unitType = s.casterUnit,
                        ownerPlayer = s.ownerPlayer,
                        worldX = s.homeWorldX,
                        worldY = s.homeWorldY
                    })
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
