NetworkedGameServer_instance = NetworkedGameServer_instance or nil

--[[
    Server state lives in Persistent, not on the component.

    On a listen server the server outlives any one scene: the host starts it in
    M5LobbySelectionScene and then walks through LobbyRoom, Arsenal and the game
    scenes while it keeps running. The component is destroyed with each scene,
    but ServerRPC_* handlers are globals dispatched from C++ and keep firing -
    so anything they touch has to outlive the entity.

    It previously did not. Reading ServerLobbies() after
    a scene change dereferenced a destroyed script instance and took the whole
    process down, which is what "Leaving room" then silence in the log was.

    Persistent is exactly the mechanism for this: a global backing table that
    survives script re-execution across scene changes.
]]
local function ServerLobbies()
    if not Persistent.serverLobbyList then Persistent.serverLobbyList = {} end
    return Persistent.serverLobbyList
end

local function ServerClients()
    if not Persistent.serverConnectedClients then Persistent.serverConnectedClients = {} end
    return Persistent.serverConnectedClients
end

--[[
    Instance-free lookups.

    The component keeps methods with the same names for anything holding an
    instance, but every free ServerRPC_* handler uses these, so dispatch no
    longer depends on a live entity existing in the current scene.
]]
local function IsExistingClientID(clientID)
    return ServerClients()[clientID] ~= nil
end

--[[
    Client tracking, as free globals rather than component state.

    These are called from ServerRPC_OnClientConnected / _OnClientDisconnected in
    ConnectionCallbacks.lua, which C++ dispatches directly. That matters because
    the host is a player: it walks LobbySelection -> LobbyRoom -> Arsenal -> the
    game scene, and the NetworkedGameServer component only exists in the first
    of those.

    Tracking used to live in that component's OnClientConnected subscriber, so
    it only ran for peers that happened to connect while the host was sitting in
    the lobby-selection scene. The host's own client (which connects the instant
    it hosts) was registered; anyone who joined afterwards never was. Every
    gameplay RPC from them then failed ValidateGameplayClient with
    "No valid client N found", while the lobby RPCs - which do not validate -
    kept working. That combination is why joining, readying and submitting an
    arsenal all succeeded and only deployment silently died.
]]
--[[
    Returns the server component only if it is actually alive.

    A destroyed script instance leaves NetworkedGameServer_instance pointing at
    a userdata whose C++ metadata has been freed, so `if not instance` passes
    and the very next index raises "InstanceIndex: NULL metadata pointer". A
    nil check cannot see this; only touching the thing can.

    One pcall per turn resolution is nothing, and it turns a confusing engine
    error into a message that names the actual problem.
]]
local function LiveServerInstance()
    local instance = NetworkedGameServer_instance
    if not instance then return nil end

    local ok = pcall(function() return instance.started end)
    if not ok then return nil end

    return instance
end

function ServerTrackClient(clientID)
    if clientID == nil then return end
    if ServerClients()[clientID] ~= nil then return end

    ServerClients()[clientID] = { lobby = CONSTANTS.LOBBY.NONE }
    Log.info("[NetworkedGameServer] Client " .. clientID .. " tracked")
end

function ServerUntrackClient(clientID)
    if clientID == nil then return end
    ServerClients()[clientID] = nil
end

--[[
    RPC.sendToClient, but only when there is a server to send from.

    Shutting the server down closes every peer, and each closure dispatches
    ServerRPC_OnClientDisconnected back into Lua - by which point the socket is
    already gone. The cleanup that runs there ends in a broadcast, and
    RPC.sendToClient raises "Server is not running" rather than returning, so
    the last few disconnects of every teardown produced a wall of Lua errors.
    They were harmless (the call is dispatched under pcall) but they buried the
    real ones.
]]
function ServerSendToClient(clientID, handlerName, ...)
    if clientID == nil or clientID == CONSTANTS.CLIENT.INVALID then return end
    if not RPC.isHosting() then return end

    RPC.sendToClient(clientID, handlerName, ...)
end

local function LookupLobbyByName(lobbyName)
    return ServerLobbies()[lobbyName]
end

local function LookupLobbyByClient(clientID)
    for name, lobby in pairs(ServerLobbies()) do
        for _, id in ipairs(lobby.clients or {}) do
            if id == clientID then
                return name, lobby
            end
        end
    end
    return nil, nil
end

local function ValidateGameplayClient(clientID)
    if not IsExistingClientID(clientID) then
        Log.error("[NetworkGameServer] No valid client " .. clientID .. " found..")
        return false
    end

    local _, lobby = LookupLobbyByClient(clientID)

    if not lobby then
        Log.error("[NetworkGameServer] No valid lobby for client " .. clientID .. " found..")
        return false
    end

    return (lobby.gameData.order.client == clientID)
        or (lobby.gameData.chaos.client == clientID)
end

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

    -- Prefer unused directions
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

-- Build passive lookup from an arsenal map
-- Returns: unitPassives[unitType] = { [passiveID] = true }
--          abilityToUnit[resolvedAbilityID] = unitType
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
                Log.info("[Server] Hunter's Mark: faction " .. faction .. " (" .. #markerUnits .. " marker unit(s)) marked enemy unit " .. markedEnemy)
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
            return {} -- server generates random tiles
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
local nextRoomID = 1

-- Helper: find a lobby entry by its roomID
local function FindLobbyByRoomID(roomID)
    local key = "Room_" .. roomID
    return key, ServerLobbies()[key]
end

local function LobbyHasClient(lobby, clientID)
    if not lobby or not lobby.clients then return false end
    for _, id in ipairs(lobby.clients) do
        if id == clientID then
            return true
        end
    end
    return false
end

local function AddLobbyClient(lobby, clientID)
    if not lobby then return end
    lobby.clients = lobby.clients or {}
    if not LobbyHasClient(lobby, clientID) then
        table.insert(lobby.clients, clientID)
    end
end

local function RemoveLobbyClient(lobby, clientID)
    if not lobby or not lobby.clients then return end
    for i = #lobby.clients, 1, -1 do
        if lobby.clients[i] == clientID then
            table.remove(lobby.clients, i)
        end
    end
end

local function GetClientFactionInLobby(lobby, clientID)
    if not lobby or not lobby.gameData then return nil end
    if lobby.gameData.order and lobby.gameData.order.client == clientID then
        return CONSTANTS.ORDER_PLAYER
    end
    if lobby.gameData.chaos and lobby.gameData.chaos.client == clientID then
        return CONSTANTS.CHAOS_PLAYER
    end
    return nil
end

--[[
    Drops rooms nobody is connected to any more.

    The room list is Persistent so it can outlive the scene the server
    component lives in, which is exactly why it also outlives the connections
    the rooms were built around. A room whose occupants are all gone is not
    recoverable state - it is a listing that cannot be joined (the factions
    still read as taken) and, worse, a room that ServerRPC_CreateRoom will find
    the next time its old occupant hosts.

    Membership is checked against the tracked client list rather than against
    the room's own bookkeeping, because that list is maintained from the C++
    connect/disconnect callbacks and is true regardless of what any scene did.

    Returns true when something was removed, so callers can decide whether a
    broadcast is worth sending.
]]
local function PurgeDeadLobbies()
    local clients = ServerClients()
    local lobbies = ServerLobbies()
    local changed = false

    for lobbyKey, lobby in pairs(lobbies) do
        -- Drop members the transport no longer knows about.
        if lobby.clients then
            for i = #lobby.clients, 1, -1 do
                if clients[lobby.clients[i]] == nil then
                    table.remove(lobby.clients, i)
                    changed = true
                end
            end
        else
            lobby.clients = {}
        end

        -- Free faction slots held by clients that are gone, so the room does
        -- not advertise itself as full while standing empty.
        if lobby.gameData then
            if lobby.gameData.order
                and lobby.gameData.order.client ~= CONSTANTS.CLIENT.INVALID
                and clients[lobby.gameData.order.client] == nil then
                lobby.gameData.order.client = CONSTANTS.CLIENT.INVALID
                lobby.orderName = ""
                lobby.orderReady = false
                changed = true
            end

            if lobby.gameData.chaos
                and lobby.gameData.chaos.client ~= CONSTANTS.CLIENT.INVALID
                and clients[lobby.gameData.chaos.client] == nil then
                lobby.gameData.chaos.client = CONSTANTS.CLIENT.INVALID
                lobby.chaosName = ""
                lobby.chaosReady = false
                changed = true
            end
        end

        if #lobby.clients == 0 then
            Log.info("[Server] Removing empty room " .. tostring(lobby.roomID)
                .. " (" .. lobbyKey .. ")")
            lobbies[lobbyKey] = nil
            changed = true
        end
    end

    return changed
end

function ServerRPC_OnDeployUnit(clientID, unitType, gridPosX, gridPosY)
    
    if not ValidateGameplayClient(clientID) then return end

    if unitType < 0 or unitType > CONSTANTS.UNIT.VIZOM then
        Log.error("[NetworkedGameClient] Invalid unit type " .. unitType)
        return
    end

    -- TODO: validate if gridPos is valid (check for invalid input)
    local name, lobby = LookupLobbyByClient(clientID)
    lobby.gameData.units[unitType].gridPos = {
        x = gridPosX,
        y = gridPosY
    }

    Log.info("Deployed unit type " .. unitType .. " on gridPos " .. 
                lobby.gameData.units[unitType].gridPos.x .. ", " ..
                lobby.gameData.units[unitType].gridPos.y
            )

    -- Broadcasts to both clients
    for _, id in ipairs(lobby.clients) do
        ServerSendToClient(id, "ClientRPC_OnReturnDeployUnit", unitType, gridPosX, gridPosY)
    end
end

function ServerRPC_OnClientEndDeploymentPhase(clientID)

    if not ValidateGameplayClient(clientID) then return end

    local name, lobby = LookupLobbyByClient(clientID)

    local isOrder = (lobby.gameData.order.client == clientID)

    if isOrder then 
        for i, unit in ipairs(lobby.gameData.units) do
            if (i < CONSTANTS.UNIT.IMP) and (not unit.gridPos) then 
                ServerSendToClient(clientID, "ClientRPC_OnReturnEndDeploymentPhase", false)
                return
            end
        end
        ServerSendToClient(clientID, "ClientRPC_OnReturnEndDeploymentPhase", true)
        lobby.gameData.order.ready = true
    else 
        for i, unit in ipairs(lobby.gameData.units) do
            if (i > CONSTANTS.UNIT.TIRION) and (not unit.gridPos) then 
                ServerSendToClient(clientID, "ClientRPC_OnReturnEndDeploymentPhase", false)
                return
            end
        end
        ServerSendToClient(clientID, "ClientRPC_OnReturnEndDeploymentPhase", true)
        lobby.gameData.chaos.ready = true 
    end

    -- if logic reaches here, it probably means all units are properly deployed.
    if lobby.gameData.order.ready and lobby.gameData.chaos.ready then
        -- Initialize prep phase data before broadcasting
        lobby.gameData.order.ready = false
        lobby.gameData.order.moveSupportQueue = {}
        lobby.gameData.order.attackQueue = {}
        lobby.gameData.chaos.ready = false
        lobby.gameData.chaos.moveSupportQueue = {}
        lobby.gameData.chaos.attackQueue = {}
        lobby.gameData.phase = "preparation"
        lobby.gameData.turnNumber = 1
        lobby.gameData.turnHistory = {}  -- array of per-turn consolidated queues

        -- Initialize game state tracking
        local totalTiles = CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS
        lobby.gameData.tileHealth = { order = {}, chaos = {} }
        lobby.gameData.traps = { order = {}, chaos = {} }
        for i = 1, totalTiles do
            lobby.gameData.tileHealth.order[i] = CONSTANTS.TILE_MAX_HEALTH
            lobby.gameData.tileHealth.chaos[i] = CONSTANTS.TILE_MAX_HEALTH
        end
        lobby.gameData.playerHealth = {
            order = CONSTANTS.MAX_HP,
            chaos = CONSTANTS.MAX_HP
        }

        -- Initialize buff/debuff tracking
        lobby.gameData.buffs = {
            burn = {},      -- [unitType] = turnApplied (+1 damage taken from all sources for current+next resolution)
            fervor = {},    -- [unitType] = stacks (permanent, +1 damage per stack)
            bleed = {},     -- [unitType] = turnApplied (1 damage per tile moved, expires end of next planning phase)
            berserk = {},   -- [unitType] = stacks (consumed by Vizom_1 to bounce to adjacent tiles)
        }
        -- Fervor stacks start at 0 for Tirion (unit 3)
        lobby.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] = 0
        -- Berserk stacks start at 0 for Vizom (unit 6)
        lobby.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] = 0

        -- Generate initial Hunter's Marks for turn 1
        local initPositions = {}
        for i = 1, 6 do
            initPositions[i] = { x = lobby.gameData.units[i].gridPos.x, y = lobby.gameData.units[i].gridPos.y, isOrder = (i <= 3) }
        end
        local orderP = BuildPassiveLookup(lobby.gameData.order.arsenalMap)
        local chaosP = BuildPassiveLookup(lobby.gameData.chaos.arsenalMap)
        local initFactionPassives = { [CONSTANTS.ORDER_PLAYER] = orderP, [CONSTANTS.CHAOS_PLAYER] = chaosP }
        local initMarks, initMarkCounts, initMarkedByFaction = GenerateHunterMarks(initFactionPassives, initPositions)
        lobby.gameData.hunterMarks = initMarks
        lobby.gameData.hunterMarkCounts = initMarkCounts
        lobby.gameData.hunterMarkedByFaction = initMarkedByFaction

        -- Send initial Hunter's Marks BEFORE deployment end so they arrive before prep phase Enter
        for _, id in ipairs(lobby.clients) do
            ServerSendToClient(id, "ClientRPC_OnHunterMarks", lobby.gameData.hunterMarks or {})
        end

        for _, id in ipairs(lobby.clients) do
            ServerSendToClient(id, "ClientRPC_OnEndDeploymentPhase",
                lobby.gameData.units[CONSTANTS.UNIT.ORACLE].gridPos.x,
                lobby.gameData.units[CONSTANTS.UNIT.ORACLE].gridPos.y,
                lobby.gameData.units[CONSTANTS.UNIT.DRAGOON].gridPos.x,
                lobby.gameData.units[CONSTANTS.UNIT.DRAGOON].gridPos.y,
                lobby.gameData.units[CONSTANTS.UNIT.TIRION].gridPos.x,
                lobby.gameData.units[CONSTANTS.UNIT.TIRION].gridPos.y,
                lobby.gameData.units[CONSTANTS.UNIT.IMP].gridPos.x,
                lobby.gameData.units[CONSTANTS.UNIT.IMP].gridPos.y,
                lobby.gameData.units[CONSTANTS.UNIT.EYE].gridPos.x,
                lobby.gameData.units[CONSTANTS.UNIT.EYE].gridPos.y,
                lobby.gameData.units[CONSTANTS.UNIT.VIZOM].gridPos.x,
                lobby.gameData.units[CONSTANTS.UNIT.VIZOM].gridPos.y
            )
        end
    end
end

function ServerRPC_OnSubmitActions(clientID, moveSupportQueue, attackQueue)
    if not ValidateGameplayClient(clientID) then return end

    local name, lobby = LookupLobbyByClient(clientID)

    if lobby.gameData.phase == "gameover" then
        Log.warning("[Server] Game is over, rejecting actions from client " .. clientID)
        return
    end

    local isOrder = (lobby.gameData.order.client == clientID)
    local playerData = isOrder and lobby.gameData.order or lobby.gameData.chaos

    Log.info("[Server] ====== RECEIVED ACTIONS from " .. (isOrder and "Order" or "Chaos") .. " (client " .. clientID .. ") ======")
    Log.info("[Server] moveSupportQueue: " .. (moveSupportQueue and #moveSupportQueue or "nil") ..
        " items, attackQueue: " .. (attackQueue and #attackQueue or "nil") .. " items")

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
        Log.warning("[Server] " .. (isOrder and "Order" or "Chaos") .. " exceeded AP limit (" .. totalCost .. "/" .. CONSTANTS.MAX_AP .. ")")
        return
    end

    playerData.moveSupportQueue = moveSupportQueue or {}
    playerData.attackQueue = attackQueue or {}
    playerData.ready = true

    Log.info("[Server] " .. (isOrder and "Order" or "Chaos") .. " submitted actions (moveSupport: " ..
        #playerData.moveSupportQueue .. ", attack: " .. #playerData.attackQueue .. ")")

    if lobby.gameData.order.ready and lobby.gameData.chaos.ready then
        Log.info("[Server] Both players ready! Starting resolution broadcast..")
        -- The only remaining path that still needs a live component: turn
        -- resolution reaches deep into gameplay helpers that are all methods.
        -- Everything on the lobby/room path is instance-free now, so the host
        -- only needs a GameServer entity in the scenes where combat resolves.
        --
        -- Fail loudly rather than crash. Dereferencing a destroyed script
        -- instance takes the whole process down with no Lua error at all,
        -- which is exactly the bug this refactor removed from the lobby path.
        local server = LiveServerInstance()

        if not server then
            Log.error("[Server] Cannot resolve the turn: no live NetworkedGameServer component "
                .. "in the current scene. Turn resolution is the one path that still needs one - "
                .. "add a GameServer entity (autoStart = false) to the gameplay scene.")
            return
        end

        server:BroadcastResolution(lobby)
    else
        Log.info("[Server] Waiting for " .. (lobby.gameData.order.ready and "Chaos" or "Order") .. " to submit")
    end
end

function ServerRPC_CreateOrJoinLobby(clientID, lobbyName, password, faction)
    Log.info("[NetworkedGameServer] Client " .. clientID .. " trying to create or join lobby " .. lobbyName)
    
    if not faction then
        Log.error("[NetworkedGameServer] Client " .. clientID .. " has not picked a faction!")
        return
    end
    
    if IsExistingClientID(clientID) then
        local lobby = LookupLobbyByName(lobbyName)
        Log.info("[NetworkedGameServer] Client " .. clientID .. " exists, joining lobby..")
        if lobby then
            Log.info("Lobby " .. lobbyName .. " already exists! Joining..")
            table.insert(lobby.clients, clientID)
            
            -- Check if it's even possible to join the lobby in the first place
            if (lobby.gameData.order.client >= 0) and (lobby.gameData.chaos.client >= 0) then
                Log.info("[NetworkedGameServer] Lobby " .. lobbyName .. " is full!")
                return
            end
            
            -- Join based on preference, else join available
            if faction == CONSTANTS.ORDER_PLAYER then
                if lobby.gameData.order.client < 0 then
                    Log.info("Order selected, Order was available")
                    lobby.gameData.order.client = clientID
                else
                    Log.info("Order selected, Chaos was available")
                    lobby.gameData.chaos.client = clientID
                end
            else
                if lobby.gameData.chaos.client < 0 then
                    Log.info("Chaos selected, Chaos was available")
                    lobby.gameData.chaos.client = clientID
                else
                    Log.info("Chaos selected, Order was available")
                    lobby.gameData.order.client = clientID
                end
            end

            Log.info("[NetworkedGameServer] Client " .. clientID .. " successfully joined lobby..")
            ServerSendToClient(clientID, "ClientRPC_OnReturnLobbyJoined", lobbyName)
            return
        end

        if faction == CONSTANTS.ORDER_PLAYER then
            ServerLobbies()[lobbyName] = {
                name = lobbyName,
                password = password,
                clients = {
                    clientID
                },
                gameData = {
                    units = {
                        {}, {}, {}, {}, {}, {}
                    },
                    order = {
                        client = clientID,
                        ready = false
                    },
                    chaos = {
                        client = CONSTANTS.CLIENT.INVALID,
                        ready = false
                    }
                }
            }
        else
            ServerLobbies()[lobbyName] = {
                name = lobbyName,
                password = password,
                clients = {
                    clientID
                },
                gameData = {
                    units = {
                        {}, {}, {}, {}, {}, {}
                    },
                    order = {
                        client = CONSTANTS.CLIENT.INVALID,
                        ready = false
                    },
                    chaos = {
                        client = clientID,
                        ready = false
                    }
                }
            }
        end

        ServerSendToClient(clientID, "ClientRPC_OnReturnLobbyJoined", lobbyName)
    end
end

function ServerRPC_CreateRoom(clientID, playerName, roomName, faction)
    Log.info("[Server] Client " .. clientID .. " creating room: " .. roomName)

    if faction ~= CONSTANTS.ORDER_PLAYER and faction ~= CONSTANTS.CHAOS_PLAYER then
        faction = CONSTANTS.ORDER_PLAYER
    end

    -- Sweep first. The room list survives in Persistent across scenes and
    -- across matches, so without this the lookup below could match a room from
    -- a finished game whose occupants are all long gone.
    PurgeDeadLobbies()

    local existingLobbyKey, existingLobby = LookupLobbyByClient(clientID)
    if existingLobby and existingLobby.roomID then
        local existingFaction = GetClientFactionInLobby(existingLobby, clientID)

        -- Being here at all means one of two things, and they need opposite
        -- answers:
        --
        --   The player double-clicked Create Room. The second click must not
        --   spawn a second room, and re-answering with what they already have
        --   is exactly right.
        --
        --   The player asked for the other faction. Answering with the seat
        --   they currently hold is how "host as Order" produced a Chaos
        --   player: the room was already theirs from the previous click (or,
        --   before the sweep above, from the previous match), and the faction
        --   they had just chosen was thrown away. Move them instead, when the
        --   seat they asked for is free.
        if existingFaction and existingFaction ~= faction then
            local wanted = (faction == CONSTANTS.ORDER_PLAYER) and "order" or "chaos"
            local held   = (faction == CONSTANTS.ORDER_PLAYER) and "chaos" or "order"

            if existingLobby.gameData[wanted].client == CONSTANTS.CLIENT.INVALID then
                Log.info("[Server] Client " .. clientID .. " re-seating from "
                    .. held .. " to " .. wanted .. " in room " .. existingLobby.roomID)

                existingLobby.gameData[held].client = CONSTANTS.CLIENT.INVALID
                existingLobby[held .. "Name"] = ""
                existingLobby[held .. "Ready"] = false

                existingLobby.gameData[wanted].client = clientID
                existingLobby[wanted .. "Name"] = playerName
                existingLobby[wanted .. "Ready"] = false

                existingFaction = faction
            else
                Log.warning("[Server] Client " .. clientID .. " wanted faction " .. faction
                    .. " in room " .. existingLobby.roomID .. " but that seat is taken")
            end
        end

        existingFaction = existingFaction or faction

        Log.warning("[Server] Client " .. clientID .. " already in room " .. existingLobby.roomID .. " (" .. tostring(existingLobbyKey) .. ")")
        local opponentName = ""
        if existingFaction == CONSTANTS.ORDER_PLAYER then
            opponentName = existingLobby.chaosName or ""
        else
            opponentName = existingLobby.orderName or ""
        end
        ServerSendToClient(clientID, "ClientRPC_RoomCreated", existingLobby.roomID)
        ServerSendToClient(clientID, "ClientRPC_JoinRoomSuccess", existingLobby.roomID, existingFaction, existingLobby.name, opponentName)
        ServerRPC_BroadcastRoomList()
        ServerRPC_BroadcastRoomState(existingLobby.roomID)
        return
    end

    local roomID = nextRoomID
    nextRoomID = nextRoomID + 1
    local lobbyKey = "Room_" .. roomID

    ServerLobbies()[lobbyKey] = {
        -- Room metadata
        roomID = roomID,
        name = roomName,
        hostClientID = clientID,
        orderName = "",
        chaosName = "",
        orderReady = false,
        chaosReady = false,
        status = "Waiting",

        -- Gameplay data (same format used by deploy/submit/resolution RPCs)
        password = "",
        clients = {},
        gameData = {
            units = { {}, {}, {}, {}, {}, {} },
            order = {
                client = CONSTANTS.CLIENT.INVALID,
                ready = false
            },
            chaos = {
                client = CONSTANTS.CLIENT.INVALID,
                ready = false
            }
        }
    }

    ServerSendToClient(clientID, "ClientRPC_RoomCreated", roomID)
    ServerRPC_JoinRoom(clientID, playerName, roomID, faction)
    
    Log.info("[Server] Room " .. roomID .. " created (lobby key: " .. lobbyKey .. ")")

    ServerRPC_BroadcastRoomList()
end

function ServerRPC_JoinRoom(clientID, playerName, roomID, faction)
    Log.info("[Server] Client " .. clientID .. " joining room " .. roomID .. " as faction " .. faction)
    
    local lobbyKey, lobby = FindLobbyByRoomID(roomID)
    if not lobby then
        Log.error("[Server] Room " .. roomID .. " not found")
        ServerSendToClient(clientID, "ClientRPC_JoinRoomFailed", "Room not found")
        return
    end
    
    if lobby.status == "Started" then
        Log.warning("[Server] Room already started")
        ServerSendToClient(clientID, "ClientRPC_JoinRoomFailed", "Game already started")
        return
    end

    if faction ~= CONSTANTS.ORDER_PLAYER and faction ~= CONSTANTS.CHAOS_PLAYER then
        faction = CONSTANTS.ORDER_PLAYER
    end

    local existingLobbyName, existingLobby = LookupLobbyByClient(clientID)
    if existingLobby and existingLobby.roomID and existingLobby.roomID ~= roomID then
        Log.warning("[Server] Client " .. clientID .. " attempted to join room " .. roomID ..
            " while already in room " .. existingLobby.roomID)
        ServerSendToClient(clientID, "ClientRPC_JoinRoomFailed", "Already in another room")
        return
    end

    local existingFaction = GetClientFactionInLobby(lobby, clientID)
    if existingFaction then
        Log.info("[Server] Client " .. clientID .. " already joined room " .. roomID ..
            " as faction " .. existingFaction .. " (idempotent join)")
        AddLobbyClient(lobby, clientID)
        local opponentName = ""
        if existingFaction == CONSTANTS.ORDER_PLAYER then
            opponentName = lobby.chaosName or ""
        else
            opponentName = lobby.orderName or ""
        end
        ServerSendToClient(clientID, "ClientRPC_JoinRoomSuccess", roomID, existingFaction, lobby.name, opponentName)
        ServerRPC_BroadcastRoomState(roomID)
        return
    end
    
    local assignedFaction = faction
    if faction == CONSTANTS.ORDER_PLAYER then
        if lobby.gameData.order.client == CONSTANTS.CLIENT.INVALID then
            assignedFaction = CONSTANTS.ORDER_PLAYER
        elseif lobby.gameData.chaos.client == CONSTANTS.CLIENT.INVALID then
            assignedFaction = CONSTANTS.CHAOS_PLAYER
        else
            Log.warning("[Server] Room full")
            ServerSendToClient(clientID, "ClientRPC_JoinRoomFailed", "Room is full")
            return
        end
    else
        if lobby.gameData.chaos.client == CONSTANTS.CLIENT.INVALID then
            assignedFaction = CONSTANTS.CHAOS_PLAYER
        elseif lobby.gameData.order.client == CONSTANTS.CLIENT.INVALID then
            assignedFaction = CONSTANTS.ORDER_PLAYER
        else
            Log.warning("[Server] Room full")
            ServerSendToClient(clientID, "ClientRPC_JoinRoomFailed", "Room is full")
            return
        end
    end

    if assignedFaction == CONSTANTS.ORDER_PLAYER then
        lobby.gameData.order.client = clientID
        lobby.orderName = playerName
    else
        lobby.gameData.chaos.client = clientID
        lobby.chaosName = playerName
    end

    AddLobbyClient(lobby, clientID)
    
    Log.info("[Server] Client " .. clientID .. " joined room " .. roomID ..
        " as faction " .. assignedFaction)
    
    -- Send opponent name (if Order, opponent is Chaos and vice versa)
    local opponentName = ""
    if assignedFaction == CONSTANTS.ORDER_PLAYER then
        opponentName = lobby.chaosName or ""
    else
        opponentName = lobby.orderName or ""
    end
    
    ServerSendToClient(clientID, "ClientRPC_JoinRoomSuccess", roomID, assignedFaction, lobby.name, opponentName)

    ServerRPC_BroadcastRoomList()
    ServerRPC_BroadcastRoomState(roomID)
end

function ServerRPC_LeaveRoom(clientID, roomID)
    Log.info("[Server] Client " .. clientID .. " leaving room " .. roomID)
    
    local lobbyKey, lobby = FindLobbyByRoomID(roomID)
    if not lobby then return end
    
    if lobby.gameData.order.client == clientID then
        lobby.gameData.order.client = CONSTANTS.CLIENT.INVALID
        lobby.orderName = ""
        lobby.orderReady = false
    elseif lobby.gameData.chaos.client == clientID then
        lobby.gameData.chaos.client = CONSTANTS.CLIENT.INVALID
        lobby.chaosName = ""
        lobby.chaosReady = false
    end

    -- Remove from clients list
    RemoveLobbyClient(lobby, clientID)
    
    if #lobby.clients == 0 then
        ServerLobbies()[lobbyKey] = nil
        Log.info("[Server] Room " .. roomID .. " deleted (empty)")
    end
    
    ServerRPC_BroadcastRoomList()
    if ServerLobbies()[lobbyKey] then
        ServerRPC_BroadcastRoomState(roomID)
    end
end

--[[
    A client is gone. Take them out of whatever room they were in.

    Free function, not a component subscriber, for the same reason
    ServerTrackClient is: ServerRPC_OnClientDisconnected is dispatched from C++
    and fires wherever the host happens to be, while the NetworkedGameServer
    component only exists in M5LobbySelectionScene and M6NetworkGame. The host
    spends the whole lobby-room and arsenal stretch in scenes that have no such
    component, and anyone who quit during it was simply never removed: their
    faction stayed occupied, the room stayed listed as full, and it never
    deleted itself once the last player left.

    Called before ServerUntrackClient, so it untracks the client itself first -
    PurgeDeadLobbies decides what is dead by asking the tracked-client list,
    and would otherwise still see this one as live.
]]
function ServerHandleClientDisconnected(clientID)
    if clientID == nil then return end

    ServerClients()[clientID] = nil

    local lobbyKey, lobby = LookupLobbyByClient(clientID)

    if lobby then
        -- Identify the opponent before the slots are cleared.
        local remainingClient = nil
        if lobby.gameData.order.client == clientID and
           lobby.gameData.chaos.client ~= CONSTANTS.CLIENT.INVALID then
            remainingClient = lobby.gameData.chaos.client
        elseif lobby.gameData.chaos.client == clientID and
               lobby.gameData.order.client ~= CONSTANTS.CLIENT.INVALID then
            remainingClient = lobby.gameData.order.client
        end

        RemoveLobbyClient(lobby, clientID)

        if lobby.gameData.order.client == clientID then
            lobby.gameData.order.client = CONSTANTS.CLIENT.INVALID
            lobby.orderName = ""
            lobby.orderReady = false
        elseif lobby.gameData.chaos.client == clientID then
            lobby.gameData.chaos.client = CONSTANTS.CLIENT.INVALID
            lobby.chaosName = ""
            lobby.chaosReady = false
        end

        -- A match in progress cannot continue one-sided: tell whoever is left
        -- and close the room rather than leaving half a game listed.
        if lobby.status == "Started" then
            if remainingClient then
                Log.info("[Server] Match in room " .. tostring(lobby.roomID)
                    .. " interrupted; notifying client " .. remainingClient)
                ServerSendToClient(remainingClient, "ClientRPC_OpponentDisconnected")
            end

            ServerLobbies()[lobbyKey] = nil
            Log.info("[Server] Removed started room " .. tostring(lobbyKey))
        end
    else
        Log.info("[Server] Client " .. clientID .. " was not in a room")
    end

    -- Sweeps up this room if it is now empty, plus anything an earlier
    -- disconnect left behind while no server component was around to see it.
    PurgeDeadLobbies()

    ServerRPC_BroadcastRoomList()

    if lobby and lobby.roomID and ServerLobbies()[lobbyKey] then
        ServerRPC_BroadcastRoomState(lobby.roomID)
    end
end

function ServerRPC_SetReady(clientID, roomID, isReady)
    Log.info("[Server] Client " .. clientID .. " ready: " .. tostring(isReady))
    
    local lobbyKey, lobby = FindLobbyByRoomID(roomID)
    if not lobby then return end
    
    local readyValue = (isReady == 1)
    
    if lobby.gameData.order.client == clientID then
        lobby.orderReady = readyValue
    elseif lobby.gameData.chaos.client == clientID then
        lobby.chaosReady = readyValue
    end
    
    ServerRPC_BroadcastRoomState(roomID)
end

function ServerRPC_RequestRoomList(clientID)
    Log.info("[Server] Client " .. clientID .. " requesting room list")
    ServerRPC_BroadcastRoomList(clientID)
end

function ServerRPC_SubmitArsenal(clientID, roomID, arsenalMap)
    Log.info("[Server] Client " .. clientID .. " submitting arsenal for room " .. roomID)

    local lobbyKey, lobby = FindLobbyByRoomID(roomID)
    if not lobby then
        Log.error("[Server] Room " .. roomID .. " not found for arsenal submission")
        ServerSendToClient(clientID, "ClientRPC_ArsenalRejected", "Room not found")
        return
    end

    -- Determine which faction this client is
    local isOrder = (lobby.gameData.order.client == clientID)
    local isChaos = (lobby.gameData.chaos.client == clientID)

    if not isOrder and not isChaos then
        Log.error("[Server] Client " .. clientID .. " is not in room " .. roomID)
        ServerSendToClient(clientID, "ClientRPC_ArsenalRejected", "Not in room")
        return
    end

    -- Validate: arsenalMap must be a table with numeric keys and values
    if type(arsenalMap) ~= "table" then
        ServerSendToClient(clientID, "ClientRPC_ArsenalRejected", "Invalid arsenal data")
        return
    end

    -- Basic validation: ensure all mapped abilities are valid IDs (1-18)
    for originalID, equippedID in pairs(arsenalMap) do
        if type(originalID) ~= "number" or type(equippedID) ~= "number" then
            ServerSendToClient(clientID, "ClientRPC_ArsenalRejected", "Invalid ability IDs")
            return
        end
        if equippedID < 1 or equippedID > 18 then
            ServerSendToClient(clientID, "ClientRPC_ArsenalRejected", "Ability ID out of range")
            return
        end
    end

    -- Store the arsenal map on the lobby for this faction
    local factionKey = isOrder and "order" or "chaos"
    lobby.gameData[factionKey].arsenalMap = arsenalMap

    Log.info("[Server] Arsenal accepted for " .. factionKey .. " in room " .. roomID)
    ServerSendToClient(clientID, "ClientRPC_ArsenalConfirmed", arsenalMap)

    -- Check if both factions have submitted their arsenal
    if lobby.gameData.order.arsenalMap and lobby.gameData.chaos.arsenalMap then
        Log.info("[Server] Both arsenals confirmed for room " .. roomID .. " — starting game!")
        -- Send each client the opponent's arsenal map so they can display enemy abilities/passives
        ServerSendToClient(lobby.gameData.order.client, "ClientRPC_ArsenalBothReady", roomID, lobby.gameData.chaos.arsenalMap)
        ServerSendToClient(lobby.gameData.chaos.client, "ClientRPC_ArsenalBothReady", roomID, lobby.gameData.order.arsenalMap)
    end
end

function ServerRPC_StartGame(clientID, roomID)
    Log.info("[Server] Client " .. clientID .. " requesting game start for room " .. roomID)
    
    local lobbyKey, lobby = FindLobbyByRoomID(roomID)
    if not lobby then
        Log.error("[Server] Room " .. roomID .. " not found")
        return
    end
    
    if lobby.status == "Started" then
        Log.warning("[Server] Room " .. roomID .. " already started")
        return
    end

    if lobby.gameData.order.client == lobby.gameData.chaos.client then
        Log.error("[Server] Invalid room state: same client occupies both factions in room " .. roomID)
        return
    end
    
    if not (lobby.orderReady and lobby.chaosReady and
            lobby.gameData.order.client ~= CONSTANTS.CLIENT.INVALID and
            lobby.gameData.chaos.client ~= CONSTANTS.CLIENT.INVALID) then
        Log.warning("[Server] Cannot start: not all players ready")
        return
    end
    
    lobby.status = "Started"
    Log.info("[Server] Room " .. roomID .. " starting game! (lobby key: " .. lobbyKey .. ")")
    
    -- Notify both clients
    ServerSendToClient(lobby.gameData.order.client, "ClientRPC_StartGame", roomID)
    ServerSendToClient(lobby.gameData.chaos.client, "ClientRPC_StartGame", roomID)
    
    ServerRPC_BroadcastRoomList()
end


--[[
    Mirror faction occupancy into the Steam lobby metadata.

    A joiner browsing the lobby list has no connection to this host, so it
    cannot ask the server which slots are taken - the room list only exists
    once you are connected. Lobby data is the one channel that reaches a
    browser before it connects, so the host publishes the answer there.

    Host-only and no-ops without Steam; Steam ignores SetLobbyData from a
    non-owner anyway.
]]
local function PublishLobbySlots()
    if not Steam.isAvailable() then return end
    if Steam.getLobbyId() == "0" then return end
    if not Steam.isHost() then return end

    -- This build hosts one room per lobby, so the first room's occupancy is
    -- what a browser wants to see.
    local orderTaken, chaosTaken = false, false

    for _, lobby in pairs(ServerLobbies()) do
        orderTaken = lobby.gameData.order.client ~= CONSTANTS.CLIENT.INVALID
        chaosTaken = lobby.gameData.chaos.client ~= CONSTANTS.CLIENT.INVALID
        break
    end

    Steam.setLobbyData("gg_order", orderTaken and "1" or "0")
    Steam.setLobbyData("gg_chaos", chaosTaken and "1" or "0")
end

function ServerRPC_BroadcastRoomList(targetClientID)
    -- Every path that changes who is in a room routes through here, which
    -- makes it the one place worth sweeping dead rooms from as well. Without
    -- it a browser could be handed a room whose players had all gone - joining
    -- it either reported the factions as taken or dropped the joiner into a
    -- room with nobody in it.
    PurgeDeadLobbies()

    -- Same argument for the lobby metadata: this is where occupancy changes.
    PublishLobbySlots()

    local roomList = {}
    for lobbyKey, lobby in pairs(ServerLobbies()) do
        if lobby.roomID then
            table.insert(roomList, {
                roomID = lobby.roomID,
                name = lobby.name,
                orderJoined = lobby.gameData.order.client ~= CONSTANTS.CLIENT.INVALID and 1 or 0,
                chaosJoined = lobby.gameData.chaos.client ~= CONSTANTS.CLIENT.INVALID and 1 or 0,
                status = lobby.status
            })
        end
    end

    if targetClientID then
        ServerSendToClient(targetClientID, "ClientRPC_RoomList", roomList)
    else
        for clientID, _ in pairs(ServerClients()) do
            ServerSendToClient(clientID, "ClientRPC_RoomList", roomList)
        end
    end
end

function ServerRPC_BroadcastRoomState(roomID)
    local lobbyKey, lobby = FindLobbyByRoomID(roomID)
    if not lobby then return end
    
    local state = {
        roomID = roomID,
        orderName = lobby.orderName,
        chaosName = lobby.chaosName,
        orderJoined = lobby.gameData.order.client ~= CONSTANTS.CLIENT.INVALID,
        chaosJoined = lobby.gameData.chaos.client ~= CONSTANTS.CLIENT.INVALID,
        orderReady = lobby.orderReady or false,
        chaosReady = lobby.chaosReady or false
    }
    
    ServerSendToClient(lobby.gameData.order.client, "ClientRPC_RoomState", state)
    ServerSendToClient(lobby.gameData.chaos.client, "ClientRPC_RoomState", state)
end

--[[
    Sends one client the current state of the room it is in.

    The lobby room only ever learned the room's state from a broadcast, and
    broadcasts only happen when something changes. A host who created a room
    and walked into the lobby room therefore saw nothing at all until the
    other player joined or readied - the scene had missed every update that
    happened while it was still loading. Clients ask for a refresh on entry
    instead of relying on having been present for the last change.
]]
function ServerRPC_RequestRoomState(clientID, roomID)
    local _, lobby = FindLobbyByRoomID(roomID)
    if not lobby then return end
    if not GetClientFactionInLobby(lobby, clientID) then return end

    ServerRPC_BroadcastRoomState(roomID)
end

local class = DefineType("NetworkedGameServer",
{
    Serializable = 
    {
        serverPort = 9081,

        -- Start the server the moment this component starts.
        --
        -- true  the dedicated-server arrangement: M5NetworkServer.scene comes
        --       up and immediately listens. This is the existing behaviour and
        --       the default, so that scene is unaffected.
        -- false the listen-server arrangement: this component sits dormant in
        --       a normal game scene until the player chooses to host, at which
        --       point SteamCB_OnLobbyCreated calls StartListenServer(). Every
        --       other player in that same scene never starts a server at all.
        autoStart = true
    },

    started = false,

    connectedClients = {},

    lobbyList = {},

    Init = function(self, entity)
        NetworkedGameServer_instance = self

        -- Get grid component for C++ pathfinding (user attaches Grid component to server entity)
        local gridComp = ECS.getGridComponent(entity)
        if gridComp then
            self.pathfindGridID = gridComp.gridID
            Log.info("[Server] Grid component found for pathfinding, gridID: " .. self.pathfindGridID)
        else
            Log.warning("[Server] No Grid component on server entity — pathfinding will use fallback")
            self.pathfindGridID = nil
        end

        Event.subscribe("OnClientConnected", function(params)
            -- "OnClientConnected" is emitted by two different places with two
            -- different payload shapes:
            --
            --   ConnectionCallbacks.lua   { client   = id }  a remote client
            --                                                connected to US
            --   NetworkedGameClient.lua   { clientID = id }  WE connected to a
            --                                                server
            --
            -- Only the first concerns a server. That never mattered while the
            -- server and client were separate processes, because a server only
            -- ever saw the first. On a listen server both arrive here, and the
            -- second one has no `client` field - which is where the
            -- "attempt to concatenate field 'client' (a nil value)" came from.
            if params == nil or params.client == nil then return end

            -- Registration itself happens in ServerRPC_OnClientConnected, which
            -- C++ dispatches whatever scene we are in. This subscriber only
            -- exists for scene-side reactions, and must not be the thing that
            -- decides whether a client is known.
            Log.info("[NetworkedGameServer] Client " .. params.client .. " successfully connected")
        end)

        Event.subscribe("OnClientDisconnected", function(params)
            -- Same two-payload collision as OnClientConnected above: the
            -- client-side emission carries no `client` field, and on a listen
            -- server both reach this handler.
            if params == nil or params.client == nil then return end

            -- The cleanup itself lives in ServerHandleClientDisconnected and
            -- has already run by the time this fires. It used to live here,
            -- which meant it only ran when the host happened to be standing in
            -- one of the two scenes that carry this component - so a player
            -- who left while the host was in the lobby room or the arsenal was
            -- never removed from the room at all. This subscriber is now only
            -- for scene-side reactions, and nothing that matters may depend on
            -- it existing.
            Log.info("[NetworkedGameServer] Client " .. params.client .. " disconnected")
        end)
    end,

    Start = function(self, entity)
        if not self.autoStart then
            -- Listen-server arrangement: dormant until the player hosts.
            -- Deliberately silent - this is the common case in a game scene,
            -- where every non-hosting player also has this component.
            return
        end

        if RPC.startServer(self.serverPort) then
            Log.info("[NetworkGameServer] Server started successfully")
            self.started = true
        else
            Log.error("[NetworkGameServer] Failed to start server")
        end
    end,

    --[[
        Starts a Steam listen server: this process serves the match AND plays
        in it.

        Two transports come up, and the second one is the whole trick. Steam
        P2P cannot connect to its own SteamID, so this process cannot reach its
        own server over Steam. RPC.startSteamHost() therefore also opens a
        Winsock listener on 127.0.0.1, which is what our own client half
        connects to a moment later. Remote players arrive over the Steam relay.
        Nothing is exposed and nothing is port-forwarded - the loopback socket
        never leaves this machine.

        Returns true when the server is listening. The caller connects the
        local client afterwards.
    ]]
    StartListenServer = function(self)
        if self.started then
            Log.warning("[NetworkGameServer] Already started")
            return true
        end

        if not RPC.startSteamHost() then
            Log.error("[NetworkGameServer] Failed to start the Steam listen server")
            return false
        end

        Log.info("[NetworkGameServer] Listen server up over Steam P2P")
        self.started = true
        return true
    end,

    IsExistingClient = function(self, clientID)
        return ServerClients()[clientID] ~= nil
    end,

    FindLobbyByName = function(self, lobbyName)
        return ServerLobbies()[lobbyName]
    end,

    FindLobbyByClient = function(self, clientID)
        for name, lobby in pairs(ServerLobbies()) do
            for _, id in ipairs(lobby.clients) do
                if id == clientID then
                    return name, lobby
                end
            end
        end
        return nil, nil
    end,

    ValidateClientGameplay = function(self, clientID)
        if not IsExistingClientID(clientID) then
            Log.error("[NetworkGameServer] No valid client " .. clientID .. " found..")
            return false
        end

        local name, lobby = LookupLobbyByClient(clientID)

        if not lobby then
            Log.error("[NetworkGameServer] No valid lobby for client " .. clientID .. " found..")
            return false
        end

        if (lobby.gameData.order.client == clientID) or 
            (lobby.gameData.chaos.client == clientID) then 
            return true 
        end

        return false
    end,

    -- Use C++ Grid.validateMovement for pathfinding (trap-over checking)
    FindPath = function(self, lobby, isOrder, fromX, fromY, toX, toY)
        -- Try C++ pathfinding first
        if self.pathfindGridID then
            local result = Grid.validateMovement(self.pathfindGridID, fromX, fromY, toX, toY, 2)
            if result and result.canMove and result.path and #result.path > 0 then
                Log.info("[Server] FindPath C++: found path with " .. #result.path .. " steps")
                return result.path
            end
        end

        -- Fallback: simple Manhattan path (range 2 max, build step-by-step path)
        local COLS = CONSTANTS.GRID_COLS
        local ROWS = CONSTANTS.GRID_ROWS
        local dx = toX - fromX
        local dy = toY - fromY
        if math.abs(dx) + math.abs(dy) > 2 then return nil end

        local path = {}
        local cx, cy = fromX, fromY
        -- Move horizontally first, then vertically
        while cx ~= toX do
            cx = cx + (dx > 0 and 1 or -1)
            table.insert(path, { x = cx, y = cy })
        end
        while cy ~= toY do
            cy = cy + (dy > 0 and 1 or -1)
            table.insert(path, { x = cx, y = cy })
        end
        if #path > 0 then
            Log.info("[Server] FindPath fallback: built path with " .. #path .. " steps")
        end
        return #path > 0 and path or nil
    end,

    BroadcastResolution = function(self, lobby)
        local turnNum = lobby.gameData.turnNumber or 1
        Log.info("[Server] ====== BROADCAST RESOLUTION (Turn " .. turnNum .. ") ======")
        local ok, err = pcall(function()
            -- Arm existing traps from previous turns
            self:ArmExistingTraps(lobby)

            -- Snapshot unit positions before movements
            local syncPositions = {}
            for i = 1, 6 do
                syncPositions[i] = {
                    x = lobby.gameData.units[i].gridPos.x,
                    y = lobby.gameData.units[i].gridPos.y
                }
                Log.info("[Server] SyncPos unit " .. i .. ": (" .. syncPositions[i].x .. "," .. syncPositions[i].y .. ")")
            end

            -- Consolidate THIS TURN's actions: Order move/support → Chaos move/support → Order attack → Chaos attack
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

            addEntries(lobby.gameData.order.moveSupportQueue or {}, CONSTANTS.ORDER_PLAYER)
            addEntries(lobby.gameData.chaos.moveSupportQueue or {}, CONSTANTS.CHAOS_PLAYER)
            addEntries(lobby.gameData.order.attackQueue or {}, CONSTANTS.ORDER_PLAYER)
            addEntries(lobby.gameData.chaos.attackQueue or {}, CONSTANTS.CHAOS_PLAYER)

            -- Store this turn's actions in the full duel history
            lobby.gameData.turnHistory[turnNum] = turnActions

            Log.info("[Server] Turn " .. turnNum .. " actions: " .. #turnActions ..
                " (total turns in history: " .. #lobby.gameData.turnHistory .. ")")
            for i, entry in ipairs(turnActions) do
                Log.info("[Server] Action[" .. i .. "]: faction=" .. entry.faction ..
                    " actionType=" .. entry.actionType .. " abilityID=" .. entry.abilityID ..
                    " unit=" .. tostring(entry.unitType) ..
                    " target=(" .. entry.targetX .. "," .. entry.targetY .. ")")
            end

            -- Pre-calculate results for THIS TURN's actions only
            local results = self:PreCalculateResults(lobby, turnActions)

            -- Build active buff snapshot for client-side status UI (before ApplyResolutionToState clears _pendingBuffs)
            local activeBuffs = {}
            local buffStacks = {
                fervor = lobby.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] or 0,
                berserk = lobby.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] or 0
            }
            if lobby.gameData._pendingBuffs then
                local pb = lobby.gameData._pendingBuffs
                local snapshotTurn = lobby.gameData.turnNumber or turnNum or 1
                buffStacks.fervor = pb.fervorStacks or 0
                buffStacks.berserk = pb.berserkStacks or 0
                for ut, turnApplied in pairs(pb.burnedUnits) do
                    -- Skip burns newly applied this turn; they will be shown when Dragoon_1 visually hits
                    if turnApplied ~= nil and turnApplied < snapshotTurn then
                        activeBuffs[#activeBuffs + 1] = {
                            unit = ut,
                            statusID = CONSTANTS.ABILITIES.DRAGOON_1,
                            stackCount = 1,
                            -- Burn starts blinking on the turn after application, expires at end of that turn
                            isExpiring = (snapshotTurn > turnApplied) or false
                        }
                    end
                end
                for ut, turnApplied in pairs(pb.bleedUnits) do
                    activeBuffs[#activeBuffs + 1] = {
                        unit = ut,
                        statusID = CONSTANTS.ABILITIES.VIZOM_2,
                        stackCount = 1,
                        -- Bleed starts blinking on the turn after application, expires at end of that turn
                        isExpiring = (turnApplied ~= nil and snapshotTurn > turnApplied) or false
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

            -- Apply state changes to server
            self:ApplyResolutionToState(lobby, results)

            -- Strip zero-value fields to reduce payload size
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

            -- Send only this turn's data to both clients
            local resolutionData = {
                turn = turnNum,
                syncPositions = syncPositions,
                queue = results,
                finalHealth = {
                    order = lobby.gameData.playerHealth.order,
                    chaos = lobby.gameData.playerHealth.chaos
                },
                winner = self:CheckWinner(lobby),
                activeBuffs = activeBuffs,
                buffStacks = buffStacks
            }

            Log.info("[Server] Sending turn " .. turnNum .. " resolution data (" .. #results .. " results) to " .. #lobby.clients .. " clients")

            for _, id in ipairs(lobby.clients) do
                ServerSendToClient(id, "ClientRPC_OnResolutionData", resolutionData)
            end
        end)

        if not ok then
            Log.error("[Server] BroadcastResolution failed: " .. tostring(err))
        end

        -- Reset queues for next turn and advance turn counter
        lobby.gameData.order.ready = false
        lobby.gameData.order.moveSupportQueue = {}
        lobby.gameData.order.attackQueue = {}
        lobby.gameData.chaos.ready = false
        lobby.gameData.chaos.moveSupportQueue = {}
        lobby.gameData.chaos.attackQueue = {}

        local winner = self:CheckWinner(lobby)
        if winner > 0 then
            lobby.gameData.phase = "gameover"
            Log.info("[Server] Game over! Winner: " .. winner)
        else
            lobby.gameData.turnNumber = turnNum + 1
            lobby.gameData.phase = "preparation"

            -- Generate Hunter's Marks for next turn
            local nextPositions = {}
            for i = 1, 6 do
                nextPositions[i] = { x = lobby.gameData.units[i].gridPos.x, y = lobby.gameData.units[i].gridPos.y, isOrder = (i <= 3) }
            end
            local oP = BuildPassiveLookup(lobby.gameData.order.arsenalMap)
            local cP = BuildPassiveLookup(lobby.gameData.chaos.arsenalMap)
            local nextFP = { [CONSTANTS.ORDER_PLAYER] = oP, [CONSTANTS.CHAOS_PLAYER] = cP }
            local nextMarks, nextMarkCounts, nextMarkedByFaction = GenerateHunterMarks(nextFP, nextPositions)
            lobby.gameData.hunterMarks = nextMarks
            lobby.gameData.hunterMarkCounts = nextMarkCounts
            lobby.gameData.hunterMarkedByFaction = nextMarkedByFaction
            for _, id in ipairs(lobby.clients) do
                ServerSendToClient(id, "ClientRPC_OnHunterMarks", lobby.gameData.hunterMarks or {})
            end

            Log.info("[Server] Broadcast resolution complete, advancing to turn " .. (turnNum + 1))
        end
    end,

    CheckWinner = function(self, lobby)
        local orderHP = lobby.gameData.playerHealth.order
        local chaosHP = lobby.gameData.playerHealth.chaos

        if orderHP <= 0 and chaosHP > 0 then
            return CONSTANTS.CHAOS_PLAYER
        elseif chaosHP <= 0 and orderHP > 0 then
            return CONSTANTS.ORDER_PLAYER
        elseif orderHP <= 0 and chaosHP <= 0 then
            return 3 -- tie
        end
        return 0
    end,

    ArmExistingTraps = function(self, lobby)
        local totalTiles = CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS
        for i = 1, totalTiles do
            if lobby.gameData.traps.order[i] and not lobby.gameData.traps.order[i].armed then
                lobby.gameData.traps.order[i].armed = true
                Log.info("[Server] Armed order trap on tile " .. i)
            end
            if lobby.gameData.traps.chaos[i] and not lobby.gameData.traps.chaos[i].armed then
                lobby.gameData.traps.chaos[i].armed = true
                Log.info("[Server] Armed chaos trap on tile " .. i)
            end
        end
    end,

    PreCalculateResults = function(self, lobby, consolidatedQueue)
        Log.info("[Server] PreCalculateResults: processing " .. #consolidatedQueue .. " actions")
        local totalTiles = CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS

        -- Snapshot tile health
        local orderHealth = {}
        local chaosHealth = {}
        for i = 1, totalTiles do
            orderHealth[i] = lobby.gameData.tileHealth.order[i]
            chaosHealth[i] = lobby.gameData.tileHealth.chaos[i]
        end

        -- Snapshot armed traps (with trigger counts for Tirion_2 re-trigger)
        local orderTraps = {}
        local chaosTraps = {}
        for i = 1, totalTiles do
            if lobby.gameData.traps.order[i] and lobby.gameData.traps.order[i].armed then
                orderTraps[i] = {
                    owner = lobby.gameData.traps.order[i].owner,
                    damage = lobby.gameData.traps.order[i].damage,
                    sideDamage = lobby.gameData.traps.order[i].sideDamage,
                    isConsTrap = lobby.gameData.traps.order[i].isConsTrap or false,
                    isBleedTrap = lobby.gameData.traps.order[i].isBleedTrap or false,
                    triggerCount = 0
                }
            end
            if lobby.gameData.traps.chaos[i] and lobby.gameData.traps.chaos[i].armed then
                chaosTraps[i] = {
                    owner = lobby.gameData.traps.chaos[i].owner,
                    damage = lobby.gameData.traps.chaos[i].damage,
                    sideDamage = lobby.gameData.traps.chaos[i].sideDamage,
                    isConsTrap = lobby.gameData.traps.chaos[i].isConsTrap or false,
                    isBleedTrap = lobby.gameData.traps.chaos[i].isBleedTrap or false,
                    triggerCount = 0
                }
            end
        end

        local orderTrapCount, chaosTrapCount = 0, 0
        for _ in pairs(orderTraps) do orderTrapCount = orderTrapCount + 1 end
        for _ in pairs(chaosTraps) do chaosTrapCount = chaosTrapCount + 1 end
        Log.info("[Server] Armed traps snapshot: order=" .. orderTrapCount .. " chaos=" .. chaosTrapCount)

        -- Track unit positions
        local unitPositions = {}
        for i = 1, 6 do
            unitPositions[i] = {
                x = lobby.gameData.units[i].gridPos.x,
                y = lobby.gameData.units[i].gridPos.y,
                isOrder = (i <= 3)
            }
        end

        -- Track active shields (unit type → remaining stacks, each damage point removes 1 stack)
        local shieldedUnits = {}

        -- Track units damaged this resolution (for Vizom_1 recast condition)
        local damagedUnitsThisTurn = {}

        -- Track total tiles moved by Order units this turn (for Dragoon_2 feathers)
        local orderTilesMoved = 0

        -- Track Wrath buff (per faction, consumed on next ability, +2 damage)
        local wrathActive = { [CONSTANTS.ORDER_PLAYER] = false, [CONSTANTS.CHAOS_PLAYER] = false }

        -- Track Fervor stacks (persistent, loaded from game data)
        local fervorStacks = lobby.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] or 0
        local fervorGrantedThisTurn = false

        -- Track Berserk stacks (persistent, loaded from game data)
        local berserkStacks = lobby.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] or 0

        -- Track Burn debuff (expires at end of turn it was applied)
        local currentTurn = lobby.gameData.turnNumber or 1
        local burnedUnits = {}
        for ut, turnApplied in pairs(lobby.gameData.buffs.burn) do
            -- Burn lasts through the turn after application
            if turnApplied and currentTurn <= turnApplied + 1 then
                burnedUnits[ut] = turnApplied
            else
                Log.info("[Server] Burn expired on unit " .. ut .. " (applied turn " .. tostring(turnApplied) .. ", current turn " .. currentTurn .. ")")
            end
        end

        -- Track Bleed debuff
        local bleedUnits = {}
        for ut, turnApplied in pairs(lobby.gameData.buffs.bleed) do
            -- Bleed lasts through the turn after application
            if turnApplied and currentTurn <= turnApplied + 1 then
                bleedUnits[ut] = turnApplied
            else
                Log.info("[Server] Bleed expired on unit " .. ut .. " (applied turn " .. tostring(turnApplied) .. ", current turn " .. currentTurn .. ")")
            end
        end

        -- Build passive lookups from arsenal maps
        local orderPassives, orderAbilityToUnit = BuildPassiveLookup(lobby.gameData.order.arsenalMap)
        local chaosPassives, chaosAbilityToUnit = BuildPassiveLookup(lobby.gameData.chaos.arsenalMap)
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
        local hunterMarks = lobby.gameData.hunterMarks or {}
        local hunterMarkCounts = lobby.gameData.hunterMarkCounts or {
            [CONSTANTS.ORDER_PLAYER] = 0,
            [CONSTANTS.CHAOS_PLAYER] = 0
        }
        local hunterMarkedByFaction = lobby.gameData.hunterMarkedByFaction or {}
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
                -- Apply Burn: +1 damage taken from all sources while active
                if burnedUnits[hitUnit] ~= nil then
                    actualDamage = actualDamage + 1
                    hasBurnBonus = true
                end
                -- Apply shield: each point of damage removes 1 shield stack (damage still goes through)
                local shieldStacksRemoved = 0
                if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                    shieldStacksRemoved = math.min(shieldedUnits[hitUnit], actualDamage)
                    shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - shieldStacksRemoved
                    Log.info("[Server] Unit " .. hitUnit .. " SHIELDED - " .. shieldStacksRemoved .. " stacks removed, " .. (shieldedUnits[hitUnit] or 0) .. " remaining")
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
                return true -- hit a unit
            else
                -- No unit: apply tile damage
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

                -- Track tiles moved for Dragoon_2 feather count
                local isOrder = (entry.faction == CONSTANTS.ORDER_PLAYER)
                local tilesMoved = math.abs(entry.targetX - fromX) + math.abs(entry.targetY - fromY)
                if isOrder then
                    orderTilesMoved = orderTilesMoved + tilesMoved
                end

                -- Apply Bleed damage (1 damage per tile moved)
                if bleedUnits[entry.unitType] ~= nil and tilesMoved > 0 then
                    local bleedDmg = tilesMoved
                    local bleedShielded = false
                    local bleedShieldStacksRemoved = 0
                    if burnedUnits[entry.unitType] ~= nil then
                        bleedDmg = bleedDmg + 1
                    end
                    if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                        bleedShieldStacksRemoved = math.min(shieldedUnits[entry.unitType], bleedDmg)
                        shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - bleedShieldStacksRemoved
                        bleedShielded = true
                    end
                    result.damage = result.damage + bleedDmg
                    result.damagePlayer = isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
                    result.bleedHit = {
                        unitType = entry.unitType,
                        isOrder = isOrder,
                        damage = bleedDmg,
                        shielded = bleedShielded,
                        shieldStacksRemoved = bleedShieldStacksRemoved,
                        shieldStacksRemaining = shieldedUnits[entry.unitType] or 0
                    }
                    -- Each bleed trigger grants Vizom 1 Berserk stack
                    berserkStacks = berserkStacks + 1
                    result.berserkGranted = true
                    Log.info("[Server] Bleed damage on unit " .. entry.unitType .. ": " .. bleedDmg .. " (" .. tilesMoved .. " tiles), Vizom Berserk=" .. berserkStacks)
                end

                -- Check path for armed traps
                local trapMap = isOrder and orderTraps or chaosTraps

                local path = self:FindPath(lobby, isOrder, fromX, fromY, entry.targetX, entry.targetY)
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
                                local trapShieldStacksRemoved = 0
                                if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                                    trapShieldStacksRemoved = math.min(shieldedUnits[entry.unitType], trapDmg)
                                    shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - trapShieldStacksRemoved
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
                                    shieldStacksRemoved = trapShieldStacksRemoved,
                                    shieldStacksRemaining = shieldedUnits[entry.unitType] or 0
                                }
                                bleedUnits[entry.unitType] = currentTurn
                                berserkStacks = berserkStacks + 1
                                result.berserkGranted = true
                                Log.info("[Server] BLEED TRAP triggered at (" .. step.x .. "," .. step.y .. ") - unit " .. entry.unitType .. " takes " .. trapDmg .. " + Bleed, Vizom Berserk=" .. berserkStacks)
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
                                -- (unitPositions was already updated to final target above)
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
                                            trapMap[detIdx] = nil -- destroy immediately
                                            table.insert(detonatedTrapIndices, detIdx)
                                            local detX = (detIdx - 1) % CONSTANTS.GRID_COLS
                                            local detY = math.floor((detIdx - 1) / CONSTANTS.GRID_COLS)

                                            -- 3x3 AoE: damage all units in radius
                                            for dy = -1, 1 do
                                                for dx = -1, 1 do
                                                    local ax = detX + dx
                                                    local ay = detY + dy
                                                    if ax >= 0 and ax < CONSTANTS.GRID_COLS and ay >= 0 and ay < CONSTANTS.GRID_ROWS then
                                                        local aIdx = ay * CONSTANTS.GRID_COLS + ax + 1
                                                        -- Check for units on this tile
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
                                                                    local consShieldStacksRemoved = 0
                                                                    if shieldedUnits[ut] and shieldedUnits[ut] > 0 then
                                                                        consShieldStacksRemoved = math.min(shieldedUnits[ut], actualDetDmg)
                                                                        shieldedUnits[ut] = shieldedUnits[ut] - consShieldStacksRemoved
                                                                        consShielded = true
                                                                    end
                                                                    totalConsDmg = totalConsDmg + actualDetDmg
                                                                    table.insert(consHits, { unitType = ut, gridX = ax, gridY = ay, damage = actualDetDmg, detonationIdx = detonationCount, shielded = consShielded, shieldStacksRemoved = consShieldStacksRemoved, shieldStacksRemaining = shieldedUnits[ut] or 0 })
                                                                    damagedUnitsThisTurn[ut] = true
                                                                end
                                                            end
                                                        end
                                                        -- Chain-detonate other Consecration traps in radius
                                                        if trapMap[aIdx] and trapMap[aIdx].isConsTrap and not detonated[aIdx] then
                                                            table.insert(detonationQueue, aIdx)
                                                        end
                                                    end
                                                end
                                            end
                                            Log.info("[Server] CONSECRATION detonated at tile " .. detIdx .. " centerDamage=" .. detCenterDmg .. " sideDamage=" .. detSideDmg)
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
                                Log.info("[Server] CONSECRATION chain total damage=" .. totalConsDmg .. " hits=" .. #consHits)
                                break
                            else
                                -- Legacy trap behavior
                                local trapDmg = trap.damage
                                local trapShielded = false
                                local trapShieldStacksRemoved = 0
                                if burnedUnits[entry.unitType] ~= nil then
                                    trapDmg = trapDmg + 1
                                end
                                if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                                    trapShieldStacksRemoved = math.min(shieldedUnits[entry.unitType], trapDmg)
                                    shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - trapShieldStacksRemoved
                                    trapShielded = true
                                end
                                result.trapHit = {
                                    isOrderGrid = isOrder,
                                    unitType = entry.unitType,
                                    gridX = step.x,
                                    gridY = step.y,
                                    damage = trapDmg,
                                    shielded = trapShielded,
                                    shieldStacksRemoved = trapShieldStacksRemoved,
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
                        local trapShieldStacksRemoved = 0
                        if burnedUnits[entry.unitType] ~= nil then
                            trapDmg = trapDmg + 1
                        end
                        if shieldedUnits[entry.unitType] and shieldedUnits[entry.unitType] > 0 then
                            trapShieldStacksRemoved = math.min(shieldedUnits[entry.unitType], trapDmg)
                            shieldedUnits[entry.unitType] = shieldedUnits[entry.unitType] - trapShieldStacksRemoved
                            trapShielded = true
                        end
                        result.trapHit = {
                            isOrderGrid = isOrder,
                            unitType = entry.unitType,
                            gridX = entry.targetX,
                            gridY = entry.targetY,
                            damage = trapDmg,
                            shielded = trapShielded,
                            shieldStacksRemoved = trapShieldStacksRemoved,
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

                    -- Calculate Wrath bonus damage
                    local wrathBonus = 0
                    local wrathConsumed = false
                    if wrathActive[entry.faction] and not abilityData.isSupport then
                        wrathBonus = 2
                        wrathConsumed = true
                        wrathActive[entry.faction] = false
                        Log.info("[Server] Wrath consumed by " .. (isOrderAbility and "Order" or "Chaos") .. " +2 damage")
                    end

                    -- Calculate actual unit damage for this ability
                    local effectiveUnitDamage = abilityData.unitDamage + wrathBonus

                    -- Tirion_1 Fervor: add stacks to base damage
                    if abilityData.grantsFervor then
                        effectiveUnitDamage = abilityData.baseUnitDamage + fervorStacks + wrathBonus
                        Log.info("[Server] Tirion_1 damage: " .. abilityData.baseUnitDamage .. " + " .. fervorStacks .. " fervor + " .. wrathBonus .. " wrath = " .. effectiveUnitDamage)
                    end

                    -- Mark result as having consumed wrath for client-side indicator
                    if wrathConsumed then
                        result.wrathConsumed = true
                    end

                    -- Oracle_1: server generates crystals (uses crystalCount from ability data)
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

                    -- Dragoon_2 Feather Cyclone: center + feathers
                    elseif abilityData.isFeatherCyclone then
                        -- Process center tile
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

                        -- Calculate feather count: tiles moved by Order units + 2
                        local featherCount = 2 + (orderTilesMoved * 2)
                        Log.info("[Server] Feather Cyclone: orderTilesMoved=" .. orderTilesMoved .. " feathers=" .. featherCount)

                        -- Generate feather targets (random unbroken tiles, prefer unused directions)
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

                    -- Shield placement (Oracle_2): flat HP shield, absorbs damage until depleted
                    elseif abilityData.isShield then
                        local shieldUnit = entry.param1 or 0
                        if shieldUnit > 0 and shieldUnit <= 6 then
                            local shieldAmount = abilityData.shieldAmount or 5
                            shieldedUnits[shieldUnit] = shieldAmount
                            result.shieldTarget = shieldUnit
                            result.shieldAmount = shieldAmount
                            Log.info("[Server] Shield (" .. shieldAmount .. "HP) applied to unit " .. shieldUnit)
                        end

                    -- Eye_2 Fractured Earth: damage tile, grant Wrath
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
                        Log.info("[Server] Wrath granted to " .. (isOrderAbility and "Order" or "Chaos"))

                    -- Trap placement (Tirion_2, Vizom_2)
                    elseif abilityData.isTrap then
                        local trapIdx = entry.targetY * CONSTANTS.GRID_COLS + entry.targetX + 1
                        local trapHealthMap = targetIsOrder and orderHealth or chaosHealth
                        local existingTrapMap = targetIsOrder and orderTraps or chaosTraps

                        if not trapHealthMap[trapIdx] or trapHealthMap[trapIdx] <= 0 then
                            Log.info("[Server] Trap placement skipped: tile destroyed")
                        elseif existingTrapMap[trapIdx] then
                            Log.info("[Server] Trap placement skipped: tile already has a trap")
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

                        -- Tirion_1 Fervor: grant stack on unit hit (max 1 per turn)
                        if abilityData.grantsFervor and hitAUnit and not fervorGrantedThisTurn then
                            fervorStacks = fervorStacks + 1
                            fervorGrantedThisTurn = true
                            result.fervorGranted = true
                            Log.info("[Server] Fervor granted! Stacks now: " .. fervorStacks)
                        end

                        -- Dragoon_1 Burn: inflict on hit units
                        if abilityData.inflictsBurn and hitAUnit then
                            for _, hit in ipairs(result.hits) do
                                if hit.unitType and not hit.shielded then
                                    burnedUnits[hit.unitType] = currentTurn
                                    Log.info("[Server] Burn applied to unit " .. hit.unitType .. " on turn " .. currentTurn)
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
                                        Log.info("[Server] Imp_1 recast at (" .. recastTile.x .. "," .. recastTile.y .. ")")
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

                        -- Eye_1 recast: if tile destroyed, recast on left+right
                        if abilityData.recastOnTileDestroy then
                            local centerIdx = entry.targetY * CONSTANTS.GRID_COLS + entry.targetX + 1
                            if targetHealthMap[centerIdx] and targetHealthMap[centerIdx] <= 0 then
                                local recastTiles = {}
                                -- Left tile
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
                                -- Right tile
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
                                    Log.info("[Server] Eye_1 tile destroyed, recasting on " .. #recastTiles .. " tiles")
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
                                    Log.info("[Server] Vizom_1 no valid adjacent tiles, recasting on same tile (" .. lastX .. "," .. lastY .. ")")
                                else
                                    pick = validAdj[ServerRandom(1, #validAdj)]
                                end
                                lastX = pick.x
                                lastY = pick.y
                                table.insert(result.berserkBounces, { x = pick.x, y = pick.y })
                                ProcessTileHit(pick.idx, targetHealthMap, targetIsOrder, effectiveUnitDamage, abilityData.tileDamage, result, true)
                                Log.info("[Server] Vizom_1 Berserk bounce to (" .. pick.x .. "," .. pick.y .. "), stacks remaining=" .. berserkStacks)
                            end
                            if #result.berserkBounces == 0 then result.berserkBounces = nil end
                        end

                        -- Imp_2: force-move + bonus damage on trap/destroyed tile + trigger traps
                        if abilityData.isPull and #result.hits > 0 then
                            local hitUnit = result.hits[1].unitType
                            if hitUnit and entry.param1 and entry.param2 and unitPositions[hitUnit] then
                                local destIdx = entry.param2 * CONSTANTS.GRID_COLS + entry.param1 + 1
                                -- Check for bonus damage conditions
                                local destTrapMap = targetIsOrder and orderTraps or chaosTraps
                                local destHealthMap = targetIsOrder and orderHealth or chaosHealth
                                local hasBonusDamage = false
                                local destIsBroken = destHealthMap[destIdx] and destHealthMap[destIdx] <= 0
                                if destTrapMap[destIdx] then
                                    hasBonusDamage = true
                                    Log.info("[Server] Imp_2 bonus: pulled onto active trap")

                                    -- Trigger the active trap at destination
                                    local trap = destTrapMap[destIdx]
                                    if trap.isBleedTrap then
                                        local trapDmg = trap.damage
                                        if burnedUnits[hitUnit] ~= nil then
                                            trapDmg = trapDmg + 1
                                        end
                                        local trapShielded = false
                                        local trapShieldStacksRemoved = 0
                                        if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                            trapShieldStacksRemoved = math.min(shieldedUnits[hitUnit], trapDmg)
                                            shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - trapShieldStacksRemoved
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
                                            shieldStacksRemoved = trapShieldStacksRemoved,
                                            shieldStacksRemaining = shieldedUnits[hitUnit] or 0
                                        }
                                        bleedUnits[hitUnit] = currentTurn
                                        berserkStacks = berserkStacks + 1
                                        result.berserkGranted = true
                                        Log.info("[Server] Imp_2 triggered Bleed trap at dest, inflicting Bleed + Berserk=" .. berserkStacks)
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
                                                                            if shieldedUnits[ut] and shieldedUnits[ut] > 0 then
                                                                                local consAbsorbed = math.min(shieldedUnits[ut], actualDetDmg)
                                                                                shieldedUnits[ut] = shieldedUnits[ut] - consAbsorbed
                                                                                actualDetDmg = actualDetDmg - consAbsorbed
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
                                        Log.info("[Server] Imp_2 triggered Consecration chain, total damage=" .. totalConsDmg)
                                    else
                                        local destTrapDmg = trap.damage
                                        local destTrapShieldStacksRemoved = 0
                                        if burnedUnits[hitUnit] ~= nil then
                                            destTrapDmg = destTrapDmg + 1
                                        end
                                        if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                            destTrapShieldStacksRemoved = math.min(shieldedUnits[hitUnit], destTrapDmg)
                                            shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - destTrapShieldStacksRemoved
                                        end
                                        result.trapHit = {
                                            isOrderGrid = targetIsOrder,
                                            unitType = hitUnit,
                                            gridX = entry.param1,
                                            gridY = entry.param2,
                                            damage = destTrapDmg,
                                            shielded = destTrapShieldStacksRemoved > 0,
                                            shieldStacksRemoved = destTrapShieldStacksRemoved,
                                            shieldStacksRemaining = shieldedUnits[hitUnit] or 0
                                        }
                                        destTrapMap[destIdx] = nil
                                        Log.info("[Server] Imp_2 triggered trap at dest, damage=" .. trap.damage)
                                    end
                                end
                                if destIsBroken then
                                    hasBonusDamage = true
                                    Log.info("[Server] Imp_2 bonus: pulled onto destroyed tile")
                                end
                                if hasBonusDamage then
                                    local pullBonus = abilityData.pullBonusDamage
                                    local pullBonusShieldStacksRemoved = 0
                                    if burnedUnits[hitUnit] ~= nil then
                                        pullBonus = pullBonus + 1
                                    end
                                    if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                        pullBonusShieldStacksRemoved = math.min(shieldedUnits[hitUnit], pullBonus)
                                        shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - pullBonusShieldStacksRemoved
                                    end
                                    result.damage = result.damage + pullBonus
                                end
                                -- Apply Bleed damage if pulled unit is bleeding (flat 1 for forced move)
                                if bleedUnits[hitUnit] ~= nil then
                                    local bleedMoveDmg = 1
                                    local bleedMoveShieldStacksRemoved = 0
                                    if burnedUnits[hitUnit] ~= nil then
                                        bleedMoveDmg = bleedMoveDmg + 1
                                    end
                                    if shieldedUnits[hitUnit] and shieldedUnits[hitUnit] > 0 then
                                        bleedMoveShieldStacksRemoved = math.min(shieldedUnits[hitUnit], bleedMoveDmg)
                                        shieldedUnits[hitUnit] = shieldedUnits[hitUnit] - bleedMoveShieldStacksRemoved
                                    end
                                    result.damage = result.damage + bleedMoveDmg
                                    result.bleedHit = {
                                        unitType = hitUnit,
                                        isOrder = targetIsOrder,
                                        damage = bleedMoveDmg,
                                        shielded = bleedMoveShieldStacksRemoved > 0,
                                        shieldStacksRemoved = bleedMoveShieldStacksRemoved,
                                        shieldStacksRemaining = shieldedUnits[hitUnit] or 0
                                    }
                                    Log.info("[Server] Imp_2 triggered Bleed on unit " .. hitUnit .. " for " .. bleedMoveDmg)
                                end
                                -- Don't move unit onto a broken tile
                                if destIsBroken then
                                    result.pullBlocked = true
                                    Log.info("[Server] Imp_2 pull blocked: destination tile is destroyed")
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
                                        Log.info("[Server] Isolation Execution: +1 damage to isolated unit " .. hit.unitType)
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
                                        lobby.gameData.hunterMarks = hunterMarks
                                        lobby.gameData.hunterMarkedByFaction = hunterMarkedByFaction
                                        Log.info("[Server] Hunter's Mark consumed: +" .. markBonus .. " damage to unit " .. markedEnemy ..
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
            Log.info("[Server] Result[" .. i .. "]: damage=" .. result.damage ..
                " hits=" .. #result.hits ..
                " heals=" .. result.healAmount ..
                " tileEffects=" .. #result.tileEffects ..
                (result.trap and " +trap" or "") ..
                (result.trapHit and " +trapHit" or ""))
        end

        -- Store updated buff state for ApplyResolutionToState
        lobby.gameData._pendingBuffs = {
            fervorStacks = fervorStacks,
            berserkStacks = berserkStacks,
            burnedUnits = burnedUnits,
            bleedUnits = bleedUnits,
            shieldUnits = shieldedUnits
        }

        Log.info("[Server] Pre-calculated " .. #results .. " actions")
        return results
    end,

    ApplyResolutionToState = function(self, lobby, results)
        for _, result in ipairs(results) do
            local action = result.action

            -- Apply movements
            if action.actionType == CONSTANTS.ACTION_MOVEMENT and action.unitType >= 1 and action.unitType <= 6 then
                lobby.gameData.units[action.unitType].gridPos.x = action.targetX
                lobby.gameData.units[action.unitType].gridPos.y = action.targetY
            end

            -- Apply tile effects
            for _, effect in ipairs(result.tileEffects) do
                local healthMap = effect.isOrder and lobby.gameData.tileHealth.order or lobby.gameData.tileHealth.chaos
                healthMap[effect.tileIdx] = effect.newHealth
            end

            -- Place new traps (unarmed)
            if result.trap then
                local traps = result.trap.isOrder and lobby.gameData.traps.order or lobby.gameData.traps.chaos
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
                local traps = result.trapHit.isOrderGrid and lobby.gameData.traps.order or lobby.gameData.traps.chaos
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

            -- Apply player health changes
            if result.damage > 0 and result.damagePlayer > 0 then
                local key = result.damagePlayer == CONSTANTS.ORDER_PLAYER and "order" or "chaos"
                lobby.gameData.playerHealth[key] = lobby.gameData.playerHealth[key] - result.damage
            end

            -- Apply Imp_2 force-move: move hit unit to destination (skip if pull was blocked)
            if action.abilityID == CONSTANTS.ABILITIES.IMP_2 and result.hits and #result.hits > 0 and not result.pullBlocked then
                local hit = result.hits[1]
                if hit.unitType and action.param1 and action.param2 then
                    lobby.gameData.units[hit.unitType].gridPos.x = action.param1
                    lobby.gameData.units[hit.unitType].gridPos.y = action.param2
                end
            end
            if result.healAmount > 0 and result.healPlayer > 0 then
                local key = result.healPlayer == CONSTANTS.ORDER_PLAYER and "order" or "chaos"
                lobby.gameData.playerHealth[key] = lobby.gameData.playerHealth[key] + result.healAmount
            end
            if result.trapHit then
                local key = result.trapHit.isOrderGrid and "order" or "chaos"
                lobby.gameData.playerHealth[key] = lobby.gameData.playerHealth[key] - result.trapHit.damage
            end

            -- Bleed no longer triggers on ability use (only on tile movement)
        end

        -- Apply persistent buff updates from PreCalculateResults
        if lobby.gameData._pendingBuffs then
            local pb = lobby.gameData._pendingBuffs

            -- Update Fervor stacks (permanent)
            lobby.gameData.buffs.fervor[CONSTANTS.UNIT.TIRION] = pb.fervorStacks

            -- Update Berserk stacks (persistent, consumed by Vizom_1)
            lobby.gameData.buffs.berserk[CONSTANTS.UNIT.VIZOM] = pb.berserkStacks

            -- Update Burn (stores turnApplied for expiration tracking)
            lobby.gameData.buffs.burn = {}
            for ut, turnApplied in pairs(pb.burnedUnits) do
                if turnApplied then
                    lobby.gameData.buffs.burn[ut] = turnApplied
                end
            end

            -- Update Bleed (stores turnApplied for expiration tracking)
            lobby.gameData.buffs.bleed = {}
            for ut, turnApplied in pairs(pb.bleedUnits) do
                if turnApplied then
                    lobby.gameData.buffs.bleed[ut] = turnApplied
                end
            end

            lobby.gameData._pendingBuffs = nil
        end

        -- Consecration traps now persist until triggered (destroyed on detonation during movement)

        Log.info("[Server] Applied resolution. Order HP: " ..
            lobby.gameData.playerHealth.order .. ", Chaos HP: " .. lobby.gameData.playerHealth.chaos)
    end
})

return class
