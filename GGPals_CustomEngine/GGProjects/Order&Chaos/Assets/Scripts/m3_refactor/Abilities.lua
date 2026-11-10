AbilityManager_instance = AbilityManager_instance or {}
local passAndPlayDisplayed = false

function IsAbilityOrder(abilityType)
    if abilityType < CONSTANTS.ABILITIES.IMP_1 or (abilityType >= CONSTANTS.ABILITIES.TIRION_1 and abilityType <= CONSTANTS.ABILITIES.TIRION_2) then
        return true
    end

    if (abilityType >= CONSTANTS.ABILITIES.IMP_1 and abilityType <= CONSTANTS.ABILITIES.EYE_2) or (abilityType >= CONSTANTS.ABILITIES.VIZOM_1 and abilityType <= CONSTANTS.ABILITIES.VIZOM_2) then
        return false
    end
end

-- Returns adjusted damage if the unit on the tile is shielded (Barrier = 50% reduction)
local function GetShieldAdjustedDamage(baseDamage, grid, tileIndex)
    if AbilityManager_instance.shieldedUnit and
       grid.Tiles[tileIndex].unit == AbilityManager_instance.shieldedUnit then
        return math.ceil(baseDamage * 0.5)
    end
    return baseDamage
end

-- records if a tile was hit
-- if a unit is standing on a hit tile, record that info
local function RecordHit(ability, isOrderGrid, grid, tileIndex)
    ability.hitTiles = ability.hitTiles or {}

    table.insert(ability.hitTiles, {
        isOrderGrid = isOrderGrid,
        gridX = (tileIndex - 1) % CONSTANTS.GRID_COLS,
        gridY = math.floor((tileIndex - 1) / CONSTANTS.GRID_COLS),
        unitType = grid.Tiles[tileIndex].unit
    })
end

-- Spawns floating text at the world position of a given tile.
-- damage = 0 shows "MISS", any other value shows the damage number.
local function SpawnTextAtTile(gridID, tileIndex, damage)
   local col = (tileIndex - 1) % CONSTANTS.GRID_COLS
    local row = math.floor((tileIndex - 1) / CONSTANTS.GRID_COLS)
    local world = Grid.gridToWorld(gridID, col, row)
    if not world then
        Log.error("[SpawnTextAtTile] Could not convert grid pos to world")
        return
    end
    -- Queue it instead of emitting directly
    AbilityManager_instance.pendingFloatingTexts = AbilityManager_instance.pendingFloatingTexts or {}
    table.insert(AbilityManager_instance.pendingFloatingTexts, {
        worldX = world.x,
        worldY = world.y,
        damage = damage
    })
end

local function AdjustStereoPan(targetGridX)
    -- Maps column 0..GRID_COLS-1 to pan range -1..1
    local pan = (targetGridX / (CONSTANTS.GRID_COLS - 1)) * 2 - 1
    Event.emitLocal("SetStereoPan", { pan = pan })
end

-- if tiles have been hit, emit event
function EmitHits(ability)
    if not ability then 
        return 
    end
    Log.info("Emitting hits!")


    if ability.hitTiles and #ability.hitTiles > 0 then
        Event.emitLocal("OnTilesHit", {
            hits = ability.hitTiles
        })
    end
end

function CountNumTiles(grid)
    local numRed = 0
    local numBlue = 0

    if not grid then return -1, -1 end

    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            local index = row * CONSTANTS.GRID_COLS + col + 1

            if grid.Tiles[index].occupied and grid.Tiles[index].color == CONSTANTS.TILE_RED then
                numRed = numRed + 1
            end

            if grid.Tiles[index].occupied and grid.Tiles[index].color == CONSTANTS.TILE_BLUE then
                numBlue = numBlue + 1
            end
        end
    end

    return numRed, numBlue
end


-- Random number generator
RandomState = 12345

local function Random(min, max)
    local a = 1664525
    local c = 1013904223
    local m = 2^32
    
    RandomState = (a * RandomState + c) % m
    
    if min and max then
        return min + (RandomState % (max - min + 1))
    else
        return RandomState / m
    end
end

-- Helper function to get N random valid tiles
local function GetRandomValidTiles(grid, count)
    if not grid or not grid.Tiles then
        Log.error("[GetRandomValidTiles] Invalid grid!")
        return {}, 0
    end

    local validTiles = {}
    local validCount = 0
    
    -- Collect all valid (non-destroyed) tiles
    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            local index = GetTileIndex(col, row)
            if index > 0 and grid.Tiles[index] and grid.Tiles[index].health > 0 then
                validCount = validCount + 1
                validTiles[validCount] = {x = col, y = row, index = index}
            end
        end
    end
    
    if validCount < count then
        Log.warning("[GetRandomValidTiles] Only " .. validCount .. " valid tiles available")
        return validTiles, validCount
    end
    
    -- Randomly select 'count' tiles
    local selectedTiles = {}
    local selectedCount = 0
    
    for i = 1, count do
        local randomIndex = Random(1, validCount)
        selectedCount = selectedCount + 1
        selectedTiles[selectedCount] = validTiles[randomIndex]
        
        -- Remove selected tile from pool
        for j = randomIndex, validCount - 1 do
            validTiles[j] = validTiles[j + 1]
        end
        validTiles[validCount] = nil
        validCount = validCount - 1
    end
    
    return selectedTiles, selectedCount
end

-- Universal unit damage resolver.
function ResolveUnitDamage(baseDamage, targetUnitType)
    local damage = baseDamage

    -- Damage bonus (Eye Wrath buff, future passives) — consumed on use
    if AbilityManager_instance.damageBonus and AbilityManager_instance.damageBonus > 0 then
        Log.info("[Damage] Bonus applied: +" .. AbilityManager_instance.damageBonus)
        damage = damage + AbilityManager_instance.damageBonus
        AbilityManager_instance.damageBonus = 0
    end

    -- Oracle shield: 50% reduction, rounded up (defender pays less)
    if targetUnitType ~= nil and
       AbilityManager_instance.shieldedUnit ~= nil and
       targetUnitType == AbilityManager_instance.shieldedUnit then
        damage = math.ceil(damage * 0.5)
        Log.info("[Damage] Shield reduction applied, final: " .. damage)
    end

    -- Future modifiers: vulnerability, global multipliers, etc.

    return math.max(0, damage)
end


AbilityList = {
    -- ORACLE_1
    [1] = {
        -- UNIT.ORACLE
        unit = 2,
        name = "Crystal Cascade",
        type = "Assault",
        description = {
            "Spawn 4 crystals on random unbroken tiles. Each crystal deals 7 unit damage and 3 tile damage."
        },
        cost = ABILITY_COST,
        numBlue = 3,
        numRed = 0,
        unitDamage = 7,
        tileDamage = 3,
        crystalCount = 4,
        
        Trigger = function(self, entity, ability)
            local grid = Grids_instance.ChaosGrid
            if not grid then return end

            local randomTiles, tileCount = GetRandomValidTiles(grid, ability.crystalCount)
            ability.tileCount = tileCount
            ability.hitTiles = {}
            ability.hitIndices = {}

            -- Pre-calculate all damage and hits here, VFXPlayer just reads these
            for i = 1, tileCount do
                local tileData = randomTiles[i]
                local tile = grid.Tiles[tileData.index]
                if tile and tile.health > 0 then
                    if tile.occupied then
                        tileData.finalDamage = ResolveUnitDamage(ability.unitDamage, tile.unit)
                        tileData.hitUnit = tile.unit
                        table.insert(ability.hitTiles, {
                            isOrderGrid = false,
                            gridX = tileData.x,
                            gridY = tileData.y,
                            unitType = tile.unit
                        })
                        Event.emitLocal("TakeDamage", {
                            player = CONSTANTS.CHAOS_PLAYER,
                            damage = tileData.finalDamage
                        })
                else
                    -- Don't apply health here — VFXPlayer applies it per crystal
                    tileData.finalDamage = 0
                    tileData.tileDamage = ability.tileDamage  -- store for VFXPlayer to apply
                    tileData.hitUnit = nil
                    end
                else
                    tileData.finalDamage = nil  -- skip destroyed tile
                end
            end

            ability.randomTiles = randomTiles
            Event.emitLocal("TriggerOracle1", { targetX = 0, targetY = 0 })
        end,
        
        Selected = function(self, entity)
            if passAndPlayDisplayed then return end

            if self.orderAP < ABILITY_COST then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return false
            end

            -- Prevent double queue
            if self.selectedAbility == 1 then return false end

            Log.info("ORACLE_1 Selected!")

            local grid = Grids_instance.OrderGrid
            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[1]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                ability.Queue(self, entity)
                return false
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        
        Update = function(self, entity)
            -- No targeting needed
        end,
        
        Queue = function(self, entity)
            local ability = AbilityList[1]

            local queuedAbility = {
                player = CONSTANTS.ORDER_PLAYER,
                affects = CONSTANTS.CHAOS_PLAYER,
                unitDamage = ability.unitDamage,
                tileDamage = ability.tileDamage,
                crystalCount = ability.crystalCount,
                Trigger = ability.Trigger,
                isQueuedAbility = true
            }

            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities + 1] = queuedAbility
            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 1,
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.ORDER_PLAYER
            })

            self.orderAP = self.orderAP - ABILITY_COST
            Event.emitLocal("OnAPUpdate", {
                value = self.orderAP
            })

            Log.info("Queued ORACLE_1 (Crystal Cascade) - AP remaining: " .. self.orderAP)
            self.selectedAbility = 0
        end
    },

    -- ORACLE_2
    [2] = {
        unit = 2,
        name = "Fiberglass Finesse",
        type = "Support",
        description = {
            "Shields an ally unit, granting a Barrier that reduces damage by 50% (rounded down) until the end of this resolution phase."
        },
        description2 = {
            "[Barrier]: Unit receives 50% less damage from all sources"
        },
        cost = ABILITY_COST,
        numBlue = 1,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        targetUnit = 0,
        state = 0,

        Trigger = function(self, entity, ability)
            Log.info("ORACLE_2 Trigger!")

            local grid = Grids_instance.OrderGrid
            if not grid then return end

            local currentX, currentY
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    if grid.Tiles[index].occupied and grid.Tiles[index].unit == ability.targetUnit then
                        currentX = col
                        currentY = row
                        break
                    end
                end
                if currentX then break end
            end

            if not currentX then
                Log.info("[Oracle2] Target unit not found, skipping")
                Event.emitLocal("OnResolveAbilities", {})
                return
            end

            AbilityManager_instance.shieldedUnit = ability.targetUnit

            Event.emitLocal("TriggerOracle2", {
                targetX = currentX,
                targetY = currentY,
                unitType = ability.targetUnit
            })

            self.selectedAbility = 0
        end,

        Selected = function(self, entity)
            if passAndPlayDisplayed then return end

            -- Block if already used this turn
            if AbilityManager_instance.usedAbilities and AbilityManager_instance.usedAbilities[2] then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return false
            end

            if self.orderAP < ABILITY_COST then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return
            end

            Log.info("ORACLE_2 Selected!")

            local grid = Grids_instance.OrderGrid
            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[2]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusOrderGrid", {})
                ability.state = 1
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,

        Update = function(self, entity)
            local grid = Grids_instance.OrderGrid
            local ability = AbilityList[2]

            if not grid or ability.state == 0 then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)

            if not tilePos then return end  -- this guard was already there, debug log was ABOVE it

            Log.info("[Oracle2 Update] tilePos=(" .. tilePos.x .. "," .. tilePos.y .. ")")  -- NOW safe

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            -- Highlight all occupied tiles, crosshair on hovered tile if occupied
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    if grid.Tiles[index].occupied then
                        Grid.setTileColorAdd(grid.gridID, row, col, 0, CONSTANTS.COLOR_SUPPORT_ABILITY_HIGHLIGHT)
                    else
                        Grid.setTileColorAdd(grid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                    end
                end
            end
        end,

        Queue = function(self, entity)
            local ability = AbilityList[2]
            if ability.state ~= 1 then return end

            local cost = ability.numRed + ability.numBlue
            local grid = Grids_instance.OrderGrid
            local tileIdx = ability.targetGridY * CONSTANTS.GRID_COLS + ability.targetGridX + 1

            if not grid.Tiles[tileIdx] or not grid.Tiles[tileIdx].occupied then
                Log.info("[Oracle2] No allied unit at target")
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return
            end

            ability.targetUnit = grid.Tiles[tileIdx].unit or 0
            AbilityManager_instance.shieldedUnit = ability.targetUnit

            -- Show shield immediately during planning phase
            Event.emitLocal("ApplyShield", { unitType = ability.targetUnit })

            -- Trigger VFX preview
            Event.emitLocal("TriggerOracle2", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY,
                unitType = ability.targetUnit
            })

            self.orderAP = self.orderAP - cost
            Event.emitLocal("OnAPUpdate", { value = self.orderAP })

            local action = {
                player = CONSTANTS.ORDER_PLAYER,
                affects = CONSTANTS.ORDER_PLAYER,
                targetUnit = ability.targetUnit,
                targetGridX = ability.targetGridX,
                targetGridY = ability.targetGridY,
                Trigger = ability.Trigger
            }

            AbilityManager_instance.ActionsQueue[AbilityManager_instance.numActions + 1] = action
            AbilityManager_instance.numActions = AbilityManager_instance.numActions + 1

            -- Mark as used so it can't be cast again this turn
            Event.emitLocal("OnAbilityQueued", {
                abilityID = 2,
                queueIndex = AbilityManager_instance.numActions,
                player = CONSTANTS.ORDER_PLAYER
            })

            ability.state = 0
            self.selectedAbility = 0
            Event.emitLocal("ClearHighlights", {})
            Event.emitLocal("PlayUIAbilityValid", {})
        end
    },

    -- DRAGOON_1
    [3] = {
        -- UNIT.DRAGOON
        unit = 1,
        name = "Azure Blaze",
        type = "Support",
        description = {
            "Bombard 3 vertical tiles. Each deals 4 unit damage and 2 tile damage."
        },
        description2 = {

        },
        cost = ABILITY_COST,
        numBlue = 0,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 4,
        tileDamage = 2, 
        Trigger = function(self, entity, ability)
            Log.info("DRAGOON_1 Trigger!")

            local grid = Grids_instance.ChaosGrid

            local tile1 = ability.targetGridY * CONSTANTS.GRID_COLS + ability.targetGridX + 1
            local tile2 = (ability.targetGridY + 1 < CONSTANTS.GRID_ROWS) 
                and ((ability.targetGridY + 1) * CONSTANTS.GRID_COLS + ability.targetGridX + 1)
                or nil
            local tile3 = (ability.targetGridY - 1 >= 0)
                and ((ability.targetGridY - 1) * CONSTANTS.GRID_COLS + ability.targetGridX + 1)
                or nil

            local damageVal = 0
            ability.hitTiles = {}
            local isOrderGrid = false

            if grid.Tiles[tile1].occupied then
                damageVal = damageVal + ability.unitDamage
                RecordHit(ability, isOrderGrid, grid, tile1)
                SpawnTextAtTile(grid.gridID, tile1, ability.unitDamage)
            else
                grid.Tiles[tile1].health = math.max(0, math.min(5, grid.Tiles[tile1].health - ability.tileDamage))
                SpawnTextAtTile(grid.gridID, tile1, 0)
            end

            if tile2 then
                if grid.Tiles[tile2].occupied then
                    damageVal = damageVal + ability.unitDamage
                    RecordHit(ability, isOrderGrid, grid, tile2)
                    SpawnTextAtTile(grid.gridID, tile2, ability.unitDamage)
                else
                    grid.Tiles[tile2].health = math.max(0, math.min(5, grid.Tiles[tile2].health - ability.tileDamage))
                    SpawnTextAtTile(grid.gridID, tile2, 0)
                end
            end

            if tile3 then
                if grid.Tiles[tile3].occupied then
                    damageVal = damageVal + ability.unitDamage
                    RecordHit(ability, isOrderGrid, grid, tile3)
                    SpawnTextAtTile(grid.gridID, tile3, ability.unitDamage)
                else
                    grid.Tiles[tile3].health = math.max(0, math.min(5, grid.Tiles[tile3].health - ability.tileDamage))
                    SpawnTextAtTile(grid.gridID, tile3, 0)
                end
            end

            Event.emitLocal("TakeDamage", {
                player = CONSTANTS.CHAOS_PLAYER,
                damage = damageVal
            })

            Event.emitLocal("TriggerDragoon1", {
                targetX = ability.targetGridX, 
                targetY = ability.targetGridY
            })
        end,
      
        Selected = function(self, entity)
            if passAndPlayDisplayed then return end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewDragoon1Projectile"
            })

            if self.orderAP < ABILITY_COST then 
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return false
            end

            Log.info("DRAGOON_1 Selected!")

            local grid = Grids_instance.OrderGrid
            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[3]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusChaosGrid", {})
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        Update = function(self, entity)
            local grid = Grids_instance.ChaosGrid

            if not grid then return end

            local mouse = Input.getMousePosition()
            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)

            local ability = AbilityList[3]

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            local tile1 = tilePos.y * CONSTANTS.GRID_COLS + tilePos.x + 1
            local tile2 = (tilePos.y + 1 < CONSTANTS.GRID_ROWS) 
            and ((tilePos.y + 1) * CONSTANTS.GRID_COLS + tilePos.x + 1)
            or nil
            local tile3 = (tilePos.y - 1 >= 0)
            and ((tilePos.y - 1) * CONSTANTS.GRID_COLS + tilePos.x + 1)
            or nil
            
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1

                    Grid.setTileColorAdd(grid.gridID,
                        row, col, 0, CONSTANTS.COLOR_NONE
                    )

                    if index == tile1 then
                        Grid.setTileColorAdd(grid.gridID, 
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    end

                    if tile2 and (index == tile2) then
                        Grid.setTileColorAdd(grid.gridID, 
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    end
                    if tile3 and (index == tile3) then
                        Grid.setTileColorAdd(grid.gridID, 
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    end
                end
            end
        end,
        Queue = function(self, entity)
            local grid = Grids_instance.ChaosGrid
            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)

            if not tilePos then
                Log.error("DRAGOON_1 Queue: No valid tile position")
                self.selectedAbility = 0
                return
            end

            local ability = AbilityList[3]

            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1
            
            local queuedAbility = {
                player = CONSTANTS.ORDER_PLAYER,
                affects = CONSTANTS.CHAOS_PLAYER,
                targetGridX = tilePos.x,
                targetGridY = tilePos.y,
                targetWorldX= world.x,
                targetWorldY= world.y,
                unitDamage = AbilityList[3].unitDamage,
                tileDamage = AbilityList[3].tileDamage,
                Trigger = AbilityList[3].Trigger,

                isQueuedAbility = true
            }
            
            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities] = queuedAbility

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 3,              -- DRAGOON_1
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.ORDER_PLAYER
            })

            self.orderAP = self.orderAP - ABILITY_COST
            Event.emitLocal("OnAPUpdate", {
                value = self.orderAP
            })
            self.selectedAbility = 0
            Log.info("Queued DRAGOON_1")
            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("FocusOrderGrid", {})
        end
    },

    -- DRAGOON_2
    [4] = {
        -- UNIT.DRAGOON
        unit = 1,
        name = "Feather Cyclone",
        type = "Assault",
        description = {
            "Summon a cyclone on 1 tile, dealing 8 unit damage and 5 tile damage. Feathers fly in random directions from the center (2 + the number of tiles your units moved this turn), each dealing 2 unit damage."
        },
        description2 = {

        },
        cost = ABILITY_COST,
        numBlue = 1,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        cycloneUnitDamage = 8,
        cycloneTileDamage = 5,
        featherUnitDamage = 2,

        Trigger = function(self, entity, ability)
            Log.info("DRAGOON_2 (Feather Cyclone) Trigger!")

            local grid = Grids_instance.ChaosGrid
            if not grid then return end

            local centerIndex = ability.targetGridY * CONSTANTS.GRID_COLS + ability.targetGridX + 1
            local damageVal = 0

            ability.hitTiles = {}
            local isOrderGrid = false

            -- Cyclone damage on center tile
            if grid.Tiles[centerIndex].occupied then
                damageVal = damageVal + ability.cycloneUnitDamage
                RecordHit(ability, isOrderGrid, grid, centerIndex)
                SpawnTextAtTile(grid.gridID, centerIndex, ability.cycloneUnitDamage)
            else
                grid.Tiles[centerIndex].health = math.max(0, math.min(5, grid.Tiles[centerIndex].health - ability.cycloneTileDamage))
                SpawnTextAtTile(grid.gridID, centerIndex, 0)
            end

            Event.emitLocal("TakeDamage", {
                player = CONSTANTS.CHAOS_PLAYER,
                damage = damageVal
            })

            -- Calculate feather count
            local tilesMoved = AbilityManager_instance.orderTilesMoved or 0
            local featherCount = tilesMoved + 2
            Log.info("[FEATHER_CYCLONE] Tiles moved: " .. tilesMoved .. ", Feathers: " .. featherCount)
            Log.info("[FEATHER_CYCLONE] orderTilesMoved raw: " .. tostring(AbilityManager_instance.orderTilesMoved))


            -- Get valid adjacent tiles (8 directions, 1 tile from center)
            local directions = {
                {-1, -1}, {0, -1}, {1, -1},
                {-1,  0},          {1,  0},
                {-1,  1}, {0,  1}, {1,  1}
            }

            local function GetValidAdjacentTiles(centerX, centerY)
                local valid = {}
                for _, dir in ipairs(directions) do
                    local tx = centerX + dir[1]
                    local ty = centerY + dir[2]
                    if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                        local index = GetTileIndex(tx, ty)
                        if index > 0 and grid.Tiles[index] and grid.Tiles[index].health > 0 then
                            table.insert(valid, {x = tx, y = ty, index = index})
                        end
                    end
                end
                return valid
            end

            -- Assign feather targets
            local featherTargets = {}
            local assigned = 0

            while assigned < featherCount do
                local validTiles = GetValidAdjacentTiles(ability.targetGridX, ability.targetGridY)
                if #validTiles == 0 then
                    Log.warning("[FEATHER_CYCLONE] No valid adjacent tiles!")
                    break
                end

                while #validTiles > 0 and assigned < featherCount do
                    local pick = Random(1, #validTiles)
                    assigned = assigned + 1
                    featherTargets[assigned] = validTiles[pick]
                    table.remove(validTiles, pick)
                end
            end

            ability.featherTargets = featherTargets
            ability.featherCount = assigned
            ability.featherUnitDamage = AbilityList[4].featherUnitDamage

            Log.info("[FEATHER_CYCLONE] featherCount: " .. assigned)

            Event.emitLocal("TriggerDragoon2", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })
        end,

        Selected = function(self, entity)
            if passAndPlayDisplayed then return end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewDragoon2Projectile"
            })

            if self.orderAP < ABILITY_COST then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return
            end

            Log.info("DRAGOON_2 Selected!")
            local grid = Grids_instance.OrderGrid
            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[4]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusChaosGrid", {})
                return true
            end

            return false
        end,

        Update = function(self, entity)
            local grid = Grids_instance.ChaosGrid
            if not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
            local ability = AbilityList[4]

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            -- Highlight only center tile
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    if col == tilePos.x and row == tilePos.y then
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,

        Queue = function(self, entity)
            local ability = AbilityList[4]

            local queuedAbility = {
                player = CONSTANTS.ORDER_PLAYER,
                affects = CONSTANTS.CHAOS_PLAYER,
                targetGridX = ability.targetGridX,
                targetGridY = ability.targetGridY,
                cycloneUnitDamage = ability.cycloneUnitDamage,
                cycloneTileDamage = ability.cycloneTileDamage,
                featherUnitDamage = ability.featherUnitDamage,
                Trigger = ability.Trigger,
                isQueuedAbility = true
            }

            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities + 1] = queuedAbility
            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 4,
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.ORDER_PLAYER
            })

            self.orderAP = self.orderAP - ABILITY_COST
            Event.emitLocal("OnAPUpdate", {
                value = self.orderAP
            })

            self.selectedAbility = 0
            Log.info("Queued DRAGOON_2 (Feather Cyclone)")
            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("FocusOrderGrid", {})
        end
    },

    -- IMP_1
    [5] = {
        -- UNIT.IMP
        unit = 4,
        name = "Imp-Pact!",
        type = "Assault",
        description = {
            "Blast 2 tiles vertically with Imp-Pact. Each Imp-Pact deals 4 unit damage and 3 tile damage. For each Imp-Pact, if a unit is hit, recast once and spawn another Imp-Pact on a random unbroken tile.",
        },
        cost = ABILITY_COST,
        numBlue = 1,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 4,
        tileDamage = 3,

        Trigger = function(self, entity, ability)
            Log.info("IMP_1 Trigger!")

            local grid = Grids_instance.OrderGrid
            ability.hitTiles = {}
            local isOrderGrid = true
            local totalDamage = 0

            -- Two tiles: target and one above
            local tiles = {}
            tiles[1] = { x = ability.targetGridX, y = ability.targetGridY }
            if ability.targetGridY - 1 >= 0 then
                tiles[2] = { x = ability.targetGridX, y = ability.targetGridY - 1 }
            end

            -- Process each tile and check for recast
            ability.recastTiles = {}
            local recastCount = 0

            for i, tileData in ipairs(tiles) do
                local index = tileData.y * CONSTANTS.GRID_COLS + tileData.x + 1
                local tile = grid.Tiles[index]

                if tile and tile.health > 0 then
                    if tile.occupied then
                        totalDamage = totalDamage + ability.unitDamage
                        RecordHit(ability, isOrderGrid, grid, index)
                        SpawnTextAtTile(grid.gridID, index, ability.unitDamage)

                        -- Find recast target
                        local directions = {
                            {-1, -1}, {0, -1}, {1, -1},
                            {-1,  0},          {1,  0},
                            {-1,  1}, {0,  1}, {1,  1}
                        }
                        local validTiles = {}
                        for _, dir in ipairs(directions) do
                            local tx = tileData.x + dir[1]
                            local ty = tileData.y + dir[2]
                            if tx >= 0 and tx < CONSTANTS.GRID_COLS and ty >= 0 and ty < CONSTANTS.GRID_ROWS then
                                local rIndex = GetTileIndex(tx, ty)
                                if rIndex > 0 and grid.Tiles[rIndex] and grid.Tiles[rIndex].health > 0 then
                                    table.insert(validTiles, {x = tx, y = ty, index = rIndex})
                                end
                            end
                        end
                        if #validTiles > 0 then
                            local pick = Random(1, #validTiles)
                            recastCount = recastCount + 1
                            ability.recastTiles[recastCount] = validTiles[pick]
                            Log.info("[IMP_1] Tile " .. i .. " hit unit, recast to (" .. validTiles[pick].x .. ", " .. validTiles[pick].y .. ")")
                        end
                    else
                        tile.health = math.max(0, math.min(5, tile.health - ability.tileDamage))
                        SpawnTextAtTile(grid.gridID, index, 0)
                    end
                end
            end

            ability.hasRecast = recastCount > 0
            ability.recastCount = recastCount

            Event.emitLocal("TakeDamage", {
                player = CONSTANTS.ORDER_PLAYER,
                damage = totalDamage
            })

            Event.emitLocal("TriggerImp1", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })
        end,

        Selected = function(self, entity)
            if passAndPlayDisplayed then return end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewImp1Projectile"
            })

            if self.chaosAP < ABILITY_COST then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return
            end

            Log.info("IMP_1 Selected!")
            local grid = Grids_instance.ChaosGrid
            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[5]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusOrderGrid", {})
                return true
            end

            return false
        end,

        Update = function(self, entity)
            local grid = Grids_instance.OrderGrid
            if not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
            local ability = AbilityList[5]

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local isHighlighted = false
                    if col == tilePos.x and row == tilePos.y then
                        isHighlighted = true
                    end
                    if col == tilePos.x and row == tilePos.y - 1 then
                        isHighlighted = true
                    end

                    if isHighlighted then
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,

        Queue = function(self, entity)
            local ability = AbilityList[5]
            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1

            local queuedAbility = {
                player = CONSTANTS.CHAOS_PLAYER,
                affects = CONSTANTS.ORDER_PLAYER,
                targetGridX = ability.targetGridX,
                targetGridY = ability.targetGridY,
                unitDamage = ability.unitDamage,
                tileDamage = ability.tileDamage,
                Trigger = ability.Trigger,
                isQueuedAbility = true
            }

            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities] = queuedAbility
            Log.info("Queued IMP_1")

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 5,
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.CHAOS_PLAYER
            })

            self.chaosAP = self.chaosAP - ABILITY_COST
            Event.emitLocal("OnAPUpdate", {
                value = self.chaosAP
            })
            self.selectedAbility = 0
            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("FocusChaosGrid", {})
        end
    },

    -- IMP_2
    [6] = {
        -- UNIT.IMP
        unit = 4,
        name = "Wicked Shift",
        type = "Assault",
        description = {
            "Crushes 1 tile, dealing 8 unit damage and 4 tile damage. Choose an adjacent tile; if a unit is hit, pull it there. If it lands on a trap or destroyed tile, it takes 2 additional damage."
        },
        cost = ABILITY_COST,
        numBlue = 0,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        pullTargetGridX = 0,
        pullTargetGridY = 0,
        -- 0: Not active, 1: selecting first tile, 2: selecting adjacent tile
        state = 0,
        unitDamage = 8,
        tileDamage = 4,

        Trigger = function(self, entity, ability)
            Log.info("IMP_2 Trigger!")

            local grid = Grids_instance.OrderGrid

            local centerIndex = GetTileIndex(ability.targetGridX, ability.targetGridY)
            
            if centerIndex <= 0 then return end

            local damageVal = 0
            ability.hitTiles = {}
            local isOrderGrid = true

            local hitUnit = CONSTANTS.UNIT.NONE

            local abilityFromX = ability.targetGridX
            local abilityFromY = ability.targetGridY
            local abilityToX = -1
            local abilityToY = -1

            -- Check if the target tile is occupied by an enemy unit
            if grid.Tiles[centerIndex].occupied then
                -- Deal 3 damage to the unit
                damageVal = damageVal + ability.unitDamage
                RecordHit(ability, isOrderGrid, grid, centerIndex)
                SpawnTextAtTile(grid.gridID, centerIndex, ability.unitDamage)
                Event.emitLocal("TakeDamage", {
                    player = CONSTANTS.ORDER_PLAYER,
                    damage = damageVal
                })

                hitUnit = grid.Tiles[centerIndex].unit

                -- Pull the unit to the adjacent tile
                local pullIndex = GetTileIndex(ability.pullTargetGridX, ability.pullTargetGridY)
                if pullIndex > 0 and not grid.Tiles[pullIndex].occupied then
                    if TrySetTileOccupied(-1, true, ability.pullTargetGridX, ability.pullTargetGridY, hitUnit) then
                        SetTileUnoccupied(-1, true, ability.targetGridX, ability.targetGridY)
                        
                        abilityToX = ability.pullTargetGridX
                        abilityToY = ability.pullTargetGridY
                        abilityFromX = ability.targetGridX
                        abilityFromY = ability.targetGridY

                        Event.emitLocal("TriggerImp2", {
                            isOrder = false,
                            unitType = hitUnit,
                            fromX = ability.targetGridX,
                            fromY = ability.targetGridY,
                            toX = ability.pullTargetGridX,
                            toY = ability.pullTargetGridY
                        })

                        return
                    end
                end
            else
                -- Deal 1 damage to the tile
                grid.Tiles[centerIndex].health = math.max(0, math.min(5, grid.Tiles[centerIndex].health - ability.tileDamage))
                SpawnTextAtTile(grid.gridID, centerIndex, 0)
            end

            Event.emitLocal("TriggerImp2", {
                isOrder = false,
                unitType = hitUnit,
                fromX = abilityFromX,
                fromY = abilityFromY,
                toX = abilityToX,
                toY = abilityToY
            })
        end,

        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewImp2Projectile"
            })

            if self.chaosAP < ABILITY_COST then 
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return
            end
            Log.info("IMP_2 Selected!")

            local grid = Grids_instance.ChaosGrid

            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[6]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusOrderGrid", {})
                ability.state = 1
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,

        Update = function(self, entity)
            local grid = Grids_instance.OrderGrid
            local ability = AbilityList[6]

            if not grid or ability.state == 0 then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)

            if not tilePos then return end
            
            if ability.state == 1 then
                -- First selection: choose target tile on enemy grid
                ability.targetGridX = tilePos.x
                ability.targetGridY = tilePos.y

                -- Highlight the target tile
                for row = 0, CONSTANTS.GRID_ROWS - 1 do
                    for col = 0, CONSTANTS.GRID_COLS - 1 do
                        if col == tilePos.x and row == tilePos.y then
                            Grid.setTileColorAdd(grid.gridID, 
                                row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                            )
                        else
                            Grid.setTileColorAdd(grid.gridID,
                                row, col, 0, CONSTANTS.COLOR_NONE
                            )
                        end
                    end
                end

            elseif ability.state == 2 then
                -- Second selection: choose adjacent tile for pull destination
                ability.pullTargetGridX = tilePos.x
                ability.pullTargetGridY = tilePos.y

                -- Highlight valid adjacent tiles
                local adjacentOffsets = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}
                local validTiles = {}
                
                for _, offset in ipairs(adjacentOffsets) do
                    local adjX = ability.targetGridX + offset[1]
                    local adjY = ability.targetGridY + offset[2]
                    
                    if adjX >= 0 and adjX < CONSTANTS.GRID_COLS and
                        adjY >= 0 and adjY < CONSTANTS.GRID_ROWS then
                        local index = GetTileIndex(adjX, adjY)
                        validTiles[index] = true
                    end
                end

                for row = 0, CONSTANTS.GRID_ROWS - 1 do
                    for col = 0, CONSTANTS.GRID_COLS - 1 do
                        local index = GetTileIndex(col, row)
                        
                        if validTiles[index] then
                            Grid.setTileColorAdd(grid.gridID, 
                                row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                            )
                        else
                            Grid.setTileColorAdd(grid.gridID,
                                row, col, 0, CONSTANTS.COLOR_NONE
                            )
                        end
                    end
                end
            end
        end,

        Queue = function(self, entity)
            local ability = AbilityList[6]

            if ability.state == 1 then
                -- User selected the first target tile
                ability.state = 2
                Log.info("IMP_2: First tile selected, now select adjacent tile")

            elseif ability.state == 2 then
                -- User selected the adjacent tile
                local dx = math.abs(ability.pullTargetGridX - ability.targetGridX)
                local dy = math.abs(ability.pullTargetGridY - ability.targetGridY)
                
                -- Validate that the pull target is adjacent to the target tile
                if (dx == 1 and dy == 0) or (dx == 0 and dy == 1) then
                    AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1

                    local queuedAbility = {
                        player = CONSTANTS.CHAOS_PLAYER,
                        affects = CONSTANTS.ORDER_PLAYER,
                        targetGridX = ability.targetGridX,
                        targetGridY = ability.targetGridY,
                        pullTargetGridX = ability.pullTargetGridX,
                        pullTargetGridY = ability.pullTargetGridY,
                        unitDamage = ability.unitDamage,
                        tileDamage = ability.tileDamage,
                        Trigger = ability.Trigger,

                        isQueuedAbility = true
                    }

                    AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities] = queuedAbility
                    Log.info("Queued IMP_2")

                    Event.emitLocal("OnAbilityQueued", {
                        abilityID = 6,
                        queueIndex = AbilityManager_instance.numAbilities,
                        player = CONSTANTS.CHAOS_PLAYER
                    })

                    self.chaosAP = self.chaosAP - ABILITY_COST
                    Event.emitLocal("OnAPUpdate", {
                        value = self.chaosAP
                    })
                    
                    self.selectedAbility = 0
                    ability.state = 0
                    Event.emitLocal("ClearHighlights", {})
                    Event.emitLocal("PlayUIAbilityValid", {})
                    Event.emitLocal("FocusChaosGrid", {})
                end
            end
        end
    },

    -- EYE_1
    [7] = {
        -- UNIT.EYE
        unit = 3,
        name = "Ruinous Gaze",
        type = "Assault",
        description = {
            "Smite 1 tile with lightning, dealing 5 unit damage and 2 tile damage. If this ability destroys the tile, recast once on the left and right adjacent tiles."
        },
        cost = ABILITY_COST,
        numBlue = 2,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 5,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("EYE_1 Trigger!")

            local grid = Grids_instance.OrderGrid

            -- 3x3 square centered on target
            local tiles = {}
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local targetX = ability.targetGridX + dx
                    local targetY = ability.targetGridY + dy
                    
                    if targetX >= 0 and targetX < CONSTANTS.GRID_COLS and
                    targetY >= 0 and targetY < CONSTANTS.GRID_ROWS then
                        local index = targetY * CONSTANTS.GRID_COLS + targetX + 1
                        table.insert(tiles, index)
                    end
                end
            end

            local damageVal = 0

            ability.hitTiles = {}       -- reset hit list for this resolution
            local isOrderGrid = true    -- hitting the order grid

            for _, tileIndex in ipairs(tiles) do
                if grid.Tiles[tileIndex].occupied then
                    damageVal = damageVal + ability.unitDamage
                    RecordHit(ability, isOrderGrid, grid, tileIndex)
                    SpawnTextAtTile(grid.gridID, tileIndex, ability.unitDamage)
                else
                    grid.Tiles[tileIndex].health = math.max(0, math.min(5, grid.Tiles[tileIndex].health - ability.tileDamage))
                    SpawnTextAtTile(grid.gridID, tileIndex, 0)
                end
            end

            Event.emitLocal("TakeDamage", {
                player = CONSTANTS.ORDER_PLAYER,
                damage = damageVal
            })

            Event.emitLocal("TriggerEye1", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewEye1Projectile"
            })

            if self.chaosAP < ABILITY_COST then 
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return
            end
            Log.info("EYE_1 Selected!")

            local grid = Grids_instance.ChaosGrid

            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[7]

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusOrderGrid", {})
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        Update = function(self, entity)
            local grid = Grids_instance.OrderGrid

            if not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
            local ability = AbilityList[7]

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

             -- Highlight single tile
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    if col == tilePos.x and row == tilePos.y then
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,

        Queue = function(self, entity)
            local grid = Grids_instance.OrderGrid
            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
            if not tilePos then
                Log.error("EYE_1 Queue: No valid tile position")
                self.selectedAbility = 0
                return
            end
            
            -- Create a copy with captured coordinates
            local queuedAbility = {
                player = CONSTANTS.CHAOS_PLAYER,
                affects = CONSTANTS.ORDER_PLAYER,
                targetGridX = tilePos.x,
                targetGridY = tilePos.y,
                unitDamage = AbilityList[7].unitDamage,
                tileDamage = AbilityList[7].tileDamage,
                Trigger = AbilityList[7].Trigger,

                isQueuedAbility = true
            }
            
            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1
            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities] = queuedAbility

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 7,              -- EYE_1
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.CHAOS_PLAYER
            })

            self.chaosAP = self.chaosAP - ABILITY_COST
            Event.emitLocal("OnAPUpdate", {
                value = self.chaosAP
            })
            self.selectedAbility = 0
            Log.info("Queued EYE_1 at (" .. tilePos.x .. ", " .. tilePos.y .. ")")
            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("FocusChaosGrid", {})
        end
    },

    -- EYE_2
    [8] = {
        -- UNIT.EYE
        unit = 3,
        name = "Fractured Earth",
        type = "Support",
        description = {
            "Crush the tile beneath the Eye, dealing 1 tile damage and healing you by 3 health."
        },
        cost = ABILITY_COST,
        numBlue = 1,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        selfDamage = 1,
        selfHeal = 3,
        Trigger = function(self, entity, ability)
            Log.info("EYE_2 Trigger!")

            local grid = Grids_instance.ChaosGrid

            -- Find Eye's position
            local eyeIndex = -1
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = GetTileIndex(col, row)
                    if grid.Tiles[index].occupied and grid.Tiles[index].unit == CONSTANTS.UNIT.EYE then
                        eyeIndex = index
                        break
                    end
                end
                if eyeIndex >= 0 then break end
            end

            if eyeIndex < 0 then
                Log.error("EYE_2 Trigger: Eye unit not found!")
                return
            end

            -- Damage tile
            grid.Tiles[eyeIndex].health = math.max(0, math.min(5, grid.Tiles[eyeIndex].health - ability.selfDamage))
            SpawnTextAtTile(grid.gridID, eyeIndex, ability.selfDamage)  

            -- Heal player
            Event.emitLocal("GainHealth", {
                player = CONSTANTS.CHAOS_PLAYER,
                healthAmount = ability.selfHeal
            })

            Event.emitLocal("TriggerEye2", {})
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end

            if self.chaosAP < ABILITY_COST then 
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return
            end
            
            Log.info("EYE_2 Selected!")

            local grid = Grids_instance.ChaosGrid

            local numRed, numBlue = CountNumTiles(grid)
            local ability = AbilityList[8]

            -- Check cost
            if not ((numRed >= ability.numRed) and (numBlue >= ability.numBlue)) then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return false
            end

            -- Find Eye's position and check tile health
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = GetTileIndex(col, row)
                    if grid.Tiles[index].occupied and grid.Tiles[index].unit == CONSTANTS.UNIT.EYE then
                        if grid.Tiles[index].health >= 4 then
                            -- Immediately trigger
                            local immediateAbility = {
                                selfDamage = ability.selfDamage,
                                selfHeal = ability.selfHeal,
                                Trigger = ability.Trigger
                            }
                            self.chaosAP = self.chaosAP - ABILITY_COST
                            Event.emitLocal("OnAPUpdate", {
                                value = self.chaosAP
                            })
                            Event.emitLocal("PlayUIAbilityValid", {})
                            ability.Queue(self, entity)
                            if ability.targetGridX ~= nil then
                                AdjustStereoPan(col)
                            end
                            ability.Trigger(self, entity, immediateAbility)
                            return false
                        else
                            Log.info("EYE_2: Tile health too low (need 4+, have " .. grid.Tiles[index].health .. ")")
                            return false
                        end
                    end
                end
            end

            Log.error("EYE_2: Eye unit not found!")
            return false
        end,
        Update = function(self, entity)
            -- No update needed - instant cast ability
        end,
        Queue = function(self, entity)
            local action = {
                player = CONSTANTS.CHAOS_PLAYER,
                affects = CONSTANTS.CHAOS_PLAYER,
                Trigger = function(self, entity, ability)
                    Event.emitLocal("ReplaySupportAbility", {
                        unitType = CONSTANTS.UNIT.EYE,
                        ability = CONSTANTS.ABILITIES.EYE_2
                    })
                end
            }

            AbilityManager_instance.ActionsQueue[AbilityManager_instance.numActions + 1] = action
            AbilityManager_instance.numActions = AbilityManager_instance.numActions + 1
        end
    },

    -- TIRION_1
    [9] = {
        -- UNIT.TIRION
        unit = 6,
        name = "Luminous Cleave",
        type = "Assault",
        description = {
            "Strike 2 horizontal tiles, dealing 5 + Fervor stacks unit damage and 2 tile damage.",
            "Hitting units grants 1 stack of Fervor. At 4 stacks, strikes a 2x2 area instead."
        },
        cost = 2,
        numBlue = 0,
        numRed = 2,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 5,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("TIRION_1 Trigger!")

            local grid = Grids_instance.ChaosGrid

            -- Two horizontal tiles: cursor position and right
            local tile1Index = GetTileIndex(ability.targetGridX, ability.targetGridY)
            local tile2Index = GetTileIndex(ability.targetGridX + 1, ability.targetGridY)

            local damageVal = 0
            ability.hitTiles = {}
            local isOrderGrid = false

            -- Handle first tile (cursor position)
            if tile1Index > 0 then
                if grid.Tiles[tile1Index].occupied then
                    damageVal = damageVal + ability.unitDamage
                    RecordHit(ability, isOrderGrid, grid, tile1Index)
                    SpawnTextAtTile(grid.gridID, tile1Index, ability.unitDamage)
                else
                    grid.Tiles[tile1Index].health = math.max(0, math.min(CONSTANTS.TILE_MAX_HEALTH, grid.Tiles[tile1Index].health - ability.tileDamage))
                    SpawnTextAtTile(grid.gridID, tile1Index, 0)
                end
            end

            -- Handle second tile (right)
            if tile2Index > 0 and ability.targetGridX + 1 < CONSTANTS.GRID_COLS then
                if grid.Tiles[tile2Index].occupied then
                    damageVal = damageVal + ability.unitDamage
                    RecordHit(ability, isOrderGrid, grid, tile2Index)
                    SpawnTextAtTile(grid.gridID, tile2Index, ability.unitDamage)
                else
                    grid.Tiles[tile2Index].health = math.max(0, math.min(CONSTANTS.TILE_MAX_HEALTH, grid.Tiles[tile2Index].health - ability.tileDamage))
                    SpawnTextAtTile(grid.gridID, tile2Index, 0)
                end
            end

            Event.emitLocal("TakeDamage", {
                player = CONSTANTS.CHAOS_PLAYER,
                damage = damageVal
            })

            Event.emitLocal("TriggerTirion1", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })

            Event.emitLocal("TriggerTirion1Projectile", {})
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewTirion1Projectile"
            })

            local ability = AbilityList[9]
            if self.orderAP < ability.cost then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return
            end

            Log.info("TIRION_1 Selected!")
            local grid = Grids_instance.OrderGrid
            local numRed, numBlue = CountNumTiles(grid)

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusChaosGrid", {})
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        Update = function(self, entity)
            local grid = Grids_instance.ChaosGrid
            if not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
            local ability = AbilityList[9]

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            -- Highlight 2 horizontal tiles: cursor and right
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local isHighlighted = false

                    if col == tilePos.x and row == tilePos.y then
                        isHighlighted = true
                    end

                    if col == tilePos.x + 1 and row == tilePos.y then
                        isHighlighted = true
                    end

                    if isHighlighted then
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,
        Queue = function(self, entity)
            local ability = AbilityList[9]

            -- Validate that right tile is within bounds
            if ability.targetGridX + 1 >= CONSTANTS.GRID_COLS then
                Log.info("TIRION_1: Right tile out of bounds")
                return
            end

            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1

            local queuedAbility = {
                player = CONSTANTS.ORDER_PLAYER,
                affects = CONSTANTS.CHAOS_PLAYER,
                targetGridX = ability.targetGridX,
                targetGridY = ability.targetGridY,
                unitDamage = ability.unitDamage,
                tileDamage = ability.tileDamage,
                Trigger = ability.Trigger,

                isQueuedAbility = true
            }

            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities] = queuedAbility

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 9,
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.ORDER_PLAYER
            })

            self.orderAP = self.orderAP - ability.cost
            Event.emitLocal("OnAPUpdate", {
                value = self.orderAP
            })
            Log.info("Queued TIRION_1")
            self.selectedAbility = 0
            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("FocusOrderGrid", {})
        end
    },

    -- TIRION_2
    [10] = {
        unit = 3,
        name = "Sigil of Consecration",
        type = "Support",
        description = {
            "Place a hidden trap, active from the next planning phase.",
            "When triggered, deals 5 damage in a 3x3 radius, detonates other Sigils caught in the blast, and is destroyed."
        },
        cost = 2,
        numBlue = 1,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        trapDamage = 5,
        Trigger = function(self, entity, ability)
            Log.info("TIRION_2 Trigger - Placing trap!")

            Event.emitLocal("TriggerTirion2", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })

            Event.emitLocal("TriggerTirion2Projectile", {})
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewTirion2Projectile"
            })

            local ability = AbilityList[10]
            if self.orderAP < ability.cost then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return
            end

            Log.info("TIRION_2 Selected!")
            local grid = Grids_instance.OrderGrid
            local numRed, numBlue = CountNumTiles(grid)

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                self.selectedAbility = 10
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusChaosGrid", {})
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        Update = function(self, entity)
            local ability = AbilityList[10]
            local grid = Grids_instance.ChaosGrid

            if self.selectedAbility ~= 10 or not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            -- Highlight the target tile
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    if col == tilePos.x and row == tilePos.y then
                        Grid.setTileColorAdd(grid.gridID, 
                            row, col, 0, CONSTANTS.COLOR_SUPPORT_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,
        Queue = function(self, entity)
            local ability = AbilityList[10]

            self.orderAP = self.orderAP - ability.cost
            Event.emitLocal("OnAPUpdate", {
                value = self.orderAP
            })

            self.selectedAbility = 0
            Event.emitLocal("ClearHighlights", {})
            Event.emitLocal("PlayUIAbilityValid", {})

            if ability.targetGridX ~= nil then
                AdjustStereoPan(ability.targetGridX)
            end
            ability.Trigger(self, entity, ability)
        end
    },

    -- VIZOM_1
    [11] = {
        -- UNIT.VIZOM
        unit = 7,
        name = "Umbral Cleave",
        type = "Assault",
        description = {
            "Strike 1 tile, dealing 4 unit damage and 2 tile damage.",
            "Consumes 1 Berserk stack to repeat on a random adjacent tile (no diagonal)."
        },
        cost = 2,
        numBlue = 0,
        numRed = 2,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 4,
        tileDamage = 2,

        Trigger = function(self, entity, ability)
            Log.info("VIZOM_1 Trigger!")

            local grid = Grids_instance.OrderGrid

            -- Single tile (Berserk bounces resolved server-side)
            local tile1Index = GetTileIndex(ability.targetGridX, ability.targetGridY)

            local damageVal = 0
            ability.hitTiles = {}
            local isOrderGrid = true

            if tile1Index > 0 then
                if grid.Tiles[tile1Index].occupied then
                    damageVal = damageVal + ability.unitDamage
                    RecordHit(ability, isOrderGrid, grid, tile1Index)
                    SpawnTextAtTile(grid.gridID, tile1Index, ability.unitDamage)
                else
                    grid.Tiles[tile1Index].health = math.max(0, math.min(CONSTANTS.TILE_MAX_HEALTH, grid.Tiles[tile1Index].health - ability.tileDamage))
                    SpawnTextAtTile(grid.gridID, tile1Index, 0)
                end
            end

            Event.emitLocal("TakeDamage", {
                player = CONSTANTS.ORDER_PLAYER,
                damage = damageVal
            })

            Event.emitLocal("TriggerVizom1", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })

            Event.emitLocal("TriggerVizom1Projectile", {})
        end,
        Selected = function(self, entity)

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewVizom1Projectile"
            })

            local ability = AbilityList[11]
            if self.chaosAP < ability.cost then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                Event.emitLocal("PlayUIAbilityInvalid", {})
                return
            end

            Log.info("VIZOM_1 Selected!")
            local grid = Grids_instance.ChaosGrid
            local numRed, numBlue = CountNumTiles(grid)

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusOrderGrid", {})
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        Update = function(self, entity)
            local grid = Grids_instance.OrderGrid
            if not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
            local ability = AbilityList[11]

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            -- Highlight single tile
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    if col == tilePos.x and row == tilePos.y then
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_OFFENSIVE_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,
        Queue = function(self, entity)
            local ability = AbilityList[11]

            AbilityManager_instance.numAbilities = AbilityManager_instance.numAbilities + 1

            local queuedAbility = {
                player = CONSTANTS.CHAOS_PLAYER,
                affects = CONSTANTS.ORDER_PLAYER,
                targetGridX = ability.targetGridX,
                targetGridY = ability.targetGridY,
                unitDamage = ability.unitDamage,
                tileDamage = ability.tileDamage,
                Trigger = ability.Trigger,

                isQueuedAbility = true
            }

            AbilityManager_instance.AbilitiesQueue[AbilityManager_instance.numAbilities] = queuedAbility

            Event.emitLocal("OnAbilityQueued", {
                abilityID = 11,
                queueIndex = AbilityManager_instance.numAbilities,
                player = CONSTANTS.CHAOS_PLAYER
            })

            self.chaosAP = self.chaosAP - ability.cost
            Event.emitLocal("OnAPUpdate", {
                value = self.chaosAP
            })
            Log.info("Queued VIZOM_1")
            self.selectedAbility = 0
            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("FocusChaosGrid", {})
        end
    },

    -- VIZOM_2
    [12] = {
        unit = 7,
        name = "Sigil of Malevolence",
        type = "Support",
        description = {
            "Place a hidden trap, active from the next planning phase.",
            "When triggered, the trap is destroyed, the unit takes 5 damage and is inflicted with Bleed (2 damage per tile moved)."
        },
        description2 = {

        },
        cost = 2,
        numBlue = 1,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        trapDamage = 5,
        Trigger = function(self, entity, ability)
            Log.info("VIZOM_2 Trigger - Placing trap!")

            Event.emitLocal("TriggerVizom2", {
                targetX = ability.targetGridX,
                targetY = ability.targetGridY
            })

            Event.emitLocal("TriggerVizom2Projectile", {})
        end,
        Selected = function(self, entity)

            Event.emitLocal("AbilityChosen", {
                selectedAbility = "PreviewVizom2Projectile"
            })

            local ability = AbilityList[12]
            if self.chaosAP < ability.cost then
                Event.emitLocal("PlayUIAbilityInvalid", {})
                self.selectedAbility = 0
                Event.emitLocal("OutOfAPWarning", {})
                Event.emitLocal("PlayCombatNoAP", {})
                return
            end

            Log.info("VIZOM_2 Selected!")
            local grid = Grids_instance.ChaosGrid
            local numRed, numBlue = CountNumTiles(grid)

            if (numRed >= ability.numRed) and (numBlue >= ability.numBlue) then
                self.selectedAbility = 12
                Event.emitLocal("PlayUIAbilityValid", {})
                Event.emitLocal("FocusOrderGrid", {})
                return true
            end

            Event.emitLocal("PlayUIAbilityInvalid", {})
            return false
        end,
        Update = function(self, entity)
            local ability = AbilityList[12]
            local grid = Grids_instance.OrderGrid

            if self.selectedAbility ~= 12 or not grid then return end

            local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)

            if not tilePos then return end

            ability.targetGridX = tilePos.x
            ability.targetGridY = tilePos.y

            -- Highlight the target tile
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    if col == tilePos.x and row == tilePos.y then
                        Grid.setTileColorAdd(grid.gridID, 
                            row, col, 0, CONSTANTS.COLOR_SUPPORT_ABILITY_HIGHLIGHT
                        )
                    else
                        Grid.setTileColorAdd(grid.gridID,
                            row, col, 0, CONSTANTS.COLOR_NONE
                        )
                    end
                end
            end
        end,
        Queue = function(self, entity)
            local ability = AbilityList[12]

            self.chaosAP = self.chaosAP - ability.cost
            Event.emitLocal("OnAPUpdate", {
                value = self.chaosAP
            })

            self.selectedAbility = 0
            Event.emitLocal("ClearHighlights", {})
            Event.emitLocal("PlayUIAbilityValid", {})

            if ability.targetGridX ~= nil then
                AdjustStereoPan(ability.targetGridX)
            end
            ability.Trigger(self, entity, ability)
        end
    },
    
    -- Movement
    [13] = {
        -- UNIT.NONE
        unit = 0,
        fromGridX = 0,
        fromGridY = 0,
        targetGridX = 0,
        targetGridY = 0,
        -- self here is the Abilities script
        Trigger = function(self, entity, ability)
            Log.info("Trigger Movement for unit " .. ability.unit)

            Event.emitLocal("ReplayUnitMovement", {
                unitType = ability.unit,
                fromX = ability.fromGridX,
                fromY = ability.fromGridY,
                toX = ability.targetGridX,
                toY = ability.targetGridY
            })
        end,
        -- self here is each unit's script
        Queue = function(self, entity)
            local action = {
                player = self.ownerPlayer,
                affects = self.ownerPlayer,
                unit = self.unitType,
                fromGridX = self.previousGridX,
                fromGridY = self.previousGridY,
                targetGridX = self.gridX,
                targetGridY = self.gridY,
                Trigger = AbilityList[13].Trigger
            }

            AbilityManager_instance.ActionsQueue[AbilityManager_instance.numActions + 1] = action
            AbilityManager_instance.numActions = AbilityManager_instance.numActions + 1
        end
    },

    -- GENERIC_1
    [14] = {
        -- UNIT.NONE
        unit = 0,
        name = "Crimson Slash",
        type = "Assault",
        description = {
            "Slash 2 tiles with a ferocious strike, dealing 6 unit damage and 3 tile damage. Press 'R' to change directions.",
        },
        numBlue = 0,
        numRed = 1,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 1,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("GENERIC_2 Trigger!")
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end
            
            Log.info("GENERIC_2 Selected!")
        end,
        Update = function(self, entity)

        end,
        Queue = function(self, entity)

        end
    },

    -- GENERIC_2
    [15] = {
        -- UNIT.NONE
        unit = 0,
        name = "Sapphire Crescent",
        type = "Assault",
        description = {
            "Slash 2 tiles with a flowing strike, dealing 6 unit damage and 3 tile damage. Press 'R' to change directions.",
        },
        numBlue = 1,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 1,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("GENERIC_2 Trigger!")
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end
            
            Log.info("GENERIC_2 Selected!")
        end,
        Update = function(self, entity)

        end,
        Queue = function(self, entity)

        end
    },

    [16] = {
        -- UNIT.NONE
        unit = 0,
        name = "Overclock Trigger",
        type = "Assault",
        description = {
            "This unit can cast its abilities multiple times per turn (AP cost still applies).",
        },
        numBlue = 1,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 1,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("GENERIC_2 Trigger!")
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end
            
            Log.info("GENERIC_2 Selected!")
        end,
        Update = function(self, entity)

        end,
        Queue = function(self, entity)

        end
    },

    [17] = {
        -- UNIT.NONE
        unit = 0,
        name = "Hunter's Mark",
        type = "Assault",
        description = {
            "A player bringing Hunter's Mark marks an enemy unit during the start of Planning Phase. Whenever a player's unit with Hunter's Mark damages a unit, it takes (2 + Number of Hunter's Mark brought by the player) additional damage.",
        },
        numBlue = 1,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 1,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("GENERIC_2 Trigger!")
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end
            
            Log.info("GENERIC_2 Selected!")
        end,
        Update = function(self, entity)

        end,
        Queue = function(self, entity)

        end
    },

    [18] = {
        -- UNIT.NONE
        unit = 0,
        name = "Isolation Execution",
        type = "Assault",
        description = {
            "Inflict 1 additional damage to units without any allied units in its movement range.",
        },
        numBlue = 1,
        numRed = 0,
        targetGridX = 0,
        targetGridY = 0,
        unitDamage = 1,
        tileDamage = 2,
        Trigger = function(self, entity, ability)
            Log.info("GENERIC_2 Trigger!")
        end,
        Selected = function(self, entity)
            if passAndPlayDisplayed then
                return
            end
            
            Log.info("GENERIC_2 Selected!")
        end,
        Update = function(self, entity)

        end,
        Queue = function(self, entity)

        end
    }
}

local class = DefineType("Abilities",
{
    Serializable = 
    {
        resolutionDelay = 0.001,
        delayTime = 3.001
    },
    isWaiting = false,
    
    currentAction = 0,
    numActions = 0,
    ActionsQueue = {},
    
    currentAbility = 0,
    numAbilities = 0,
    AbilitiesQueue = {},

    nextAction = nil,

    ResetAbilityQueue = function(self)
        self.currentAbility = 0
        self.numAbilities = 0
        self.AbilitiesQueue = {}
        self.nextAction = nil
    end,

    ResetActionsQueue = function(self)
        self.currentAction = 0
        self.numActions = 0
        self.ActionsQueue = {}
        self.nextAction = nil
    end,

    Init = function(self, entity)
        AbilityManager_instance = self

        Event.subscribe("ReplayComplete", function()
            Event.emitLocal("OnResolveAbilities", {})
        end)

        Event.subscribe("OnResolveAbilities", function()
            self.currentAction = self.currentAction + 1

            if self.currentAction <= self.numActions then
                local action = self.ActionsQueue[self.currentAction]

                if action then
                    self.isWaiting = true
                    self.resolutionDelay = 0
                    self.nextAction = action
                    Log.info("[Abilities] Current Action is Movement!")
                    return
                end
            end

            Abilities.ResetActionsQueue(self)

            self.currentAbility = self.currentAbility + 1

            -- No more abilities to resolve, return to GameManager
            if self.currentAbility > self.numAbilities then
                Abilities.ResetAbilityQueue(self)
                self.isWaiting = true
                self.resolutionDelay = 0
                self.nextAction = nil
                return
            end

            local ability = self.AbilitiesQueue[self.currentAbility]
            if ability then
                self.isWaiting = true
                self.resolutionDelay = 0
                self.nextAction = ability
            else
                Log.error("[Abilities] No ability at index " .. self.currentAbility)
                Abilities.ResetActionsQueue(self)
                Abilities.ResetAbilityQueue(self)

                self.isWaiting = true
                self.resolutionDelay = 0
                self.nextAction = nil
                return
            end
        end)

        Event.subscribe("OnPassAndPlayDisplayed", function()
            passAndPlayDisplayed = true
        end)

        Event.subscribe("OnPassAndPlayHidden", function()
            passAndPlayDisplayed = false
        end)
        Event.subscribe("TriggerVFXFinished", function()
           if self.pendingFloatingTexts then
            for _, textData in ipairs(self.pendingFloatingTexts) do
             Event.emitLocal("SpawnFloatingText", textData)
            end
            self.pendingFloatingTexts = nil
            end
        end)
    end, -- end of Init

    Update = function(self, entity)
        if passAndPlayDisplayed then
            return
        end

        if self.isWaiting then
            self.resolutionDelay = self.resolutionDelay + Time.getDeltaTime()

            if self.resolutionDelay >= self.delayTime then
                self.isWaiting = false
                self.resolutionDelay = 0

                local ability = self.nextAction
                if ability then
                    if ability.affects == CONSTANTS.ORDER_PLAYER then Event.emitLocal("FocusOrderGrid", {})
                    else Event.emitLocal("FocusChaosGrid", {}) end

                    if ability.targetGridX ~= nil then
                        AdjustStereoPan(ability.targetGridX)
                    else
                        Event.emitLocal("SetStereoPan", { pan = 0 })
                    end
                    
                    ability.Trigger(self, entity, ability)
                    Event.emitLocal("OnAbilityResolved", {
                        player = ability.player,
                        isQueuedAbility = ability.isQueuedAbility,
                        owner = ability.player
                    })
                else
                    Event.emitLocal("OnChangePhase", {})
                end
            end
        end
    end
})

return class