local class = DefineType("ClientProjectedMoveState",
{
    stateMachine = nil,

    Serializable =
    {
        moveEndDelay = 0.3,
        consChainDelay = 0.3
    },

    isEnding = false,
    endTimer = 0,

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)
        self.isEnding = false
        self.endTimer = 0
        local unit = self.stateMachine.unit
        -- Clear stale chain state only when a brand-new projected movement starts.
        -- When re-entering from hitState during the same movement, preserve chain progress.
        if (not unit.movementPath) or unit.currentPathIndex <= 1 then
            self._consChain = nil
        end

        unit:PlayAnimation(entity, "MoveLoop")
    end,

    Exit = function(self, entity)
    end,

    HandleInput = function(self, entity)
    end,

    OnClick = function(self, entity)
    end,

    OnDrop = function(self, entity)
    end,

    Update = function(self, entity)
        local unit = self.stateMachine.unit

        -- Process sequential Consecration chain detonation
        if self._consChain then
            self._consChain.timer = self._consChain.timer + Time.getDeltaTime()
            if self._consChain.timer >= self.consChainDelay then
                self._consChain.timer = 0
                self:_ProcessNextConsDetonation(entity)
            end
            return
        end

        if self.isEnding then
            self.endTimer = self.endTimer - Time.getDeltaTime()
            if self.endTimer <= 0 then
                self.stateMachine:ChangeState(entity, self.stateMachine.states.readyState)
            end
            return
        end

        if not unit.movementPath or unit.currentPathIndex > unit.totalPathIndex then
            self:FinishMovement(entity)
            return
        end

        local transform = ECS.getTransform(entity)
        local dx = unit.nextPathWorldX - transform.x
        local dy = unit.nextPathWorldY - transform.y
        local dist = math.sqrt(dx * dx + dy * dy)

        if dist < CONSTANTS.EPSILON then
            transform.x = unit.nextPathWorldX
            transform.y = unit.nextPathWorldY
            ECS.setTransform(entity, transform)

            -- Save arrived tile before advancing the path index
            local arrivedTile = unit.movementPath[unit.currentPathIndex]

            -- Advance path index and set up next waypoint first,
            -- so if a trap triggers hitState we resume on the correct tile
            unit.currentPathIndex = unit.currentPathIndex + 1

            if unit.currentPathIndex <= unit.totalPathIndex then
                local tile = unit.movementPath[unit.currentPathIndex]
                local isOrder = (unit.ownerPlayer == 1)
                local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

                local worldPos = Grid.gridToWorld(grid.gridID, tile.x, tile.y)
                if worldPos then
                    unit.nextPathWorldX = worldPos.x + unit.gridOffsetX
                    unit.nextPathWorldY = worldPos.y + unit.gridOffsetY
                end
            end

            -- Check for armed enemy trap at the tile the unit just reached
            if arrivedTile then
                self:CheckTrapAtTile(entity, unit, arrivedTile.x, arrivedTile.y)
            end

            -- If trap triggered a state change (e.g. hitState), stop here;
            -- movement resumes from the next tile when we re-enter
            if self.stateMachine.currentState ~= self then return end

            -- If no more tiles, finish movement
            if unit.currentPathIndex > unit.totalPathIndex then
                self:FinishMovement(entity)
            end
            return
        end

        dx = dx / dist
        dy = dy / dist

        local step = CONSTANTS.MOVE_SPEED_NORMAL * Time.getDeltaTime()
        transform.x = transform.x + dx * math.min(step, dist)
        transform.y = transform.y + dy * math.min(step, dist)
        ECS.setTransform(entity, transform)
    end,

    CheckTrapAtTile = function(self, entity, unit, gridX, gridY)
        local gm = ClientGameManager_instance
        if not gm then return end

        local isOrder = (unit.ownerPlayer == 1)
        local grid = isOrder and gm.orderGrid or gm.chaosGrid
        local tileIdx = grid:GetTileIndex(gridX, gridY)
        if tileIdx <= 0 then return end

        local tile = grid.Tiles[tileIdx]
        -- Don't check armed: during prep preview, last-turn traps are unarmed
        -- but ArmExistingTraps() will arm them before resolution processes movement
        if not tile or not tile.trap then return end

        -- Only trigger on enemy traps (trap owner != unit owner)
        local unitFaction = isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
        if tile.trap.owner == unitFaction then return end

        -- One telegraph per trap per planning phase. The trap object is left on
        -- the grid on purpose so resolution still sees it, which means nothing
        -- stops CheckTrapAtTile firing again the next time any unit paths over
        -- the tile. At resolution the first unit to arrive consumes the trap
        -- (the server nils it out), so every later preview was inventing damage.
        if gm:HasPreviewedTrap(grid.gridID, tileIdx) then
            return
        end
        gm:MarkPreviewedTrap(grid.gridID, tileIdx)

        -- Prefer explicit trap type flags so preview works with custom arsenals
        -- where either faction can place either trap type.
        local isBleedTrap = (tile.trap.isBleedTrap == true)
        local isConsTrap = (tile.trap.isConsTrap == true)
        if not isBleedTrap and not isConsTrap then
            -- Backward-compat fallback for older trap records missing type flags.
            isBleedTrap = (tile.trap.owner == CONSTANTS.CHAOS_PLAYER)
            isConsTrap = not isBleedTrap
        end
        local vfxPrefab = isBleedTrap and "Vizom_Ability2_VFX" or "Tirion_Ability2_VFX"

        -- Apply tileXCorrection for trap VFX positioning
        local tileXCorrection = {
            [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
            [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
            [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
            [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
        }
        local xCorrection = tileXCorrection[gridY] and tileXCorrection[gridY][gridX] or 0

        -- Spawn temporary VFX for the preview trigger (original stays intact for resolution)
        local world = Grid.gridToWorld(grid.gridID, gridX, gridY)
        local tempVFX = Prefab.spawn(vfxPrefab, world.x + 0.01 + xCorrection, world.y)
        if tempVFX then
            ECS.setMeshRenderer(tempVFX, {
                colorMultiply = { r = 1, g = 1, b = 1, a = 1 }
            })
            Event.emitLocal("OnTrapTriggered", {
                vfxEntity = tempVFX,
                unitType = unit.unitType,
                gridX = gridX,
                gridY = gridY,
                damage = tile.trap.damage
            })
        end

        -- Check if unit is shielded for damage preview halving
        local isShielded = AbilityManager_instance and AbilityManager_instance.shieldedUnit == unit.unitType

        -- Consecration trap (Tirion_2): pre-compute all detonations via BFS, then show sequentially
        if isConsTrap then
            -- Pre-compute all detonations
            local detonations = {}  -- ordered list of { centerX, centerY, damage }
            local detonationQueue = { { x = gridX, y = gridY } }
            local detonated = {}
            detonated[gridY * CONSTANTS.GRID_COLS + gridX] = true

            local bfsIdx = 1
            while bfsIdx <= #detonationQueue do
                local center = detonationQueue[bfsIdx]
                local detDmg = tile.trap.damage or 0
                -- Perimeter damage. Falls back to the centre value only when the
                -- trap record has no sideDamage at all, matching the server's
                -- `detTrap.sideDamage or detCenterDmg`.
                local detSideDmg = tile.trap.sideDamage or detDmg
                bfsIdx = bfsIdx + 1

                -- For chained traps, get their specific damage
                if #detonations > 0 then
                    local chainIdx = grid:GetTileIndex(center.x, center.y)
                    if chainIdx > 0 and grid.Tiles[chainIdx] and grid.Tiles[chainIdx].trap then
                        detDmg = grid.Tiles[chainIdx].trap.damage or 0
                        detSideDmg = grid.Tiles[chainIdx].trap.sideDamage or detDmg
                    end
                end

                -- Collect units hit by this detonation's 3x3 AoE
                local hitUnitsThisDet = {}
                local chainTargets = {}

                for dy = -1, 1 do
                    for dx = -1, 1 do
                        local tx = center.x + dx
                        local ty = center.y + dy
                        if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                            local aoeTileIdx = grid:GetTileIndex(tx, ty)
                            if aoeTileIdx > 0 and grid.Tiles[aoeTileIdx] then
                                if grid.Tiles[aoeTileIdx].unit then
                                    local hitUnit = grid.Tiles[aoeTileIdx].unit
                                    hitUnitsThisDet[hitUnit] = { gridX = tx, gridY = ty }
                                end
                                -- Chain detonation: check for another Consecration trap in AoE
                                -- Note: don't check armed here - during prep preview, last-turn traps
                                -- are unarmed but WILL be armed when resolution runs ArmExistingTraps()
                                local aKey = ty * CONSTANTS.GRID_COLS + tx
                                if not detonated[aKey] then
                                    local aoeTile = grid.Tiles[aoeTileIdx]
                                    if aoeTile.trap and aoeTile.trap.owner ~= unitFaction then
                                        -- Check for Consecration trap using explicit trap flags first.
                                        local chainIsConsTrap = (aoeTile.trap.isConsTrap == true)
                                        if not chainIsConsTrap and aoeTile.trap.isBleedTrap ~= true then
                                            -- Backward-compat fallback for older trap records.
                                            chainIsConsTrap = (aoeTile.trap.owner == CONSTANTS.ORDER_PLAYER)
                                        end
                                        if chainIsConsTrap then
                                            detonated[aKey] = true
                                            detonationQueue[#detonationQueue + 1] = { x = tx, y = ty }
                                            table.insert(chainTargets, { x = tx, y = ty })
                                        end
                                    end
                                end
                            end
                        end
                    end
                end

                -- Place the triggering unit on the tile it actually stepped on.
                --
                -- The scan above reads grid.Tiles[].unit, which during a projected
                -- move still holds this unit's ORIGIN tile -- the move has not been
                -- committed yet. Walking onto a trap from an adjacent tile is the
                -- normal case, so that stale origin usually lands inside the blast
                -- and the scan records the triggering unit at a perimeter tile.
                -- It then gets telegraphed at side damage when it is standing on
                -- the trap and should take the full centre hit.
                --
                -- Overwrite, do not skip: the entry is already there and wrong.
                -- The server does the same thing by parking the unit at the step
                -- position for the whole chain before it scans, and it holds that
                -- override across chained detonations too, not just the first.
                local unitInAoE = (math.abs(gridX - center.x) <= 1 and math.abs(gridY - center.y) <= 1)
                -- Guard the key: `t[nil] = nil` raises "table index is nil" in
                -- LuaJIT, and an error thrown here would abort the whole move
                -- handler before FinishMovement ever runs.
                if unit.unitType then
                    -- nil when out of blast: drops any stale entry the scan made,
                    -- so the unit is not previewed taking a hit the server will
                    -- never deal.
                    hitUnitsThisDet[unit.unitType] =
                        unitInAoE and { gridX = gridX, gridY = gridY } or nil
                end

                table.insert(detonations, {
                    centerX = center.x,
                    centerY = center.y,
                    damage = detDmg,
                    sideDamage = detSideDmg,
                    hitUnits = hitUnitsThisDet,
                    unitInAoE = unitInAoE
                })
            end

            -- A chain destroys every trap in it, not just the one stepped on
            -- (the server clears them all via detonatedTrapIndices), so none of
            -- them may telegraph again this planning phase.
            for _, d in ipairs(detonations) do
                local dIdx = grid:GetTileIndex(d.centerX, d.centerY)
                if dIdx > 0 then
                    gm:MarkPreviewedTrap(grid.gridID, dIdx)
                end
            end

            -- Show first detonation immediately (skip center VFX since tempVFX already spawned it)
            if #detonations > 0 then
                self:_ShowConsDetonation(entity, detonations[1], grid, isOrder, unit, unitFaction, isShielded, true)
            end

            -- If there are chained detonations, store state for sequential processing
            if #detonations > 1 then
                self._consChain = {
                    detonations = detonations,
                    currentIdx = 2,
                    timer = 0,
                    grid = grid,
                    isOrder = isOrder,
                    unit = unit,
                    unitFaction = unitFaction,
                    isShielded = isShielded,
                    entity = entity
                }
            end

            -- Camera shake + sound for initial detonation
            Camera.startShake(3.0, 0.4)
            Event.emitLocal("PlayVFXTirionVizomA2", {})
            return
        end

        -- Bleed trap handling
        -- Show trap damage number
        if tile.trap.damage and tile.trap.damage > 0 then
            local trapDamage = tile.trap.damage
            
            -- Check if unit has shield stacks
            local shieldStacks = gm.unitStatuses and gm.unitStatuses[unit.unitType]
                and gm.unitStatuses[unit.unitType][CONSTANTS.ABILITIES.ORACLE_2] or 0
            if shieldStacks > 0 then
                -- Calculate stacks consumed (1 stack per damage point)
                local stacksConsumed = math.min(shieldStacks, trapDamage)
                local remainingStacks = shieldStacks - stacksConsumed
                
                -- Update shield status UI
                if remainingStacks > 0 then
                    Event.emitLocal("OnUnitStatusInflicted", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        ownerUnit = 0,
                        targetUnit = unit.unitType,
                        stackCount = remainingStacks,
                        isExpiring = false
                    })
                else
                    Event.emitLocal("OnUnitStatusRemoved", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        targetUnit = unit.unitType
                    })
                    Event.emitLocal("RemoveShield", {
                        unitType = unit.unitType
                    })
                end
            end
            
            Event.emitLocal("SpawnFloatingText", {
                worldX = world.x + 0.01,
                worldY = world.y + 0.5,
                damage = trapDamage
            })
            gm:PreviewTrapDamage(unitFaction, trapDamage)
        end
        -- Show BLEED status text above damage
        Event.emitLocal("BleedProc", { unitType = unit.unitType })
        Event.emitLocal("SpawnFloatingText", {
            worldX = world.x + 0.01,
            worldY = world.y + 0.8,
            damage = 0,
            text = "BLEED"
        })

        -- Camera shake + sound
        Camera.startShake(3.0, 0.4)
        Event.emitLocal("PlayVFXTirionVizomA2", {})

        -- Hit reaction for bleed trap
        Event.emitLocal("OnTilesHit", {
            hits = { { unitType = unit.unitType, isOrderGrid = isOrder } }
        })
    end,

    -- Show VFX and damage for a single Consecration detonation
    -- skipCenterVFX: true for first detonation (center VFX already spawned as tempVFX)
    _ShowConsDetonation = function(self, entity, det, grid, isOrder, unit, unitFaction, isShielded, skipCenterVFX)
        local gm = ClientGameManager_instance
        local centerWorld = Grid.gridToWorld(grid.gridID, det.centerX, det.centerY)

        -- Spawn center VFX (skip for first detonation since tempVFX handles it).
        -- For chained/domino detonations, this gives the 9th center burst in the 3x3.
        if not skipCenterVFX then
            -- Apply tileXCorrection for trap VFX positioning
            local tileXCorrection = {
                [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
            }
            local centerXCorrection = tileXCorrection[det.centerY] and tileXCorrection[det.centerY][det.centerX] or 0
            
            local centerVFX = Prefab.spawn("Tirion_Ability2_VFX", centerWorld.x + 0.01 + centerXCorrection, centerWorld.y + 0.5)
            if centerVFX then
                ECS.setMeshRenderer(centerVFX, { colorMultiply = { r = 1, g = 1, b = 1, a = 1 } })
                Event.emitLocal("OnTrapTriggered", {
                    vfxEntity = centerVFX,
                    unitType = 0,
                    gridX = det.centerX,
                    gridY = det.centerY,
                    damage = det.damage
                })
            end
        end

        -- tileXCorrection for AoE VFX
        local tileXCorrection = {
            [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
            [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
            [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
            [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
        }

        -- Spawn AoE VFX on surrounding tiles
        for dy = -1, 1 do
            for dx = -1, 1 do
                if dx ~= 0 or dy ~= 0 then
                    local tx = det.centerX + dx
                    local ty = det.centerY + dy
                    if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                        local aoeWorld = Grid.gridToWorld(grid.gridID, tx, ty)
                        local aoeXCorrection = tileXCorrection[ty] and tileXCorrection[ty][tx] or 0
                        local aoeVFX = Prefab.spawn("Tirion_Ability2_VFX", aoeWorld.x + 0.01 + aoeXCorrection, aoeWorld.y + 0.5)
                        if aoeVFX then
                            ECS.setMeshRenderer(aoeVFX, { colorMultiply = { r = 1, g = 1, b = 1, a = 1 } })
                            Event.emitLocal("OnTrapTriggered", {
                                vfxEntity = aoeVFX,
                                unitType = 0,
                                gridX = tx,
                                gridY = ty,
                                damage = 0
                            })
                        end
                    end
                end
            end
        end

        -- Floating text + preview damage: apply per unit hit in this detonation AoE
        -- (matches resolution behavior where every hit unit gets its own damage number).
        for ut, pos in pairs(det.hitUnits) do
            -- Centre tile takes the full hit, everything else in the 3x3 takes
            -- the perimeter value. This mirrors the server's
            -- `(dx == 0 and dy == 0) and detCenterDmg or detSideDmg` exactly;
            -- previewing det.damage for every unit over-predicted perimeter hits
            -- (8 instead of 1 for Sigil of Consecration), and the health bar then
            -- jumped back up when the authoritative result arrived, reading as a
            -- heal in a game that has no heal.
            local isCenterHit = (pos.gridX == det.centerX and pos.gridY == det.centerY)
            local trapDamage = isCenterHit and (det.damage or 0)
                or (det.sideDamage or det.damage or 0)

            -- Burn adds 1 to every source of damage, the server included.
            if gm.unitStatuses and gm.unitStatuses[ut]
                and gm.unitStatuses[ut][CONSTANTS.ABILITIES.DRAGOON_1] then
                trapDamage = trapDamage + 1
            end

            -- Check if unit has shield stacks
            local shieldStacks = gm.unitStatuses and gm.unitStatuses[ut]
                and gm.unitStatuses[ut][CONSTANTS.ABILITIES.ORACLE_2] or 0
            if shieldStacks > 0 then
                -- Calculate stacks consumed (1 stack per damage point)
                local stacksConsumed = math.min(shieldStacks, trapDamage)
                local remainingStacks = shieldStacks - stacksConsumed
                
                -- Update shield status UI
                if remainingStacks > 0 then
                    Event.emitLocal("OnUnitStatusInflicted", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        ownerUnit = 0,
                        targetUnit = ut,
                        stackCount = remainingStacks,
                        isExpiring = false
                    })
                else
                    Event.emitLocal("OnUnitStatusRemoved", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        targetUnit = ut
                    })
                    Event.emitLocal("RemoveShield", {
                        unitType = ut
                    })
                end
            end

            local hitWorld = Grid.gridToWorld(grid.gridID, pos.gridX, pos.gridY)
            Event.emitLocal("SpawnFloatingText", {
                worldX = hitWorld.x + 0.01,
                worldY = hitWorld.y + 0.5,
                damage = trapDamage
            })

            gm:PreviewTrapDamage(unitFaction, trapDamage)
        end

        -- Hit reactions for units caught in this detonation's AoE
        local hitList = {}
        for ut, pos in pairs(det.hitUnits) do
            table.insert(hitList, {
                unitType = ut,
                isOrderGrid = isOrder,
                gridX = pos.gridX,
                gridY = pos.gridY
            })
        end
        if #hitList > 0 then
            Event.emitLocal("OnTilesHit", { hits = hitList })
        end
    end,

    -- Process the next Consecration chain detonation (called from Update timer)
    _ProcessNextConsDetonation = function(self, entity)
        local chain = self._consChain
        if not chain then return end

        local det = chain.detonations[chain.currentIdx]
        if det then
            self:_ShowConsDetonation(entity, det, chain.grid, chain.isOrder, chain.unit, chain.unitFaction, chain.isShielded)
            Camera.startShake(3.0, 0.4)
            Event.emitLocal("PlayVFXTirionVizomA2", {})
        end

        chain.currentIdx = chain.currentIdx + 1
        if chain.currentIdx > #chain.detonations then
            self._consChain = nil
        end
    end,

    FinishMovement = function(self, entity)
        self._consChain = nil
        self.isEnding = true
        self.endTimer = self.moveEndDelay

        local unit = self.stateMachine.unit
        unit:PlayAnimation(entity, "MoveEnd")

        -- Bleed movement damage preview: 1 damage per tile moved (Manhattan distance to match server)
        local gm = ClientGameManager_instance
        if gm and gm.unitStatuses and gm.unitStatuses[unit.unitType]
            and gm.unitStatuses[unit.unitType][CONSTANTS.ABILITIES.VIZOM_2] then
            local tilesMoved = unit.manhattanTilesMoved or 0
            if tilesMoved > 0 then
                local bleedDmg = tilesMoved
                local isOrder = (unit.ownerPlayer == 1)
                local unitFaction = isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER

                if gm.unitStatuses[unit.unitType][CONSTANTS.ABILITIES.DRAGOON_1] then
                    bleedDmg = bleedDmg + 1
                end

                -- Check if unit has shield stacks
                local shieldStacks = gm.unitStatuses and gm.unitStatuses[unit.unitType]
                    and gm.unitStatuses[unit.unitType][CONSTANTS.ABILITIES.ORACLE_2] or 0
                if shieldStacks > 0 then
                    -- Calculate stacks consumed (1 stack per damage point)
                    local stacksConsumed = math.min(shieldStacks, bleedDmg)
                    local remainingStacks = shieldStacks - stacksConsumed
                    
                    -- Update shield status UI
                    if remainingStacks > 0 then
                        Event.emitLocal("OnUnitStatusInflicted", {
                            statusID = CONSTANTS.ABILITIES.ORACLE_2,
                            ownerUnit = 0,
                            targetUnit = unit.unitType,
                            stackCount = remainingStacks,
                            isExpiring = false
                        })
                    else
                        Event.emitLocal("OnUnitStatusRemoved", {
                            statusID = CONSTANTS.ABILITIES.ORACLE_2,
                            targetUnit = unit.unitType
                        })
                        Event.emitLocal("RemoveShield", {
                            unitType = unit.unitType
                        })
                    end
                end

                gm:PreviewTrapDamage(unitFaction, bleedDmg)

                -- Show floating text at unit position
                Event.emitLocal("BleedProc", { unitType = unit.unitType })
                local transform = ECS.getTransform(entity)
                Event.emitLocal("SpawnFloatingText", {
                    worldX = transform.x,
                    worldY = transform.y + 0.5,
                    damage = bleedDmg
                })
                Event.emitLocal("SpawnFloatingText", {
                    worldX = transform.x,
                    worldY = transform.y + 0.8,
                    damage = 0,
                    text = "BLEED"
                })
            end
        end
    end
})

return class
