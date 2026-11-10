local class = DefineType("VFXPlayer", 
{
    Serializable = 
    {
        gridOffsetX = 0.01,
        gridOffsetY = 0.5,
        crystalSpawnDelay = 0.5,
        featherSpawnDelay = 0.3,
        tileUpdateDelay = 0.15
    },

    vfxSpawnQueue = {},
    vfxSpawnTimer = 0.0,
    isSpawningVFX = false,

    featherSpawnQueue = {},
    featherSpawnTimer = 0.0,
    isSpawningFeathers = false,
    cycloneCenterX = 0,
    cycloneCenterY = 0,
    pendingFeathers = {},
    feathersFinished = 0,
    totalFeathersLaunched = 0,

    pendingTileUpdates = {},
    tileUpdateTimer = 0.0,

    nextTargetPosition = {
        x = 0,
        y = 0,
        x1 = 0,
        y1 = 0
    },

    abilityParams = {},

    nextAbility = {},

    spawnedEntities = {},

    abilityTransitionHandler = 0,

    isVFXPlaying = false,
    hasVFXTransitioned = false,

        Init = function(self, entity)

        Event.subscribe("TriggerVFXFinished", function(params)
        Log.info("[VFXPlayer] TriggerVFXFinished - isVFXPlaying: " .. tostring(self.isVFXPlaying) .. " waitingForRecast: " .. tostring(self.waitingForRecast))
        if self.isSpawningVFX then return end
        if self.isSpawningFeathers then return end
        if self.waitingForFeathers then return end

        -- Wait for both initial Imp1 VFX to finish before recasting
        if self.waitingForImp1Initial then
            self.imp1InitialVFXFinished = (self.imp1InitialVFXFinished or 0) + 1
            if self.imp1InitialVFXFinished < self.imp1InitialVFXCount then
                return
            end
            self.waitingForImp1Initial = false
        end

        -- Handle Imp recast
        if self.waitingForRecast then
            self.imp1RecastIndex = (self.imp1RecastIndex or 0) + 1
            local recastTile = self.imp1RecastTiles and self.imp1RecastTiles[self.imp1RecastIndex]

            if not recastTile then
                self.waitingForRecast = false
            else
                local ability = AbilityManager_instance.nextAction
                local grid = Grids_instance.OrderGrid
                local tile = grid.Tiles[recastTile.index]

                local recastPos = Grid.gridToWorld(grid.gridID, recastTile.x, recastTile.y)
                if recastPos then
                    Prefab.spawn("Imp_Ability1_VFX", recastPos.x + self.gridOffsetX, recastPos.y + self.gridOffsetY)

                    local damageVal = 0
                    if tile and tile.health > 0 then
                        if tile.occupied then
                            damageVal = ability.unitDamage
                            ability.hitTiles = ability.hitTiles or {}
                            table.insert(ability.hitTiles, {
                                isOrderGrid = true,
                                gridX = recastTile.x,
                                gridY = recastTile.y,
                                unitType = tile.unit
                            })
                            local world = Grid.gridToWorld(grid.gridID, recastTile.x, recastTile.y)
                            if world then
                                Event.emitLocal("SpawnFloatingText", {
                                    worldX = world.x, worldY = world.y,
                                    damage = ability.unitDamage
                                })
                            end
                        else
                            tile.health = math.max(0, math.min(5, tile.health - ability.tileDamage))
                            local world = Grid.gridToWorld(grid.gridID, recastTile.x, recastTile.y)
                            if world then
                                Event.emitLocal("SpawnFloatingText", {
                                    worldX = world.x, worldY = world.y,
                                    damage = 0
                                })
                            end
                        end
                    end

                    Event.emitLocal("TakeDamage", {
                        player = CONSTANTS.ORDER_PLAYER,
                        damage = damageVal
                    })

                    self.abilityTransitionHandler = Event.subscribe("Imp_Ability1_VFX_Transition", function(transParams)
                        if not self.hasVFXTransitioned then
                            Event.emitLocal("PlayVFXImpA1", {})
                            EmitHits(AbilityManager_instance.nextAction)
                            Event.emitLocal("UpdateGridTextures", {
                                playerId = CONSTANTS.ORDER_PLAYER
                            })
                            Event.emitLocal("PlayHealthBarAnimations", {})
                            self.hasVFXTransitioned = true
                        end
                    end)
                    self.isVFXPlaying = true
                    self.hasVFXTransitioned = false

                    if self.imp1RecastIndex >= self.imp1RecastTotal then
                        self.waitingForRecast = false
                    end
                end
                return
            end
        end

        if self.isVFXPlaying then
            Event.unsubscribe(self.abilityTransitionHandler)
            self.isVFXPlaying = false
            Event.emitLocal("OnResolveAbilities", {})
        end
    end)

    Event.subscribe("TriggerOracle1Projectile", function(params)
        grid = Grids_instance.ChaosGrid
        
        Log.info("=== TriggerOracle1Projectile ===")
        
        local ability = AbilityManager_instance.nextAction
        
        if ability and ability.randomTiles and ability.tileCount then
            Log.info("Starting Crystal Cascade VFX sequence!")
            
            self.vfxSpawnQueue = {}
            self.vfxSpawnTimer = 0.0
            
            for i = 1, ability.tileCount do
                local tileData = ability.randomTiles[i]
                if tileData and grid.Tiles[tileData.index] and grid.Tiles[tileData.index].health > 0 then
                    local gridPos = Grid.gridToWorld(grid.gridID, tileData.x, tileData.y)
                    if gridPos then
                        table.insert(self.vfxSpawnQueue, {
                            index = i,
                            x = gridPos.x + self.gridOffsetX,
                            y = gridPos.y + self.gridOffsetY,
                            tileData = tileData
                        })
                    end
                end
            end
            
            if #self.vfxSpawnQueue > 0 then
                self.isSpawningVFX = true
                
                self.abilityTransitionHandler = Event.subscribe("Oracle_Ability1_VFX_Transition", function(params)
                    if not self.hasVFXTransitioned then 
                        Log.info("VFX Transition triggered - finalizing Crystal Cascade")
                        Event.emitLocal("PlayHealthBarAnimations", {})
                        self.hasVFXTransitioned = true
                    end
                end)
                self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end
        else
            Log.warning("Crystal Cascade: No randomTiles data found!")
        end
    end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewOracle1Projectile", function(params)
            grid = Grids_instance.ChaosGrid
            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )
            local gridPos2 = Grid.gridToWorld(
                grid.gridID, params.targetX + 1, params.targetY
            )

            if gridPos then 
                local entity = Prefab.spawn("Oracle_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
            if gridPos and gridPos2 then 
                local entity = Prefab.spawn("Oracle_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerOracle2", function(params)
            local grid = Grids_instance.OrderGrid
            local gridPos = Grid.gridToWorld(grid.gridID, params.targetX, params.targetY)

            if not gridPos then
                Event.emitLocal("OnResolveAbilities", {})
                return
            end

            Prefab.spawn("Oracle_Ability2_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
            Event.emitLocal("PlayVFXOracleA2", {})

            -- Always reset transition state before subscribing
            self.hasVFXTransitioned = false

            self.abilityTransitionHandler = Event.subscribe("Oracle_Ability2_VFX_Transition", function()
                if not self.hasVFXTransitioned then
                    self.hasVFXTransitioned = true
                    Event.emitLocal("ActivateShieldAfterVFX", { unitType = params.unitType })
                end
            end)

            if AbilityManager_instance.nextAction ~= nil then
                self.isVFXPlaying = true
            end
        end)

        Event.subscribe("TriggerDragoon1", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerDragoon1Projectile", function(params)
            Log.info("Trigger Dragoon Projectile!")

            grid = Grids_instance.ChaosGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
            )
            local gridPos2 = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y + 1
            )
            local gridPos3 = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y - 1
            )

            if gridPos then
                Log.info("Spawning Ability Prefab!")
                Prefab.spawn("Dragoon_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                self.abilityTransitionHandler = Event.subscribe("Dragoon_Ability1_VFX_Transition", function(transParams)
                if not self.hasVFXTransitioned then
                    Log.info("VFX Transition triggered - finalizing Azure Blaze")
                    Event.emitLocal("PlayVFXDragonA1", {})

                    -- Flush pending floating texts
                    if AbilityManager_instance.pendingFloatingTexts then
                        for _, textData in ipairs(AbilityManager_instance.pendingFloatingTexts) do
                            Event.emitLocal("SpawnFloatingText", textData)
                        end
                        AbilityManager_instance.pendingFloatingTexts = nil
                    end

                    EmitHits(AbilityManager_instance.nextAction)
                    Event.emitLocal("UpdateGridTextures", {
                        playerId = CONSTANTS.CHAOS_PLAYER
                    })
                    Event.emitLocal("PlayHealthBarAnimations", {})
                    self.hasVFXTransitioned = true
                end
            end)
                self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end

            if gridPos and gridPos2 then Prefab.spawn("Dragoon_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY) end

            if gridPos and gridPos3 then Prefab.spawn("Dragoon_Ability1_VFX", gridPos3.x + self.gridOffsetX, gridPos3.y + self.gridOffsetY) end
        end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewDragoon1Projectile", function(params)
            grid = Grids_instance.ChaosGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )
            local gridPos2 = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY + 1
            )
            local gridPos3 = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY - 1
            )

            if gridPos then
                local entity = Prefab.spawn("Dragoon_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos2 then 
                local entity = Prefab.spawn("Dragoon_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos3 then 
                local entity = Prefab.spawn("Dragoon_Ability1_VFX", gridPos3.x + self.gridOffsetX, gridPos3.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerDragoon2", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerDragoon2Projectile", function(params)
            Log.info("=== TriggerDragoon2Projectile (Feather Cyclone) ===")

            local grid = Grids_instance.ChaosGrid
            local ability = AbilityManager_instance.nextAction

            Log.info("[D2P] ability: " .. tostring(ability))
            Log.info("[D2P] featherTargets: " .. tostring(ability and ability.featherTargets))
            Log.info("[D2P] featherCount: " .. tostring(ability and ability.featherCount))

            -- Spawn cyclone VFX on center tile
            local gridPos = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
            )

            if gridPos then
                Prefab.spawn("Dragoon_Ability2_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                Event.emitLocal("PlayVFXDragonA2", {})
                self.cycloneCenterX = gridPos.x + self.gridOffsetX
                self.cycloneCenterY = gridPos.y + self.gridOffsetY
            end

            -- Set up feather queue if ability has feather data
            if ability and ability.featherTargets and ability.featherCount then
                self.featherSpawnQueue = {}
                self.featherSpawnTimer = 0.0

                for i = 1, ability.featherCount do
                    local tileData = ability.featherTargets[i]
                    if tileData then
                        local featherPos = Grid.gridToWorld(grid.gridID, tileData.x, tileData.y)
                        if featherPos then
                            table.insert(self.featherSpawnQueue, {
                                index = i,
                                x = featherPos.x + self.gridOffsetX,
                                y = featherPos.y + self.gridOffsetY,
                                tileData = tileData
                            })
                        end
                    end
                end

                if #self.featherSpawnQueue > 0 then
                    self.isSpawningFeathers = true
                    Log.info("[D2P] Feather queue size: " .. #self.featherSpawnQueue)
                else
                    Log.warning("[D2P] Feather queue is EMPTY after building!")
                end

                -- Transition handler for cyclone center hit
                self.abilityTransitionHandler = Event.subscribe("Dragoon_Ability2_VFX_Transition", function(transParams)
                    if not self.hasVFXTransitioned then
                        EmitHits(AbilityManager_instance.nextAction)
                        Event.emitLocal("UpdateGridTextures", {
                            playerId = CONSTANTS.CHAOS_PLAYER
                        })
                        Event.emitLocal("PlayHealthBarAnimations", {})
                        self.hasVFXTransitioned = true
                    end
                end)
                self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end
        end)

        Event.subscribe("Dragoon_Ability2_VFX_Feather_Transition", function(params)
            local featherData = self.pendingFeathers and self.pendingFeathers[params.featherIndex]
            if not featherData then return end

            local grid = Grids_instance.ChaosGrid
            local ability = AbilityManager_instance.nextAction
            if not grid or not ability then return end

            local tileIndex = featherData.tileData.index
            local tile = grid.Tiles[tileIndex]

            if tile and tile.health > 0 then
                local world = Grid.gridToWorld(grid.gridID, featherData.tileData.x, featherData.tileData.y)

                if tile.occupied then
                    Log.info("[Feather " .. params.featherIndex .. "] HIT UNIT")
                    Event.emitLocal("PlaySFXFeatherHit", {})

                    ability.hitTiles = ability.hitTiles or {}
                    table.insert(ability.hitTiles, {
                        isOrderGrid = false,
                        gridX = featherData.tileData.x,
                        gridY = featherData.tileData.y,
                        unitType = tile.unit
                    })

                    Event.emitLocal("TakeDamage", {
                        player = CONSTANTS.CHAOS_PLAYER,
                        damage = ability.featherUnitDamage
                    })

                    if world then
                        Event.emitLocal("SpawnFloatingText", {
                            worldX = world.x,
                            worldY = world.y,
                            damage = ability.featherUnitDamage
                        })
                    end

                    Event.emitLocal("PlayHealthBarAnimations", {})
                else
                    Log.info("[Feather " .. params.featherIndex .. "] MISS")
                    if world then
                        Event.emitLocal("SpawnFloatingText", {
                            worldX = world.x,
                            worldY = world.y,
                            damage = 0
                        })
                    end
                end
            end

            self.pendingFeathers[params.featherIndex] = nil
        end)

        self.feathersFinished = 0
        self.totalFeathersLaunched = 0

        Event.subscribe("Dragoon_Ability2_VFX_Feather_Finished", function(params)
            self.feathersFinished = self.feathersFinished + 1

            if self.feathersFinished >= self.totalFeathersLaunched and not self.isSpawningFeathers then
                Log.info("=== All feathers complete! ===")
                self.feathersFinished = 0
                self.totalFeathersLaunched = 0

                EmitHits(AbilityManager_instance.nextAction)
                Event.emitLocal("PlayHealthBarAnimations", {})

                Event.unsubscribe(self.abilityTransitionHandler)
                self.isVFXPlaying = false
                Event.emitLocal("OnResolveAbilities", {})
            end
        end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewDragoon2Projectile", function(params)
            grid = Grids_instance.ChaosGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )
            local gridPos2 = Grid.gridToWorld(
                grid.gridID, params.targetX + 1, params.targetY + 1
            )
            local gridPos3 = Grid.gridToWorld(
                grid.gridID, params.targetX + 1, params.targetY - 1
            )
            local gridPos4 = Grid.gridToWorld(
                grid.gridID, params.targetX - 1, params.targetY + 1
            )
            local gridPos5 = Grid.gridToWorld(
                grid.gridID, params.targetX - 1, params.targetY - 1
            )

            if gridPos then
                local entity = Prefab.spawn("Dragoon_Ability2_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos2 then 
                local entity = Prefab.spawn("Dragoon_Ability2_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos3 then 
                local entity = Prefab.spawn("Dragoon_Ability2_VFX", gridPos3.x + self.gridOffsetX, gridPos3.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos4 then 
                local entity = Prefab.spawn("Dragoon_Ability2_VFX", gridPos4.x + self.gridOffsetX, gridPos4.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos5 then 
                local entity = Prefab.spawn("Dragoon_Ability2_VFX", gridPos5.x + self.gridOffsetX, gridPos5.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("PreviewImp1Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )
            local gridPos2 = nil
            if params.targetY - 1 >= 0 then
                gridPos2 = Grid.gridToWorld(
                    grid.gridID, params.targetX, params.targetY - 1
                )
            end

            if gridPos then
                local entity = Prefab.spawn("Imp_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos2 then
                local entity = Prefab.spawn("Imp_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerImp1", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerImp1Projectile", function(params)
            local grid = Grids_instance.OrderGrid
            local ability = AbilityManager_instance.nextAction

            -- Count how many initial VFX to spawn
            self.imp1InitialVFXCount = 0
            self.imp1InitialVFXFinished = 0

            local gridPos = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
            )
            local gridPos2 = nil
            if self.nextTargetPosition.y - 1 >= 0 then
                gridPos2 = Grid.gridToWorld(
                    grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y - 1
                )
            end

            if gridPos then
                Prefab.spawn("Imp_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                self.imp1InitialVFXCount = self.imp1InitialVFXCount + 1
            end
            if gridPos2 then
                Prefab.spawn("Imp_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                self.imp1InitialVFXCount = self.imp1InitialVFXCount + 1
            end

            self.abilityTransitionHandler = Event.subscribe("Imp_Ability1_VFX_Transition", function(transParams)
                if not self.hasVFXTransitioned then
                    Event.emitLocal("PlayVFXImpA1", {})

                    if AbilityManager_instance.pendingFloatingTexts then
                        for _, textData in ipairs(AbilityManager_instance.pendingFloatingTexts) do
                            Event.emitLocal("SpawnFloatingText", textData)
                        end
                        AbilityManager_instance.pendingFloatingTexts = nil
                    end

                    EmitHits(AbilityManager_instance.nextAction)
                    Event.emitLocal("UpdateGridTextures", {
                        playerId = CONSTANTS.ORDER_PLAYER
                    })
                    Event.emitLocal("PlayHealthBarAnimations", {})
                    self.hasVFXTransitioned = true
                end
            end)
            self.isVFXPlaying = true
            self.hasVFXTransitioned = false
            self.waitingForImp1Initial = true

            if ability and ability.hasRecast and ability.recastTiles and ability.recastCount > 0 then
                self.waitingForRecast = true
                self.imp1RecastIndex = 0
                self.imp1RecastTiles = ability.recastTiles
                self.imp1RecastTotal = ability.recastCount
            else
                self.waitingForRecast = false
            end
        end)

        Event.subscribe("TriggerImp2", function(params)
            self.nextTargetPosition.x = params.fromX
            self.nextTargetPosition.y = params.fromY
            self.abilityParams = params
            self.nextAbility = AbilityManager_instance.nextAction
        end)

        Event.subscribe("TriggerImp2Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
            )

            if gridPos then
                Prefab.spawn("Imp_Ability2_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                self.abilityTransitionHandler = Event.subscribe("Imp_Ability2_VFX_Transition", function(params)
                    if not self.hasVFXTransitioned then 
                        Event.emitLocal("PlayVFXImpA2", {})
                        Event.emitLocal("OnForceMoveUnit", {
                            ability = CONSTANTS.ABILITIES.IMP_2,
                            unitType = self.abilityParams.unitType,
                            fromX = self.abilityParams.fromX,
                            fromY = self.abilityParams.fromY,
                            toX = self.abilityParams.toX,
                            toY = self.abilityParams.toY
                        })
                        EmitHits(self.nextAbility)
                        Event.emitLocal("UpdateGridTextures", {
                            playerId = CONSTANTS.ORDER_PLAYER
                        })
                        Event.emitLocal("PlayHealthBarAnimations", {})
                        self.hasVFXTransitioned = true
                    end
                end)
                self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end
        end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewImp2Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )

            if gridPos then
                local entity = Prefab.spawn("Imp_Ability2_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerEye1", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerEye1Projectile", function(params)
            grid = Grids_instance.OrderGrid
            local ability = AbilityManager_instance.nextAction
            
            local centerX = self.nextTargetPosition.x
            local centerY = self.nextTargetPosition.y
            
            local centerPos = Grid.gridToWorld(grid.gridID, centerX, centerY)
            
            if centerPos then
                -- Always spawn the center VFX
                Prefab.spawn("Eye_Ability1_VFX", centerPos.x + self.gridOffsetX + 0.15, centerPos.y + self.gridOffsetY + 1.0)
                
                -- Store ability data to check if recast happened
                self.eyeRecastData = {
                    centerX = centerX,
                    centerY = centerY,
                    hadRecast = (ability and ability.hadRecast) or false
                }
                
                self.abilityTransitionHandler = Event.subscribe("Eye_Ability1_VFX_Transition", function(params)
                    if not self.hasVFXTransitioned then
                        Log.info("[EYE_1] VFX Transition - Checking for recast hits")
                        
                        -- Check if we need to spawn recast VFX
                        if self.eyeRecastData and self.eyeRecastData.hadRecast then
                            Log.info("[EYE_1] Center tile was destroyed! Spawning left/right VFX")
                            
                            -- Spawn left VFX if valid
                            local leftX = self.eyeRecastData.centerX - 1
                            if leftX >= 0 then
                                local leftPos = Grid.gridToWorld(grid.gridID, leftX, self.eyeRecastData.centerY)
                                if leftPos then
                                    Prefab.spawn("Eye_Ability1_VFX", leftPos.x + self.gridOffsetX + 0.15, leftPos.y + self.gridOffsetY + 1.0)
                                end
                            end
                            
                            -- Spawn right VFX if valid
                            local rightX = self.eyeRecastData.centerX + 1
                            if rightX < CONSTANTS.GRID_COLS then
                                local rightPos = Grid.gridToWorld(grid.gridID, rightX, self.eyeRecastData.centerY)
                                if rightPos then
                                    Prefab.spawn("Eye_Ability1_VFX", rightPos.x + self.gridOffsetX + 0.15, rightPos.y + self.gridOffsetY + 1.0)
                                end
                            end
                        end
                        
                        EmitHits(AbilityManager_instance.nextAction)
                        Event.emitLocal("PlayVFXEyeA1", {})
                        Event.emitLocal("UpdateGridTextures", {
                            playerId = CONSTANTS.ORDER_PLAYER
                        })
                        Event.emitLocal("PlayHealthBarAnimations", {})
                        self.hasVFXTransitioned = true
                    end
                end)
                self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end
        end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewEye1Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )

            if gridPos then
                local entity = Prefab.spawn("Eye_Ability1_VFX", gridPos.x + self.gridOffsetX + 0.15, gridPos.y + self.gridOffsetY + 1.0)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerTirion1", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerTirion1Projectile", function(params)
        grid = Grids_instance.ChaosGrid

        local gridPos = Grid.gridToWorld(
            grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
        )
        local gridPos2 = Grid.gridToWorld(
            grid.gridID, self.nextTargetPosition.x + 1, self.nextTargetPosition.y + 1
        )

        if gridPos then
            Prefab.spawn("TirionVizom_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
            self.abilityTransitionHandler = Event.subscribe("TirionVizom_Ability1_VFX_Transition", function(_)
                if not self.hasVFXTransitioned then
                    Event.emitLocal("PlayVFXTirionVizomA1", {})
                    EmitHits(AbilityManager_instance.nextAction)
                    Event.emitLocal("UpdateGridTextures", {
                        playerId = CONSTANTS.CHAOS_PLAYER
                    })
                    Event.emitLocal("PlayHealthBarAnimations", {})
                    self.hasVFXTransitioned = true
                end
            end)
            self.isVFXPlaying = true
            self.hasVFXTransitioned = false
        end

        if gridPos and gridPos2 then
            Prefab.spawn("TirionVizom_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
        end
    end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewTirion1Projectile", function(params)
            grid = Grids_instance.ChaosGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )
            local gridPos2 = Grid.gridToWorld(
                grid.gridID, params.targetX + 1, params.targetY + 1
            )

            if gridPos then
                local entity = Prefab.spawn("TirionVizom_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos2 then 
                local entity = Prefab.spawn("TirionVizom_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerTirion2", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerTirion2Projectile", function(params)
            grid = Grids_instance.ChaosGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
            )

            if gridPos then
                -- Apply tileXCorrection for trap VFX positioning
                local tileXCorrection = {
                    [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                    [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                    [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                    [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
                }
                local row = self.nextTargetPosition.y
                local col = self.nextTargetPosition.x
                local xCorrection = tileXCorrection[row] and tileXCorrection[row][col] or 0
                
                local trapEntity = Prefab.spawn("Tirion_Ability2_VFX", gridPos.x + self.gridOffsetX + 0.1 + xCorrection, gridPos.y)
                Event.emitLocal("PlayVFXTirionVizomA2", {})
                PlaceTrap(false, self.nextTargetPosition.x, self.nextTargetPosition.y, 5, trapEntity)
                
                -- self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end
        end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewTirion2Projectile", function(params)
            grid = Grids_instance.ChaosGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )

            if gridPos then
                -- Apply tileXCorrection for trap VFX preview positioning
                local tileXCorrection = {
                    [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                    [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                    [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                    [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
                }
                local xCorrection = tileXCorrection[params.targetY] and tileXCorrection[params.targetY][params.targetX] or 0
                
                local entity = Prefab.spawn("Tirion_Ability2_VFX_Preview", gridPos.x + self.gridOffsetX + xCorrection, gridPos.y)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerVizom2", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerVizom2Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
            )

            if gridPos then
                -- Apply tileXCorrection for trap VFX positioning
                local tileXCorrection = {
                    [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                    [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                    [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                    [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
                }
                local row = self.nextTargetPosition.y
                local col = self.nextTargetPosition.x
                local xCorrection = tileXCorrection[row] and tileXCorrection[row][col] or 0
                
                local trapEntity = Prefab.spawn("Vizom_Ability2_VFX", gridPos.x + self.gridOffsetX + xCorrection, gridPos.y)
                Event.emitLocal("PlayVFXTirionVizomA2", {})
                PlaceTrap(true, self.nextTargetPosition.x, self.nextTargetPosition.y, 5, trapEntity)
                
                -- self.isVFXPlaying = true
                self.hasVFXTransitioned = false
            end
        end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewVizom2Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )

            if gridPos then
                -- Apply tileXCorrection for trap VFX preview positioning
                local tileXCorrection = {
                    [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                    [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                    [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                    [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
                }
                local xCorrection = tileXCorrection[params.targetY] and tileXCorrection[params.targetY][params.targetX] or 0
                
                local entity = Prefab.spawn("Vizom_Ability2_VFX_Preview", gridPos.x + self.gridOffsetX + xCorrection, gridPos.y)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)

        Event.subscribe("TriggerVizom1", function(params)
            self.nextTargetPosition.x = params.targetX
            self.nextTargetPosition.y = params.targetY
        end)

        Event.subscribe("TriggerVizom1Projectile", function(params)
        grid = Grids_instance.OrderGrid

        local gridPos = Grid.gridToWorld(
            grid.gridID, self.nextTargetPosition.x, self.nextTargetPosition.y
        )
        local gridPos2 = Grid.gridToWorld(
            grid.gridID, self.nextTargetPosition.x + 1, self.nextTargetPosition.y + 1
        )

        if gridPos then
            Prefab.spawn("TirionVizom_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
            self.abilityTransitionHandler = Event.subscribe("TirionVizom_Ability1_VFX_Transition", function(_)
                if not self.hasVFXTransitioned then
                    Event.emitLocal("PlayVFXTirionVizomA1", {})
                    EmitHits(AbilityManager_instance.nextAction)
                    Event.emitLocal("UpdateGridTextures", {
                        playerId = CONSTANTS.ORDER_PLAYER
                    })
                    Event.emitLocal("PlayHealthBarAnimations", {})
                    self.hasVFXTransitioned = true
                end
            end)
            self.isVFXPlaying = true
            self.hasVFXTransitioned = false
        end

        if gridPos2 then
            Prefab.spawn("TirionVizom_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
        end
    end)

        -- when ability is selected, before the player clicks on the grid,
        -- show preview of what the cast ability will look like on the grid
        Event.subscribe("PreviewVizom1Projectile", function(params)
            grid = Grids_instance.OrderGrid

            local gridPos = Grid.gridToWorld(
                grid.gridID, params.targetX, params.targetY
            )
            local gridPos2 = Grid.gridToWorld(
                grid.gridID, params.targetX + 1, params.targetY + 1
            )

            if gridPos then
                local entity = Prefab.spawn("TirionVizom_Ability1_VFX", gridPos.x + self.gridOffsetX, gridPos.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end

            if gridPos and gridPos2 then 
                local entity = Prefab.spawn("TirionVizom_Ability1_VFX", gridPos2.x + self.gridOffsetX, gridPos2.y + self.gridOffsetY)
                ECS.setMeshRenderer(entity, {
                    colorMultiply = CONSTANTS.COLOR_ABILITY_HOVER_MULTIPLY
                })
            end
        end)
    end,

    Start = function(self, entity)
        
    end,

    Update = function(self, entity)
        local dt = Time.getDeltaTime()
        
        -- Process delayed tile texture updates
        for i = #self.pendingTileUpdates, 1, -1 do
            local update = self.pendingTileUpdates[i]
            update.timer = update.timer + dt
            
            if update.timer >= update.delay then
                Log.info("[TILE UPDATE] Applying delayed texture update for tile index " .. update.tileIndex)
                Event.emitLocal("UpdateGridTextures", {
                    playerId = update.playerId
                })
                table.remove(self.pendingTileUpdates, i)
            end
        end
        
        if self.isSpawningVFX and #self.vfxSpawnQueue > 0 then
            self.vfxSpawnTimer = self.vfxSpawnTimer + dt

            if self.vfxSpawnTimer >= self.crystalSpawnDelay then
                local vfxData = table.remove(self.vfxSpawnQueue, 1)

                if vfxData then
                    Prefab.spawn("Oracle_Ability1_VFX", vfxData.x, vfxData.y)
                    Event.emitLocal("PlayVFXOracleA1", {})

                    Log.info(">>> Crystal " .. vfxData.index .. " spawned at (" .. vfxData.tileData.x .. ", " .. vfxData.tileData.y .. ") <<<")

                    if vfxData.tileData.finalDamage ~= nil then
                        if vfxData.tileData.finalDamage > 0 then
                            Event.emitLocal("SpawnFloatingText", {
                                worldX = vfxData.x,
                                worldY = vfxData.y,
                                damage = vfxData.tileData.finalDamage
                            })
                            Event.emitLocal("PlayHealthBarAnimations", {})
                        elseif vfxData.tileData.hitUnit == nil then
                            -- Apply tile damage now, at the visual moment
                            local grid = Grids_instance.ChaosGrid
                            if grid and vfxData.tileData.tileDamage then
                                local tile = grid.Tiles[vfxData.tileData.index]
                                if tile then
                                    tile.health = math.max(0, math.min(5, tile.health - vfxData.tileData.tileDamage))
                                end
                            end
                            Event.emitLocal("SpawnFloatingText", {
                                worldX = vfxData.x,
                                worldY = vfxData.y,
                                damage = 0
                            })
                            Event.emitLocal("UpdateGridTextures", { playerId = CONSTANTS.CHAOS_PLAYER })
                            Log.info("[Crystal " .. vfxData.index .. "] Applied tile damage and updated texture")
                        end
                    end
                end

                self.vfxSpawnTimer = 0.0

                if #self.vfxSpawnQueue == 0 then
                    self.isSpawningVFX = false
                    Log.info("=== All Crystal Cascade VFX spawned! ===")
                    EmitHits(AbilityManager_instance.nextAction)
                    Event.emitLocal("PlayHealthBarAnimations", {})
                end
            end
        end

        -- Handle Feather Cyclone feathers
        if self.isSpawningFeathers and #self.featherSpawnQueue > 0 then
            self.featherSpawnTimer = self.featherSpawnTimer + dt

            if self.featherSpawnTimer >= self.featherSpawnDelay then
                local featherData = table.remove(self.featherSpawnQueue, 1)

                if featherData then
                    local featherEntity = Prefab.spawn("Dragoon_Ability2_VFX_Feather", self.cycloneCenterX, self.cycloneCenterY)
                    
                    -- Register target in global table so feather picks it up next frame
                    FeatherTargets_pending = FeatherTargets_pending or {}
                    table.insert(FeatherTargets_pending, {
                        entity = featherEntity,
                        targetX = featherData.x,
                        targetY = featherData.y,
                        featherIndex = featherData.index,
                        flySpeed = 4.0  -- adjust this value
                    })

                    Log.info(">>> Feather " .. featherData.index .. " spawned, target registered <<<")

                    self.pendingFeathers = self.pendingFeathers or {}
                    self.pendingFeathers[featherData.index] = featherData
                    self.totalFeathersLaunched = self.totalFeathersLaunched + 1
                end

                self.featherSpawnTimer = 0.0

                if #self.featherSpawnQueue == 0 then
                    self.isSpawningFeathers = false
                    Log.info("=== All feathers launched! ===")
                end
            end
        end
    end,

})

return class