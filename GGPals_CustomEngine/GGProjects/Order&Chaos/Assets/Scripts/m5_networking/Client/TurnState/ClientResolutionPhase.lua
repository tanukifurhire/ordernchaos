local RESOLVE_SETTLING      = 0
local RESOLVE_NEXT_ACTION   = 1
local RESOLVE_MOVING        = 2
local RESOLVE_ABILITY       = 3
local RESOLVE_DELAY         = 4

-- Maps every abilityID to its caster unit type and animation name
-- Uses literal IDs to avoid CONSTANTS load-order issues (module-level code)
-- DRAGOON=1, ORACLE=2, TIRION=3, IMP=4, EYE=5, VIZOM=6
local ABILITY_CAST_MAP = {
    [1]  = { unit = 2, anim = "Ability1" },  -- Oracle 1
    [2]  = { unit = 2, anim = "Ability2" },  -- Oracle 2
    [3]  = { unit = 1, anim = "Ability1" },  -- Dragoon 1
    [4]  = { unit = 1, anim = "Ability2" },  -- Dragoon 2
    [9]  = { unit = 3, anim = "Ability1" },  -- Tirion 1
    [10] = { unit = 3, anim = "Ability2" },  -- Tirion 2
    [5]  = { unit = 4, anim = "Ability1" },  -- Imp 1
    [6]  = { unit = 4, anim = "Ability2" },  -- Imp 2
    [7]  = { unit = 5, anim = "Ability1" },  -- Eye 1
    [8]  = { unit = 5, anim = "Ability2" },  -- Eye 2
    [11] = { unit = 6, anim = "Ability1" },  -- Vizom 1
    [12] = { unit = 6, anim = "Ability2" }   -- Vizom 2
}

-- Maps abilityID to the AnimPreview trigger event name
local ABILITY_PREVIEW_MAP = {
    [1]  = "TriggerOracle1AnimPreview",
    [2]  = "TriggerOracle2AnimPreview",
    [3]  = "TriggerDragoon1AnimPreview",
    [4]  = "TriggerDragoon2AnimPreview",
    [5]  = "TriggerImp1AnimPreview",
    [6]  = "TriggerImp2AnimPreview",
    [7]  = "TriggerEye1AnimPreview",
    [8]  = "TriggerEye2AnimPreview",
    [9]  = "TriggerTirion1AnimPreview",
    [10] = "TriggerTirion2AnimPreview",
    [11] = "TriggerVizom1AnimPreview",
    [12] = "TriggerVizom2AnimPreview",
    [14] = "TriggerGeneric1AnimPreview",
    [15] = "TriggerGeneric2AnimPreview"
}

local class = DefineType("ClientResolutionPhase",
{
    NON_STACKING_STATUSES = {
        [3] = true,  -- Burn (DRAGOON_1)
        [12] = true, -- Bleed (VIZOM_2)
        [8] = true,  -- Wrath (EYE_2)
        [17] = true  -- Mark (PASSIVE_2)
    },
    STACK_STATUS_LOOKUP = {
        [9] = "fervor",   -- TIRION_1
        [11] = "berserk"  -- VIZOM_1
    },

    Serializable =
    {
        phaseEnterDelay = 0.5,           -- Delay after overlay before actions start
        phaseOverlayDuration = 1.0,      -- How long the phase overlay displays
        delayBetweenActions = 1.5,
        syncSettleDelay = 0.5,
        consChainDetonationDelay = 0.5,
        consAoEFlashDuration = 0.4
    },

    currentActionIndex = 0,
    totalActions = 0,
    isResolving = false,
    resolveState = RESOLVE_SETTLING,
    stateTimer = 0,
    visualWatchdog = 0,
    preCalculatedResults = nil,
    moveCompleteHandler = 0,
    triggerCompleteHandler = 0,
    trapHitHandler = 0,
    waitingForVisual = false,
    activeAbility = nil,
    phaseDelayTimer = 0,
    phaseReady = false,

    Enter = function(self)
        Log.info("[ResolutionPhase] Entered Resolution Phase!")
        Event.emitLocal("InResolutionPhase", {})
        Event.emitLocal("ShowPhaseOverlay", { phase = "resphase" })
        
        -- Start phase delay timer
        self.phaseDelayTimer = 0
        self.phaseReady = false

        local gm = ClientGameManager_instance
        local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)

        -- Play announcer SFX for resolution phase
        if isOrder then
            Event.emitLocal("PlayAnnouncerOrderResolution", {})
        else
            Event.emitLocal("PlayAnnouncerChaosResolution", {})
        end

        self.currentActionIndex = 0
        self.totalActions = 0
        self.waitingForVisual = false
        self.activeAbility = nil
        self.stateTimer = 0

        -- Revert any previewed heals and trap damage from prep phase before resolution
        gm:RevertPreviewedHeals()
        gm:RevertPreviewedTrapDamage()
        gm:ClearAllPreviewedTileDamage()

        -- Sync starting health from server and validate client state
        if gm.resolutionData and gm.resolutionData.startingHealth then
            local serverStart = gm.resolutionData.startingHealth
            local clientOrder = gm:GetCurrentHealth(true)
            local clientChaos = gm:GetCurrentHealth(false)
            
            if serverStart.order and clientOrder ~= serverStart.order then
                Log.warning("[ResolutionPhase] Starting health desync! Order client: " .. clientOrder .. ", server: " .. serverStart.order)
                gm:SetHealth(true, serverStart.order)
            end
            if serverStart.chaos and clientChaos ~= serverStart.chaos then
                Log.warning("[ResolutionPhase] Starting health desync! Chaos client: " .. clientChaos .. ", server: " .. serverStart.chaos)
                gm:SetHealth(false, serverStart.chaos)
            end
        end
        
        -- Store validated starting health for damage tracking
        self._startingHealth = {
            order = gm:GetCurrentHealth(true),
            chaos = gm:GetCurrentHealth(false)
        }

        -- Sync unit positions from server data
        -- Reset unit visuals first (un-darken moved units)
        Event.emitLocal("OnPrepPhaseReset", {})

        if gm.resolutionData and gm.resolutionData.syncPositions then
            for i = 1, 6 do
                local pos = gm.resolutionData.syncPositions[i]
                if pos then
                    Event.emitLocal("UpdatePosForce", { unitType = i, x = pos.x, y = pos.y })
                end
            end
        end

        -- Trap visibility during resolution
        gm.orderGrid:HideAllTraps()
        gm.chaosGrid:HideAllTraps()
        if PassAndPlayServer_instance then
            -- PnP: hide both sides' traps during resolution
            Event.emitLocal("ShowOrderAbilities", {})
            Event.emitLocal("HideChaosAbilities", {})
        else
            -- Networked: show own traps, hide opponent's
            gm.orderGrid:ShowOwnTraps(gm.playerFaction)
            gm.chaosGrid:ShowOwnTraps(gm.playerFaction)
        end

        if not gm.resolutionData then
            Log.warning("[ResolutionPhase] No resolution data!")
            self:FinishResolution()
            return
        end

        if not gm.resolutionData.queue then
            Log.warning("[ResolutionPhase] Resolution data has no queue field!")
            self:FinishResolution()
            return
        end

        local queueLen = #gm.resolutionData.queue
        Log.info("[ResolutionPhase] Queue has " .. queueLen .. " actions")

        if queueLen == 0 then
            Log.info("[ResolutionPhase] No actions to resolve, returning to prep")
            self:FinishResolution()
            return
        end

        if gm.resolutionData and gm.resolutionData.buffStacks then
            gm.buffStacks = {
                fervor = gm.resolutionData.buffStacks.fervor or 0,
                berserk = gm.resolutionData.buffStacks.berserk or 0
            }
        end
        for _, buff in ipairs(gm.resolutionData.activeBuffs or {}) do
            local buffUnit = tonumber(buff and buff.unit)
            local buffStatusID = tonumber(buff and buff.statusID)
            if buffUnit and buffStatusID then
                local stackCount = buff.stackCount or 1
                if buffStatusID == CONSTANTS.ABILITIES.TIRION_1 then
                    stackCount = math.max(1, gm.buffStacks and gm.buffStacks.fervor or stackCount)
                elseif buffStatusID == CONSTANTS.ABILITIES.VIZOM_1 then
                    stackCount = math.max(1, gm.buffStacks and gm.buffStacks.berserk or stackCount)
                end
                Event.emitLocal("OnUnitStatusInflicted", {
                    statusID = buffStatusID,
                    ownerUnit = 0,
                    targetUnit = buffUnit,
                    stackCount = stackCount,
                    isExpiring = buff.isExpiring and true or false
                })
            else
                Log.warning("[ResolutionPhase] Skipping malformed activeBuff entry in Enter()")
            end
        end

        -- Use server's pre-calculated results directly
        self.preCalculatedResults = gm.resolutionData.queue
        if type(self.preCalculatedResults) ~= "table" then
            Log.error("[ResolutionPhase] Resolution queue is malformed; aborting resolution")
            self:FinishResolution()
            return
        end
        self.totalActions = queueLen

        -- Populate ability queue for UI display
        -- In networked mode: local player's queue was already populated during prep,
        -- so only add the opponent's abilities.
        -- In PnP mode: neither faction's prep-phase UI survives the faction switch,
        -- so add ALL abilities for both factions.
        for _, result in ipairs(self.preCalculatedResults) do
            if result.action and result.action.actionType == CONSTANTS.ACTION_ABILITY then
                local isOpponent = (result.action.faction ~= gm.playerFaction)
                if PassAndPlayServer_instance or isOpponent then
                    local ability = gm:GetAbilityByIDGlobal(result.action.abilityID)
                    if ability and ability.abilityType ~= CONSTANTS.ABILITY_SUPPORT then
                        Event.emitLocal("OnAbilityQueued", {
                            abilityID = result.action.abilityID,
                            queueIndex = 0,
                            player = result.action.faction
                        })
                    end
                end
            end
        end

        -- Log first action for debugging
        local firstResult = self.preCalculatedResults[1]
        if firstResult then
            if firstResult.action then
                Log.info("[ResolutionPhase] First action: type=" .. tostring(firstResult.action.actionType) ..
                    " abilityID=" .. tostring(firstResult.action.abilityID) ..
                    " faction=" .. tostring(firstResult.action.faction))
            else
                Log.error("[ResolutionPhase] First result has no 'action' field!")
            end
        end

        -- Subscribe to visual completion events
        self.moveCompleteHandler = Event.subscribe("OnReplayMoveComplete", function(params)
            Log.info("[ResolutionPhase] OnReplayMoveComplete received (unit=" .. tostring(params.unitType) .. ") state=" .. tostring(self.resolveState))
            -- Only handle during RESOLVE_MOVING; during RESOLVE_ABILITY the ability handles it
            if self.resolveState == RESOLVE_MOVING and self.waitingForVisual then
                self.waitingForVisual = false
            end
        end)

        self.triggerCompleteHandler = Event.subscribe("OnAbilityTriggerComplete", function(params)
            Log.info("[ResolutionPhase] OnAbilityTriggerComplete received (ability=" .. tostring(params.abilityID) .. ") waitingForVisual=" .. tostring(self.waitingForVisual))
            if self.waitingForVisual then
                self.waitingForVisual = false
            end
        end)

        -- Subscribe to mid-movement trap hit from unit
        self.trapHitHandler = Event.subscribe("OnUnitTrapHit", function(params)
            Log.info("[ResolutionPhase] OnUnitTrapHit received (unit=" .. tostring(params.unitType) .. ")")
            self:HandleTrapHitVisuals()
        end)

        self.trapHitHandled = false
        self.isResolving = true
        self.resolveState = RESOLVE_SETTLING
        self.stateTimer = 0
    end,

    Exit = function(self)
        self.isResolving = false
        self.waitingForVisual = false
        self.preCalculatedResults = nil
        self.activeAbility = nil

        if self.moveCompleteHandler ~= 0 then
            Event.unsubscribe(self.moveCompleteHandler)
            self.moveCompleteHandler = 0
        end
        if self.triggerCompleteHandler ~= 0 then
            Event.unsubscribe(self.triggerCompleteHandler)
            self.triggerCompleteHandler = 0
        end
        if self.trapHitHandler ~= 0 then
            Event.unsubscribe(self.trapHitHandler)
            self.trapHitHandler = 0
        end
    end,

    -- Arm all unarmed traps placed in previous resolution phases
    ArmExistingTraps = function(self, gm)
        local grids = { gm.orderGrid, gm.chaosGrid }
        for _, grid in ipairs(grids) do
            for i = 1, CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS do
                local tile = grid.Tiles[i]
                if tile.trap and not tile.trap.armed then
                    tile.trap.armed = true
                    Log.info("[ResolutionPhase] Armed trap on tile " .. i)
                end
            end
        end
    end,

    -- Spawn damage modifier indicators (Hunter, Isolation, Burn, Wrath) centered on unit
    -- @param worldX, worldY: world position of the unit
    -- @param modifiers: table of modifier names to show (e.g., {"BurnBonus", "IsolationBonus"})
    SpawnDamageIndicators = function(self, worldX, worldY, modifiers)
        if not modifiers or #modifiers == 0 then return end

        local indicatorWidth = 0.5
        local indicatorSpacing = 0.05
        local totalWidth = (#modifiers * indicatorWidth) + ((#modifiers - 1) * indicatorSpacing)
        local startX = worldX - (totalWidth / 2) + (indicatorWidth / 2)
        local yOffset = 1.0  -- Spawn above the damage number

        for i, prefabName in ipairs(modifiers) do
            local offsetX = (i - 1) * (indicatorWidth + indicatorSpacing)
            local spawnX = startX + offsetX
            local indicator = Prefab.spawn(prefabName, spawnX, worldY + yOffset)
            if indicator then
                Log.info("[ResolutionPhase] Spawned damage indicator: " .. prefabName)
            else
                Log.warning("[ResolutionPhase] Failed to spawn indicator: " .. prefabName)
            end
        end
    end,

    Update = function(self, entity)
        -- Handle phase enter delay
        if not self.phaseReady then
            self.phaseDelayTimer = self.phaseDelayTimer + Time.getDeltaTime()
            if self.phaseDelayTimer >= self.phaseEnterDelay then
                self.phaseReady = true
            end
            return
        end
        
        if not self.isResolving then return end

        local dt = Time.getDeltaTime()
        local gm = ClientGameManager_instance

        -- Consecration chain detonation state machine
        if self._consChainState then
            local cs = self._consChainState
            cs.timer = cs.timer + dt

            if cs.phase == "flash" then
                -- Current AoE is showing, wait for flash duration then check for chain
                if cs.timer >= cs.flashDuration then
                    if cs.chainIdx < #cs.chainQueue then
                        -- More traps to chain-detonate: transition to delay
                        cs.phase = "chain_delay"
                        cs.timer = 0
                    else
                        -- No more chains, clear all highlights and finish
                        for _, tile in ipairs(cs.allAoETiles) do
                            Grid.setTileColorAdd(cs.gridID, tile.y, tile.x, 0, CONSTANTS.COLOR_NONE)
                        end
                        self._consChainState = nil
                    end
                end

            elseif cs.phase == "chain_delay" then
                -- Waiting between chain detonations
                if cs.timer >= cs.chainDelay then
                    cs.chainIdx = cs.chainIdx + 1
                    local trapIdx = cs.chainQueue[cs.chainIdx]

                    -- Get the chained trap's VFX entity before removing
                    local chainVFX = nil
                    if cs.trapGrid.Tiles[trapIdx] and cs.trapGrid.Tiles[trapIdx].trap then
                        chainVFX = cs.trapGrid.Tiles[trapIdx].trap.vfxEntity
                    end

                    -- Remove the chained trap from grid
                    self:RemoveTrap(cs.isOrderGrid, trapIdx)

                    -- Trigger the chained trap's VFX
                    if chainVFX then
                        ECS.setMeshRenderer(chainVFX, { colorMultiply = CONSTANTS.COLOR_WHITE })
                        local chainX = (trapIdx - 1) % CONSTANTS.GRID_COLS
                        local chainY = math.floor((trapIdx - 1) / CONSTANTS.GRID_COLS)
                        Event.emitLocal("OnTrapTriggered", {
                            vfxEntity = chainVFX,
                            unitType = 0,
                            gridX = chainX,
                            gridY = chainY,
                            damage = 0
                        })
                    end

                    -- Play trap trigger audio for chain
                    Event.emitLocal("PlayVFXTirionVizomA2", {})

                    -- Show 3x3 AoE around the chained trap
                    local chainCenterX = (trapIdx - 1) % CONSTANTS.GRID_COLS
                    local chainCenterY = math.floor((trapIdx - 1) / CONSTANTS.GRID_COLS)
                    
                    -- tileXCorrection for AoE VFX
                    local tileXCorrection = {
                        [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                        [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                        [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                        [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
                    }
                    
                    for dy = -1, 1 do
                        for dx = -1, 1 do
                            local tx = chainCenterX + dx
                            local ty = chainCenterY + dy
                            if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                                Grid.setTileColorAdd(cs.gridID, ty, tx, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT)
                                -- Spawn VFX on surrounding tiles (center already has the trap's own VFX)
                                if dx ~= 0 or dy ~= 0 then
                                    local world = Grid.gridToWorld(cs.gridID, tx, ty)
                                    local aoeXCorrection = tileXCorrection[ty] and tileXCorrection[ty][tx] or 0
                                    local aoeVFX = Prefab.spawn("Tirion_Ability2_VFX", world.x + 0.01 + aoeXCorrection, world.y + 0.5)
                                    if aoeVFX then
                                        ECS.setMeshRenderer(aoeVFX, { colorMultiply = CONSTANTS.COLOR_WHITE })
                                        Event.emitLocal("OnTrapTriggered", {
                                            vfxEntity = aoeVFX,
                                            unitType = 0,
                                            gridX = tx,
                                            gridY = ty,
                                            damage = 0
                                        })
                                    end
                                end
                                -- Track for cleanup
                                table.insert(cs.allAoETiles, { x = tx, y = ty })
                            end
                        end
                    end

                    -- Show per-unit damage from this chained trap's consHits
                    -- (consHits contains all hits across all detonations, filter by detonationIdx)
                    local chainDetonationIdx = cs.chainIdx + 1  -- chainIdx is 1-based into chainQueue, +1 because initial = detonationIdx 1
                    local chainHitUnits = {}
                    local chainDmgTotal = 0
                    for _, hit in ipairs(cs.consHits) do
                        if hit.detonationIdx == chainDetonationIdx then
                            local world = Grid.gridToWorld(cs.gridID, hit.gridX, hit.gridY)
                            Event.emitLocal("SpawnFloatingText", {
                                worldX = world.x + 0.01,
                                worldY = world.y + 0.5,
                                damage = hit.damage or 0
                            })
                            chainDmgTotal = chainDmgTotal + (hit.damage or 0)
                            if hit.unitType then
                                chainHitUnits[hit.unitType] = { gridX = hit.gridX, gridY = hit.gridY }
                            end
                        end
                    end
                    -- Trigger hit reaction on units caught in this chain's AoE
                    local chainHitList = {}
                    for ut, pos in pairs(chainHitUnits) do
                        table.insert(chainHitList, {
                            unitType = ut,
                            isOrderGrid = cs.isOrderGrid,
                            gridX = pos.gridX,
                            gridY = pos.gridY
                        })
                    end
                    if #chainHitList > 0 then
                        Event.emitLocal("OnTilesHit", { hits = chainHitList })
                    end
                    -- Apply chain detonation damage to health bar
                    if chainDmgTotal > 0 then
                        local trapPlayer = cs.isOrderGrid and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                        gm:ApplyDamage(trapPlayer, chainDmgTotal)
                    end
                    -- Camera shake for chain detonation
                    Camera.startShake(3.0, 0.4)

                    -- Go back to flash phase to show this AoE
                    cs.phase = "flash"
                    cs.timer = 0
                end
            end
        end

        if self.resolveState == RESOLVE_SETTLING then
            self.stateTimer = self.stateTimer + dt
            if self.stateTimer >= self.syncSettleDelay then
                self.resolveState = RESOLVE_NEXT_ACTION
            end

        elseif self.resolveState == RESOLVE_NEXT_ACTION then
            self.currentActionIndex = self.currentActionIndex + 1
            if self.currentActionIndex > self.totalActions then
                self:FinishResolution()
                return
            end

            local result = self.preCalculatedResults[self.currentActionIndex]
            if not result then
                Log.error("[ResolutionPhase] Nil result at index " .. self.currentActionIndex)
                self.resolveState = RESOLVE_DELAY
                self.stateTimer = 0
                return
            end

            local action = result.action
            if not action then
                Log.error("[ResolutionPhase] Result at index " .. self.currentActionIndex .. " has no action field!")
                self.resolveState = RESOLVE_DELAY
                self.stateTimer = 0
                return
            end

            Log.info("[ResolutionPhase] Action " .. self.currentActionIndex .. "/" .. self.totalActions ..
                ": type=" .. tostring(action.actionType) .. " ability=" .. tostring(action.abilityID) ..
                " faction=" .. tostring(action.faction))

            -- In PnP, update opponent status panel to show the faction currently resolving
            if PassAndPlayServer_instance and action.faction then
                Event.emitLocal("OnGameplayFactionReady", {
                    isOrder = (action.faction == CONSTANTS.ORDER_PLAYER)
                })

                -- Show current side abilities and hide other side
                if action.faction == CONSTANTS.ORDER_PLAYER then
                    Event.emitLocal("ShowOrderAbilities", {})
                    Event.emitLocal("HideChaosAbilities", {})
                else
                    Event.emitLocal("ShowChaosAbilities", {})
                    Event.emitLocal("HideOrderAbilities", {})
                end
            end

            if action.actionType == CONSTANTS.ACTION_MOVEMENT then
                self:StartMovementResolve(result)
            elseif action.actionType == CONSTANTS.ACTION_ABILITY then
                self:StartAbilityResolve(result)
            else
                Log.warning("[ResolutionPhase] Unknown action type: " .. tostring(action.actionType))
                self.resolveState = RESOLVE_DELAY
                self.stateTimer = 0
            end

        elseif self.resolveState == RESOLVE_MOVING then
            if self.waitingForVisual then
                self.visualWatchdog = self.visualWatchdog + dt
                if self.visualWatchdog > 10.0 then
                    Log.warning("[ResolutionPhase] Movement visual timed out, force-advancing")
                    self.waitingForVisual = false
                end
            end
            if not self.waitingForVisual then
                self.visualWatchdog = 0
                local result = self.preCalculatedResults[self.currentActionIndex]
                -- Trap hits are now handled mid-movement via OnUnitTrapHit
                -- Only process here as fallback if somehow not handled
                if not self.trapHitHandled then
                    if result.trapHit then
                        Log.info("[ResolutionPhase] Trap hit not handled mid-movement, applying now (fallback)")
                        self:HandleTrapHitVisuals()
                    end
                end
                -- Apply movement damage (e.g. Bleed damage from moving while debuffed)
                local moveDamage = result.damage or 0
                local moveDamagePlayer = result.damagePlayer or 0
                if moveDamage > 0 and moveDamagePlayer > 0 then
                    gm:ApplyDamage(moveDamagePlayer, moveDamage)
                    Log.info("[ResolutionPhase] Applied movement damage: " .. moveDamage .. " to player " .. moveDamagePlayer)
                end
                -- Bleed visual feedback: cut camera to unit, hitstop + floating text
                if result.bleedHit then
                    local bh = result.bleedHit
                    local bleedGrid = bh.isOrder and gm.orderGrid or gm.chaosGrid
                    -- Cut camera to the bleeding unit's grid
                    if bh.isOrder then
                        Event.emitLocal("FocusOrderGrid", {})
                    else
                        Event.emitLocal("FocusChaosGrid", {})
                    end
                    Event.emitLocal("OnTilesHit", { hits = {
                        { unitType = bh.unitType, isOrderGrid = bh.isOrder,
                          gridX = result.action.targetX, gridY = result.action.targetY }
                    }})
                    Event.emitLocal("BleedProc", { unitType = bh.unitType })
                    local world = Grid.gridToWorld(bleedGrid.gridID, result.action.targetX, result.action.targetY)
                    Event.emitLocal("SpawnFloatingText", {
                        worldX = world.x + 0.01, worldY = world.y + 0.5,
                        damage = bh.damage
                    })
                    Event.emitLocal("SpawnFloatingText", {
                        worldX = world.x + 0.01, worldY = world.y + 0.8,
                        damage = 0, text = "BLEED"
                    })
                    if gm.unitStatuses
                        and bh.unitType
                        and gm.unitStatuses[bh.unitType]
                        and gm.unitStatuses[bh.unitType][CONSTANTS.ABILITIES.DRAGOON_1] then
                        Event.emitLocal("SpawnFloatingText", {
                            worldX = world.x + 0.01, worldY = world.y + 1.1,
                            damage = 0, text = "BURN"
                        })
                    end
                    Camera.startShake(1.5, 0.3)

                    -- Emit bleed status event for movement bleed
                    self:EmitStatusInflicted(
                        CONSTANTS.ABILITIES.VIZOM_2,
                        CONSTANTS.UNIT.VIZOM,
                        bh.unitType,
                        true
                    )
                    -- Bleed trigger grants Vizom Berserk (per point of Bleed damage)
                    if result.berserkGranted then
                        -- Update local buffStacks with the stacks gained
                        local stacksGained = result.berserkStacksGained or 1
                        if gm.buffStacks then
                            gm.buffStacks.berserk = (gm.buffStacks.berserk or 0) + stacksGained
                        end
                        self:EmitStatusInflicted(
                            CONSTANTS.ABILITIES.VIZOM_1,
                            CONSTANTS.UNIT.VIZOM,
                            CONSTANTS.UNIT.VIZOM,
                            false
                        )
                    end
                end
                self.resolveState = RESOLVE_DELAY
                self.stateTimer = 0
            end

        elseif self.resolveState == RESOLVE_ABILITY then
            if self.activeAbility then
                self.activeAbility:UpdateTrigger(dt)
            end
            if self.waitingForVisual then
                self.visualWatchdog = self.visualWatchdog + dt
                local timeout = self.abilityVisualTimeout or 10.0
                if self.visualWatchdog > timeout then
                    Log.warning("[ResolutionPhase] Ability visual timed out after " .. string.format("%.1f", timeout) .. "s, force-advancing")
                    self.waitingForVisual = false
                end
            end
            if not self.waitingForVisual then
                self.visualWatchdog = 0
                local currentResult = self.preCalculatedResults[self.currentActionIndex]
                self:ApplyAbilityResults(currentResult)

                -- Notify AbilityQueueUI that ability animation finished (dequeues icon)
                if currentResult and currentResult.action then
                    Event.emitLocal("AbilityAnimationFinished", {
                        player = currentResult.action.faction
                    })
                end

                -- Bleed-on-ability visual feedback: cut camera to caster, hitstop + floating text
                if currentResult.bleedHit then
                    local bh = currentResult.bleedHit
                    local bleedGrid = bh.isOrder and gm.orderGrid or gm.chaosGrid
                    -- Cut camera to the bleeding caster's grid
                    if bh.isOrder then
                        Event.emitLocal("FocusOrderGrid", {})
                    else
                        Event.emitLocal("FocusChaosGrid", {})
                    end
                    -- Apply bleed damage to caster's team
                    local bleedPlayer = bh.isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                    gm:ApplyDamage(bleedPlayer, bh.damage)
                    -- Trigger hit state on caster unit
                    Event.emitLocal("OnTilesHit", { hits = {
                        { unitType = bh.unitType, isOrderGrid = bh.isOrder,
                          gridX = 0, gridY = 0 }
                    }})
                    Event.emitLocal("BleedProc", { unitType = bh.unitType })
                    -- Find caster world position for floating text
                    for i = 1, 6 do
                        if i == bh.unitType then
                            local syncPos = gm.resolutionData and gm.resolutionData.syncPositions and gm.resolutionData.syncPositions[i]
                            if syncPos then
                                local world = Grid.gridToWorld(bleedGrid.gridID, syncPos.x, syncPos.y)
                                Event.emitLocal("SpawnFloatingText", {
                                    worldX = world.x + 0.01, worldY = world.y + 0.5,
                                    damage = bh.damage
                                })
                                Event.emitLocal("SpawnFloatingText", {
                                    worldX = world.x + 0.01, worldY = world.y + 0.8,
                                    damage = 0, text = "BLEED"
                                })
                                if gm.unitStatuses
                                    and bh.unitType
                                    and gm.unitStatuses[bh.unitType]
                                    and gm.unitStatuses[bh.unitType][CONSTANTS.ABILITIES.DRAGOON_1] then
                                    Event.emitLocal("SpawnFloatingText", {
                                        worldX = world.x + 0.01, worldY = world.y + 1.1,
                                        damage = 0, text = "BURN"
                                    })
                                end
                            end
                            break
                        end
                    end
                    Camera.startShake(1.5, 0.3)
                end

                if self.activeAbility then
                    self.activeAbility._currentResult = nil
                end
                self.activeAbility = nil
                self.resolveState = RESOLVE_DELAY
                self.stateTimer = 0
            end

        elseif self.resolveState == RESOLVE_DELAY then
            self.stateTimer = self.stateTimer + dt
            if self.stateTimer >= self.delayBetweenActions then
                self.resolveState = RESOLVE_NEXT_ACTION
            end
        end
    end,

    StartMovementResolve = function(self, result)
        local action = result.action
        local isOrder = (action.faction == CONSTANTS.ORDER_PLAYER)

        if isOrder then
            Event.emitLocal("FocusOrderGrid", {})
        else
            Event.emitLocal("FocusChaosGrid", {})
        end

        self.waitingForVisual = true
        self.visualWatchdog = 0
        self.trapHitHandled = false
        self.resolveState = RESOLVE_MOVING

        Log.info("[ResolutionPhase] Moving unit " .. tostring(action.unitType) ..
            " from (" .. tostring(action.param1) .. "," .. tostring(action.param2) .. ")" ..
            " to (" .. tostring(action.targetX) .. "," .. tostring(action.targetY) .. ")")

        -- Pass trap tile to unit so it can pause mid-movement
        local trapHitTile = nil
        if result.trapHit then
            trapHitTile = { x = result.trapHit.gridX, y = result.trapHit.gridY }
        end

        Event.emitLocal("ReplayMoveUnit", {
            unitType = action.unitType,
            isOrder = isOrder,
            fromX = action.param1 or 0,
            fromY = action.param2 or 0,
            toX = action.targetX,
            toY = action.targetY,
            trapHitTile = trapHitTile
        })
    end,

    EmitStatusInflicted = function(self, statusID, ownerUnit, targetUnit, isExpiring)
        local gm = ClientGameManager_instance
        local stackCount = 1

        if gm then
            local stackKey = self.STACK_STATUS_LOOKUP[statusID]
            if stackKey and gm.buffStacks then
                stackCount = tonumber(gm.buffStacks[stackKey]) or 0
                if stackCount < 1 then stackCount = 1 end
            elseif not self.NON_STACKING_STATUSES[statusID] then
                local currentStacks = gm.unitStatuses
                    and gm.unitStatuses[targetUnit]
                    and tonumber(gm.unitStatuses[targetUnit][statusID]) or 0
                stackCount = math.max(1, currentStacks + 1)
            end
        end

        Event.emitLocal("OnUnitStatusInflicted", {
            statusID = statusID,
            ownerUnit = ownerUnit,
            targetUnit = targetUnit,
            stackCount = stackCount,
            isExpiring = isExpiring and true or false
        })
    end,

    StartAbilityResolve = function(self, result)
        local action = result.action
        local gm = ClientGameManager_instance
        local ability = gm:GetAbilityByIDGlobal(action.abilityID)
        if not ability then
            Log.error("[ResolutionPhase] No ability found for ID " .. tostring(action.abilityID))
            self.resolveState = RESOLVE_DELAY
            self.stateTimer = 0
            return
        end

        local isOrder = (action.faction == CONSTANTS.ORDER_PLAYER)

        -- Focus the targeted grid
        if ability.targetsOpponent then
            if isOrder then
                Event.emitLocal("FocusChaosGrid", {})
            else
                Event.emitLocal("FocusOrderGrid", {})
            end
        else
            if isOrder then
                Event.emitLocal("FocusOrderGrid", {})
            else
                Event.emitLocal("FocusChaosGrid", {})
            end
        end

        -- Play the caster unit's ability animation
        local castInfo = ABILITY_CAST_MAP[action.abilityID]
        if castInfo then
            Event.emitLocal("OnUnitSupportCast", {
                unitType = castInfo.unit,
                animName = castInfo.anim
            })
        end

        -- Trigger animation preview (the slide-in ability card)
        local previewEvent = ABILITY_PREVIEW_MAP[action.abilityID]
        if previewEvent then
            -- Generic abilities need faction for the AnimPreview handler
            Event.emitLocal(previewEvent, { faction = action.faction })
        end

        self.waitingForVisual = true
        self.visualWatchdog = 0
        -- Scale timeout for abilities with berserk bounces (~2.4s per bounce + base)
        local bounceCount = (result and result.berserkBounces) and #result.berserkBounces or 0
        self.abilityVisualTimeout = 10.0 + bounceCount * 3.0
        self.activeAbility = ability
        self.resolveState = RESOLVE_ABILITY

        -- Notify AbilityQueueUI that this ability is now resolving (starts fade)
        local isQueuedAbility = (ability.abilityType ~= CONSTANTS.ABILITY_SUPPORT)
        Event.emitLocal("OnAbilityResolved", {
            player = action.faction,
            isQueuedAbility = isQueuedAbility,
            owner = action.faction
        })

        Log.info("[ResolutionPhase] Triggering ability " .. ability.name ..
            " at (" .. tostring(action.targetX) .. "," .. tostring(action.targetY) .. ")")

        -- Store result on ability so Trigger can access hit data (e.g. Imp_2 force move)
        ability._currentResult = result

        -- Pre-separate recast hits so UpdateTrigger can access them during animation
        if result and result.recastTiles and #result.recastTiles > 0 then
            local recastHits = {}
            local recastDamage = 0
            for _, hit in ipairs(result.hits or {}) do
                if hit.isRecast then
                    table.insert(recastHits, hit)
                    recastDamage = recastDamage + (hit.damage or 0)
                end
            end
            result._recastHits = recastHits
            result._recastDamage = recastDamage
        end

        -- Pre-compute deferred hit data so abilities can call ShowDamageNumbers() during animation
        local hits = result.hits or {}
        local damage = result.damage or 0
        local hasRecastHits = result._recastHits and #result._recastHits > 0
        local initialHits = {}
        local initialDamage = 0
        if hasRecastHits then
            for _, hit in ipairs(hits) do
                if not hit.isRecast then
                    table.insert(initialHits, hit)
                    initialDamage = initialDamage + (hit.damage or 0)
                end
            end
        end
        local displayHits = hasRecastHits and initialHits or hits
        local displayDamage = hasRecastHits and initialDamage or damage
        local fallbackDamage = ability.unitDamage or displayDamage
        local useTotalDamage = (#displayHits == 1 and not hasRecastHits)

        result._deferredHits = displayHits
        result._deferredDamage = displayDamage
        result._initialDamage = hasRecastHits and initialDamage or nil
        result._useTotalDamage = useTotalDamage
        result._fallbackDamage = fallbackDamage
        result._wrathConsumed = result.wrathConsumed

        ability:Trigger(gm.orderGrid, gm.chaosGrid,
            action.targetX, action.targetY,
            action.param1 or 0, action.param2 or 0)

        -- Hide trap VFX: in PnP hide all traps, in networked hide enemy traps only
        if ability._spawnedTrapVFX and (PassAndPlayServer_instance or action.faction ~= gm.playerFaction) then
            ECS.setMeshRenderer(ability._spawnedTrapVFX, {
                colorMultiply = CONSTANTS.COLOR_NONE
            })
        end
    end,

    ApplyAbilityResults = function(self, result)
        local gm = ClientGameManager_instance
        local hits = result.hits or {}
        local damage = result.damage or 0
        local healAmount = result.healAmount or 0
        local healPlayer = result.healPlayer or 0
        local damagePlayer = result.damagePlayer or 0
        local tileEffects = result.tileEffects or {}

        Log.info("[ResolutionPhase] ApplyAbilityResults: hits=" .. #hits ..
            " damage=" .. damage .. " heal=" .. healAmount ..
            " tileEffects=" .. #tileEffects)

        -- Feather Cyclone handles its own per-feather damage display
        local featherCycloneHandled = (result.featherHitData ~= nil)

        -- Use pre-computed display hits from before Trigger() was called
        local displayHits = result._deferredHits or hits
        local displayDamage = result._deferredDamage or damage
        local hasRecastHits = result._recastHits and #result._recastHits > 0

        local ability = self.activeAbility

        if #displayHits > 0 and not featherCycloneHandled then
            -- OnTilesHit is now emitted by ShowDamageNumbers() during the animation
            -- Only emit here as fallback if hits weren't shown (e.g., ability didn't call ShowDamageNumbers)
            if result._deferredHits then
                Event.emitLocal("OnTilesHit", { hits = displayHits })
            end

            local actualHits = #displayHits

            -- Update shield stacks when units with shields take damage
            for _, hit in ipairs(displayHits) do
                if hit.shielded and hit.unitType and hit.shieldStacksRemaining ~= nil then
                    local stacksRemaining = hit.shieldStacksRemaining or 0
                    if stacksRemaining > 0 then
                        -- Update shield stack count
                        Event.emitLocal("OnUnitStatusInflicted", {
                            statusID = CONSTANTS.ABILITIES.ORACLE_2,
                            ownerUnit = 0,
                            targetUnit = hit.unitType,
                            stackCount = stacksRemaining,
                            isExpiring = false
                        })
                    else
                        -- Shield depleted, remove it
                        Event.emitLocal("OnUnitStatusRemoved", {
                            statusID = CONSTANTS.ABILITIES.ORACLE_2,
                            targetUnit = hit.unitType
                        })
                        Event.emitLocal("RemoveShield", {
                            unitType = hit.unitType
                        })
                    end
                end
            end

            -- Camera shake scaled by actual (non-shielded) hits and total damage
            if actualHits > 0 then
                local shakeIntensity = math.min(2.0 + (actualHits * 1.5) + (displayDamage * 0.1), 8.0)
                local shakeDuration = math.min(0.5 + (actualHits * 0.15), 1.2)
                Log.info("[ResolutionPhase] Camera shake: intensity=" .. shakeIntensity .. " duration=" .. shakeDuration)
                Camera.startShake(shakeIntensity, shakeDuration)
            end

        elseif damage == 0 and self.activeAbility
            and self.activeAbility.abilityType == CONSTANTS.ABILITY_OFFENSIVE
            and result.action then
            -- Offensive ability that missed all units: show MISS at target
            local action = result.action
            local isOrder = (action.faction == CONSTANTS.ORDER_PLAYER)
            local targetGrid = isOrder and gm.chaosGrid or gm.orderGrid
            if targetGrid then
                local world = Grid.gridToWorld(targetGrid.gridID, action.targetX, action.targetY)
                Event.emitLocal("SpawnFloatingText", {
                    worldX = world.x + 0.01,
                    worldY = world.y + 0.5,
                    damage = 0
                })
            end
        end

        -- Update player health bar for damage (skip for Feather Cyclone, it applies per-feather)
        if damage > 0 and damagePlayer > 0 and not featherCycloneHandled then
            if hasRecastHits then
                -- Apply only initial damage now; recast damage applied by ability's UpdateTrigger
                local initialDamage = result._initialDamage or 0
                if initialDamage > 0 then
                    gm:ApplyDamage(damagePlayer, initialDamage)
                end
            else
                gm:ApplyDamage(damagePlayer, damage)
            end
        end

        -- Update player health bar for healing
        if healAmount > 0 and healPlayer > 0 then
            gm:ApplyHeal(healPlayer, healAmount)
        end

        -- Update tile visuals for tile damage
        for _, effect in ipairs(tileEffects) do
            local grid = effect.isOrder and gm.orderGrid or gm.chaosGrid
            if grid.Tiles[effect.tileIdx] then
                grid.Tiles[effect.tileIdx].health = effect.newHealth
                grid:UpdateTileTexture(effect.tileIdx)
            end
        end

        -- Newly placed traps start unarmed (armed next resolution phase)
        if result.trap then
            local grid = result.trap.isOrder and gm.orderGrid or gm.chaosGrid
            if grid.Tiles[result.trap.tileIdx] then
                local vfxEntity = nil
                if self.activeAbility and self.activeAbility._spawnedTrapVFX then
                    vfxEntity = self.activeAbility._spawnedTrapVFX
                    self.activeAbility._spawnedTrapVFX = nil
                end
                grid.Tiles[result.trap.tileIdx].trap = {
                    owner = result.trap.owner,
                    damage = result.trap.damage,
                    sideDamage = result.trap.sideDamage,
                    armed = false,
                    vfxEntity = vfxEntity,
                    isConsTrap = result.trap.isConsTrap or false,
                    isBleedTrap = result.trap.isBleedTrap or false
                }
                Log.info("[ResolutionPhase] Placed trap at tileIdx=" .. result.trap.tileIdx .. " vfxEntity=" .. tostring(vfxEntity))
                -- Show trap indicator: in PnP hide all during resolution,
                -- in networked show only the local player's traps
                local isLocalPlayerTrap = (result.trap.owner == gm.playerFaction)
                if isLocalPlayerTrap and not PassAndPlayServer_instance then
                    local row = math.floor((result.trap.tileIdx - 1) / CONSTANTS.GRID_COLS)
                    local col = (result.trap.tileIdx - 1) % CONSTANTS.GRID_COLS
                    Grid.setTileColorAdd(grid.gridID, row, col, 0, CONSTANTS.COLOR_TRAP_INDICATOR)
                end
                -- Hide VFX entity for enemy traps (networked) or all traps (PnP) until triggered
                if (PassAndPlayServer_instance or not isLocalPlayerTrap) and vfxEntity then
                    ECS.setMeshRenderer(vfxEntity, {
                        colorMultiply = CONSTANTS.COLOR_NONE
                    })
                end
            end
        end

        -- Emit status effect events for UI
        local action = result.action
        if action then
            local castInfo = ABILITY_CAST_MAP[action.abilityID]
            local casterUnit = castInfo and castInfo.unit or 0

            -- Burn: inflicted on hit enemy units by Dragoon_1 (ID 3)
            if result.burnApplied and result.hits then
                for _, hit in ipairs(result.hits) do
                    if hit.unitType and not hit.shielded then
                        local hitGrid = hit.isOrderGrid and gm.orderGrid or gm.chaosGrid
                        if hitGrid then
                            local world = Grid.gridToWorld(hitGrid.gridID, hit.gridX, hit.gridY)
                            Event.emitLocal("SpawnFloatingText", {
                                worldX = world.x + 0.01,
                                worldY = world.y + 0.8,
                                damage = 0,
                                text = "Burn"
                            })
                        end
                        self:EmitStatusInflicted(
                            CONSTANTS.ABILITIES.DRAGOON_1,
                            casterUnit,
                            hit.unitType,
                            true
                        )
                    end
                end
            end

            -- Fervor: self-buff on Tirion (ID 9)
            if result.fervorGranted then
                self:EmitStatusInflicted(
                    CONSTANTS.ABILITIES.TIRION_1,
                    casterUnit,
                    CONSTANTS.UNIT.TIRION,
                    false
                )
            end

            -- Wrath: faction-wide buff from Eye_2 (ID 8)
            if result.wrathGranted then
                self:EmitStatusInflicted(
                    CONSTANTS.ABILITIES.EYE_2,
                    casterUnit,
                    CONSTANTS.UNIT.EYE,
                    true
                )
            end

            -- Shield: granted by Oracle_2 (ID 2)
            if result.shieldTarget and result.shieldAmount then
                Event.emitLocal("OnUnitStatusInflicted", {
                    statusID = CONSTANTS.ABILITIES.ORACLE_2,
                    ownerUnit = 0,
                    targetUnit = result.shieldTarget,
                    stackCount = result.shieldAmount,
                    isExpiring = false
                })
            end

            -- Bleed: applied via trap trigger (Vizom_2, ID 12)
            if result.bleedHit and result.bleedHit.unitType then
                self:EmitStatusInflicted(
                    CONSTANTS.ABILITIES.VIZOM_2,
                    CONSTANTS.UNIT.VIZOM,
                    result.bleedHit.unitType,
                    true
                )
            end

            -- Update shield stacks if bleed hit a shielded unit
            if result.bleedHit and result.bleedHit.shielded and result.bleedHit.unitType and result.bleedHit.shieldStacksRemaining ~= nil then
                local stacksRemaining = result.bleedHit.shieldStacksRemaining or 0
                if stacksRemaining > 0 then
                    Event.emitLocal("OnUnitStatusInflicted", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        ownerUnit = 0,
                        targetUnit = result.bleedHit.unitType,
                        stackCount = stacksRemaining,
                        isExpiring = false
                    })
                else
                    Event.emitLocal("OnUnitStatusRemoved", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        targetUnit = result.bleedHit.unitType
                    })
                    Event.emitLocal("RemoveShield", {
                        unitType = result.bleedHit.unitType
                    })
                end
            end

            -- Berserk: granted to Vizom when bleed triggers (Vizom_1, ID 11)
            if result.berserkGranted then
                -- Update local buffStacks with the stacks gained
                local stacksGained = result.berserkStacksGained or 1
                if gm.buffStacks then
                    gm.buffStacks.berserk = (gm.buffStacks.berserk or 0) + stacksGained
                end
                self:EmitStatusInflicted(
                    CONSTANTS.ABILITIES.VIZOM_1,
                    CONSTANTS.UNIT.VIZOM,
                    CONSTANTS.UNIT.VIZOM,
                    false
                )
            end

            -- Hunter's Mark consumed (Passive_2, ID 17) — remove the mark debuff
            if result.hunterMarkConsumed then
                Event.emitLocal("OnUnitStatusRemoved", {
                    statusID = CONSTANTS.ABILITIES.PASSIVE_2,
                    targetUnit = result.hunterMarkConsumed.markedUnit
                })
            end
        end
    end,

    -- Handle trap hit visuals/damage (called mid-movement or as fallback after movement)
    HandleTrapHitVisuals = function(self)
        if self.trapHitHandled then return end
        self.trapHitHandled = true

        local gm = ClientGameManager_instance
        local result = self.preCalculatedResults[self.currentActionIndex]
        if not result or not result.trapHit then return end

        Log.info("[ResolutionPhase] HandleTrapHitVisuals: grid=" .. tostring(result.trapHit.isOrderGrid) ..
            " pos=(" .. tostring(result.trapHit.gridX) .. "," .. tostring(result.trapHit.gridY) ..
            ") damage=" .. tostring(result.trapHit.damage))

        local destIdx = result.trapHit.gridY * CONSTANTS.GRID_COLS + result.trapHit.gridX + 1

        -- Get stored VFX entity BEFORE clearing trap data
        local trapGrid = result.trapHit.isOrderGrid and gm.orderGrid or gm.chaosGrid
        local vfxEntity = nil
        if trapGrid and trapGrid.Tiles[destIdx] and trapGrid.Tiles[destIdx].trap then
            vfxEntity = trapGrid.Tiles[destIdx].trap.vfxEntity
        end

        -- Clear trap data and color indicator
        self:RemoveTrap(result.trapHit.isOrderGrid, destIdx)

        -- Trigger the existing VFX entity (plays "Trigger" animation, then self-destructs)
        if vfxEntity then
            -- Reveal hidden enemy trap VFX so the trigger animation is visible
            ECS.setMeshRenderer(vfxEntity, {
                colorMultiply = CONSTANTS.COLOR_WHITE
            })
            Log.info("[ResolutionPhase] Triggering trap VFX entity " .. tostring(vfxEntity))
            Event.emitLocal("OnTrapTriggered", {
                vfxEntity = vfxEntity,
                unitType = result.trapHit.unitType,
                gridX = result.trapHit.gridX,
                gridY = result.trapHit.gridY,
                damage = result.trapHit.damage
            })
        else
            Log.warning("[ResolutionPhase] No VFX entity stored for trap at idx=" .. destIdx)
        end

        -- Play trap trigger audio
        Event.emitLocal("PlayVFXTirionVizomA2", {})

        -- Don't emit OnTilesHit for traps — the unit already handles its own
        -- Hit animation pause inline in ReplayMoveState

        local trapPlayer = result.trapHit.isOrderGrid and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER

        if result.trapHit.isConsTrap then
            -- === CONSECRATION 3x3 AoE VISUALS ===
            local centerX = result.trapHit.gridX
            local centerY = result.trapHit.gridY

            -- tileXCorrection for AoE VFX
            local tileXCorrection = {
                [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
            }

            -- Spawn VFX on surrounding tiles in the 3x3 blast radius (center already has the original trap VFX)
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local tx = centerX + dx
                    local ty = centerY + dy
                    if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                        -- Color flash on each AoE tile
                        Grid.setTileColorAdd(trapGrid.gridID, ty, tx, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT)
                        -- Spawn explosion VFX on surrounding tiles, play "Trigger" animation (not "Fire")
                        if dx ~= 0 or dy ~= 0 then
                            local world = Grid.gridToWorld(trapGrid.gridID, tx, ty)
                            local aoeXCorrection = tileXCorrection[ty] and tileXCorrection[ty][tx] or 0
                            local aoeVFX = Prefab.spawn("Tirion_Ability2_VFX", world.x + 0.01 + aoeXCorrection, world.y + 0.5)
                            if aoeVFX then
                                ECS.setMeshRenderer(aoeVFX, { colorMultiply = CONSTANTS.COLOR_WHITE })
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

            -- Per-unit floating damage numbers for INITIAL detonation only (detonationIdx == 1)
            -- Chain detonation hits are shown later by _consChainState
            local initialConsDmg = 0
            if result.trapHit.consHits and #result.trapHit.consHits > 0 then
                local hitUnits = {}
                local hasInitialHits = false
                for _, hit in ipairs(result.trapHit.consHits) do
                    if not hit.detonationIdx or hit.detonationIdx == 1 then
                        hasInitialHits = true
                        local world = Grid.gridToWorld(trapGrid.gridID, hit.gridX, hit.gridY)
                        Event.emitLocal("SpawnFloatingText", {
                            worldX = world.x + 0.01,
                            worldY = world.y + 0.5,
                            damage = hit.damage or 0
                        })
                        initialConsDmg = initialConsDmg + (hit.damage or 0)
                        -- Track hit units for hit animation
                        if hit.unitType then
                            hitUnits[hit.unitType] = { gridX = hit.gridX, gridY = hit.gridY }
                        end
                    end
                end
                if not hasInitialHits then
                    -- No unit hits in initial AoE but still show total damage at center
                    local world = Grid.gridToWorld(trapGrid.gridID, centerX, centerY)
                    Event.emitLocal("SpawnFloatingText", {
                        worldX = world.x + 0.01,
                        worldY = world.y + 0.5,
                        damage = result.trapHit.damage or 0
                    })
                    initialConsDmg = result.trapHit.damage or 0
                end
                -- Trigger hit reaction on each affected unit
                local hitList = {}
                for ut, pos in pairs(hitUnits) do
                    table.insert(hitList, {
                        unitType = ut,
                        isOrderGrid = result.trapHit.isOrderGrid,
                        gridX = pos.gridX,
                        gridY = pos.gridY
                    })
                end
                if #hitList > 0 then
                    Event.emitLocal("OnTilesHit", { hits = hitList })
                end
            else
                -- No unit hits but still show total damage at center
                local world = Grid.gridToWorld(trapGrid.gridID, centerX, centerY)
                Event.emitLocal("SpawnFloatingText", {
                    worldX = world.x + 0.01,
                    worldY = world.y + 0.5,
                    damage = result.trapHit.damage or 0
                })
                initialConsDmg = result.trapHit.damage or 0
            end

            -- Apply only initial detonation damage to health bar (chain damage applied by _consChainState)
            if initialConsDmg > 0 then
                gm:ApplyDamage(trapPlayer, initialConsDmg)
            end

            -- Bigger camera shake for AoE detonation
            local hitCount = result.trapHit.consHits and #result.trapHit.consHits or 1
            local shakeIntensity = math.min(3.0 + (hitCount * 2.0), 8.0)
            Camera.startShake(shakeIntensity, 0.5)

            -- Build chain detonation queue (skip the first index, already handled above)
            local chainQueue = {}
            if result.trapHit.detonatedTrapIndices and #result.trapHit.detonatedTrapIndices > 1 then
                for i = 2, #result.trapHit.detonatedTrapIndices do
                    table.insert(chainQueue, result.trapHit.detonatedTrapIndices[i])
                end
            end

            -- Set up chain detonation state (handles both chaining and cleanup)
            self._consChainState = {
                timer = 0,
                phase = "flash",  -- "flash" = showing current AoE, "chain_delay" = waiting for next detonation
                flashDuration = self.consAoEFlashDuration,
                chainDelay = self.consChainDetonationDelay,
                gridID = trapGrid.gridID,
                trapGrid = trapGrid,
                isOrderGrid = result.trapHit.isOrderGrid,
                -- Track all AoE tiles that need cleanup (accumulate across chain)
                allAoETiles = {},
                chainQueue = chainQueue,
                chainIdx = 0,
                consHits = result.trapHit.consHits or {}
            }
            -- Record initial AoE tiles for cleanup
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local tx = centerX + dx
                    local ty = centerY + dy
                    if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                        table.insert(self._consChainState.allAoETiles, { x = tx, y = ty })
                    end
                end
            end

        elseif result.trapHit.isBleedTrap then
            -- === BLEED TRAP VISUALS ===
            -- Apply bleed trap's direct damage to health bar
            if result.trapHit.damage and result.trapHit.damage > 0 then
                gm:ApplyDamage(trapPlayer, result.trapHit.damage)
            end

            Camera.startShake(3.0, 0.4)

            if trapGrid then
                local world = Grid.gridToWorld(trapGrid.gridID, result.trapHit.gridX, result.trapHit.gridY)
                -- Show trap damage number
                if result.trapHit.damage and result.trapHit.damage > 0 then
                    Event.emitLocal("SpawnFloatingText", {
                        worldX = world.x + 0.01,
                        worldY = world.y + 0.5,
                        damage = result.trapHit.damage
                    })
                end
                -- Show BLEED status text above the damage
                Event.emitLocal("BleedProc", { unitType = result.trapHit.unitType })
                Event.emitLocal("SpawnFloatingText", {
                    worldX = world.x + 0.01,
                    worldY = world.y + 0.8,
                    damage = 0,
                    text = "BLEED"
                })
                if gm.unitStatuses
                    and result.trapHit.unitType
                    and gm.unitStatuses[result.trapHit.unitType]
                    and gm.unitStatuses[result.trapHit.unitType][CONSTANTS.ABILITIES.DRAGOON_1] then
                    Event.emitLocal("SpawnFloatingText", {
                        worldX = world.x + 0.01,
                        worldY = world.y + 1.1,
                        damage = 0,
                        text = "BURN"
                    })
                end
                -- Inflict bleed debuff on the unit that stepped on the trap
                if result.trapHit.unitType then
                    self:EmitStatusInflicted(
                        CONSTANTS.ABILITIES.VIZOM_2,
                        CONSTANTS.UNIT.VIZOM,
                        result.trapHit.unitType,
                        true
                    )
                end
                -- Bleed trap trigger grants Vizom Berserk
                if result.berserkGranted then
                    -- Update local buffStacks with the stacks gained
                    local stacksGained = result.berserkStacksGained or 1
                    if gm.buffStacks then
                        gm.buffStacks.berserk = (gm.buffStacks.berserk or 0) + stacksGained
                    end
                    self:EmitStatusInflicted(
                        CONSTANTS.ABILITIES.VIZOM_1,
                        CONSTANTS.UNIT.VIZOM,
                        CONSTANTS.UNIT.VIZOM,
                        false
                    )
                end
            end

            -- Update shield stacks if trap hit a shielded unit
            if result.trapHit.shielded and result.trapHit.unitType and result.trapHit.shieldStacksRemaining ~= nil then
                local stacksRemaining = result.trapHit.shieldStacksRemaining or 0
                if stacksRemaining > 0 then
                    Event.emitLocal("OnUnitStatusInflicted", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        ownerUnit = 0,
                        targetUnit = result.trapHit.unitType,
                        stackCount = stacksRemaining,
                        isExpiring = false
                    })
                else
                    Event.emitLocal("OnUnitStatusRemoved", {
                        statusID = CONSTANTS.ABILITIES.ORACLE_2,
                        targetUnit = result.trapHit.unitType
                    })
                    Event.emitLocal("RemoveShield", {
                        unitType = result.trapHit.unitType
                    })
                end
            end

        else
            -- === GENERIC TRAP VISUALS ===
            if not result.trapHit.isBleedTrap then
                gm:ApplyDamage(trapPlayer, result.trapHit.damage)
            end
            Camera.startShake(3.0, 0.4)

            if trapGrid then
                local world = Grid.gridToWorld(trapGrid.gridID, result.trapHit.gridX, result.trapHit.gridY)
                Event.emitLocal("SpawnFloatingText", {
                    worldX = world.x + 0.01,
                    worldY = world.y + 0.5,
                    damage = result.trapHit.damage or 0
                })
            end
        end
    end,

    -- Remove a triggered trap from the actual grid and clear its visual
    RemoveTrap = function(self, isOrder, tileIdx)
        local gm = ClientGameManager_instance
        local grid = isOrder and gm.orderGrid or gm.chaosGrid
        if grid.Tiles[tileIdx] then
            -- Destroy trap VFX entity if still exists (fallback)
            if grid.Tiles[tileIdx].trap and grid.Tiles[tileIdx].trap.vfxEntity then
                Log.info("[ResolutionPhase] Destroying trap VFX entity " .. tostring(grid.Tiles[tileIdx].trap.vfxEntity))
            end
            grid.Tiles[tileIdx].trap = nil
            -- Clear the trap indicator color
            local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
            local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
            Grid.setTileColorAdd(grid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
            Log.info("[ResolutionPhase] Removed trap at tile " .. tileIdx .. " (row=" .. row .. ", col=" .. col .. ")")
        end
    end,

    FinishResolution = function(self)
        self.isResolving = false
        Log.info("[ResolutionPhase] ====== RESOLUTION COMPLETE ====== (resolved " .. self.currentActionIndex .. "/" .. self.totalActions .. " actions)")

        local gm = ClientGameManager_instance
        if gm then
            -- Validate damage tracking: compare current health with expected final health
            if gm.resolutionData and gm.resolutionData.finalHealth and self._startingHealth then
                local fh = gm.resolutionData.finalHealth
                local currentOrder = gm:GetCurrentHealth(true)
                local currentChaos = gm:GetCurrentHealth(false)
                
                -- Log mismatch if client's accumulated damage doesn't match server
                if fh.order and currentOrder ~= fh.order then
                    Log.warning("[ResolutionPhase] Order health mismatch! Client: " .. currentOrder .. 
                        ", Server: " .. fh.order .. " (diff: " .. (currentOrder - fh.order) .. ")")
                end
                if fh.chaos and currentChaos ~= fh.chaos then
                    Log.warning("[ResolutionPhase] Chaos health mismatch! Client: " .. currentChaos .. 
                        ", Server: " .. fh.chaos .. " (diff: " .. (currentChaos - fh.chaos) .. ")")
                end
            end
            self._startingHealth = nil

            -- Sync final health from server if provided
            if gm.resolutionData and gm.resolutionData.finalHealth then
                local fh = gm.resolutionData.finalHealth
                if fh.order then
                    gm:SetHealth(true, fh.order)
                end
                if fh.chaos then
                    gm:SetHealth(false, fh.chaos)
                end
                Event.emitLocal("PlayHealthBarAnimations", {})
            end

            -- Check for winner
            local winner = gm.resolutionData and gm.resolutionData.winner or 0
            if winner and winner > 0 then
                -- Transition to EndGame phase for tile destruction animation
                Log.info("[ResolutionPhase] Winner detected (" .. winner .. "), transitioning to EndGame phase")
                if gm.turnPhases and gm.turnPhases.endGamePhase then
                    gm.turnPhases:ChangePhase(gm.turnPhases.endGamePhase)
                else
                    -- Fallback: show banner directly if no endGamePhase
                    Log.warning("[ResolutionPhase] No endGamePhase found, showing banner directly")
                    if winner == CONSTANTS.ORDER_PLAYER then
                        Log.info("[ResolutionPhase] === ORDER WINS ===")
                        Event.emitLocal("ShowPhaseOverlay", { phase = "OrderWin" })
                        Event.emitLocal("PlayCombatOrderWin", {})
                        Event.emitLocal("PlayAnnouncerOrderWins", {})
                    elseif winner == CONSTANTS.CHAOS_PLAYER then
                        Log.info("[ResolutionPhase] === CHAOS WINS ===")
                        Event.emitLocal("ShowPhaseOverlay", { phase = "ChaosWin" })
                        Event.emitLocal("PlayCombatChaosWin", {})
                        Event.emitLocal("PlayAnnouncerChaosWins", {})
                    else
                        Log.info("[ResolutionPhase] === DRAW ===")
                        Event.emitLocal("ShowPhaseOverlay", { phase = "Draw" })
                        Event.emitLocal("PlayCombatDraw", {})
                        Event.emitLocal("PlayAnnouncerDraw", {})
                    end
                    gm.resolutionData = nil
                end
                return
            end

            -- Remove expired status effects based on server's active buff snapshot
            local activeBuffs = (gm.resolutionData and gm.resolutionData.activeBuffs) or {}
            if gm.resolutionData and gm.resolutionData.buffStacks then
                gm.buffStacks = {
                    fervor = gm.resolutionData.buffStacks.fervor or 0,
                    berserk = gm.resolutionData.buffStacks.berserk or 0
                }
            else
                gm.buffStacks = gm.buffStacks or { fervor = 0, berserk = 0 }
                local fervorPresent = false
                local berserkPresent = false
                for _, buff in ipairs(activeBuffs) do
                    local buffUnit = tonumber(buff and buff.unit)
                    local buffStatusID = tonumber(buff and buff.statusID)
                    if buffUnit == CONSTANTS.UNIT.TIRION and buffStatusID == CONSTANTS.ABILITIES.TIRION_1 then
                        fervorPresent = true
                    elseif buffUnit == CONSTANTS.UNIT.VIZOM and buffStatusID == CONSTANTS.ABILITIES.VIZOM_1 then
                        berserkPresent = true
                    end
                end
                gm.buffStacks.fervor = fervorPresent and math.max(1, gm.buffStacks.fervor or 0) or 0
                gm.buffStacks.berserk = berserkPresent and math.max(1, gm.buffStacks.berserk or 0) or 0
            end
            if gm.unitStatuses and gm.unitStatuses[CONSTANTS.UNIT.TIRION]
                and gm.unitStatuses[CONSTANTS.UNIT.TIRION][CONSTANTS.ABILITIES.TIRION_1] then
                gm.unitStatuses[CONSTANTS.UNIT.TIRION][CONSTANTS.ABILITIES.TIRION_1] = math.max(1, gm.buffStacks.fervor or 0)
            end
            if gm.unitStatuses and gm.unitStatuses[CONSTANTS.UNIT.VIZOM]
                and gm.unitStatuses[CONSTANTS.UNIT.VIZOM][CONSTANTS.ABILITIES.VIZOM_1] then
                gm.unitStatuses[CONSTANTS.UNIT.VIZOM][CONSTANTS.ABILITIES.VIZOM_1] = math.max(1, gm.buffStacks.berserk or 0)
            end
            local survivingSet = {}
            for _, buff in ipairs(activeBuffs) do
                local buffUnit = tonumber(buff and buff.unit)
                local buffStatusID = tonumber(buff and buff.statusID)
                if buffUnit and buffStatusID then
                    -- Buffs marked as expiring should NOT survive into next turn
                    -- (Burn/Bleed that are blinking this turn expire at end of resolution)
                    if not buff.isExpiring then
                        local key = buffUnit .. "_" .. buffStatusID
                        survivingSet[key] = true
                    end
                else
                    Log.warning("[ResolutionPhase] Skipping malformed activeBuff entry in FinishResolution()")
                end
            end
            local snapshotManagedStatuses = {
                [CONSTANTS.ABILITIES.DRAGOON_1] = true, -- Burn
                [CONSTANTS.ABILITIES.VIZOM_2] = true,   -- Bleed
                [CONSTANTS.ABILITIES.TIRION_1] = true,  -- Fervor
                [CONSTANTS.ABILITIES.VIZOM_1] = true,   -- Berserk
                [CONSTANTS.ABILITIES.EYE_2] = true,     -- Wrath (always end-of-resolution)
                [CONSTANTS.ABILITIES.ORACLE_2] = true   -- UnitShield (stacks, removed when depleted)
            }

            -- Reconcile client status state against authoritative server snapshot.
            -- This guarantees stale debuffs/buffs are removed even if they were not touched this resolution.
            local pendingRemovals = {}
            if gm.unitStatuses then
                for unitType, statusMap in pairs(gm.unitStatuses) do
                    if statusMap then
                        for statusID, _ in pairs(statusMap) do
                            if snapshotManagedStatuses[statusID] then
                                local key = unitType .. "_" .. statusID
                                if not survivingSet[key] then
                                    pendingRemovals[#pendingRemovals + 1] = {
                                        statusID = statusID,
                                        targetUnit = unitType
                                    }
                                end
                            end
                        end
                    end
                end
            end

            for _, removal in ipairs(pendingRemovals) do
                Event.emitLocal("OnUnitStatusRemoved", removal)
                -- Also remove shield VFX when Oracle_2 expires
                if removal.statusID == CONSTANTS.ABILITIES.ORACLE_2 then
                    Event.emitLocal("RemoveShield", {
                        unitType = removal.targetUnit
                    })
                end
            end

            if gm.unitStatusExpiring then
                for unitType, statusMap in pairs(gm.unitStatusExpiring) do
                    if statusMap then
                        for statusID, _ in pairs(statusMap) do
                            if snapshotManagedStatuses[statusID] then
                                local key = unitType .. "_" .. statusID
                                if not survivingSet[key] then
                                    statusMap[statusID] = nil
                                end
                            end
                        end
                    end
                end
            end

            -- Arm all unarmed traps so they appear armed in next prep phase
            self:ArmExistingTraps(gm)

            Log.info("[ResolutionPhase] Returning to preparation phase, turn " .. (gm.turnNumber + 1))
            gm.resolutionData = nil
            gm.turnNumber = gm.turnNumber + 1
            if PassAndPlayServer_instance then
                gm.playerFaction = CONSTANTS.ORDER_PLAYER
                Persistent.playerFaction = CONSTANTS.ORDER_PLAYER
            end
            if gm.turnPhases and gm.turnPhases.preparationPhase then
                gm.turnPhases:ChangePhase(gm.turnPhases.preparationPhase)
            else
                Log.error("[ResolutionPhase] Cannot transition to preparation phase: turn phase object missing")
            end
        end
    end
})

return class
