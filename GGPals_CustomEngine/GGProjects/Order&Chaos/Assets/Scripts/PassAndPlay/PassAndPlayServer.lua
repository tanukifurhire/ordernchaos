PassAndPlayServer_instance = PassAndPlayServer_instance or nil

-- Maps abilityID to the unit type that casts it
local ABILITY_OWNER_MAP = {
    [1] = 2, [2] = 2,   -- Oracle
    [3] = 1, [4] = 1,   -- Dragoon
    [5] = 4, [6] = 4,   -- Imp
    [7] = 5, [8] = 5,   -- Eye
    [9] = 3, [10] = 3,  -- Tirion
    [11] = 6, [12] = 6  -- Vizom
}

-- Server-side deterministic random number generator
local ServerRandomState = 12345

local function ServerRandom(min, max)
    local a = 1664525
    local c = 1013904223
    local m = 2^32
    ServerRandomState = (a * ServerRandomState + c) % m
    if min and max then
        return min + (ServerRandomState % (max - min + 1))
    else
        return ServerRandomState / m
    end
end

-- Get N random unbroken tiles from a health map
local function GetRandomUnbrokenTiles(healthMap, count)
    local valid = {}
    for i = 1, CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS do
        if healthMap[i] and healthMap[i] > 0 then
            table.insert(valid, i)
        end
    end
    if #valid < count then count = #valid end
    local selected = {}
    for n = 1, count do
        local pick = ServerRandom(1, #valid)
        table.insert(selected, valid[pick])
        table.remove(valid, pick)
    end
    return selected
end

-- Get random adjacent/diagonal unbroken tile around a position
local function GetRandomAdjacentTile(healthMap, centerX, centerY, usedDirs)
    local directions = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    }
    usedDirs = usedDirs or {}

    local preferred = {}
    local fallback = {}
    for _, dir in ipairs(directions) do
        local tx = centerX + dir[1]
        local ty = centerY + dir[2]
        if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
            local idx = ty * CONSTANTS.GRID_COLS + tx + 1
            if healthMap[idx] and healthMap[idx] > 0 then
                local dirKey = dir[1] .. "," .. dir[2]
                if not usedDirs[dirKey] then
                    table.insert(preferred, { x = tx, y = ty, idx = idx, dirKey = dirKey })
                else
                    table.insert(fallback, { x = tx, y = ty, idx = idx, dirKey = dirKey })
                end
            end
        end
    end

    local pool = #preferred > 0 and preferred or fallback
    if #pool == 0 then return nil end
    local pick = ServerRandom(1, #pool)
    return pool[pick]
end

-- Build passive lookup tables from an arsenal map
-- Returns: unitPassives[unitType][passiveID] = true, abilityToUnit[abilityID] = unitType
local function BuildPassiveLookup(arsenalMap)
    local unitPassives = {}
    local abilityToUnit = {}
    if not arsenalMap then return unitPassives, abilityToUnit end

    for originalSlot, equippedID in pairs(arsenalMap) do
        -- Virtual passive slots use offset +100 (e.g. slot 101 = passive for unit at slot 1)
        local realSlot = originalSlot > 100 and (originalSlot - 100) or originalSlot
        local unitType = CONSTANTS.ABILITY_SLOT_TO_UNIT[realSlot]
        if unitType then
            if equippedID >= 16 and equippedID <= 18 then
                if not unitPassives[unitType] then unitPassives[unitType] = {} end
                unitPassives[unitType][equippedID] = true
            else
                abilityToUnit[equippedID] = unitType
            end
        end
    end
    -- Default mappings for non-remapped slots
    for slot = 1, 12 do
        if not arsenalMap[slot] then
            local unitType = CONSTANTS.ABILITY_SLOT_TO_UNIT[slot]
            if unitType then
                abilityToUnit[slot] = unitType
            end
        end
    end
    return unitPassives, abilityToUnit
end

-- Generate Hunter's Mark targets per faction (PASSIVE_2).
-- Returns:
--   hunterMarks[markerUnitType] = markedEnemyUnitType (for client UI ownership text/icon)
--   hunterMarkCounts[faction] = number of units bringing Hunter's Mark
--   hunterMarkedByFaction[faction] = markedEnemyUnitType
local function GenerateHunterMarks(factionPassives, unitPositions)
    local hunterMarks = {}
    local hunterMarkCounts = {
        [CONSTANTS.ORDER_PLAYER] = 0,
        [CONSTANTS.CHAOS_PLAYER] = 0
    }
    local hunterMarkedByFaction = {
        [CONSTANTS.ORDER_PLAYER] = nil,
        [CONSTANTS.CHAOS_PLAYER] = nil
    }

    for faction = CONSTANTS.ORDER_PLAYER, CONSTANTS.CHAOS_PLAYER do
        local markerUnits = {}
        local passives = factionPassives[faction]
        if passives then
            for unitType, pList in pairs(passives) do
                if pList[CONSTANTS.ABILITIES.PASSIVE_2] then
                    table.insert(markerUnits, unitType)
                end
            end
        end

        hunterMarkCounts[faction] = #markerUnits
        if #markerUnits > 0 then
            local enemies = {}
            for ut, _ in pairs(unitPositions) do
                local enemyFaction = (ut <= 3) and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                if enemyFaction ~= faction then
                    table.insert(enemies, ut)
                end
            end
            if #enemies > 0 then
                local markedEnemy = enemies[ServerRandom(1, #enemies)]
                local markerUnit = markerUnits[1]
                hunterMarks[markerUnit] = markedEnemy
                hunterMarkedByFaction[faction] = markedEnemy
                Log.info("[PnPServer] Hunter's Mark: faction " .. faction .. " (" .. #markerUnits .. " marker unit(s)) marked enemy unit " .. markedEnemy)
            end
        end
    end

    return hunterMarks, hunterMarkCounts, hunterMarkedByFaction
end

-- Server-side ability definitions for pre-calculation
local SERVER_ABILITY_DATA = {
    -- Oracle 1 - Crystal Cascade: 4 random unbroken tiles on enemy grid
    [1] = {
        cost = 3, targetsOpponent = true, isSupport = false, isTrap = false,
        isRandomTarget = true,
        crystalCount = 4,
        unitDamage = 7, tileDamage = 3,
        GetTargetTiles = function(targetX, targetY)
            return {}
        end
    },
    -- Oracle 2 - Fiberglass Finesse: 5HP shield (absorbs damage until depleted, lasts until end of resolution)
    [2] = {
        cost = 1, targetsOpponent = false, isSupport = true, isTrap = false, isShield = true,
        shieldAmount = 5,
        unitDamage = 0, tileDamage = 0,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Dragoon 1 - Azure Blaze: vertical 3 tiles, inflicts Burn
    [3] = {
        cost = 1, targetsOpponent = true, isSupport = false, isTrap = false,
        inflictsBurn = true,
        unitDamage = 4, tileDamage = 3,
        GetTargetTiles = function(targetX, targetY)
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
    },
    -- Dragoon 2 - Feather Cyclone: center tile + feathers based on Order tiles moved
    [4] = {
        cost = 2, targetsOpponent = true, isSupport = false, isTrap = false,
        isFeatherCyclone = true,
        unitDamage = 6, tileDamage = 4,
        featherDamage = 1,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Tirion 1 - Luminous Cleave: 2 horizontal tiles (2x2 at 3+ Fervor), grants Fervor on hit
    [9] = {
        cost = 2, targetsOpponent = true, isSupport = false, isTrap = false,
        grantsFervor = true,
        fervorUpgradeThreshold = 3,
        baseUnitDamage = 5, tileDamage = 2,
        unitDamage = 5,
        GetTargetTiles = function(targetX, targetY, param1, fervorStacks)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if fervorStacks and fervorStacks >= 3 then
                -- Upgraded: 2x2 area
                if targetX + 1 < CONSTANTS.GRID_COLS then
                    table.insert(tiles, targetY * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
                end
                if targetY + 1 < CONSTANTS.GRID_ROWS then
                    table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + targetX + 1)
                end
                if targetX + 1 < CONSTANTS.GRID_COLS and targetY + 1 < CONSTANTS.GRID_ROWS then
                    table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
                end
            else
                -- Base: 2 horizontal tiles
                if targetX + 1 < CONSTANTS.GRID_COLS then
                    table.insert(tiles, targetY * CONSTANTS.GRID_COLS + (targetX + 1) + 1)
                end
            end
            return tiles
        end
    },
    -- Tirion 2 - Sigil of Consecration: trap, 3x3 AoE on trigger, chain-detonates other Sigils, destroyed immediately
    [10] = {
        cost = 2, targetsOpponent = true, isSupport = true, isTrap = true,
        isConsTrap = true,
        unitDamage = 8, tileDamage = 0,
        sideUnitDamage = 1,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Imp 1 - Imp-Pact: 2 vertical tiles, recasts on random adjacent if hits unit
    [5] = {
        cost = 2, targetsOpponent = true, isSupport = false, isTrap = false,
        hasRecast = true, recastOnHit = true,
        unitDamage = 4, tileDamage = 3,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if targetY + 1 < CONSTANTS.GRID_ROWS then
                table.insert(tiles, (targetY + 1) * CONSTANTS.GRID_COLS + targetX + 1)
            end
            return tiles
        end
    },
    -- Imp 2 - Wicked Shift: pull + bonus damage on trap/destroyed tile
    [6] = {
        cost = 1, targetsOpponent = true, isSupport = false, isTrap = false,
        isPull = true,
        unitDamage = 8, tileDamage = 4,
        pullBonusDamage = 2,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Eye 1 - Ruinous Gaze: single tile, recasts left+right if tile destroyed, splash 1 unit dmg on surrounding
    [7] = {
        cost = 2, targetsOpponent = true, isSupport = false, isTrap = false,
        hasRecast = true, recastOnTileDestroy = true, hasSplash = true,
        splashUnitDamage = 1,
        unitDamage = 8, tileDamage = 2,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Eye 2 - Fractured Earth: self-damage tile, grants Wrath
    [8] = {
        cost = 1, targetsOpponent = false, isSupport = true, isTrap = false,
        isWrath = true,
        unitDamage = 0, tileDamage = 1,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Vizom 1 - Umbral Cleave: 1 tile, consumes Berserk stacks to bounce to adjacent tiles
    [11] = {
        cost = 2, targetsOpponent = true, isSupport = false, isTrap = false,
        hasBerserkRecast = true,
        unitDamage = 4, tileDamage = 2,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Vizom 2 - Sigil of Malevolence: trap, 5 damage + Bleed (1 per tile moved), grants Berserk to Vizom
    [12] = {
        cost = 2, targetsOpponent = true, isSupport = true, isTrap = true,
        isBleedTrap = true,
        unitDamage = 5, tileDamage = 0,
        GetTargetTiles = function(targetX, targetY)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            return tiles
        end
    },
    -- Generic 1 - Crimson Slash: 2 tiles horizontal/vertical (param1: 0=horiz, 1=vert)
    [14] = {
        cost = 1, targetsOpponent = true, isSupport = false, isTrap = false,
        unitDamage = 6, tileDamage = 3,
        GetTargetTiles = function(targetX, targetY, param1)
            local isVertical = (param1 == 1)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if isVertical then
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
    },
    -- Generic 2 - Sapphire Swirl: 2 tiles horizontal/vertical (param1: 0=horiz, 1=vert)
    [15] = {
        cost = 1, targetsOpponent = true, isSupport = false, isTrap = false,
        unitDamage = 6, tileDamage = 3,
        GetTargetTiles = function(targetX, targetY, param1)
            local isVertical = (param1 == 1)
            local tiles = {}
            table.insert(tiles, targetY * CONSTANTS.GRID_COLS + targetX + 1)
            if isVertical then
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
    }
}

local class = DefineType("PassAndPlayServer",
{
    Contains =
    {
        turnPhases = "PnPTurnPhases"
    },

    gameData = nil,
    lastResolutionData = nil,

    Init = function(self, entity)
        PassAndPlayServer_instance = self

        -- Get grid component for C++ pathfinding
        local gridComp = ECS.getGridComponent(entity)
        if gridComp then
            self.pathfindGridID = gridComp.gridID
            Log.info("[PnPServer] Grid component found for pathfinding, gridID: " .. self.pathfindGridID)
        else
            Log.warning("[PnPServer] No Grid component on server entity -- pathfinding will use fallback")
            self.pathfindGridID = nil
        end
    end,

    Start = function(self, entity)
        Log.info("[PnPServer] Pass and Play Server started")
    end,

    InitializeGameState = function(self)
        local totalTiles = CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS
        self.gameData = {
            units = { {}, {}, {}, {}, {}, {} },
            order = {
                ready = false,
                moveSupportQueue = {},
                attackQueue = {}
            },
            chaos = {
                ready = false,
                moveSupportQueue = {},
                attackQueue = {}
            },
            phase = "deployment",
            turnNumber = 1,
            turnHistory = {},
            tileHealth = { order = {}, chaos = {} },
            traps = { order = {}, chaos = {} },
            playerHealth = {
                order = CONSTANTS.MAX_HP,
                chaos = CONSTANTS.MAX_HP
            },
            buffs = {
                burn = {},
                fervor = {},
                bleed = {},
                berserk = {},   -- [unitType] = stacks (consumed by Vizom_1 to bounce)
            }
        }
        for i = 1, totalTiles do
            self.gameData.tileHealth.order[i] = CONSTANTS.TILE_MAX_HEALTH
            self.gameData.tileHealth.chaos[i] = CONSTANTS.TILE_MAX_HEALTH
        end
        self.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] = 0
        self.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] = 0
        Log.info("[PnPServer] Game state initialized")
    end,

    -- Called when a unit is deployed (replaces ServerRPC_OnDeployUnit)
    OnDeployUnit = function(self, unitType, gridPosX, gridPosY)
        if not self.gameData then
            self:InitializeGameState()
        end

        self.gameData.units[unitType].gridPos = {
            x = gridPosX,
            y = gridPosY
        }

        Log.info("[PnPServer] Deployed unit type " .. unitType .. " at (" .. gridPosX .. ", " .. gridPosY .. ")")

        -- Acknowledge deployment locally (same as ClientRPC_OnReturnDeployUnit)
        if ClientGameManager_instance then
            ClientGameManager_instance.awaitingServer = false
        end
    end,

    -- Called when a faction ends deployment (replaces ServerRPC_OnClientEndDeploymentPhase)
    OnEndDeployment = function(self, faction)
        if not self.gameData then
            self:InitializeGameState()
        end

        local isOrder = (faction == CONSTANTS.ORDER_PLAYER)

        if isOrder then
            self.gameData.order.ready = true
            Log.info("[PnPServer] Order ended deployment")
        else
            self.gameData.chaos.ready = true
            Log.info("[PnPServer] Chaos ended deployment")
        end

        if self.gameData.order.ready and self.gameData.chaos.ready then
            -- Both factions deployed, reset queues for preparation phase
            self.gameData.order.ready = false
            self.gameData.order.moveSupportQueue = {}
            self.gameData.order.attackQueue = {}
            self.gameData.chaos.ready = false
            self.gameData.chaos.moveSupportQueue = {}
            self.gameData.chaos.attackQueue = {}
            self.gameData.phase = "preparation"
            self.gameData.turnNumber = 1
            self.gameData.turnHistory = {}

            -- Generate initial Hunter's Marks
            local initPositions = {}
            for i = 1, 6 do
                initPositions[i] = {
                    x = self.gameData.units[i].gridPos.x,
                    y = self.gameData.units[i].gridPos.y,
                    isOrder = (i <= 3)
                }
            end
            local orderP = BuildPassiveLookup(Persistent["arsenalMap_order"] or {})
            local chaosP = BuildPassiveLookup(Persistent["arsenalMap_chaos"] or {})
            local initFactionPassives = {
                [CONSTANTS.ORDER_PLAYER] = orderP,
                [CONSTANTS.CHAOS_PLAYER] = chaosP
            }
            local initMarks, initMarkCounts, initMarkedByFaction = GenerateHunterMarks(initFactionPassives, initPositions)
            self.gameData.hunterMarks = initMarks
            self.gameData.hunterMarkCounts = initMarkCounts
            self.gameData.hunterMarkedByFaction = initMarkedByFaction

            Log.info("[PnPServer] Both factions deployed! PnP phase will handle transition.")
            return true
        end
        -- If only one faction done, PnP overlay will show and OnPassAndPlayConfirmed handles the switch
        return false
    end,

    -- Called when a faction submits actions (replaces ServerRPC_OnSubmitActions)
    OnSubmitActions = function(self, faction, moveSupportQueue, attackQueue)
        if self.gameData.phase == "gameover" then
            Log.warning("[PnPServer] Game is over, rejecting actions")
            return
        end

        local isOrder = (faction == CONSTANTS.ORDER_PLAYER)
        local playerData = isOrder and self.gameData.order or self.gameData.chaos

        Log.info("[PnPServer] Received actions from " .. (isOrder and "Order" or "Chaos"))

        -- Validate AP
        local totalCost = 0
        for _, action in ipairs(moveSupportQueue or {}) do
            if action.actionType == CONSTANTS.ACTION_ABILITY then
                local abilityData = SERVER_ABILITY_DATA[action.abilityID]
                totalCost = totalCost + (abilityData and abilityData.cost or CONSTANTS.TEMP_ABILITY_COST)
            end
        end
        for _, action in ipairs(attackQueue or {}) do
            local abilityData = SERVER_ABILITY_DATA[action.abilityID]
            totalCost = totalCost + (abilityData and abilityData.cost or CONSTANTS.TEMP_ABILITY_COST)
        end

        if totalCost > CONSTANTS.MAX_AP then
            Log.warning("[PnPServer] " .. (isOrder and "Order" or "Chaos") .. " exceeded AP limit (" .. totalCost .. "/" .. CONSTANTS.MAX_AP .. ")")
            return
        end

        playerData.moveSupportQueue = moveSupportQueue or {}
        playerData.attackQueue = attackQueue or {}
        playerData.ready = true

        if self.gameData.order.ready and self.gameData.chaos.ready then
            Log.info("[PnPServer] Both players ready! Starting resolution..")
            self:BroadcastResolution()
            return true
        end
        -- If only one faction done, PnP overlay handles the switch
        return false
    end,

    -- Use C++ Grid.validateMovement for pathfinding
    FindPath = function(self, isOrder, fromX, fromY, toX, toY)
        if self.pathfindGridID then
            local result = Grid.validateMovement(self.pathfindGridID, fromX, fromY, toX, toY, 2)
            if result and result.canMove and result.path and #result.path > 0 then
                return result.path
            end
        end

        -- Fallback: simple Manhattan path
        local dx = toX - fromX
        local dy = toY - fromY
        if math.abs(dx) + math.abs(dy) > 2 then return nil end

        local path = {}
        local cx, cy = fromX, fromY
        while cx ~= toX do
            cx = cx + (dx > 0 and 1 or -1)
            table.insert(path, { x = cx, y = cy })
        end
        while cy ~= toY do
            cy = cy + (dy > 0 and 1 or -1)
            table.insert(path, { x = cx, y = cy })
        end
        return #path > 0 and path or nil
    end,

    BroadcastResolution = function(self)
        local turnNum = self.gameData.turnNumber or 1
        Log.info("[PnPServer] ====== BROADCAST RESOLUTION (Turn " .. turnNum .. ") ======")
        local ok, err = pcall(function()
            -- Arm existing traps from previous turns
            self:ArmExistingTraps()

            -- Snapshot unit positions before movements
            local syncPositions = {}
            for i = 1, 6 do
                syncPositions[i] = {
                    x = self.gameData.units[i].gridPos.x,
                    y = self.gameData.units[i].gridPos.y
                }
            end

            -- Consolidate actions: Order move/support -> Chaos move/support -> Order attack -> Chaos attack
            local turnActions = {}

            local function addEntries(queue, faction)
                for _, action in ipairs(queue) do
                    table.insert(turnActions, {
                        faction = faction,
                        actionType = action.actionType,
                        abilityID = action.abilityID,
                        unitType = action.unitType or 0,
                        targetX = action.targetX,
                        targetY = action.targetY,
                        param1 = action.param1 or 0,
                        param2 = action.param2 or 0
                    })
                end
            end

            addEntries(self.gameData.order.moveSupportQueue or {}, CONSTANTS.ORDER_PLAYER)
            addEntries(self.gameData.chaos.moveSupportQueue or {}, CONSTANTS.CHAOS_PLAYER)
            addEntries(self.gameData.order.attackQueue or {}, CONSTANTS.ORDER_PLAYER)
            addEntries(self.gameData.chaos.attackQueue or {}, CONSTANTS.CHAOS_PLAYER)

            self.gameData.turnHistory[turnNum] = turnActions

            Log.info("[PnPServer] Turn " .. turnNum .. " actions: " .. #turnActions)

            -- Snapshot starting health for client validation
            local startingHealth = {
                order = self.gameData.playerHealth.order,
                chaos = self.gameData.playerHealth.chaos
            }

            -- Snapshot existing burn/bleed states BEFORE pre-calculation for activeBuffs
            -- (so newly applied burns this turn don't show prematurely)
            local preExistingBurns = {}
            local preExistingBleeds = {}
            for ut, turnApplied in pairs(self.gameData.buffs.burn or {}) do
                if turnApplied and turnNum <= turnApplied + 1 then
                    preExistingBurns[ut] = turnApplied
                end
            end
            for ut, turnApplied in pairs(self.gameData.buffs.bleed or {}) do
                if turnApplied and turnNum <= turnApplied + 1 then
                    preExistingBleeds[ut] = turnApplied
                end
            end

            -- Pre-calculate results
            local results = self:PreCalculateResults(turnActions)

            -- Build active buff snapshot for client-side status UI (before ApplyResolutionToState clears _pendingBuffs)
            local activeBuffs = {}
            local buffStacks = {
                fervor = self.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] or 0,
                berserk = self.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] or 0
            }
            if self.gameData._pendingBuffs then
                local pb = self.gameData._pendingBuffs
                local snapshotTurn = self.gameData.turnNumber or turnNum or 1
                buffStacks.fervor = pb.fervorStacks or 0
                buffStacks.berserk = pb.berserkStacks or 0
                
                -- Use pre-existing burns snapshot (not pb.burnedUnits which includes new burns)
                for ut, turnApplied in pairs(preExistingBurns) do
                    activeBuffs[#activeBuffs + 1] = {
                        unit = ut,
                        statusID = CONSTANTS.ABILITIES.DRAGOON_1,
                        stackCount = 1,
                        -- Burn starts blinking on the turn after application, expires at end of that turn
                        isExpiring = (snapshotTurn > turnApplied) or false
                    }
                end
                -- Use pre-existing bleeds snapshot (not pb.bleedUnits which includes new bleeds)
                for ut, turnApplied in pairs(preExistingBleeds) do
                    activeBuffs[#activeBuffs + 1] = {
                        unit = ut,
                        statusID = CONSTANTS.ABILITIES.VIZOM_2,
                        stackCount = 1,
                        -- Bleed starts blinking on the turn after application, expires at end of that turn
                        isExpiring = (snapshotTurn > turnApplied) or false
                    }
                end
                if pb.fervorStacks > 0 then
                    activeBuffs[#activeBuffs + 1] = {
                        unit = CONSTANTS.UNIT.TIRION,
                        statusID = CONSTANTS.ABILITIES.TIRION_1,
                        stackCount = pb.fervorStacks,
                        isExpiring = false
                    }
                end
                if pb.berserkStacks and pb.berserkStacks > 0 then
                    activeBuffs[#activeBuffs + 1] = {
                        unit = CONSTANTS.UNIT.VIZOM,
                        statusID = CONSTANTS.ABILITIES.VIZOM_1,
                        stackCount = pb.berserkStacks,
                        isExpiring = false
                    }
                end
                -- Add shield stacks to activeBuffs snapshot for UI
                -- Shields expire at end of resolution (isExpiring = true)
                if pb.shieldUnits then
                    for ut, stacks in pairs(pb.shieldUnits) do
                        if stacks and stacks > 0 then
                            activeBuffs[#activeBuffs + 1] = {
                                unit = ut,
                                statusID = CONSTANTS.ABILITIES.ORACLE_2,
                                stackCount = stacks,
                                isExpiring = true
                            }
                        end
                    end
                end
            end

            -- Apply state changes
            self:ApplyResolutionToState(results)

            -- Strip zero-value fields
            for _, result in ipairs(results) do
                if result.damage == 0 then result.damage = nil end
                if result.damagePlayer == 0 then result.damagePlayer = nil end
                if result.healAmount == 0 then result.healAmount = nil end
                if result.healPlayer == 0 then result.healPlayer = nil end
                if #result.hits == 0 then result.hits = nil end
                if #result.tileEffects == 0 then result.tileEffects = nil end
                if result.action.param1 == 0 then result.action.param1 = nil end
                if result.action.param2 == 0 then result.action.param2 = nil end
                if result.action.unitType == 0 then result.action.unitType = nil end
            end

            local resolutionData = {
                turn = turnNum,
                syncPositions = syncPositions,
                queue = results,
                startingHealth = startingHealth,
                finalHealth = {
                    order = self.gameData.playerHealth.order,
                    chaos = self.gameData.playerHealth.chaos
                },
                winner = self:CheckWinner(),
                activeBuffs = activeBuffs,
                buffStacks = buffStacks
            }

            -- Store resolution data for PnP phase to pick up
            self.lastResolutionData = resolutionData
        end)

        if not ok then
            Log.error("[PnPServer] BroadcastResolution failed: " .. tostring(err))
        end

        -- Reset queues for next turn
        self.gameData.order.ready = false
        self.gameData.order.moveSupportQueue = {}
        self.gameData.order.attackQueue = {}
        self.gameData.chaos.ready = false
        self.gameData.chaos.moveSupportQueue = {}
        self.gameData.chaos.attackQueue = {}

        local winner = self:CheckWinner()
        if winner > 0 then
            self.gameData.phase = "gameover"
            Log.info("[PnPServer] Game over! Winner: " .. winner)
        else
            self.gameData.turnNumber = turnNum + 1
            self.gameData.phase = "preparation"

            -- Generate Hunter's Marks for next turn
            local nextPositions = {}
            for i = 1, 6 do
                nextPositions[i] = {
                    x = self.gameData.units[i].gridPos.x,
                    y = self.gameData.units[i].gridPos.y,
                    isOrder = (i <= 3)
                }
            end
            local oP = BuildPassiveLookup(Persistent["arsenalMap_order"] or {})
            local cP = BuildPassiveLookup(Persistent["arsenalMap_chaos"] or {})
            local nextFP = {
                [CONSTANTS.ORDER_PLAYER] = oP,
                [CONSTANTS.CHAOS_PLAYER] = cP
            }
            local nextMarks, nextMarkCounts, nextMarkedByFaction = GenerateHunterMarks(nextFP, nextPositions)
            self.gameData.hunterMarks = nextMarks
            self.gameData.hunterMarkCounts = nextMarkCounts
            self.gameData.hunterMarkedByFaction = nextMarkedByFaction

            Log.info("[PnPServer] Resolution complete, advancing to turn " .. (turnNum + 1))
        end
    end,

    CheckWinner = function(self)
        local orderHP = self.gameData.playerHealth.order
        local chaosHP = self.gameData.playerHealth.chaos

        if orderHP <= 0 and chaosHP > 0 then
            return CONSTANTS.CHAOS_PLAYER
        elseif chaosHP <= 0 and orderHP > 0 then
            return CONSTANTS.ORDER_PLAYER
        elseif orderHP <= 0 and chaosHP <= 0 then
            return 3
        end
        return 0
    end,

    ArmExistingTraps = function(self)
        local totalTiles = CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS
        for i = 1, totalTiles do
            if self.gameData.traps.order[i] and not self.gameData.traps.order[i].armed then
                self.gameData.traps.order[i].armed = true
                Log.info("[PnPServer] Armed order trap on tile " .. i)
            end
            if self.gameData.traps.chaos[i] and not self.gameData.traps.chaos[i].armed then
                self.gameData.traps.chaos[i].armed = true
                Log.info("[PnPServer] Armed chaos trap on tile " .. i)
            end
        end
    end,

    PreCalculateResults = function(self, consolidatedQueue)
        Log.info("[PnPServer] PreCalculateResults: processing " .. #consolidatedQueue .. " actions")
        local totalTiles = CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS

        -- Snapshot tile health
        local orderHealth = {}
        local chaosHealth = {}
        for i = 1, totalTiles do
            orderHealth[i] = self.gameData.tileHealth.order[i]
            chaosHealth[i] = self.gameData.tileHealth.chaos[i]
        end

        -- Snapshot armed traps
        local orderTraps = {}
        local chaosTraps = {}
        for i = 1, totalTiles do
            if self.gameData.traps.order[i] and self.gameData.traps.order[i].armed then
                orderTraps[i] = {
                    owner = self.gameData.traps.order[i].owner,
                    damage = self.gameData.traps.order[i].damage,
                    sideDamage = self.gameData.traps.order[i].sideDamage,
                    isConsTrap = self.gameData.traps.order[i].isConsTrap or false,
                    isBleedTrap = self.gameData.traps.order[i].isBleedTrap or false,
                    triggerCount = 0
                }
            end
            if self.gameData.traps.chaos[i] and self.gameData.traps.chaos[i].armed then
                chaosTraps[i] = {
                    owner = self.gameData.traps.chaos[i].owner,
                    damage = self.gameData.traps.chaos[i].damage,
                    sideDamage = self.gameData.traps.chaos[i].sideDamage,
                    isConsTrap = self.gameData.traps.chaos[i].isConsTrap or false,
                    isBleedTrap = self.gameData.traps.chaos[i].isBleedTrap or false,
                    triggerCount = 0
                }
            end
        end

        -- Track unit positions
        local unitPositions = {}
        for i = 1, 6 do
            unitPositions[i] = {
                x = self.gameData.units[i].gridPos.x,
                y = self.gameData.units[i].gridPos.y,
                isOrder = (i <= 3)
            }
        end

        -- Track active shields (unit type → remaining stacks, each damage point removes 1 stack)
        local shieldedUnits = {}
        local damagedUnitsThisTurn = {}
        local orderTilesMoved = 0
        local wrathActive = { [CONSTANTS.ORDER_PLAYER] = false, [CONSTANTS.CHAOS_PLAYER] = false }
        local fervorStacks = self.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] or 0
        local fervorGrantedThisTurn = false

        -- Track Berserk stacks (persistent, loaded from game data)
        local berserkStacks = self.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] or 0

        local currentTurn = self.gameData.turnNumber or 1
        local burnedUnits = {}
        for ut, turnApplied in pairs(self.gameData.buffs.burn) do
            -- Burn lasts through the turn after application
            if turnApplied and currentTurn <= turnApplied + 1 then
                burnedUnits[ut] = turnApplied
            end
        end

        local bleedUnits = {}
        for ut, turnApplied in pairs(self.gameData.buffs.bleed) do
            -- Bleed lasts through the turn after application
            if turnApplied and currentTurn <= turnApplied + 1 then
                bleedUnits[ut] = turnApplied
            end
        end

        -- Build passive lookups from arsenal maps (PnP uses Persistent)
        local orderArsenal = Persistent["arsenalMap_order"] or {}
        local chaosArsenal = Persistent["arsenalMap_chaos"] or {}
        local orderPassives, orderAbilityToUnit = BuildPassiveLookup(orderArsenal)
        local chaosPassives, chaosAbilityToUnit = BuildPassiveLookup(chaosArsenal)
        local factionPassives = {
            [CONSTANTS.ORDER_PLAYER] = orderPassives,
            [CONSTANTS.CHAOS_PLAYER] = chaosPassives
        }
        local factionAbilityToUnit = {
            [CONSTANTS.ORDER_PLAYER] = orderAbilityToUnit,
            [CONSTANTS.CHAOS_PLAYER] = chaosAbilityToUnit
        }

        local function HasPassive(faction, unitType, passiveID)
            local p = factionPassives[faction]
            return p and p[unitType] and p[unitType][passiveID] or false
        end

        local function GetCasterUnit(faction, abilityID, entryCasterUnit)
            if entryCasterUnit then return entryCasterUnit end
            local map = factionAbilityToUnit[faction]
            if map and map[abilityID] then return map[abilityID] end
            return ABILITY_OWNER_MAP[abilityID]
        end

        -- Hunter's Mark state is generated at planning start and consumed during this resolution
        local hunterMarks = self.gameData.hunterMarks or {}
        local hunterMarkCounts = self.gameData.hunterMarkCounts or {
            [CONSTANTS.ORDER_PLAYER] = 0,
            [CONSTANTS.CHAOS_PLAYER] = 0
        }
        local hunterMarkedByFaction = self.gameData.hunterMarkedByFaction or {}
        if not next(hunterMarkedByFaction) then
            for markerUnit, markedEnemy in pairs(hunterMarks) do
                local markerFaction = (markerUnit <= 3) and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                hunterMarkedByFaction[markerFaction] = markedEnemy
            end
        end

        local results = {}

        -- Helper: process damage on a single tile
        local function ProcessTileHit(tileIdx, targetHealthMap, targetIsOrder, unitDamage, tileDamage, resultObj, isRecast)
            if not targetHealthMap[tileIdx] or targetHealthMap[tileIdx] <= 0 then return false end

            local hitUnit = nil
            for ut, pos in pairs(unitPositions) do
                if pos.isOrder == targetIsOrder then
                    local posIdx = pos.y * CONSTANTS.GRID_COLS + pos.x + 1
                    if posIdx == tileIdx then
                        hitUnit = ut
                        break
                    end
                end
            end

            if hitUnit then
                local actualDamage = unitDamage
                local hasBurnBonus = false
                if burnedUnits[hitUnit] ~= nil then
                    actualDamage = actualDamage + 1
                    hasBurnBonus = true
                end
                -- Apply shield: each point of damage removes 1 shield stack (damage still goes through)
                local shieldStacksRemoved = 0
                if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                    shieldStacksRemoved = math.min(shieldedUnits[hitUnit], actualDamage)
                    shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - shieldStacksRemoved
                    table.insert(resultObj.hits, {
                        isOrderGrid = targetIsOrder,
                        unitType = hitUnit,
                        gridX = (tileIdx - 1) % CONSTANTS.GRID_COLS,
                        gridY = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS),
                        damage = actualDamage,
                        shielded = true,
                        shieldStacksRemoved = shieldStacksRemoved,
                        shieldStacksRemaining = shieldedUnits[hitUnit] or 0,
                        burnBonus = hasBurnBonus or nil,
                        isRecast = isRecast or nil
                    })
                else
                    table.insert(resultObj.hits, {
                        isOrderGrid = targetIsOrder,
                        unitType = hitUnit,
                        gridX = (tileIdx - 1) % CONSTANTS.GRID_COLS,
                        gridY = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS),
                        damage = actualDamage,
                        burnBonus = hasBurnBonus or nil,
                        isRecast = isRecast or nil
                    })
                end
                resultObj.damage = resultObj.damage + actualDamage
                damagedUnitsThisTurn[hitUnit] = true
                return true
            else
                if tileDamage > 0 then
                    local newHealth = math.max(0, targetHealthMap[tileIdx] - tileDamage)
                    if newHealth ~= targetHealthMap[tileIdx] then
                        table.insert(resultObj.tileEffects, {
                            tileIdx = tileIdx,
                            oldHealth = targetHealthMap[tileIdx],
                            newHealth = newHealth,
                            isOrder = targetIsOrder
                        })
                        targetHealthMap[tileIdx] = newHealth
                    end
                end
                return false
            end
        end

        -- Consecration traps no longer re-trigger on ability hits.
        -- They only detonate when stepped on during movement (3x3 AoE + chain).
        local function CheckConsTrapRetrigger(tileIdx, targetIsOrder, resultObj, isOrderAbility)
            -- No-op: kept for call-site compatibility
        end

        for i, entry in ipairs(consolidatedQueue) do
            local result = {
                action = {
                    faction = entry.faction,
                    actionType = entry.actionType,
                    abilityID = entry.abilityID,
                    unitType = entry.unitType,
                    targetX = entry.targetX,
                    targetY = entry.targetY,
                    param1 = entry.param1,
                    param2 = entry.param2
                },
                hits = {},
                damage = 0,
                healAmount = 0,
                healPlayer = 0,
                damagePlayer = 0,
                tileEffects = {},
            }

            if entry.actionType == CONSTANTS.ACTION_MOVEMENT then
                local fromX = entry.param1 or 0
                local fromY = entry.param2 or 0

                if unitPositions[entry.unitType] then
                    unitPositions[entry.unitType].x = entry.targetX
                    unitPositions[entry.unitType].y = entry.targetY
                end

                local isOrder = (entry.faction == CONSTANTS.ORDER_PLAYER)
                local tilesMoved = math.abs(entry.targetX - fromX) + math.abs(entry.targetY - fromY)
                if isOrder then
                    orderTilesMoved = orderTilesMoved + tilesMoved
                end

                -- Apply Bleed damage (1 damage per tile moved)
                if bleedUnits[entry.unitType] ~= nil and tilesMoved > 0 then
                    local bleedDmg = tilesMoved
                    local bleedShielded = false
                    local bleedStacksRemoved = 0
                    if burnedUnits[entry.unitType] ~= nil then
                        bleedDmg = bleedDmg + 1
                    end
                    -- Shield stacks: each damage point removes 1 stack, damage still goes through
                    if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                        bleedStacksRemoved = math.min(shieldedUnits[entry.unitType], bleedDmg)
                        shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - bleedStacksRemoved
                        bleedShielded = true
                    end
                    result.damage = result.damage + bleedDmg
                    result.damagePlayer = isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                    result.bleedHit = {
                        unitType = entry.unitType,
                        isOrder = isOrder,
                        damage = bleedDmg,
                        shielded = bleedShielded,
                        shieldStacksRemoved = bleedStacksRemoved,
                        shieldStacksRemaining = shieldedUnits[entry.unitType] or 0
                    }
                    -- For each point of Bleed damage taken by enemies, Vizom gains 1 Berserk stack
                    berserkStacks = berserkStacks + bleedDmg
                    result.berserkGranted = true
                    result.berserkStacksGained = bleedDmg
                end

                -- Check path for armed traps
                local trapMap = isOrder and orderTraps or chaosTraps
                local path = self:FindPath(isOrder, fromX, fromY, entry.targetX, entry.targetY)
                if path then
                    for _, step in ipairs(path) do
                        local stepIdx = step.y * CONSTANTS.GRID_COLS + step.x + 1
                        if trapMap[stepIdx] then
                            local trap = trapMap[stepIdx]
                            -- Vizom_2 Bleed trap: deals 5 damage, inflicts Bleed, grants Berserk to Vizom
                            if trap.isBleedTrap then
                                local trapDmg = trap.damage
                                if burnedUnits[entry.unitType] ~= nil then
                                    trapDmg = trapDmg + 1
                                end
                                local trapShielded = false
                                local trapStacksRemoved = 0
                                -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                                    trapStacksRemoved = math.min(shieldedUnits[entry.unitType], trapDmg)
                                    shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - trapStacksRemoved
                                    trapShielded = true
                                end
                                result.trapHit = {
                                    isOrderGrid = isOrder,
                                    unitType = entry.unitType,
                                    gridX = step.x,
                                    gridY = step.y,
                                    damage = trapDmg,
                                    isBleedTrap = true,
                                    shielded = trapShielded,
                                    shieldStacksRemoved = trapStacksRemoved,
                                    shieldStacksRemaining = shieldedUnits[entry.unitType] or 0
                                }
                                bleedUnits[entry.unitType] = currentTurn
                                -- Stepping on Vizom_2 trap grants 1 Berserk stack
                                berserkStacks = berserkStacks + 1
                                result.berserkGranted = true
                                result.berserkStacksGained = (result.berserkStacksGained or 0) + 1
                                trapMap[stepIdx] = nil
                                break
                            -- Tirion_2 Consecration trap: 3x3 AoE + chain detonation, destroyed immediately
                            elseif trap.isConsTrap then
                                local detonated = {}
                                local detonationQueue = { stepIdx }
                                local totalConsDmg = 0
                                local consHits = {}
                                local detonatedTrapIndices = {}
                                local detonationCount = 0

                                -- Temporarily place triggering unit at step position for AoE check
                                local savedX = unitPositions[entry.unitType].x
                                local savedY = unitPositions[entry.unitType].y
                                unitPositions[entry.unitType].x = step.x
                                unitPositions[entry.unitType].y = step.y

                                while #detonationQueue > 0 do
                                    local detIdx = table.remove(detonationQueue, 1)
                                    if not detonated[detIdx] then
                                        detonated[detIdx] = true
                                        local detTrap = trapMap[detIdx]
                                        if detTrap and detTrap.isConsTrap then
                                            detonationCount = detonationCount + 1
                                            local detCenterDmg = detTrap.damage
                                            local detSideDmg = detTrap.sideDamage or detCenterDmg
                                            trapMap[detIdx] = nil
                                            table.insert(detonatedTrapIndices, detIdx)
                                            local detX = (detIdx - 1) % CONSTANTS.GRID_COLS
                                            local detY = math.floor((detIdx - 1) / CONSTANTS.GRID_COLS)
                                            for dy = -1, 1 do
                                                for dx = -1, 1 do
                                                    local ax = detX + dx
                                                    local ay = detY + dy
                                                    if ax >= 0 and ax < CONSTANTS.GRID_COLS and ay >= 0 and ay < CONSTANTS.GRID_ROWS then
                                                        local aIdx = ay * CONSTANTS.GRID_COLS + ax + 1
                                                        for ut, pos in pairs(unitPositions) do
                                                            if pos.isOrder == isOrder then
                                                                local posIdx = pos.y * CONSTANTS.GRID_COLS + pos.x + 1
                                                                if posIdx == aIdx then
                                                                    local baseDetDmg = (dx == 0 and dy == 0) and detCenterDmg or detSideDmg
                                                                    local actualDetDmg = baseDetDmg
                                                                    if burnedUnits[ut] ~= nil then
                                                                        actualDetDmg = actualDetDmg + 1
                                                                    end
                                                                    local consShielded = false
                                                                    local consStacksRemoved = 0
                                                                    -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                                                    if shieldedUnits[ut] and shieldedUnits[ut] > 0 then
                                                                        consStacksRemoved = math.min(shieldedUnits[ut], actualDetDmg)
                                                                        shieldedUnits[ut] = shieldedUnits[ut] - consStacksRemoved
                                                                        consShielded = true
                                                                    end
                                                                    totalConsDmg = totalConsDmg + actualDetDmg
                                                                    table.insert(consHits, { unitType = ut, gridX = ax, gridY = ay, damage = actualDetDmg, detonationIdx = detonationCount, shielded = consShielded, shieldStacksRemoved = consStacksRemoved, shieldStacksRemaining = shieldedUnits[ut] or 0 })
                                                                    damagedUnitsThisTurn[ut] = true
                                                                end
                                                            end
                                                        end
                                                        if trapMap[aIdx] and trapMap[aIdx].isConsTrap and not detonated[aIdx] then
                                                            table.insert(detonationQueue, aIdx)
                                                        end
                                                    end
                                                end
                                            end
                                        end
                                    end
                                end

                                -- Restore triggering unit position to final target
                                unitPositions[entry.unitType].x = savedX
                                unitPositions[entry.unitType].y = savedY

                                result.trapHit = {
                                    isOrderGrid = isOrder,
                                    unitType = entry.unitType,
                                    gridX = step.x,
                                    gridY = step.y,
                                    damage = totalConsDmg,
                                    isConsTrap = true,
                                    consHits = consHits,
                                    detonatedTrapIndices = detonatedTrapIndices
                                }
                                break
                            else
                                -- Generic trap handling
                                local trapDmg = trap.damage
                                local trapShielded = false
                                local trapStacksRemoved = 0
                                if burnedUnits[entry.unitType] ~= nil then
                                    trapDmg = trapDmg + 1
                                end
                                -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                                    trapStacksRemoved = math.min(shieldedUnits[entry.unitType], trapDmg)
                                    shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - trapStacksRemoved
                                    trapShielded = true
                                end
                                result.trapHit = {
                                    isOrderGrid = isOrder,
                                    unitType = entry.unitType,
                                    gridX = step.x,
                                    gridY = step.y,
                                    damage = trapDmg,
                                    shielded = trapShielded,
                                    shieldStacksRemoved = trapStacksRemoved,
                                    shieldStacksRemaining = shieldedUnits[entry.unitType] or 0
                                }
                                trapMap[stepIdx] = nil
                                break
                            end
                        end
                    end
                else
                    local destIdx = entry.targetY * CONSTANTS.GRID_COLS + entry.targetX + 1
                    if trapMap[destIdx] then
                        local trapDmg = trapMap[destIdx].damage
                        local trapShielded = false
                        local trapStacksRemoved = 0
                        if burnedUnits[entry.unitType] ~= nil then
                            trapDmg = trapDmg + 1
                        end
                        -- Shield stacks: each damage point removes 1 stack, damage still goes through
                        if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                            trapStacksRemoved = math.min(shieldedUnits[entry.unitType], trapDmg)
                            shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - trapStacksRemoved
                            trapShielded = true
                        end
                        result.trapHit = {
                            isOrderGrid = isOrder,
                            unitType = entry.unitType,
                            gridX = entry.targetX,
                            gridY = entry.targetY,
                            damage = trapDmg,
                            shielded = trapShielded,
                            shieldStacksRemoved = trapStacksRemoved,
                            shieldStacksRemaining = shieldedUnits[entry.unitType] or 0
                        }
                        trapMap[destIdx] = nil
                    end
                end

            elseif entry.actionType == CONSTANTS.ACTION_ABILITY then
                local abilityData = SERVER_ABILITY_DATA[entry.abilityID]
                if abilityData then
                    local isOrderAbility = (entry.faction == CONSTANTS.ORDER_PLAYER)
                    local targetHealthMap, targetIsOrder

                    if abilityData.targetsOpponent then
                        if isOrderAbility then
                            targetHealthMap = chaosHealth
                            targetIsOrder = false
                        else
                            targetHealthMap = orderHealth
                            targetIsOrder = true
                        end
                    else
                        if isOrderAbility then
                            targetHealthMap = orderHealth
                            targetIsOrder = true
                        else
                            targetHealthMap = chaosHealth
                            targetIsOrder = false
                        end
                    end

                    local wrathBonus = 0
                    local wrathConsumed = false
                    if wrathActive[entry.faction] and not abilityData.isSupport then
                        wrathBonus = 2
                        wrathConsumed = true
                        wrathActive[entry.faction] = false
                    end

                    local effectiveUnitDamage = abilityData.unitDamage + wrathBonus

                    if abilityData.grantsFervor then
                        effectiveUnitDamage = abilityData.baseUnitDamage + fervorStacks + wrathBonus
                    end

                    -- Mark result as having consumed wrath for client-side indicator
                    if wrathConsumed then
                        result.wrathConsumed = true
                    end

                    -- Oracle_1: random tiles (uses crystalCount from ability data)
                    if abilityData.isRandomTarget then
                        local crystalCount = abilityData.crystalCount or 4
                        local randomTileIndices = GetRandomUnbrokenTiles(targetHealthMap, crystalCount)
                        local randomTiles = {}
                        for _, idx in ipairs(randomTileIndices) do
                            local col = (idx - 1) % CONSTANTS.GRID_COLS
                            local row = math.floor((idx - 1) / CONSTANTS.GRID_COLS)
                            table.insert(randomTiles, { x = col, y = row })
                            ProcessTileHit(idx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result)
                            CheckConsTrapRetrigger(idx, targetIsOrder, result, isOrderAbility)
                        end
                        result.randomTiles = randomTiles
                        if result.damage > 0 then
                            result.damagePlayer = isOrderAbility and CONSTANTS.CHAOS_PLAYER or CONSTANTS.ORDER_PLAYER
                        end

                    -- Dragoon_2 Feather Cyclone
                    elseif abilityData.isFeatherCyclone then
                        local centerIdx = entry.targetY * CONSTANTS.GRID_COLS + entry.targetX + 1

                        -- Track center hit separately
                        local hitsBefore = #result.hits
                        local damageBefore = result.damage
                        ProcessTileHit(centerIdx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result)
                        CheckConsTrapRetrigger(centerIdx, targetIsOrder, result, isOrderAbility)
                        result.centerHitData = {
                            hits = {},
                            damage = result.damage - damageBefore
                        }
                        for h = hitsBefore + 1, #result.hits do
                            table.insert(result.centerHitData.hits, result.hits[h])
                        end

                        local featherCount = 2 + (orderTilesMoved * 2)
                        local featherTiles = {}
                        local usedDirs = {}
                        local featherDmg = abilityData.featherDamage + wrathBonus
                        result.featherHitData = {}
                        for f = 1, featherCount do
                            local ft = GetRandomAdjacentTile(targetHealthMap, entry.targetX, entry.targetY, usedDirs)
                            if ft then
                                usedDirs[ft.dirKey] = true
                                table.insert(featherTiles, { x = ft.x, y = ft.y })

                                -- Track per-feather hit separately
                                local fHitsBefore = #result.hits
                                local fDamageBefore = result.damage
                                ProcessTileHit(ft.idx, targetHealthMap, targetIsOrder, featherDmg, 0, result)
                                CheckConsTrapRetrigger(ft.idx, targetIsOrder, result, isOrderAbility)
                                local fData = {
                                    hits = {},
                                    damage = result.damage - fDamageBefore
                                }
                                for h = fHitsBefore + 1, #result.hits do
                                    table.insert(fData.hits, result.hits[h])
                                end
                                table.insert(result.featherHitData, fData)
                            end
                        end
                        result.featherTiles = featherTiles

                        if result.damage > 0 then
                            result.damagePlayer = isOrderAbility and CONSTANTS.CHAOS_PLAYER or CONSTANTS.ORDER_PLAYER
                        end

                    -- Shield (Oracle_2): flat HP shield, absorbs damage until depleted
                    elseif abilityData.isShield then
                        local shieldUnit = entry.param1 or 0
                        if shieldUnit > 0 and shieldUnit <= 6 then
                            local shieldAmount = abilityData.shieldAmount or 5
                            shieldedUnits[shieldUnit] = shieldAmount
                            result.shieldTarget = shieldUnit
                            result.shieldAmount = shieldAmount
                        end

                    -- Eye_2 Fractured Earth
                    elseif abilityData.isWrath then
                        if entry.param1 and entry.param2 then
                            local eyeTileIdx = entry.param2 * CONSTANTS.GRID_COLS + entry.param1 + 1
                            if chaosHealth[eyeTileIdx] and chaosHealth[eyeTileIdx] > 1 then
                                local newHealth = math.max(0, chaosHealth[eyeTileIdx] - abilityData.tileDamage)
                                if newHealth ~= chaosHealth[eyeTileIdx] then
                                    table.insert(result.tileEffects, {
                                        tileIdx = eyeTileIdx,
                                        oldHealth = chaosHealth[eyeTileIdx],
                                        newHealth = newHealth,
                                        isOrder = false
                                    })
                                end
                                chaosHealth[eyeTileIdx] = newHealth
                            end
                        end
                        wrathActive[entry.faction] = true
                        result.wrathGranted = true

                    -- Trap placement (Tirion_2, Vizom_2)
                    elseif abilityData.isTrap then
                        local trapIdx = entry.targetY * CONSTANTS.GRID_COLS + entry.targetX + 1
                        local trapHealthMap = targetIsOrder and orderHealth or chaosHealth
                        local existingTrapMap = targetIsOrder and orderTraps or chaosTraps

                        if not trapHealthMap[trapIdx] or trapHealthMap[trapIdx] <= 0 then
                            Log.info("[PnPServer] Trap placement skipped: tile destroyed")
                        elseif existingTrapMap[trapIdx] then
                            Log.info("[PnPServer] Trap placement skipped: tile already has a trap")
                        else
                            result.trap = {
                                tileIdx = trapIdx,
                                isOrder = targetIsOrder,
                                owner = entry.faction,
                                damage = abilityData.unitDamage,
                                sideDamage = abilityData.sideUnitDamage or nil,
                                isConsTrap = abilityData.isConsTrap or false,
                                isBleedTrap = abilityData.isBleedTrap or false
                            }
                        end

                    -- Standard offensive abilities (and abilities with recasts)
                    else
                        -- Pass fervorStacks to Tirion_1 GetTargetTiles for 2x2 upgrade
                        local targetTiles
                        if abilityData.grantsFervor then
                            targetTiles = abilityData.GetTargetTiles(entry.targetX, entry.targetY, entry.param1, fervorStacks)
                        else
                            targetTiles = abilityData.GetTargetTiles(entry.targetX, entry.targetY, entry.param1)
                        end
                        local hitAUnit = false

                        for _, tileIdx in ipairs(targetTiles) do
                            local unitHit = ProcessTileHit(tileIdx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result)
                            if unitHit then
                                hitAUnit = true
                            end
                            CheckConsTrapRetrigger(tileIdx, targetIsOrder, result, isOrderAbility)
                        end

                        -- Tirion_1 Fervor
                        if abilityData.grantsFervor and hitAUnit and not fervorGrantedThisTurn then
                            fervorStacks = fervorStacks + 1
                            fervorGrantedThisTurn = true
                            result.fervorGranted = true
                        end

                        -- Dragoon_1 Burn
                        if abilityData.inflictsBurn and hitAUnit then
                            for _, hit in ipairs(result.hits) do
                                if hit.unitType and not hit.shielded then
                                    burnedUnits[hit.unitType] = currentTurn
                                end
                            end
                            result.burnApplied = true
                        end

                        -- Imp_1 recast: one recast per tile that hit a unit
                        if abilityData.recastOnHit and hitAUnit then
                            result.recastTiles = {}
                            local usedRecastDirs = {}
                            for _, tileIdx in ipairs(targetTiles) do
                                local tCol = (tileIdx - 1) % CONSTANTS.GRID_COLS
                                local tRow = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                                -- Check if this tile hit a unit
                                local tileHitUnit = false
                                for _, hit in ipairs(result.hits) do
                                    if hit.gridX == tCol and hit.gridY == tRow and not hit.isRecast then
                                        tileHitUnit = true
                                        break
                                    end
                                end
                                if tileHitUnit then
                                    local recastTile = GetRandomAdjacentTile(targetHealthMap, tCol, tRow, usedRecastDirs)
                                    if recastTile then
                                        usedRecastDirs[recastTile.dirKey] = true
                                        table.insert(result.recastTiles, { x = recastTile.x, y = recastTile.y })
                                        ProcessTileHit(recastTile.idx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result, true)
                                        CheckConsTrapRetrigger(recastTile.idx, targetIsOrder, result, isOrderAbility)
                                    end
                                end
                            end
                            if #result.recastTiles == 0 then
                                result.recastTiles = nil
                            end
                        end

                        -- Eye_1 splash helper: 2 unit damage on surrounding tiles around a center
                        local function ApplySplash(cx, cy, splashList, isRecast)
                            for dy = -1, 1 do
                                for dx = -1, 1 do
                                    if not (dx == 0 and dy == 0) then
                                        local sx = cx + dx
                                        local sy = cy + dy
                                        if sx >= 0 and sx < CONSTANTS.GRID_COLS and sy >= 0 and sy < CONSTANTS.GRID_ROWS then
                                            local sIdx = sy * CONSTANTS.GRID_COLS + sx + 1
                                            if targetHealthMap[sIdx] and targetHealthMap[sIdx] > 0 then
                                                table.insert(splashList, { x = sx, y = sy, idx = sIdx, isRecast = isRecast or nil })
                                                ProcessTileHit(sIdx, targetHealthMap, targetIsOrder, abilityData.splashUnitDamage, 0, result, isRecast)
                                            end
                                        end
                                    end
                                end
                            end
                        end

                        -- Eye_1 splash on center tile
                        if abilityData.hasSplash then
                            result.splashTiles = {}
                            ApplySplash(entry.targetX, entry.targetY, result.splashTiles, false)
                        end

                        -- Eye_1 recast
                        if abilityData.recastOnTileDestroy then
                            local centerIdx = entry.targetY * CONSTANTS.GRID_COLS + entry.targetX + 1
                            if targetHealthMap[centerIdx] and targetHealthMap[centerIdx] <= 0 then
                                local recastTiles = {}
                                if entry.targetX - 1 >= 0 then
                                    local leftIdx = entry.targetY * CONSTANTS.GRID_COLS + (entry.targetX - 1) + 1
                                    table.insert(recastTiles, { x = entry.targetX - 1, y = entry.targetY })
                                    ProcessTileHit(leftIdx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result, true)
                                    CheckConsTrapRetrigger(leftIdx, targetIsOrder, result, isOrderAbility)
                                    -- Splash on recast tile too
                                    if abilityData.hasSplash then
                                        ApplySplash(entry.targetX - 1, entry.targetY, result.splashTiles, true)
                                    end
                                end
                                if entry.targetX + 1 < CONSTANTS.GRID_COLS then
                                    local rightIdx = entry.targetY * CONSTANTS.GRID_COLS + (entry.targetX + 1) + 1
                                    table.insert(recastTiles, { x = entry.targetX + 1, y = entry.targetY })
                                    ProcessTileHit(rightIdx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result, true)
                                    CheckConsTrapRetrigger(rightIdx, targetIsOrder, result, isOrderAbility)
                                    -- Splash on recast tile too
                                    if abilityData.hasSplash then
                                        ApplySplash(entry.targetX + 1, entry.targetY, result.splashTiles, true)
                                    end
                                end
                                if #recastTiles > 0 then
                                    result.recastTiles = recastTiles
                                end
                            end
                        end

                        -- Vizom_1 Berserk bounce: consume 1 Berserk stack → hit random adjacent tile (no diagonal), repeat
                        -- Allows revisiting tiles, if no valid adjacent tiles exist, recast on same tile
                        if abilityData.hasBerserkRecast and berserkStacks > 0 then
                            result.berserkBounces = {}
                            local lastX = entry.targetX
                            local lastY = entry.targetY

                            while berserkStacks > 0 do
                                berserkStacks = berserkStacks - 1
                                -- Find random adjacent tile (no diagonal), allow revisiting tiles
                                local cardinalDirs = { {0,-1}, {0,1}, {-1,0}, {1,0} }
                                local validAdj = {}
                                for _, dir in ipairs(cardinalDirs) do
                                    local nx = lastX + dir[1]
                                    local ny = lastY + dir[2]
                                    if nx >= 0 and nx < CONSTANTS.GRID_COLS and ny >= 0 and ny < CONSTANTS.GRID_ROWS then
                                        local nIdx = ny * CONSTANTS.GRID_COLS + nx + 1
                                        if targetHealthMap[nIdx] and targetHealthMap[nIdx] > 0 then
                                            table.insert(validAdj, { x = nx, y = ny, idx = nIdx })
                                        end
                                    end
                                end
                                local pick
                                if #validAdj == 0 then
                                    -- No valid adjacent tiles, recast on same tile
                                    pick = { x = lastX, y = lastY, idx = lastY * CONSTANTS.GRID_COLS + lastX + 1 }
                                else
                                    pick = validAdj[ServerRandom(1, #validAdj)]
                                end
                                lastX = pick.x
                                lastY = pick.y
                                table.insert(result.berserkBounces, { x = pick.x, y = pick.y })
                                ProcessTileHit(pick.idx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result, true)
                            end
                            if #result.berserkBounces == 0 then result.berserkBounces = nil end
                        end

                        -- Imp_2: force-move + bonus damage
                        if abilityData.isPull and #result.hits > 0 then
                            local hitUnit = result.hits[1].unitType
                            if hitUnit and entry.param1 and entry.param2 and unitPositions[hitUnit] then
                                local destIdx = entry.param2 * CONSTANTS.GRID_COLS + entry.param1 + 1
                                local destTrapMap = targetIsOrder and orderTraps or chaosTraps
                                local destHealthMap = targetIsOrder and orderHealth or chaosHealth
                                local hasBonusDamage = false
                                local destIsBroken = destHealthMap[destIdx] and destHealthMap[destIdx] <= 0
                                if destTrapMap[destIdx] then
                                    hasBonusDamage = true
                                    local trap = destTrapMap[destIdx]
                                    if trap.isBleedTrap then
                                        local trapDmg = trap.damage
                                        if burnedUnits[hitUnit] ~= nil then
                                            trapDmg = trapDmg + 1
                                        end
                                        local trapShielded = false
                                        local trapStacksRemoved = 0
                                        -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                        if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                            trapStacksRemoved = math.min(shieldedUnits[hitUnit], trapDmg)
                                            shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - trapStacksRemoved
                                            trapShielded = true
                                        end
                                        result.trapHit = {
                                            isOrderGrid = targetIsOrder,
                                            unitType = hitUnit,
                                            gridX = entry.param1,
                                            gridY = entry.param2,
                                            damage = trapDmg,
                                            isBleedTrap = true,
                                            shielded = trapShielded,
                                            shieldStacksRemoved = trapStacksRemoved,
                                            shieldStacksRemaining = shieldedUnits[hitUnit] or 0
                                        }
                                        bleedUnits[hitUnit] = currentTurn
                                        berserkStacks = berserkStacks + 1
                                        result.berserkGranted = true
                                        destTrapMap[destIdx] = nil
                                    elseif trap.isConsTrap then
                                        -- 3x3 AoE chain detonation
                                        local detonated = {}
                                        local detonationQueue = { destIdx }
                                        local totalConsDmg = 0
                                        local impDetonatedIndices = {}

                                        -- Temporarily place pulled unit at destination for AoE check
                                        local savedPullX = unitPositions[hitUnit].x
                                        local savedPullY = unitPositions[hitUnit].y
                                        unitPositions[hitUnit].x = entry.param1
                                        unitPositions[hitUnit].y = entry.param2

                                        while #detonationQueue > 0 do
                                            local detIdx = table.remove(detonationQueue, 1)
                                            if not detonated[detIdx] then
                                                detonated[detIdx] = true
                                                local detTrap = destTrapMap[detIdx]
                                                if detTrap and detTrap.isConsTrap then
                                                    local detCenterDmg = detTrap.damage
                                                    local detSideDmg = detTrap.sideDamage or detCenterDmg
                                                    destTrapMap[detIdx] = nil
                                                    table.insert(impDetonatedIndices, detIdx)
                                                    local detX = (detIdx - 1) % CONSTANTS.GRID_COLS
                                                    local detY = math.floor((detIdx - 1) / CONSTANTS.GRID_COLS)
                                                    for dy = -1, 1 do
                                                        for dx = -1, 1 do
                                                            local ax = detX + dx
                                                            local ay = detY + dy
                                                            if ax >= 0 and ax < CONSTANTS.GRID_COLS and ay >= 0 and ay < CONSTANTS.GRID_ROWS then
                                                                local aIdx = ay * CONSTANTS.GRID_COLS + ax + 1
                                                                for ut, pos in pairs(unitPositions) do
                                                                    if pos.isOrder == targetIsOrder then
                                                                        local posIdx = pos.y * CONSTANTS.GRID_COLS + pos.x + 1
                                                                        if posIdx == aIdx then
                                                                            local baseDetDmg = (dx == 0 and dy == 0) and detCenterDmg or detSideDmg
                                                                            local actualDetDmg = baseDetDmg
                                                                            if burnedUnits[ut] ~= nil then
                                                                                actualDetDmg = actualDetDmg + 1
                                                                            end
                                                                            -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                                                            if shieldedUnits[ut] and shieldedUnits[ut] > 0 then
                                                                                local consStacksRemoved = math.min(shieldedUnits[ut], actualDetDmg)
                                                                                shieldedUnits[ut] = shieldedUnits[ut] - consStacksRemoved
                                                                            end
                                                                            totalConsDmg = totalConsDmg + actualDetDmg
                                                                            damagedUnitsThisTurn[ut] = true
                                                                        end
                                                                    end
                                                                end
                                                                if destTrapMap[aIdx] and destTrapMap[aIdx].isConsTrap and not detonated[aIdx] then
                                                                    table.insert(detonationQueue, aIdx)
                                                                end
                                                            end
                                                        end
                                                    end
                                                end
                                            end
                                        end

                                        -- Restore pulled unit position
                                        unitPositions[hitUnit].x = savedPullX
                                        unitPositions[hitUnit].y = savedPullY

                                        result.trapHit = {
                                            isOrderGrid = targetIsOrder,
                                            unitType = hitUnit,
                                            gridX = entry.param1,
                                            gridY = entry.param2,
                                            damage = totalConsDmg,
                                            isConsTrap = true,
                                            detonatedTrapIndices = impDetonatedIndices
                                        }
                                    else
                                        -- Generic trap at destination
                                        local destTrapDmg = trap.damage
                                        local destTrapStacksRemoved = 0
                                        if burnedUnits[hitUnit] ~= nil then
                                            destTrapDmg = destTrapDmg + 1
                                        end
                                        -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                        if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                            destTrapStacksRemoved = math.min(shieldedUnits[hitUnit], destTrapDmg)
                                            shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - destTrapStacksRemoved
                                        end
                                        result.trapHit = {
                                            isOrderGrid = targetIsOrder,
                                            unitType = hitUnit,
                                            gridX = entry.param1,
                                            gridY = entry.param2,
                                            damage = destTrapDmg,
                                            shielded = destTrapStacksRemoved > 0,
                                            shieldStacksRemoved = destTrapStacksRemoved,
                                            shieldStacksRemaining = shieldedUnits[hitUnit] or 0
                                        }
                                        destTrapMap[destIdx] = nil
                                    end
                                end
                                if destIsBroken then
                                    hasBonusDamage = true
                                end
                                if hasBonusDamage then
                                    local pullBonus = abilityData.pullBonusDamage
                                    local pullBonusStacksRemoved = 0
                                    if burnedUnits[hitUnit] ~= nil then
                                        pullBonus = pullBonus + 1
                                    end
                                    -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                    if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                        pullBonusStacksRemoved = math.min(shieldedUnits[hitUnit], pullBonus)
                                        shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - pullBonusStacksRemoved
                                    end
                                    result.damage = result.damage + pullBonus
                                end
                                if bleedUnits[hitUnit] ~= nil then
                                    local bleedMoveDmg = 1
                                    local bleedMoveStacksRemoved = 0
                                    if burnedUnits[hitUnit] ~= nil then
                                        bleedMoveDmg = bleedMoveDmg + 1
                                    end
                                    -- Shield stacks: each damage point removes 1 stack, damage still goes through
                                    if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                        bleedMoveStacksRemoved = math.min(shieldedUnits[hitUnit], bleedMoveDmg)
                                        shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - bleedMoveStacksRemoved
                                    end
                                    result.damage = result.damage + bleedMoveDmg
                                    result.bleedHit = {
                                        unitType = hitUnit,
                                        isOrder = targetIsOrder,
                                        damage = bleedMoveDmg,
                                        shielded = bleedMoveStacksRemoved > 0,
                                        shieldStacksRemoved = bleedMoveStacksRemoved,
                                        shieldStacksRemaining = shieldedUnits[hitUnit] or 0
                                    }
                                end
                                if destIsBroken then
                                    result.pullBlocked = true
                                else
                                    unitPositions[hitUnit].x = entry.param1
                                    unitPositions[hitUnit].y = entry.param2
                                end
                            end
                        end

                        if result.damage > 0 then
                            result.damagePlayer = isOrderAbility and CONSTANTS.CHAOS_PLAYER or CONSTANTS.ORDER_PLAYER
                        end
                    end

                    -- Passive: Isolation Execution (+1 damage on targets with no ally in move range)
                    -- Passive: Hunter's Mark (consume Marked, deal 2 + hunter-mark-unit-count)
                    if not abilityData.isSupport and #result.hits > 0 then
                        local caster = GetCasterUnit(entry.faction, entry.abilityID, entry.casterUnit)

                        -- Isolation Execution
                        if caster and HasPassive(entry.faction, caster, CONSTANTS.ABILITIES.PASSIVE_3) then
                            for _, hit in ipairs(result.hits) do
                                if hit.unitType and unitPositions[hit.unitType] then
                                    local hitPos = unitPositions[hit.unitType]
                                    local isolated = true
                                    for ut, pos in pairs(unitPositions) do
                                        if ut ~= hit.unitType and pos.isOrder == hitPos.isOrder then
                                            local dist = math.abs(pos.x - hitPos.x) + math.abs(pos.y - hitPos.y)
                                            if dist > 0 and dist <= 2 then
                                                isolated = false
                                                break
                                            end
                                        end
                                    end
                                    if isolated then
                                        result.damage = result.damage + 1
                                        hit.damage = (hit.damage or 0) + 1
                                        hit.isolationBonus = true
                                        Log.info("[PnPServer] Isolation Execution: +1 damage to isolated unit " .. hit.unitType)
                                    end
                                end
                            end
                        end

                        -- Hunter's Mark
                        if caster and HasPassive(entry.faction, caster, CONSTANTS.ABILITIES.PASSIVE_2) then
                            local markedEnemy = hunterMarkedByFaction[entry.faction]
                            local hunterMarkCount = hunterMarkCounts[entry.faction] or 0
                            if markedEnemy and hunterMarkCount > 0 then
                                for _, hit in ipairs(result.hits) do
                                    if hit.unitType == markedEnemy then
                                        local markBonus = 2 + hunterMarkCount
                                        result.damage = result.damage + markBonus
                                        hit.damage = (hit.damage or 0) + markBonus
                                        hit.hunterMarkBonus = true
                                        result.hunterMarkConsumed = { markedUnit = markedEnemy }
                                        hunterMarkedByFaction[entry.faction] = nil
                                        for markerUnit, enemyUnit in pairs(hunterMarks) do
                                            local markerFaction = (markerUnit <= 3) and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                                            if markerFaction == entry.faction and enemyUnit == markedEnemy then
                                                hunterMarks[markerUnit] = nil
                                                break
                                            end
                                        end
                                        self.gameData.hunterMarks = hunterMarks
                                        self.gameData.hunterMarkedByFaction = hunterMarkedByFaction
                                        Log.info("[PnPServer] Hunter's Mark consumed: +" .. markBonus .. " damage to unit " .. markedEnemy ..
                                            " by unit " .. caster .. " (" .. hunterMarkCount .. " Hunter's Mark unit(s))")
                                        break
                                    end
                                end
                            end
                        end

                        -- Ensure damagePlayer is set after passive bonuses
                        if result.damage > 0 and result.damagePlayer == 0 then
                            result.damagePlayer = isOrderAbility and CONSTANTS.CHAOS_PLAYER or CONSTANTS.ORDER_PLAYER
                        end

                        -- Feather Cyclone: redistribute passive bonus into centerHitData
                        -- so the client applies the full damage (passives are added after
                        -- centerHitData/featherHitData were captured)
                        if result.centerHitData and result.featherHitData then
                            local sectionTotal = result.centerHitData.damage or 0
                            for _, fd in ipairs(result.featherHitData) do
                                sectionTotal = sectionTotal + (fd.damage or 0)
                            end
                            local residual = result.damage - sectionTotal
                            if residual > 0 then
                                result.centerHitData.damage = (result.centerHitData.damage or 0) + residual
                            end
                        end
                    end

                    -- Bleed no longer triggers on ability use (only on tile movement)
                end
            end

            results[i] = result
        end

        -- Store pending buffs
        self.gameData._pendingBuffs = {
            fervorStacks = fervorStacks,
            berserkStacks = berserkStacks,
            burnedUnits = burnedUnits,
            bleedUnits = bleedUnits,
            shieldUnits = shieldedUnits
        }

        return results
    end,

    ApplyResolutionToState = function(self, results)
        for _, result in ipairs(results) do
            local action = result.action

            if action.actionType == CONSTANTS.ACTION_MOVEMENT and action.unitType >= 1 and action.unitType <= 6 then
                self.gameData.units[action.unitType].gridPos.x = action.targetX
                self.gameData.units[action.unitType].gridPos.y = action.targetY
            end

            for _, effect in ipairs(result.tileEffects) do
                local healthMap = effect.isOrder and self.gameData.tileHealth.order or self.gameData.tileHealth.chaos
                healthMap[effect.tileIdx] = effect.newHealth
            end

            if result.trap then
                local traps = result.trap.isOrder and self.gameData.traps.order or self.gameData.traps.chaos
                traps[result.trap.tileIdx] = {
                    owner = result.trap.owner,
                    damage = result.trap.damage,
                    sideDamage = result.trap.sideDamage,
                    armed = false,
                    isConsTrap = result.trap.isConsTrap or false,
                    isBleedTrap = result.trap.isBleedTrap or false
                }
            end

            -- Remove triggered traps
            if result.trapHit then
                local traps = result.trapHit.isOrderGrid and self.gameData.traps.order or self.gameData.traps.chaos
                if result.trapHit.isConsTrap and result.trapHit.detonatedTrapIndices then
                    -- Remove all chain-detonated Consecration traps
                    for _, detIdx in ipairs(result.trapHit.detonatedTrapIndices) do
                        traps[detIdx] = nil
                    end
                else
                    local destIdx = result.trapHit.gridY * CONSTANTS.GRID_COLS + result.trapHit.gridX + 1
                    traps[destIdx] = nil
                end
            end

            if result.damage > 0 and result.damagePlayer > 0 then
                local key = result.damagePlayer == CONSTANTS.ORDER_PLAYER and "order" or "chaos"
                self.gameData.playerHealth[key] = self.gameData.playerHealth[key] - result.damage
            end

            if action.abilityID == CONSTANTS.ABILITIES.IMP_2 and result.hits and #result.hits > 0 and not result.pullBlocked then
                local hit = result.hits[1]
                if hit.unitType and action.param1 and action.param2 then
                    self.gameData.units[hit.unitType].gridPos.x = action.param1
                    self.gameData.units[hit.unitType].gridPos.y = action.param2
                end
            end
            if result.healAmount > 0 and result.healPlayer > 0 then
                local key = result.healPlayer == CONSTANTS.ORDER_PLAYER and "order" or "chaos"
                self.gameData.playerHealth[key] = self.gameData.playerHealth[key] + result.healAmount
            end
            if result.trapHit then
                local key = result.trapHit.isOrderGrid and "order" or "chaos"
                self.gameData.playerHealth[key] = self.gameData.playerHealth[key] - result.trapHit.damage
            end

            -- Bleed no longer triggers on ability use (only on tile movement)
        end

        -- Apply persistent buff updates
        if self.gameData._pendingBuffs then
            local pb = self.gameData._pendingBuffs

            self.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] = pb.fervorStacks

            -- Update Berserk stacks (persistent, consumed by Vizom_1)
            self.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] = pb.berserkStacks

            self.gameData.buffs.burn = {}
            for ut, turnApplied in pairs(pb.burnedUnits) do
                if turnApplied then
                    self.gameData.buffs.burn[ut] = turnApplied
                end
            end

            self.gameData.buffs.bleed = {}
            for ut, turnApplied in pairs(pb.bleedUnits) do
                if turnApplied then
                    self.gameData.buffs.bleed[ut] = turnApplied
                end
            end

            self.gameData._pendingBuffs = nil
        end

        -- Consecration traps now persist until triggered (destroyed on detonation during movement)

        Log.info("[PnPServer] Applied resolution. Order HP: " ..
            self.gameData.playerHealth.order .. ", Chaos HP: " .. self.gameData.playerHealth.chaos)
    end
})

return class
