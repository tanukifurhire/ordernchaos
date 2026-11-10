local class = DefineType("ClientPreparationPhase",
{
    Serializable = {
        phaseEnterDelay = 0.5,       -- Delay after overlay before enabling input
        phaseOverlayDuration = 1.0,  -- How long the phase overlay displays
        trapPreviewDelay = 1.5,      -- Delay before cutting camera back from trap
        hunterMarkPreviewDelay = 2.0, -- Delay before cutting camera back from hunter mark
        hunterMarkShowDelay = 0.5    -- Delay before showing hunter mark visuals
    },

    phaseTimer = 0,
    timerActive = false,
    trapPreviewTimer = 0,
    trapPreviewActive = false,
    trapPreviewVFXList = {},
    hunterMarkPreviewTimer = 0,
    hunterMarkPreviewActive = false,
    hunterMarkShowTimer = 0,
    hunterMarkShowPending = false,
    hunterMarkPendingData = nil,
    endTurnHandler = 0,
    abilitySelectHandler = 0,
    unitSelectedHandler = 0,
    tileSelectedHandler = 0,
    moveSuccessHandler = 0,
    clearHighlightsHandler = 0,
    phaseDelayTimer = 0,
    phaseReady = false,

    Enter = function(self)
        Event.emitLocal("ShowPhaseOverlay", { phase = "planphase" })
        Event.emitLocal("InPlanningPhase", {})
        
        -- Start phase delay timer
        self.phaseDelayTimer = 0
        self.phaseReady = false

        Log.info("[PrepPhase] ====== ENTERED PREPARATION PHASE (Turn " .. (ClientGameManager_instance and ClientGameManager_instance.turnNumber or "?") .. ") ======")

        local gm = ClientGameManager_instance
        local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
        Log.info("[PrepPhase] Player faction: " .. (isOrder and "Order" or "Chaos"))

        -- Play announcer SFX for planning phase (check for low HP)
        local playerHealth = isOrder and gm.players.orderPlayer.currentHealth or gm.players.chaosPlayer.currentHealth
        local maxHealth = isOrder and gm.players.orderPlayer.maxHealth or gm.players.chaosPlayer.maxHealth
        local healthPercent = playerHealth / maxHealth

        if healthPercent <= 0.4 then
            -- Low HP announcer
            if isOrder then
                Event.emitLocal("PlayAnnouncerOrderLowHPPlan", {})
            else
                Event.emitLocal("PlayAnnouncerChaosLowHPPlan", {})
            end
        else
            -- Normal announcer
            if isOrder then
                Event.emitLocal("PlayAnnouncerOrderPlan", {})
            else
                Event.emitLocal("PlayAnnouncerChaosPlan", {})
            end
        end

        gm:ResetPrepPhase()

        -- Refresh status icon display in planning so expiring flags (blink) are reflected immediately.
        for unitType = 1, 6 do
            local statusMap = gm.unitStatuses and gm.unitStatuses[unitType]
            if statusMap then
                for statusID, stackCount in pairs(statusMap) do
                    if tonumber(stackCount) and tonumber(stackCount) > 0 then
                        Event.emitLocal("OnUnitStatusInflicted", {
                            statusID = statusID,
                            ownerUnit = 0,
                            targetUnit = unitType,
                            stackCount = stackCount,
                            isExpiring = gm.unitStatusExpiring
                                and gm.unitStatusExpiring[unitType]
                                and gm.unitStatusExpiring[unitType][statusID] and true or false
                        })
                    end
                end
            end
        end

        if isOrder then
            Event.emitLocal("FocusOrderGrid", {})
            Event.emitLocal("ShowOrderAbilities", {})
            Event.emitLocal("HideChaosAbilities", {})
        else
            Event.emitLocal("FocusChaosGrid", {})
            Event.emitLocal("ShowChaosAbilities", {})
            Event.emitLocal("HideOrderAbilities", {})
        end

        -- Clear previous turn's Hunter's Mark statuses before showing new ones
        for i = 1, 6 do
            Event.emitLocal("OnUnitStatusRemoved", {
                statusID = CONSTANTS.ABILITIES.PASSIVE_2,
                targetUnit = i
            })
        end

        -- Hunter's Mark visual: show floating text on marked enemies (with delay)
        self.hunterMarkPreviewActive = false
        self.hunterMarkShowPending = false
        self.hunterMarkPendingData = nil
        if gm.hunterMarks then
            local markData = {}
            local hasMarks = false
            for markerUnit, markedEnemy in pairs(gm.hunterMarks) do
                markerUnit = tonumber(markerUnit)
                markedEnemy = tonumber(markedEnemy)
                if not markerUnit or not markedEnemy then
                    Log.warning("[PrepPhase] Skipping malformed Hunter's Mark pair")
                else
                    -- Check if the marker belongs to the current player's faction
                    local markerIsOrder = (markerUnit <= 3)
                    if markerIsOrder == isOrder then
                        hasMarks = true
                        table.insert(markData, { markerUnit = markerUnit, markedEnemy = markedEnemy })
                    end
                end
            end
            if hasMarks then
                -- Pan to enemy grid first, then show marks after delay
                if isOrder then
                    Event.emitLocal("FocusChaosGrid", {})
                else
                    Event.emitLocal("FocusOrderGrid", {})
                end
                -- Store pending data and start delay timer
                self.hunterMarkPendingData = { marks = markData, isOrder = isOrder }
                self.hunterMarkShowTimer = self.hunterMarkShowDelay
                self.hunterMarkShowPending = true
            end
        end

        Event.emitLocal("OnChangePhase", {})

        self.phaseTimer = CONSTANTS.PREP_PHASE_TIMER
        if Persistent.isTutorialTimer then
            self.phaseTimer = 99999
        end
        self.timerActive = true

        Event.emitLocal("OnTimerUpdate", { value = self.phaseTimer })

        Event.emitLocal("TurnChanged", { currentPlayer = gm.playerFaction })

        self.endTurnHandler = Event.subscribe("OnEndActionButtonPressed", function(params)
            self:SubmitActions()
        end)

        self.abilitySelectHandler = Event.subscribe("OnAbilitySelected", function(params)
            if gm.awaitingServer then
                Log.info("[PrepPhase] OnAbilitySelected ignored: awaiting server")
                return
            end

            local abilityID = params.ability
            if not abilityID then
                Log.info("[PrepPhase] OnAbilitySelected: no ability ID in params")
                return
            end

            -- Store pending cast unit for CanUse checks (supports generics sharing abilityID)
            gm.pendingCastUnit = params.unitType or gm:GetUnitForAbility(abilityID)
            Log.info("[PrepPhase] OnAbilitySelected: abilityID=" .. tostring(abilityID) .. " unit=" .. tostring(gm.pendingCastUnit))

            local ability = gm:GetAbilityByID(abilityID)
            if not ability then
                Log.warning("[PrepPhase] No ability found for ID " .. abilityID)
                gm.pendingCastUnit = nil
                return
            end

            -- Clear transient hover previews from any previously selected ability,
            -- but keep queued (locked) preview damage.
            gm:RevertPreviewedTileDamage()

            if not ability:OnSelect(gm:GetPlayerGrid(), gm.currentAP) then
                Log.info("[PrepPhase] Ability " .. ability.name .. " failed OnSelect validation")
                Event.emitLocal("PlayUIAbilityInvalid", {})
                Event.emitLocal("OnAbilitySelectedInvalid", {}) -- for AbilityButton.lua
                return
            end

            -- Self-target or auto-target abilities queue immediately
            if (not ability.targetsOpponent and not ability.requiresTargeting) or ability.isAutoTarget then
                Log.info("[PrepPhase] Ability " .. ability.name .. " is auto-target, queueing immediately")
                local tx, ty, p1, p2 = 0, 0, 0, 0
                if type(ability.GetSelfTargetParams) == "function" then
                    local params = ability:GetSelfTargetParams()
                    if params then
                        tx = params.targetX or 0
                        ty = params.targetY or 0
                        p1 = params.param1 or 0
                        p2 = params.param2 or 0
                    end
                end
                self:QueueAbility(ability, tx, ty, p1, p2)
                return
            end

            Log.info("[PrepPhase] Ability " .. ability.name .. " selected, entering targeting mode")
            gm.selectedUnit = nil
            gm.selectedAbility = ability
            Event.emitLocal("ClearHighlights", {})

            if ability.targetsOpponent then
                if isOrder then
                    Event.emitLocal("FocusChaosGrid", {})
                else
                    Event.emitLocal("FocusOrderGrid", {})
                end
                Event.emitLocal("PlayCombatChangeGrid", {})
            else
                -- Targeting own grid (e.g. Oracle shield)
                if isOrder then
                    Event.emitLocal("FocusOrderGrid", {})
                else
                    Event.emitLocal("FocusChaosGrid", {})
                end
            end

            Event.emitLocal("PlayUIAbilityValid", {})
            Event.emitLocal("OnAbilitySelectedValid", {
                ability = params.ability
            }) -- for AbilityButton.lua
        end)

        self.unitSelectedHandler = Event.subscribe("OnUnitSelected", function(params)
            if gm.awaitingServer then return end

            Log.info("[PrepPhase] OnUnitSelected: unitType=" .. tostring(params.unitType) ..
                " at (" .. tostring(params.gridX) .. "," .. tostring(params.gridY) .. ")")

            -- If ability is selected and we click a unit tile, that's a target
            if gm.selectedAbility then
                Log.info("[PrepPhase] Ability targeting: clicked unit tile as target")
                if type(gm.selectedAbility.ConfirmTarget) == "function" then
                    local result = gm.selectedAbility:ConfirmTarget(params.gridX, params.gridY)
                    if result then
                        self:QueueAbility(gm.selectedAbility, result.targetX, result.targetY, result.param1, result.param2)
                    end
                else
                    self:QueueAbility(gm.selectedAbility, params.gridX, params.gridY, 0, 0)
                end
                return
            end

            -- Select the unit for movement
            Log.info("[PrepPhase] Selected unit " .. params.unitType .. " for movement")
            gm.selectedUnit = params.unitType
            gm.selectedAbility = nil
            Event.emitLocal("ClearHighlights", {})

            -- Only show movement highlights if unit hasn't moved this turn
            if not gm.movedUnits[params.unitType] then
                local grid = gm:GetPlayerGrid()
                grid:HighlightMovementRange(params.gridX, params.gridY, 2)
            end
        end)

        self.tileSelectedHandler = Event.subscribe("OnTileSelected", function(params)
            if gm.awaitingServer then return end

            Log.info("[PrepPhase] OnTileSelected: (" .. tostring(params.gridX) .. "," .. tostring(params.gridY) ..
                ") isOrder=" .. tostring(params.isOrder) ..
                " selectedUnit=" .. tostring(gm.selectedUnit) ..
                " selectedAbility=" .. tostring(gm.selectedAbility and gm.selectedAbility.name or "nil"))

            if gm.selectedUnit and gm.selectedUnit > 0 then
                -- Movement command (no AP cost, but unit can only move once)
                if not gm.movedUnits[gm.selectedUnit] then
                    local allowed = true
                    if self.tutorialMoveRestrictions then
                        local forUnit = self.tutorialMoveRestrictions[gm.selectedUnit] or self.tutorialMoveRestrictions[0]
                        if forUnit and #forUnit > 0 then
                            allowed = false
                            for _, tile in ipairs(forUnit) do
                                if tile.x == params.gridX and tile.y == params.gridY then
                                    allowed = true
                                    break
                                end
                            end
                        end
                    end

                    if allowed then
                        Event.emitLocal("TryMoveCommand", {
                            isOrder = params.isOrder,
                            unitType = gm.selectedUnit,
                            gridX = params.gridX,
                            gridY = params.gridY
                        })
                    else
                        Log.info("[PrepPhase] Tutorial move restriction: unit " .. tostring(gm.selectedUnit) .. " cannot move to (" .. tostring(params.gridX) .. "," .. tostring(params.gridY) .. ")")
                        Event.emitLocal("PlayUIAbilityInvalid", {})
                    end
                end

                gm.selectedAbility = nil
                gm.selectedUnit = nil
                Event.emitLocal("ClearHighlights", {})

            elseif gm.selectedAbility then
                -- Ability targeting on an empty tile
                Log.info("[PrepPhase] Ability targeting on empty tile at (" .. params.gridX .. "," .. params.gridY .. ")")
                if type(gm.selectedAbility.ConfirmTarget) == "function" then
                    local result = gm.selectedAbility:ConfirmTarget(params.gridX, params.gridY)
                    if result then
                        self:QueueAbility(gm.selectedAbility, result.targetX, result.targetY, result.param1, result.param2)
                    end
                else
                    self:QueueAbility(gm.selectedAbility, params.gridX, params.gridY, 0, 0)
                end
            else
                gm.selectedUnit = nil
                Event.emitLocal("ClearHighlights", {})
            end
        end)

        self.tutorialMoveRestrictions = nil

        self.tutorialRestrictHandler = Event.subscribe("Tutorial_Set_Unit_Move_Targets", function(params)
            self.tutorialMoveRestrictions = self.tutorialMoveRestrictions or {}
            local unitType = params.unitType or 0
            self.tutorialMoveRestrictions[unitType] = params.tiles or {}
        end)

        self.tutorialClearRestrictHandler = Event.subscribe("Tutorial_Clear_Unit_Move_Targets", function()
            self.tutorialMoveRestrictions = nil
        end)

        self.moveSuccessHandler = Event.subscribe("OnMoveCommandSuccess", function(params)
            if gm.awaitingServer then return end
            self:QueueMovement(params.unitType, params.fromX, params.fromY, params.toX, params.toY)
        end)

        self.clearHighlightsHandler = Event.subscribe("ClearHighlights", function(params)
            gm:GetPlayerGrid():ClearAllHighlights()
            gm:GetOpponentGrid():ClearAllHighlights()
        end)
    end,

    Exit = function(self)
        Log.info("[PrepPhase] ====== EXITING PREPARATION PHASE ======")
        self.timerActive = false
        local gm = ClientGameManager_instance
        gm.selectedAbility = nil
        gm.selectedUnit = nil

        gm:GetPlayerGrid():ClearAllHighlights()
        gm:GetOpponentGrid():ClearAllHighlights()

        -- Clean up all trap preview VFX
        for _, vfx in ipairs(self.trapPreviewVFXList) do
            ECS.destroyEntityDeferred(vfx)
        end
        self.trapPreviewVFXList = {}
        self.trapPreviewActive = false
        self.hunterMarkPreviewActive = false

        if self.endTurnHandler ~= 0 then
            Event.unsubscribe(self.endTurnHandler)
            self.endTurnHandler = 0
        end
        if self.abilitySelectHandler ~= 0 then
            Event.unsubscribe(self.abilitySelectHandler)
            self.abilitySelectHandler = 0
        end
        if self.unitSelectedHandler ~= 0 then
            Event.unsubscribe(self.unitSelectedHandler)
            self.unitSelectedHandler = 0
        end
        if self.tileSelectedHandler ~= 0 then
            Event.unsubscribe(self.tileSelectedHandler)
            self.tileSelectedHandler = 0
        end
        if self.moveSuccessHandler ~= 0 then
            Event.unsubscribe(self.moveSuccessHandler)
            self.moveSuccessHandler = 0
        end
        if self.clearHighlightsHandler ~= 0 then
            Event.unsubscribe(self.clearHighlightsHandler)
            self.clearHighlightsHandler = 0
        end
    end,

    Update = function(self, entity)
        local gm = ClientGameManager_instance
        if not gm then return end

        -- Handle phase enter delay
        if not self.phaseReady then
            self.phaseDelayTimer = self.phaseDelayTimer + Time.getDeltaTime()
            if self.phaseDelayTimer >= self.phaseEnterDelay then
                self.phaseReady = true
            end
            return
        end

        if self.timerActive then
            self.phaseTimer = self.phaseTimer - Time.getDeltaTime()
            Event.emitLocal("OnTimerUpdate", { value = math.max(0, self.phaseTimer) })

            if self.phaseTimer <= 0 then
                self.timerActive = false
                self:SubmitActions()
            end
        end

        -- Trap preview delay: cut camera back after timer expires
        if self.trapPreviewActive then
            self.trapPreviewTimer = self.trapPreviewTimer - Time.getDeltaTime()
            if self.trapPreviewTimer <= 0 then
                self.trapPreviewActive = false
                local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
                if isOrder then
                    Event.emitLocal("FocusOrderGrid", {})
                else
                    Event.emitLocal("FocusChaosGrid", {})
                end
            end
        end

        -- Hunter's Mark show delay: show marks after camera pan + delay
        if self.hunterMarkShowPending then
            self.hunterMarkShowTimer = self.hunterMarkShowTimer - Time.getDeltaTime()
            if self.hunterMarkShowTimer <= 0 then
                self.hunterMarkShowPending = false
                self:ShowHunterMarks()
            end
        end

        -- Hunter's Mark preview delay: cut camera back after timer expires
        if self.hunterMarkPreviewActive then
            self.hunterMarkPreviewTimer = self.hunterMarkPreviewTimer - Time.getDeltaTime()
            if self.hunterMarkPreviewTimer <= 0 then
                self.hunterMarkPreviewActive = false
                local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
                if isOrder then
                    Event.emitLocal("FocusOrderGrid", {})
                else
                    Event.emitLocal("FocusChaosGrid", {})
                end
            end
        end

        -- Ability activation hotkeys (49 = Button 1, 54 = Button 6) - refer to ASCII table
        for i = 49, 54 do
            if Input.isKeyTriggered(i) then
                Event.emitLocal("OnAbilityHotkey", {
                    slot = i - 48
                })
            end
        end

        -- Right-click cancels selection
        if Input.isKeyTriggered(MOUSE_RIGHT) then
            if gm.selectedAbility then
                local wasTargetingOpponent = gm.selectedAbility.targetsOpponent
                local targetGrid = wasTargetingOpponent and gm:GetOpponentGrid() or gm:GetPlayerGrid()
                gm.selectedAbility:ClearTargeting(targetGrid)
                gm.selectedAbility = nil
                gm.pendingCastUnit = nil
                Event.emitLocal("PlayCombatTileDeselect", {})

                local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
                if isOrder then
                    Event.emitLocal("FocusOrderGrid", {})
                else
                    Event.emitLocal("FocusChaosGrid", {})
                end
                if wasTargetingOpponent then
                    Event.emitLocal("PlayCombatChangeGrid", {})
                end
            elseif gm.selectedUnit then
                gm.selectedUnit = nil
                Event.emitLocal("PlayCombatTileDeselect", {})
                Event.emitLocal("ClearHighlights", {})
            end
        end

        -- Ctrl+D: Randomly cast valid abilities
        if Input.isKeyPressed(KEY_CTRL) and Input.isKeyTriggered(KEY_D) then
            Log.info("[PrepPhase] Ctrl+D pressed - randomly casting abilities")
            gm:RandomlyCastAbilities()
        end

        -- E: End turn (same as pressing the End Turn button)
        if Input.isKeyTriggered(69) then
            Log.info("[PrepPhase] E pressed - ending turn")
            Event.emitLocal("OnEndActionButtonPressed", {})
            Event.emitLocal("PlayUIEndTurn", {})
        end

        -- Cheat codes: Ctrl+Shift+O = Order wins, Ctrl+Shift+C = Chaos wins, Ctrl+Shift+B = Draw
        if Input.isKeyPressed(KEY_CTRL) and Input.isKeyPressed(340) then
            if Input.isKeyTriggered(79) then  -- O
                Log.info("[PrepPhase] CHEAT: Forcing Order win")
                Event.emitLocal("ForceEndGame", { winner = CONSTANTS.ORDER_PLAYER })
            elseif Input.isKeyTriggered(67) then  -- C
                Log.info("[PrepPhase] CHEAT: Forcing Chaos win")
                Event.emitLocal("ForceEndGame", { winner = CONSTANTS.CHAOS_PLAYER })
            elseif Input.isKeyTriggered(66) then  -- B
                Log.info("[PrepPhase] CHEAT: Forcing Draw")
                Event.emitLocal("ForceEndGame", { winner = 3 })
            end
        end

        -- Ability targeting update (handles mouse tracking, highlights, and layer 1 textures)
        if gm.selectedAbility and not gm.awaitingServer then
            local targetGrid = gm.selectedAbility.targetsOpponent and gm:GetOpponentGrid() or gm:GetPlayerGrid()
            gm.selectedAbility:Update(targetGrid)
        end
    end,

    ShowHunterMarks = function(self)
        local gm = ClientGameManager_instance
        if not gm or not self.hunterMarkPendingData then return end
        
        local marks = self.hunterMarkPendingData.marks
        local isOrder = self.hunterMarkPendingData.isOrder
        local enemyGrid = gm:GetOpponentGrid()
        
        for _, markInfo in ipairs(marks) do
            local markerUnit = markInfo.markerUnit
            local markedEnemy = markInfo.markedEnemy
            local markerName = CONSTANTS.UNIT_NAMES[markerUnit] or ("Unit " .. markerUnit)
            
            -- Find marked enemy's position by scanning the opponent grid tiles
            local foundPos = nil
            if enemyGrid and enemyGrid.Tiles then
                for row = 0, CONSTANTS.GRID_ROWS - 1 do
                    for col = 0, CONSTANTS.GRID_COLS - 1 do
                        local index = row * CONSTANTS.GRID_COLS + col + 1
                        if enemyGrid.Tiles[index] and enemyGrid.Tiles[index].unit == markedEnemy then
                            foundPos = { x = col, y = row }
                            break
                        end
                    end
                    if foundPos then break end
                end
            end
            
            if foundPos then
                local world = Grid.gridToWorld(enemyGrid.gridID, foundPos.x, foundPos.y)
                Event.emitLocal("SpawnFloatingText", {
                    worldX = world.x + 0.01,
                    worldY = world.y + 0.8,
                    damage = 0,
                    text = "Marked"
                })
                -- Trigger Hunter's Mark VFX on the marked enemy
                Event.emitLocal("HuntersMarkProc", { unitType = markedEnemy })
            end
            
            -- Inflict Mark debuff on the marked enemy
            Event.emitLocal("OnUnitStatusInflicted", {
                statusID = CONSTANTS.ABILITIES.PASSIVE_2,
                ownerUnit = markerUnit,
                targetUnit = markedEnemy,
                stackCount = 1,
                isExpiring = false
            })
            Log.info("[PrepPhase] Hunter's Mark: " .. markerName .. " marked enemy unit " .. markedEnemy)
        end
        
        -- Start the camera pan-back timer
        self.hunterMarkPreviewTimer = self.hunterMarkPreviewDelay
        self.hunterMarkPreviewActive = true
        self.hunterMarkPendingData = nil
    end,

    QueueAbility = function(self, ability, targetX, targetY, param1, param2)
        local gm = ClientGameManager_instance
        Log.info("[PrepPhase] QueueAbility: " .. ability.name .. " (ID=" .. ability.abilityID ..
            ") target=(" .. targetX .. "," .. targetY .. ") params=(" .. tostring(param1) .. "," .. tostring(param2) .. ")")

        -- Deselect immediately to prevent re-triggering
        gm.selectedAbility = nil

        -- Clear targeting visuals
        local targetGrid = ability.targetsOpponent and gm:GetOpponentGrid() or gm:GetPlayerGrid()
        gm:CommitPreviewedTileDamage(targetGrid)
        ability:ClearTargeting(targetGrid)
        Event.emitLocal("ClearHighlights", {})

        -- Mark this specific ability as having been used this turn
        local castingUnit = gm.pendingCastUnit or gm:GetUnitForAbility(ability.abilityID)
        if castingUnit then
            local abilityKey = castingUnit .. "_" .. ability.abilityID
            gm.usedAbilities[abilityKey] = true
        end
        gm.pendingCastUnit = nil

        -- Deduct AP
        gm.currentAP = gm.currentAP - ability.cost
        Event.emitLocal("OnAPUpdate", { value = gm.currentAP })

        local entry = {
            actionType = CONSTANTS.ACTION_ABILITY,
            abilityID = ability.abilityID,
            casterUnit = castingUnit,
            targetX = targetX,
            targetY = targetY,
            param1 = param1,
            param2 = param2
        }

        local queueIndex = 0
        if ability.abilityType == CONSTANTS.ABILITY_SUPPORT then
            table.insert(gm.localMoveSupportQueue, entry)
            queueIndex = #gm.localMoveSupportQueue
        else
            table.insert(gm.localAttackQueue, entry)
            queueIndex = #gm.localAttackQueue
        end

        -- Support ability: play caster animation + animation preview during prep
        if ability.abilityType == CONSTANTS.ABILITY_SUPPORT then
            local PREP_CAST_MAP = {
                [1]  = { unit = CONSTANTS.UNIT.ORACLE,  anim = "Ability1" },
                [2]  = { unit = CONSTANTS.UNIT.ORACLE,  anim = "Ability2" },
                [8]  = { unit = CONSTANTS.UNIT.EYE,     anim = "Ability2" },
                [10] = { unit = CONSTANTS.UNIT.TIRION,  anim = "Ability2" },
                [12] = { unit = CONSTANTS.UNIT.VIZOM,   anim = "Ability2" }
            }
            local PREP_PREVIEW_MAP = {
                [2]  = "TriggerOracle2AnimPreview",
                [8]  = "TriggerEye2AnimPreview",
                [10] = "TriggerTirion2AnimPreview",
                [12] = "TriggerVizom2AnimPreview"
            }

            local castInfo = PREP_CAST_MAP[ability.abilityID]
            if castInfo then
                Event.emitLocal("OnUnitSupportCast", {
                    unitType = castInfo.unit,
                    animName = castInfo.anim
                })
            end

            local previewEvent = PREP_PREVIEW_MAP[ability.abilityID]
            if previewEvent then
                Event.emitLocal(previewEvent, {})
            end
        end

        -- Continue with rest of support ability logic if applicable
        if ability.abilityType == CONSTANTS.ABILITY_SUPPORT then

            -- Oracle_2: activate shield VFX on the target unit during prep preview
            if ability.abilityID == CONSTANTS.ABILITIES.ORACLE_2 and param1 and param1 > 0 then
                Event.emitLocal("ApplyShield", { unitType = param1 })
                -- Also emit shield status UI immediately (5 stacks)
                Event.emitLocal("OnUnitStatusInflicted", {
                    statusID = CONSTANTS.ABILITIES.ORACLE_2,
                    ownerUnit = 0,
                    targetUnit = param1,
                    stackCount = 5,
                    isExpiring = false
                })
            end
        end

        -- For trap abilities, delay camera cut-back so player sees the trap placed
        local isTrap = (ability.abilityID == CONSTANTS.ABILITIES.TIRION_2 or
                        ability.abilityID == CONSTANTS.ABILITIES.VIZOM_2)
        if isTrap then
            -- Spawn trap VFX on opponent grid at darkened/unarmed state
            local opponentGrid = gm:GetOpponentGrid()
            local vfxPrefab = ability.abilityID == CONSTANTS.ABILITIES.TIRION_2
                and "Tirion_Ability2_VFX" or "Vizom_Ability2_VFX"
            local world = Grid.gridToWorld(opponentGrid.gridID, targetX, targetY)
            
            -- Apply tileXCorrection for trap VFX positioning
            local tileXCorrection = {
                [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
                [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
                [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
                [3] = { [0] = 0.19, [1] = 0.15, [2] = 0.12, [3] = 0.00, [4] = 0.12, [5] = -0.15 },
            }
            local xCorrection = tileXCorrection[targetY] and tileXCorrection[targetY][targetX] or 0
            
            local vfx = Prefab.spawn(vfxPrefab, world.x + 0.01 + xCorrection, world.y)
            if vfx then
                -- Show at unarmed state: darkened + lower alpha
                ECS.setMeshRenderer(vfx, {
                    colorMultiply = { r = 0.4, g = 0.4, b = 0.4, a = 0.5 }
                })
                table.insert(self.trapPreviewVFXList, vfx)
            end

            -- Show trap indicator on target tile
            local tileIdx = opponentGrid:GetTileIndex(targetX, targetY)
            if tileIdx > 0 then
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
                Grid.setTileColorAdd(opponentGrid.gridID, row, col, 0, CONSTANTS.COLOR_TRAP_INDICATOR)
            end

            -- Delay camera cut-back
            self.trapPreviewTimer = 1.5
            self.trapPreviewActive = true
        else
            -- Switch camera back to player's grid immediately
            local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
            if isOrder then
                Event.emitLocal("FocusOrderGrid", {})
            else
                Event.emitLocal("FocusChaosGrid", {})
            end
        end

        Event.emitLocal("OnAbilityQueued", {
            abilityID = ability.abilityID,
            unitType = castingUnit,
            queueIndex = queueIndex,
            player = gm.playerFaction,

            -- for AbilityQueueUI.lua playback
            targetX = targetX,
            targetY = targetY,
            param1 = param1,
            param2 = param2
        })

        Event.emitLocal("PlayUIAbilityValid", {})

        Log.info("[PrepPhase] Queued ability " .. ability.abilityID .. " (" .. ability.name ..
            ") in " .. (ability.abilityType == CONSTANTS.ABILITY_SUPPORT and "moveSupportQueue" or "attackQueue") ..
            " at index " .. queueIndex .. " | AP remaining: " .. gm.currentAP)
    end,

    QueueMovement = function(self, unitType, fromX, fromY, toX, toY)
        local gm = ClientGameManager_instance

        gm.movedUnits[unitType] = true

        table.insert(gm.localMoveSupportQueue, {
            actionType = CONSTANTS.ACTION_MOVEMENT,
            abilityID = CONSTANTS.ABILITIES.MOVEMENT,
            unitType = unitType,
            targetX = toX,
            targetY = toY,
            param1 = fromX,
            param2 = fromY
        })

        -- Clear highlights after movement is queued
        Event.emitLocal("ClearHighlights", {})

        Log.info("[PrepPhase] Queued movement for unit " .. unitType ..
            " from (" .. fromX .. "," .. fromY .. ") to (" .. toX .. "," .. toY ..
            ") | moveSupportQueue size: " .. #gm.localMoveSupportQueue)
    end,

    SubmitActions = function(self)
        local gm = ClientGameManager_instance
        if gm.awaitingServer then
            Log.info("[PrepPhase] SubmitActions ignored: already awaiting server")
            return
        end

        Log.info("[PrepPhase] ====== SUBMITTING ACTIONS ====== " ..
            "moveSupportQueue=" .. #gm.localMoveSupportQueue ..
            " attackQueue=" .. #gm.localAttackQueue)

        gm:PassToServer()
        RPC.call("ServerRPC_OnSubmitActions",
            NetworkedGameClient_instance.assignedClientID,
            gm.localMoveSupportQueue,
            gm.localAttackQueue
        )
    end
})

return class
