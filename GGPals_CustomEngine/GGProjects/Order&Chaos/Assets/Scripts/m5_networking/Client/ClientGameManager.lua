ClientGameManager_instance = ClientGameManager_instance or nil

local class = DefineType("ClientGameManager", 
{
    Contains =
    {
        orderGrid = "ClientGrid",
        chaosGrid = "ClientGrid",
        abilities = "ClientAbilities",

        turnPhases = "ClientTurnPhases"
    },

    Serializable = 
    {
        playerFaction = 1
    },

    TextureAsset =
    {
        crosshairTexture = 0,
        borderTexture = 0,
        emptyTexture = 0
    },

    players = 
    {
        orderPlayer = 
        {
            currentHealth = 0,
            maxHealth = CONSTANTS.MAX_HP,
            abilityList =
            {
                [1] = "oracleAbility1",
                [2] = "oracleAbility2",
                [3] = "dragoonAbility1",
                [4] = "dragoonAbility2",
                [5] = "tirionAbility1",
                [6] = "tirionAbility2"
            }
        },

        chaosPlayer =
        {
            currentHealth = 0,
            maxHealth = CONSTANTS.MAX_HP,
            abilityList =
            {
                [1] = "impAbility1",
                [2] = "impAbility2",
                [3] = "eyeAbility1",
                [4] = "eyeAbility2",
                [5] = "vizomAbility1",
                [6] = "vizomAbility2"
            }
        }
    },

    previewedHealOrder = 0,
    previewedHealChaos = 0,
    prePreviewHealHealthOrder = -1,
    prePreviewHealHealthChaos = -1,
    previewedTrapDamageOrder = 0,
    previewedTrapDamageChaos = 0,
    prePreviewHealthOrder = -1,
    prePreviewHealthChaos = -1,
    -- Traps already telegraphed this planning phase, keyed "gridID:tileIdx".
    -- A trap is consumed by the first unit that reaches it, so it must only
    -- telegraph once per phase no matter how many units path across the tile.
    previewedTrapTiles = nil,
    previewedTileDamageOrder = nil,
    previewedTileDamageChaos = nil,
    lockedTileDamagePreviewOrder = nil,
    lockedTileDamagePreviewChaos = nil,

    selectedAbility = nil,
    selectedUnit = nil,
    turnNumber = 1,
    awaitingServer = false,
    currentAP = 0,
    localAttackQueue = nil,
    localMoveSupportQueue = nil,
    resolutionData = nil,
    buffStacks = nil,
    movedUnits = nil,
    usedAbilities = nil,
    pendingCastUnit = nil,
    unitStatusExpiring = nil,
    queuedShieldUnits = nil,  -- Track units that received shield this prep phase (PnP only)

    Init = function(self, entity)
        ClientGameManager_instance = self
        
        -- Seed RNG for random deployment and ability casting
        math.randomseed(os.time() + (os.clock() * 1000))
        -- Warm up the RNG (first few values can be poor quality)
        for i = 1, 10 do math.random() end
        
        -- Two different questions share this manager, and they had been
        -- answered by one variable in the wrong order.
        --
        -- playerFaction is the pass-and-play HOTSEAT marker: PnPDeploymentPhase
        -- and PnPPreparationPhase flip it back and forth as the controller
        -- changes hands, and it is the right answer there.
        --
        -- sessionFaction is the seat the SERVER gave us, delivered in
        -- ClientRPC_JoinRoomSuccess. In a networked match that is the only
        -- answer there is, and it must win.
        --
        -- Reading playerFaction first meant a networked player was seated by
        -- whatever the last hotseat pass - or the last match - happened to
        -- leave behind. Play one pass-and-play game (which ends on Chaos) and
        -- every online game afterwards put you on Chaos no matter which side
        -- you picked; if both players had done that, both sat on Chaos.
        local isNetworked = Persistent.connected or false

        if isNetworked and Persistent.sessionFaction then
            self.playerFaction = Persistent.sessionFaction
            Persistent.playerFaction = self.playerFaction
            Log.info("[ClientGameManager] Player faction " .. self.playerFaction .. " from session!")
        elseif Persistent.playerFaction then
            self.playerFaction = Persistent.playerFaction
            Log.info("[ClientGameManager] Player faction " .. self.playerFaction .. " chosen!")
        elseif Persistent.sessionFaction then
            self.playerFaction = Persistent.sessionFaction
            Persistent.playerFaction = self.playerFaction
            Log.info("[ClientGameManager] Player faction " .. self.playerFaction .. " from session!")
        else
            Log.warning("[ClientGameManager] Persistent not available, using default faction " .. self.playerFaction)
        end

        self.localAttackQueue = {}
        self.localMoveSupportQueue = {}
        self.movedUnits = {}
        self.usedAbilities = {}
        self.queuedShieldUnits = {}
        self.buffStacks = {
            fervor = 0,
            berserk = 0
        }
        self.unitStatusExpiring = {}
        for i = 1, 6 do self.unitStatusExpiring[i] = {} end
        self.previewedTileDamageOrder = {}
        self.previewedTileDamageChaos = {}
        self.lockedTileDamagePreviewOrder = {}
        self.lockedTileDamagePreviewChaos = {}

        -- Global status lookup: unitStatuses[unitType][statusID] = stackCount
        -- Updated by OnUnitStatusInflicted / OnUnitStatusRemoved events
        self.unitStatuses = {}
        for i = 1, 6 do self.unitStatuses[i] = {} end

        Event.subscribe("OnUnitStatusInflicted", function(params)
            local targetUnit = tonumber(params and params.targetUnit)
            local statusID = tonumber(params and params.statusID)
            if not targetUnit or targetUnit < 1 or targetUnit > 6 or not statusID then
                Log.warning("[ClientGameManager] Ignoring malformed OnUnitStatusInflicted payload")
                return
            end

            local stackCount = tonumber(params.stackCount) or 1
            if stackCount < 1 then stackCount = 1 end
            self.unitStatuses[targetUnit][statusID] = stackCount
            if params.isExpiring ~= nil then
                self.unitStatusExpiring[targetUnit][statusID] = params.isExpiring and true or false
            end
        end)
        Event.subscribe("OnUnitStatusRemoved", function(params)
            local targetUnit = tonumber(params and params.targetUnit)
            local statusID = tonumber(params and params.statusID)
            if not targetUnit or targetUnit < 1 or targetUnit > 6 or not statusID then
                Log.warning("[ClientGameManager] Ignoring malformed OnUnitStatusRemoved payload")
                return
            end

            self.unitStatuses[targetUnit][statusID] = nil
            self.unitStatusExpiring[targetUnit][statusID] = nil
        end)

        Event.subscribe("OnReturnMainMenu", function(params)
            -- Full teardown, not just the client. On a listen server this
            -- process may also own the server and the Steam lobby, and
            -- disconnecting only our own client left the match running and the
            -- lobby advertised with nobody attached to either.
            if NetworkSessionTeardown then
                NetworkSessionTeardown("returned to main menu")
            elseif NetworkedGameClient_instance then
                RPC.disconnect()
            end

            Event.emitLocal("RequestSceneTransition", {
                scene = "M5MainMenuScene"
            })
        end)

        -- Load arsenal ability map from Persistent
        -- Maps original editor ability IDs to the actual equipped ability IDs
        -- e.g. arsenalMap[3] = 14 means DRAGOON_1 slot now has CrimsonSlash
        self.arsenalMap = {}

        local orderMap = Persistent["arsenalMap_order"] or {}
        local chaosMap = Persistent["arsenalMap_chaos"] or {}

        -- Merge both faction maps (they don't overlap since ability IDs are per-unit)
        for originalID, equippedID in pairs(orderMap) do
            self.arsenalMap[originalID] = equippedID
            Log.info("[ClientGameManager] Arsenal map: " .. originalID .. " -> " .. equippedID)
        end
        for originalID, equippedID in pairs(chaosMap) do
            self.arsenalMap[originalID] = equippedID
            Log.info("[ClientGameManager] Arsenal map: " .. originalID .. " -> " .. equippedID)
        end

        -- Build passive lookups from arsenal maps
        -- unitPassives[unitType] = { [passiveID] = true }
        -- abilityToUnit[resolvedAbilityID] = unitType
        self.unitPassives = {}
        self.abilityToUnit = {}
        self.hunterMarks = {} -- [markerUnit] = markedEnemyUnit (set by server)

        local allMaps = { orderMap, chaosMap }
        for _, map in ipairs(allMaps) do
            for originalSlot, equippedID in pairs(map) do
                -- Virtual passive slots use offset +100
                local realSlot = originalSlot > 100 and (originalSlot - 100) or originalSlot
                local unitType = CONSTANTS.ABILITY_SLOT_TO_UNIT[realSlot]
                if unitType then
                    if equippedID >= 16 and equippedID <= 18 then
                        if not self.unitPassives[unitType] then
                            self.unitPassives[unitType] = {}
                        end
                        self.unitPassives[unitType][equippedID] = true
                        Log.info("[ClientGameManager] Passive: unit " .. unitType .. " has passive " .. equippedID)
                    else
                        self.abilityToUnit[equippedID] = unitType
                    end
                end
            end
        end
        -- Default mappings for non-remapped slots
        for slot = 1, 12 do
            if not self.arsenalMap[slot] then
                local unitType = CONSTANTS.ABILITY_SLOT_TO_UNIT[slot]
                if unitType then
                    self.abilityToUnit[slot] = unitType
                end
            end
        end

        self.orderGrid:Init(entity)
        self.chaosGrid:Init(entity)
        self.abilities:Init(entity)

        -- Set generic ability ownerFaction to match this client's faction
        if self.abilities.crimsonSlash then
            self.abilities.crimsonSlash.ability.ownerFaction = self.playerFaction
        end
        if self.abilities.sapphireSwirl then
            self.abilities.sapphireSwirl.ability.ownerFaction = self.playerFaction
        end
    end,

    Start = function(self, entity)
        if PassAndPlayServer_instance then
            Log.info("[ClientGameManager] PnP mode detected -- swapping to PnPTurnPhases")
            self.turnPhases = PassAndPlayServer_instance.turnPhases
        end
        
        -- Build ability lists from Arsenal data
        self:BuildAbilityListsFromArsenal()
        
        self.turnPhases:InitPhases()

        self.orderGrid:Start(entity)
        self.chaosGrid:Start(entity)
    end,

    -- Build the player ability lists based on Arsenal selections
    -- Uses self.arsenalMap which maps slot IDs to equipped ability IDs
    BuildAbilityListsFromArsenal = function(self)
        Log.info("[ClientGameManager] Building ability lists from Arsenal map...")

        -- Mapping from ability ID to ability name
        local abilityIDToName = {
            [1] = "dragoonAbility1",
            [2] = "dragoonAbility2",
            [3] = "oracleAbility1",
            [4] = "oracleAbility2",
            [5] = "impAbility1",
            [6] = "impAbility2",
            [7] = "eyeAbility1",
            [8] = "eyeAbility2",
            [9] = "tirionAbility1",
            [10] = "tirionAbility2",
            [11] = "vizomAbility1",
            [12] = "vizomAbility2",
            [14] = "crimsonSlash",
            [15] = "sapphireSwirl"
        }

        -- Build Order player ability list (Oracle, Dragoon, Tirion)
        local orderSlots = {3, 4, 1, 2, 9, 10}  -- Oracle slots 3-4, Dragoon 1-2, Tirion 9-10
        local orderAbilities = {}
        
        for _, slotID in ipairs(orderSlots) do
            -- Check if this slot has been remapped
            local equippedID = self.arsenalMap[slotID] or slotID
            local abilityName = abilityIDToName[equippedID]
            
            if abilityName then
                table.insert(orderAbilities, abilityName)
                Log.info("[ClientGameManager] Order slot " .. slotID .. " -> ability ID " .. equippedID .. " (" .. abilityName .. ")")
            else
                Log.warning("[ClientGameManager] Order slot " .. slotID .. " has unknown ability ID " .. equippedID)
            end
        end
        
        self.players.orderPlayer.abilityList = orderAbilities
        Log.info("[ClientGameManager] Final Order ability list (" .. #orderAbilities .. " abilities): " .. table.concat(orderAbilities, ", "))

        -- Build Chaos player ability list (Imp, Eye, Vizom)
        local chaosSlots = {5, 6, 7, 8, 11, 12}  -- Imp slots 5-6, Eye 7-8, Vizom 11-12
        local chaosAbilities = {}
        
        for _, slotID in ipairs(chaosSlots) do
            -- Check if this slot has been remapped
            local equippedID = self.arsenalMap[slotID] or slotID
            local abilityName = abilityIDToName[equippedID]
            
            if abilityName then
                table.insert(chaosAbilities, abilityName)
                Log.info("[ClientGameManager] Chaos slot " .. slotID .. " -> ability ID " .. equippedID .. " (" .. abilityName .. ")")
            else
                Log.warning("[ClientGameManager] Chaos slot " .. slotID .. " has unknown ability ID " .. equippedID)
            end
        end
        
        self.players.chaosPlayer.abilityList = chaosAbilities
        Log.info("[ClientGameManager] Final Chaos ability list (" .. #chaosAbilities .. " abilities): " .. table.concat(chaosAbilities, ", "))
        
        return true
    end,

    PassToServer = function(self)
        self.awaitingServer = true
    end,

    -- Intended to be only callable thru Server
    SetHealth = function(self, isOrder, healthAmount)
        if isOrder then self.players.orderPlayer.currentHealth = healthAmount
        else self.players.chaosPlayer.currentHealth = healthAmount end

        Event.emitLocal("OnHealthChanged", {
            player = isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER,
            curr = isOrder and self.players.orderPlayer.currentHealth or self.players.chaosPlayer.currentHealth,
            max = isOrder and self.players.orderPlayer.maxHealth or self.players.chaosPlayer.maxHealth
        })
    end,

    InitializeHealth = function(self)
        self.players.orderPlayer.currentHealth = CONSTANTS.MAX_HP
        self.players.chaosPlayer.currentHealth = CONSTANTS.MAX_HP
        self.previewedHealOrder = 0
        self.previewedHealChaos = 0

        self:SetHealth(true, CONSTANTS.MAX_HP)
        self:SetHealth(false, CONSTANTS.MAX_HP)
        Event.emitLocal("PlayHealthBarAnimations", {})
    end,

    ApplyDamage = function(self, player, damage)
        local isOrder = (player == CONSTANTS.ORDER_PLAYER)
        local current = self:GetCurrentHealth(isOrder)
        local newHealth = math.max(0, current - damage)
        self:SetHealth(isOrder, newHealth)
        Event.emitLocal("PlayHealthBarAnimations", {})
    end,

    ApplyHeal = function(self, player, healAmount)
        local isOrder = (player == CONSTANTS.ORDER_PLAYER)
        local current = self:GetCurrentHealth(isOrder)
        local maxHP = isOrder and self.players.orderPlayer.maxHealth or self.players.chaosPlayer.maxHealth
        local newHealth = math.min(maxHP, current + healAmount)
        self:SetHealth(isOrder, newHealth)
        Event.emitLocal("PlayHealthBarAnimations", {})
    end,

    PreviewHeal = function(self, player, healAmount)
        local isOrder = (player == CONSTANTS.ORDER_PLAYER)
        -- Save pre-preview health on first heal preview (for accurate revert)
        if isOrder then
            if self.previewedHealOrder == 0 then
                self.prePreviewHealHealthOrder = self:GetCurrentHealth(true)
            end
            self.previewedHealOrder = self.previewedHealOrder + healAmount
        else
            if self.previewedHealChaos == 0 then
                self.prePreviewHealHealthChaos = self:GetCurrentHealth(false)
            end
            self.previewedHealChaos = self.previewedHealChaos + healAmount
        end
        self:ApplyHeal(player, healAmount)
    end,

    RevertPreviewedHeals = function(self)
        -- Restore to exact pre-preview health (avoids under-restore when heal clamped at max)
        if self.previewedHealOrder > 0 and self.prePreviewHealHealthOrder >= 0 then
            self:SetHealth(true, self.prePreviewHealHealthOrder)
        end
        if self.previewedHealChaos > 0 and self.prePreviewHealHealthChaos >= 0 then
            self:SetHealth(false, self.prePreviewHealHealthChaos)
        end
        self.previewedHealOrder = 0
        self.previewedHealChaos = 0
        self.prePreviewHealHealthOrder = -1
        self.prePreviewHealHealthChaos = -1
        Event.emitLocal("PlayHealthBarAnimations", {})
    end,

    PreviewTrapDamage = function(self, player, damage)
        local isOrder = (player == CONSTANTS.ORDER_PLAYER)
        -- Save pre-preview health on first preview call (for accurate revert)
        if isOrder then
            if self.previewedTrapDamageOrder == 0 then
                self.prePreviewHealthOrder = self:GetCurrentHealth(true)
            end
            self.previewedTrapDamageOrder = self.previewedTrapDamageOrder + damage
        else
            if self.previewedTrapDamageChaos == 0 then
                self.prePreviewHealthChaos = self:GetCurrentHealth(false)
            end
            self.previewedTrapDamageChaos = self.previewedTrapDamageChaos + damage
        end
        self:ApplyDamage(player, damage)
    end,

    -- Has this trap already been telegraphed in the current planning phase?
    -- Re-previewing it would show damage the server will never deal a second
    -- time: the first unit to reach the tile consumes the trap.
    HasPreviewedTrap = function(self, gridID, tileIdx)
        if not self.previewedTrapTiles then return false end
        return self.previewedTrapTiles[tostring(gridID) .. ":" .. tostring(tileIdx)] == true
    end,

    MarkPreviewedTrap = function(self, gridID, tileIdx)
        self.previewedTrapTiles = self.previewedTrapTiles or {}
        self.previewedTrapTiles[tostring(gridID) .. ":" .. tostring(tileIdx)] = true
    end,

    RevertPreviewedTrapDamage = function(self)
        -- Restore to exact pre-preview health (avoids over-restore when damage clamped at 0)
        if self.previewedTrapDamageOrder > 0 and self.prePreviewHealthOrder >= 0 then
            self:SetHealth(true, self.prePreviewHealthOrder)
        end
        if self.previewedTrapDamageChaos > 0 and self.prePreviewHealthChaos >= 0 then
            self:SetHealth(false, self.prePreviewHealthChaos)
        end
        self.previewedTrapDamageOrder = 0
        self.previewedTrapDamageChaos = 0
        self.prePreviewHealthOrder = -1
        self.prePreviewHealthChaos = -1
        Event.emitLocal("PlayHealthBarAnimations", {})
    end,

    _GetTilePreviewContext = function(self, targetGrid)
        if not targetGrid then return nil end
        if targetGrid.isOrderGrid then
            return self.orderGrid, self.previewedTileDamageOrder, self.lockedTileDamagePreviewOrder
        end
        return self.chaosGrid, self.previewedTileDamageChaos, self.lockedTileDamagePreviewChaos
    end,

    _ApplyTileDamagePreviewTexture = function(self, grid, tileIdx, hoverMap, lockedMap)
        local tile = grid and grid.Tiles and grid.Tiles[tileIdx]
        if not tile then return end

        local lockedDamage = (lockedMap and lockedMap[tileIdx]) or 0
        local hoverDamage = (hoverMap and hoverMap[tileIdx]) or 0
        local totalPreviewDamage = lockedDamage + hoverDamage
        if totalPreviewDamage <= 0 then
            grid:UpdateTileTexture(tileIdx)
            return
        end

        local baseHealth = tile.health or CONSTANTS.TILE_MAX_HEALTH
        local predictedHealth = math.max(0, baseHealth - totalPreviewDamage)
        local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
        local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
        local clampedHealth = math.max(0, math.min(predictedHealth, CONSTANTS.TILE_MAX_HEALTH))
        local texture = nil

        if tile.color == CONSTANTS.TILE_RED then
            texture = grid.RedTileAssets[clampedHealth]
        else
            texture = grid.BlueTileAssets[clampedHealth]
        end

        if texture then
            Grid.setTileTexture(grid.gridID, row, col, 0, texture)
        else
            grid:UpdateTileTexture(tileIdx)
        end
    end,

    PreviewTileDamage = function(self, targetGrid, targetTiles, tileDamage)
        self:RevertPreviewedTileDamage(targetGrid)

        if not targetGrid or not targetTiles or not tileDamage or tileDamage <= 0 then return end
        local grid, hoverMap, lockedMap = self:_GetTilePreviewContext(targetGrid)
        if not grid or not hoverMap or not lockedMap then return end

        local seen = {}
        for _, tileIdx in ipairs(targetTiles) do
            if not seen[tileIdx] then
                seen[tileIdx] = true
                local tile = grid.Tiles[tileIdx]
                if tile and tile.health and tile.health > 0 then
                    hoverMap[tileIdx] = (hoverMap[tileIdx] or 0) + tileDamage
                end
            end
        end

        for tileIdx, _ in pairs(hoverMap) do
            self:_ApplyTileDamagePreviewTexture(grid, tileIdx, hoverMap, lockedMap)
        end
    end,

    RevertPreviewedTileDamage = function(self, targetGrid)
        local function revertOnGrid(grid, hoverMap, lockedMap)
            if not grid or not hoverMap then return end
            for tileIdx, _ in pairs(hoverMap) do
                self:_ApplyTileDamagePreviewTexture(grid, tileIdx, nil, lockedMap)
                hoverMap[tileIdx] = nil
            end
        end

        if targetGrid then
            local grid, hoverMap, lockedMap = self:_GetTilePreviewContext(targetGrid)
            revertOnGrid(grid, hoverMap, lockedMap)
            return
        end

        revertOnGrid(self.orderGrid, self.previewedTileDamageOrder, self.lockedTileDamagePreviewOrder)
        revertOnGrid(self.chaosGrid, self.previewedTileDamageChaos, self.lockedTileDamagePreviewChaos)
    end,

    CommitPreviewedTileDamage = function(self, targetGrid)
        local function commitOnGrid(grid, hoverMap, lockedMap)
            if not grid or not hoverMap or not lockedMap then return end
            for tileIdx, hoverDamage in pairs(hoverMap) do
                lockedMap[tileIdx] = (lockedMap[tileIdx] or 0) + hoverDamage
                self:_ApplyTileDamagePreviewTexture(grid, tileIdx, nil, lockedMap)
                hoverMap[tileIdx] = nil
            end
        end

        if targetGrid then
            local grid, hoverMap, lockedMap = self:_GetTilePreviewContext(targetGrid)
            commitOnGrid(grid, hoverMap, lockedMap)
            return
        end

        commitOnGrid(self.orderGrid, self.previewedTileDamageOrder, self.lockedTileDamagePreviewOrder)
        commitOnGrid(self.chaosGrid, self.previewedTileDamageChaos, self.lockedTileDamagePreviewChaos)
    end,

    ClearAllPreviewedTileDamage = function(self)
        -- First remove ephemeral hover previews while preserving queued previews.
        self:RevertPreviewedTileDamage()

        if self.lockedTileDamagePreviewOrder then
            for tileIdx, _ in pairs(self.lockedTileDamagePreviewOrder) do
                self.orderGrid:UpdateTileTexture(tileIdx)
            end
        end
        if self.lockedTileDamagePreviewChaos then
            for tileIdx, _ in pairs(self.lockedTileDamagePreviewChaos) do
                self.chaosGrid:UpdateTileTexture(tileIdx)
            end
        end

        self.previewedTileDamageOrder = {}
        self.previewedTileDamageChaos = {}
        self.lockedTileDamagePreviewOrder = {}
        self.lockedTileDamagePreviewChaos = {}
    end,

    GetCurrentHealth = function(self, isOrder)
        if isOrder then return self.players.orderPlayer.currentHealth
        else return self.players.chaosPlayer.currentHealth end
    end,

    GetPlayerGrid = function(self)
        if self.playerFaction == CONSTANTS.ORDER_PLAYER then
            return self.orderGrid
        end
        return self.chaosGrid
    end,

    GetOpponentGrid = function(self)
        if self.playerFaction == CONSTANTS.ORDER_PLAYER then
            return self.chaosGrid
        end
        return self.orderGrid
    end,

    -- Check if a unit has a specific passive equipped
    HasPassive = function(self, unitType, passiveID)
        return self.unitPassives and self.unitPassives[unitType]
            and self.unitPassives[unitType][passiveID] or false
    end,

    -- Get the unit type that owns an ability (from arsenal mapping)
    GetUnitForAbility = function(self, abilityID)
        return self.abilityToUnit and self.abilityToUnit[abilityID] or nil
    end,

    -- Resolve an original (editor) ability ID to the actual equipped ability ID
    -- If the ability wasn't remapped by Arsenal, returns the original ID
    ResolveArsenalAbility = function(self, originalAbilityID)
        if self.arsenalMap and self.arsenalMap[originalAbilityID] then
            return self.arsenalMap[originalAbilityID]
        end
        return originalAbilityID
    end,

    GetAbilityByID = function(self, abilityID)
        local isOrder = (self.playerFaction == CONSTANTS.ORDER_PLAYER)
        local player = isOrder and self.players.orderPlayer or self.players.chaosPlayer

        for _, abilityName in ipairs(player.abilityList) do
            local abilityComponent = self.abilities[abilityName]
            if abilityComponent and abilityComponent.ability.abilityID == abilityID then
                return abilityComponent.ability
            end
        end
        return nil
    end,

    GetAbilityByIDGlobal = function(self, abilityID)
        local allAbilities = {
            "oracleAbility1", "oracleAbility2",
            "dragoonAbility1", "dragoonAbility2",
            "tirionAbility1", "tirionAbility2",
            "impAbility1", "impAbility2",
            "eyeAbility1", "eyeAbility2",
            "vizomAbility1", "vizomAbility2",
            "crimsonSlash", "sapphireSwirl"
        }

        for _, abilityName in ipairs(allAbilities) do
            local abilityComponent = self.abilities[abilityName]
            if abilityComponent and abilityComponent.ability.abilityID == abilityID then
                return abilityComponent.ability
            end
        end
        return nil
    end,

    ResetPrepPhase = function(self)
        Log.info("[GameManager] ResetPrepPhase: AP=" .. CONSTANTS.MAX_AP .. " clearing queues")
        self:ClearAllPreviewedTileDamage()
        -- New planning phase: every trap is live again for telegraph purposes.
        self.previewedTrapTiles = {}
        self.currentAP = CONSTANTS.MAX_AP
        self.selectedAbility = nil
        self.selectedUnit = nil
        self.pendingCastUnit = nil
        self.localAttackQueue = {}
        self.localMoveSupportQueue = {}
        self.movedUnits = {}
        self.usedAbilities = {}
        self.queuedShieldUnits = {}  -- Reset shield tracking for new prep phase
        self.awaitingServer = false
        self.previewedHealOrder = 0
        self.previewedHealChaos = 0
        self.prePreviewHealHealthOrder = -1
        self.prePreviewHealHealthChaos = -1
        self.previewedTrapDamageOrder = 0
        self.previewedTrapDamageChaos = 0
        self.prePreviewHealthOrder = -1
        self.prePreviewHealthChaos = -1
        local expiresThisResolution = {
            [CONSTANTS.ABILITIES.DRAGOON_1] = true, -- Burn
            [CONSTANTS.ABILITIES.VIZOM_2] = true    -- Bleed
        }
        for unitType = 1, 6 do
            local statusMap = self.unitStatusExpiring and self.unitStatusExpiring[unitType]
            local activeStatuses = self.unitStatuses and self.unitStatuses[unitType]
            if statusMap then
                for statusID, _ in pairs(statusMap) do
                    if not (activeStatuses and activeStatuses[statusID]) then
                        statusMap[statusID] = nil
                    end
                end
                if activeStatuses then
                    for statusID, _ in pairs(activeStatuses) do
                        if expiresThisResolution[statusID] then
                            statusMap[statusID] = true
                        end
                    end
                end
            end
        end

        Event.emitLocal("OnPrepPhaseReset", {})
        Event.emitLocal("OnAPUpdate", { value = self.currentAP })

        -- Show own traps, hide opponent's
        self.orderGrid:HideAllTraps()
        self.chaosGrid:HideAllTraps()
        self.orderGrid:ShowOwnTraps(self.playerFaction)
        self.chaosGrid:ShowOwnTraps(self.playerFaction)
    end,

    -- Randomly deploys any undeployed units of the specified faction (or current playerFaction if not specified)
    -- Returns true if any units were deployed, false if all units were already deployed
    RandomlyDeployUndeployedUnits = function(self, faction)
        local bit = require("bit")

        -- Use provided faction or default to current playerFaction
        local targetFaction = faction or self.playerFaction
        local isOrder = (targetFaction == CONSTANTS.ORDER_PLAYER)
        local grid = isOrder and self.orderGrid or self.chaosGrid

        -- Get the unit types for this faction
        local unitTypes = isOrder
            and { CONSTANTS.UNIT.DRAGOON, CONSTANTS.UNIT.ORACLE, CONSTANTS.UNIT.TIRION }
            or  { CONSTANTS.UNIT.IMP, CONSTANTS.UNIT.EYE, CONSTANTS.UNIT.VIZOM }

        -- Find which units are not yet deployed
        local undeployedUnits = {}
        for _, unitType in ipairs(unitTypes) do
            local unitFlag = bit.lshift(1, unitType)
            if bit.band(grid.unitFlags, unitFlag) == 0 then
                table.insert(undeployedUnits, unitType)
            end
        end

        if #undeployedUnits == 0 then
            Log.info("[ClientGameManager] All units already deployed for faction " .. targetFaction)
            return false
        end

        -- Get all available (unoccupied, healthy) tiles
        local availableTiles = {}
        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1
                local tile = grid.Tiles[index]
                if tile and not tile.occupied and tile.health > 0 then
                    table.insert(availableTiles, { x = col, y = row })
                end
            end
        end

        -- Shuffle available tiles using Fisher-Yates
        for i = #availableTiles, 2, -1 do
            local j = math.random(1, i)
            availableTiles[i], availableTiles[j] = availableTiles[j], availableTiles[i]
        end

        -- Deploy each undeployed unit to a random available tile
        local tileIndex = 1
        for _, unitType in ipairs(undeployedUnits) do
            if tileIndex > #availableTiles then
                Log.error("[ClientGameManager] Not enough available tiles to deploy all units")
                break
            end

            local tile = availableTiles[tileIndex]
            tileIndex = tileIndex + 1

            Log.info("[ClientGameManager] Randomly deploying unit " .. unitType .. 
                " at (" .. tile.x .. ", " .. tile.y .. ")")

            -- Use UpdatePosForce event to deploy the unit (handles grid state, flags, and visuals)
            Event.emitLocal("UpdatePosForce", {
                unitType = unitType,
                x = tile.x,
                y = tile.y
            })

            -- Notify EndTurnButton and other listeners that a unit was deployed
            Event.emitLocal("OnUnitDeployed", {
                isOrder = isOrder,
                unitType = unitType
            })

            -- Notify server about the deployment
            if PassAndPlayServer_instance then
                PassAndPlayServer_instance:OnDeployUnit(unitType, tile.x, tile.y)
            else
                RPC.call("ServerRPC_OnDeployUnit",
                    NetworkedGameClient_instance.assignedClientID,
                    unitType,
                    tile.x,
                    tile.y
                )
            end
        end

        return true
    end,

    -- Get a list of valid abilities that can be cast with current AP
    -- Returns a list of { ability, abilityName, unitType } tables
    GetValidAbilities = function(self)
        local validAbilities = {}
        local isOrder = (self.playerFaction == CONSTANTS.ORDER_PLAYER)
        local player = isOrder and self.players.orderPlayer or self.players.chaosPlayer
        local grid = isOrder and self.orderGrid or self.chaosGrid

        for _, abilityName in ipairs(player.abilityList) do
            local abilityComponent = self.abilities[abilityName]
            if abilityComponent and abilityComponent.ability then
                local ability = abilityComponent.ability
                local unitType = self:GetUnitForAbility(ability.abilityID)
                
                -- Temporarily set pendingCastUnit for CanUse check
                self.pendingCastUnit = unitType
                
                if ability:CanUse(grid, self.currentAP) then
                    table.insert(validAbilities, {
                        ability = ability,
                        abilityName = abilityName,
                        unitType = unitType
                    })
                end
                
                self.pendingCastUnit = nil
            end
        end

        return validAbilities
    end,

    -- Randomly casts valid abilities until AP runs out or no valid abilities remain
    -- Returns the number of abilities cast
    RandomlyCastAbilities = function(self)
        local castCount = 0
        local maxIterations = 20  -- Safety limit
        
        for i = 1, maxIterations do
            local validAbilities = self:GetValidAbilities()
            
            if #validAbilities == 0 then
                Log.info("[ClientGameManager] No valid abilities to cast (AP=" .. self.currentAP .. ")")
                break
            end

            -- Shuffle and pick a random ability
            local randomIndex = math.random(1, #validAbilities)
            local selected = validAbilities[randomIndex]
            local ability = selected.ability
            local unitType = selected.unitType

            Log.info("[ClientGameManager] Randomly casting ability: " .. ability.name .. 
                " (ID=" .. ability.abilityID .. ") from unit " .. tostring(unitType))

            -- Set pending cast unit for the ability system
            self.pendingCastUnit = unitType

            -- Initialize ability targeting state (resets multi-phase abilities like Imp_2)
            if type(ability.OnSelect) == "function" then
                ability:OnSelect(self:GetPlayerGrid(), self.currentAP)
            end

            -- Determine target parameters
            local targetX, targetY, param1, param2 = 0, 0, 0, 0

            -- Auto-target abilities (like Oracle Ability 1) have GetSelfTargetParams
            if ability.isAutoTarget and type(ability.GetSelfTargetParams) == "function" then
                local params = ability:GetSelfTargetParams()
                targetX = params.targetX or 0
                targetY = params.targetY or 0
                param1 = params.param1 or 0
                param2 = params.param2 or 0
            else
                -- Targeted abilities: find random valid tile on the appropriate grid
                local targetGrid = ability.targetsOpponent and self:GetOpponentGrid() or self:GetPlayerGrid()
                
                -- Collect all possible tiles
                local possibleTiles = {}
                for row = 0, CONSTANTS.GRID_ROWS - 1 do
                    for col = 0, CONSTANTS.GRID_COLS - 1 do
                        table.insert(possibleTiles, { x = col, y = row })
                    end
                end
                
                -- Shuffle tiles
                for j = #possibleTiles, 2, -1 do
                    local k = math.random(1, j)
                    possibleTiles[j], possibleTiles[k] = possibleTiles[k], possibleTiles[j]
                end
                
                -- Pick the first tile as target (could add validation here)
                if #possibleTiles > 0 then
                    local tile = possibleTiles[1]
                    targetX = tile.x
                    targetY = tile.y

                    -- If ability has ConfirmTarget, use it to validate and get proper params
                    if type(ability.ConfirmTarget) == "function" then
                        local result = ability:ConfirmTarget(targetX, targetY)
                        if result then
                            targetX = result.targetX
                            targetY = result.targetY
                            param1 = result.param1 or 0
                            param2 = result.param2 or 0
                        elseif ability.abilityID == CONSTANTS.ABILITIES.IMP_2 then
                            -- Imp_2 has two-phase targeting: phase 0 stores target (returns nil),
                            -- phase 1 needs an adjacent destination tile
                            if type(ability.GetAdjacentTiles) == "function" then
                                local adjTiles = ability:GetAdjacentTiles(targetX, targetY)
                                -- Shuffle adjacent tiles
                                for aj = #adjTiles, 2, -1 do
                                    local ak = math.random(1, aj)
                                    adjTiles[aj], adjTiles[ak] = adjTiles[ak], adjTiles[aj]
                                end
                                -- Try each adjacent tile until one is accepted
                                local imp2Queued = false
                                for _, adjIdx in ipairs(adjTiles) do
                                    local adjRow = math.floor((adjIdx - 1) / CONSTANTS.GRID_COLS)
                                    local adjCol = (adjIdx - 1) % CONSTANTS.GRID_COLS
                                    local result2 = ability:ConfirmTarget(adjCol, adjRow)
                                    if result2 then
                                        targetX = result2.targetX
                                        targetY = result2.targetY
                                        param1 = result2.param1 or 0
                                        param2 = result2.param2 or 0
                                        imp2Queued = true
                                        break
                                    end
                                end
                                if not imp2Queued then
                                    Log.info("[ClientGameManager] Imp_2: no valid adjacent tile found, skipping")
                                    self.pendingCastUnit = nil
                                    goto continue
                                end
                            end
                        end
                    end
                end
            end
            
            -- Get the preparation phase to queue the ability
            local prepPhase = self.turnPhases.preparationPhase
            if prepPhase and type(prepPhase.QueueAbility) == "function" then
                prepPhase:QueueAbility(ability, targetX, targetY, param1, param2)
                castCount = castCount + 1
            else
                Log.warning("[ClientGameManager] Could not find QueueAbility function")
                break
            end
            ::continue::
        end

        Log.info("[ClientGameManager] Randomly cast " .. castCount .. " abilities")
        return castCount
    end,

    Update = function(self, entity)
        self:GetPlayerGrid():UpdateGrid()
        self:GetOpponentGrid():UpdateGrid()
        self.turnPhases:UpdatePhase(entity)
    end
})

return class
