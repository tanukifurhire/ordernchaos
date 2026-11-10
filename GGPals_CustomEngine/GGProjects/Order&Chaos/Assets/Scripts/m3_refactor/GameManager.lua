local startPhases = {
    -- Deployment start
    [1] = function(self, entity)
        self.planTimer = 0.0

        Log.info("[GameManager] ========================================")
        Log.info("[GameManager] === PLAYER " .. self.currentPlayer ..  " DEPLOYMENT ===")
        Log.info("[GameManager] Place your 3 units on the grid!")
        Log.info("[GameManager] ========================================")
        
        Event.emitLocal("ShowPhaseOverlay", { phase = "deploy" })

        if self.currentPlayer == CONSTANTS.ORDER_PLAYER then
            Event.emitLocal("FocusOrderGrid", {})
        else
            Event.emitLocal("FocusChaosGrid", {})
        end

        Event.emitLocal("TurnChanged", {
            currentPlayer = self.currentPlayer,
            turnNumber = self.turnNumber
        })
    end,

    -- Planning start
    [2] = function(self, entity)
        self.planTimer = 0.0

        Log.info("[GameManager] ========================================")
        Log.info("[GameManager] === PLAYER " .. self.currentPlayer ..  " PLANNING ===")
        Log.info("[GameManager] Use your abilities!")
        Log.info("[GameManager] ========================================")

        Event.emitLocal("InPlanningPhase", {
            player = self.currentPlayer
        })
        Event.emitLocal("ShowPhaseOverlay", { phase = "planphase" })

        if self.currentPlayer == CONSTANTS.ORDER_PLAYER then
            Event.emitLocal("OnAPUpdate", {
                value = self.orderAP
            })
            Event.emitLocal("FocusOrderGrid", {})
        else
            Event.emitLocal("OnAPUpdate", {
                value = self.chaosAP
            })
            Event.emitLocal("FocusChaosGrid", {})
        end

        Event.emitLocal("ShowAP", {})

        Event.emitLocal("TurnChanged", {
            currentPlayer = self.currentPlayer,
            turnNumber = self.turnNumber
        })

        if self.unitSelectedHandler > 0 then Event.unsubscribe(self.unitSelectedHandler) end
        if self.tileSelectedHandler > 0 then Event.unsubscribe(self.tileSelectedHandler) end
        if self.abilitySelectedHandler > 0 then Event.unsubscribe(self.abilitySelectedHandler) end

        self.selectedUnit = 0

        self.unitSelectedHandler = Event.subscribe("OnUnitSelected", function(params)
            Log.info("A Unit was selected! UnitType: " .. params.unitType)

            if self.selectedAbility > 0 then
                Log.info("[GameManager] Ability Cast Detected!")

                if AbilityList[self.selectedAbility] then 
                    AbilityList[self.selectedAbility].Queue(self, entity) 
                else
                    self.selectedAbility = 0
                end

                Event.emitLocal("ClearHighlights", {})
                return
            end

            self.selectedUnit = params.unitType
            self.selectedAbility = 0
            Event.emitLocal("ClearHighlights", {})

            Event.emitLocal("TryMoveUnit", params)
        end)

        self.tileSelectedHandler = Event.subscribe("OnTileSelected", function(params)
            if self.selectedUnit > 0 then
                -- This is a movement command!
                Event.emitLocal("TryMoveCommand", {
                    isOrder = params.isOrder,
                    unitType = self.selectedUnit,
                    gridX = params.gridX,
                    gridY = params.gridY
                })

                self.selectedAbility = 0
                self.selectedUnit = 0
            else
                -- Check if an ability is selected!
                if self.selectedAbility > 0 then
                    Log.info("[GameManager] Ability Cast Detected!")

                    if AbilityList[self.selectedAbility] then
                        AbilityList[self.selectedAbility].Queue(self, entity)
                    else
                        self.selectedAbility = 0
                    end
                end
                self.selectedUnit = 0
                Event.emitLocal("ClearHighlights", {})
            end
        end)

        self.abilitySelectedHandler = Event.subscribe("OnAbilitySelected", function(params)
            local isOrder = IsAbilityOrder(params.ability)

            self.selectedUnit = 0
            Event.emitLocal("ClearHighlights", {})
                
            -- Can't use ability if not your turn!
            if isOrder and self.currentPlayer == CONSTANTS.CHAOS_PLAYER then return end
            if (not isOrder) and self.currentPlayer == CONSTANTS.ORDER_PLAYER then return end
            
            if AbilityList[params.ability] then 
                if AbilityList[params.ability].Selected(self, entity) then
                    self.selectedAbility = params.ability
                end
            else
                self.selectedAbility = 0
            end
        end)
    end,

    -- Resolution end
    [3] = function(self, entity)
        -- Resolution doesn't have fixed Order/Chaos so currentPlayer doesn't matter here
        self.planTimer = 0.0

        Log.info("[GameManager] ========================================")
        Log.info("[GameManager] === RESOLUTION PHASE ===")
        Log.info("[GameManager] ========================================")

        Event.emitLocal("InResolutionPhase", {})
        Event.emitLocal("ShowPhaseOverlay", { phase = "resphase" })
        Event.emitLocal("OnResolveAbilities", {})
    end
}

local endPhases = {
    -- Deployment end
    [1] = function(self, entity)
        Log.info("[GameManager] ========================================")
        Log.info("[GameManager] === START OF NEW TURN ===")
        Log.info("[GameManager] ========================================")

        self.orderAP = CONSTANTS.MAX_AP
        self.chaosAP = CONSTANTS.MAX_AP
    end,

    -- Planning end
    [2] = function(self, entity)
        Log.info("[GameManager] ========================================")
        Log.info("[GameManager] === END OF PLANNING PHASE ===")
        Log.info("[GameManager] ========================================")

        if self.unitSelectedHandler > 0 then Event.unsubscribe(self.unitSelectedHandler) end
        if self.tileSelectedHandler > 0 then Event.unsubscribe(self.tileSelectedHandler) end
        if self.abilitySelectedHandler > 0 then Event.unsubscribe(self.abilitySelectedHandler) end

        Event.emitLocal("OnEndPlanningPhase", {})
        Event.emitLocal("HideAP", {})
    end,

    -- Resolution end
    [3] = function(self, entity)
        Log.info("[GameManager] ========================================")
        Log.info("[GameManager] === END OF RESOLUTION PHASE ===")
        Log.info("[GameManager] ========================================")
    end
}

local class = DefineType("GameManager",
{
    Serializable = 
    {
        -- Order starts first
        startingPlayer = 1,
        resolutionTime = 2.0
    },

    currentPhase = 0,
    currentPlayer = 1,
    turnNumber = 1,

    -- Planning Phase handlers
    unitSelectedHandler = 0,
    tileSelectedHandler = 0,
    abilitySelectedHandler = 0,
    selectedUnit = 0,
    selectedAbility = 0,

    -- Persistent event handlers (need cleanup)
    returnMainMenuHandler = 0,
    changePhaseHandler = 0,
    endActionButtonHandler = 0,
    unitDeployedHandler = 0,
    takeDamageHandler = 0,
    gainHealthHandler = 0,

    orderHealth = 0,
    orderMaxHealth = 20,
    chaosHealth = 0,
    chaosMaxHealth = 20,

    orderAP = 6,
    chaosAP = 6,

    -- We declare as 0.000001 for floating points because compiler needs to register value
    planTimer = 0.000001,

    Init = function(self, entity)
        -- Clean up existing event subscriptions first
        if self.returnMainMenuHandler > 0 then Event.unsubscribe(self.returnMainMenuHandler) end
        if self.changePhaseHandler > 0 then Event.unsubscribe(self.changePhaseHandler) end
        if self.endActionButtonHandler > 0 then Event.unsubscribe(self.endActionButtonHandler) end
        if self.unitDeployedHandler > 0 then Event.unsubscribe(self.unitDeployedHandler) end
        if self.takeDamageHandler > 0 then Event.unsubscribe(self.takeDamageHandler) end
        if self.gainHealthHandler > 0 then Event.unsubscribe(self.gainHealthHandler) end

        -- Set current phase to 0 so when invoking OnPhaseChange it will increment to
        -- Deployment Phase.
        self.currentPhase = CONSTANTS.PHASE.PHASE_NONE
        self.currentPlayer = self.startingPlayer

        self.orderHealth = self.orderMaxHealth
        self.chaosHealth = self.chaosMaxHealth

        -- Zero out floating point num
        self.planTimer = 0.0

        -- Handle Return Main Menu Logic
        self.returnMainMenuHandler = Event.subscribe("OnReturnMainMenu", function(params)
            Scene.setActive("MainMenuScene")
        end)

        -- Main game loop manager
        self.changePhaseHandler = Event.subscribe("OnChangePhase", function(params)
            -- Get previous phase
            local exitPhase = endPhases[self.currentPhase]

            -- Phase exit logic
            if exitPhase then
                exitPhase(self, entity)
            end
            
            self.currentPhase = self.currentPhase + 1

            -- Get new phase
            local enterPhase = startPhases[self.currentPhase]

            if enterPhase then
                enterPhase(self, entity)
            else
                Log.info("[GameManager] Ending Turn " .. self.turnNumber .. "!")
                local winner = GameManager.WinconCheck(self, entity)

                if winner == 0 then
                    self.turnNumber = self.turnNumber + 1
                    -- Set currentPhase to 0, it will increment to 1 next turn
                    self.currentPhase = CONSTANTS.PHASE.DEPLOYMENT
                    -- Recursively invoke the same event
                    Event.emitLocal("OnChangePhase", {})
                else
                    if winner == CONSTANTS.ORDER_PLAYER then
                        Log.info("[GameManager] === ORDER WINS ===")
                        Event.emitLocal("ShowPhaseOverlay", {
                            phase = "OrderWin"
                        })
                        Event.emitLocal("OnPassAndPlayContinue", {})
                    elseif winner == CONSTANTS.CHAOS_PLAYER then
                        Log.info("[GameManager] === CHAOS WINS ===")
                        Event.emitLocal("ShowPhaseOverlay", {
                            phase = "ChaosWin"
                        })
                        Event.emitLocal("OnPassAndPlayContinue", {})
                    else
                        Log.info("[GameManager] === TIE ===")
                        Event.emitLocal("ShowPhaseOverlay", {
                            phase = "ChaosWin"
                        })
                        Event.emitLocal("OnPassAndPlayContinue", {})
                    end
                end
            end
        end)

        -- Swapping between Order and Chaos interaction for EndTurn button
        self.endActionButtonHandler = Event.subscribe("OnEndActionButtonPressed", function(params)
            if self.currentPhase == CONSTANTS.PHASE.RESOLUTION then
                Log.info("[GameManager] Cannot End Turn in Resolution Phase!")
                Event.emitLocal("OnChangePhase", {})
                return
            end

            -- Tutorial: skip the Chaos player's turn entirely for both deployment and planning.
            -- TutorialManager handles scene navigation when the tutorial is complete.
            if Persistent.isTutorial then
                if self.currentPhase == CONSTANTS.PHASE.DEPLOYMENT then
                    -- Validate all Order units are deployed before skipping
                    local bit = require("bit")
                    local grid = Grids_instance.OrderGrid
                    if not grid then return end
                    local flags = grid.unitFlags
                    local allUnitsFlags = bit.bor(
                        bit.lshift(1, CONSTANTS.UNIT.DRAGOON),
                        bit.lshift(1, CONSTANTS.UNIT.ORACLE),
                        bit.lshift(1, CONSTANTS.UNIT.TIRION)
                    )
                    if bit.band(flags, allUnitsFlags) ~= allUnitsFlags then
                        Log.info("[GameManager] Tutorial: not all units deployed!")
                        return
                    end
                    Log.info("[GameManager] Tutorial: skipping Chaos deployment, advancing to planning.")
                    self.currentPlayer = self.startingPlayer
                    Event.emitLocal("OnChangePhase", {})
                else
                    -- In planning phase during tutorial, TutorialManager controls flow.
                    Log.info("[GameManager] Tutorial: End Turn blocked in planning - TutorialManager controls.")
                end
                return
            end

            -- Normal (non-tutorial) flow below
            if self.currentPhase == CONSTANTS.PHASE.DEPLOYMENT then
                local session = SessionManager_instance.sessionID
                local isOrder = self.currentPlayer == CONSTANTS.ORDER_PLAYER

                local bit = require("bit")

                local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
                if not grid then return end

                local flags = grid.unitFlags
                    local allUnitsFlags = isOrder
                        and bit.bor(bit.lshift(1, CONSTANTS.UNIT.DRAGOON), 
                                    bit.lshift(1, CONSTANTS.UNIT.ORACLE), 
                                    bit.lshift(1, CONSTANTS.UNIT.TIRION))
                        or bit.bor(bit.lshift(1, CONSTANTS.UNIT.EYE), 
                                bit.lshift(1, CONSTANTS.UNIT.IMP), 
                                bit.lshift(1, CONSTANTS.UNIT.VIZOM))

                if bit.band(flags, allUnitsFlags) == allUnitsFlags then
                    Log.info("[GameManager] All units deployed! We may proceed")
                else
                    Log.info("[GameManager] Not all units deployed!")
                    return
                end
            end

            if self.currentPlayer == self.startingPlayer then
                -- Other player hasn't played yet
                -- Swap between Order and Chaos player
                self.currentPlayer = (self.currentPlayer == CONSTANTS.ORDER_PLAYER) 
                    and CONSTANTS.CHAOS_PLAYER or CONSTANTS.ORDER_PLAYER 

                -- Get current phase handler again but handle for next player
                local enterPhase = startPhases[self.currentPhase]
                if enterPhase then enterPhase(self, entity) end
            else
                -- Both players have played, we can end the current phase now
                self.currentPlayer = self.startingPlayer
                Event.emitLocal("OnChangePhase", {})
            end
        end)

        self.unitDeployedHandler = Event.subscribe("OnUnitDeployed", function(params)
            local bit = require("bit")

            local grid = params.isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
            if grid then
                grid.unitFlags = bit.bor(grid.unitFlags, bit.lshift(1, params.unitType))
            end
        end)

        self.takeDamageHandler = Event.subscribe("TakeDamage", function(params)
            if params.player == CONSTANTS.ORDER_PLAYER then
                self.orderHealth = math.max(0, self.orderHealth - params.damage)
                Event.emitLocal("OnHealthChanged", {
                    player = CONSTANTS.ORDER_PLAYER,
                    curr = self.orderHealth,
                    max = self.orderMaxHealth
                })
            else
                self.chaosHealth = math.max(0, self.chaosHealth - params.damage)
                Event.emitLocal("OnHealthChanged", {
                    player = CONSTANTS.CHAOS_PLAYER,
                    curr = self.chaosHealth,
                    max = self.chaosMaxHealth
                })
            end

            Log.info("[GameManager] Player " .. params.player .. " has taken " .. params.damage .. " damage.")
        end)

        self.gainHealthHandler = Event.subscribe("GainHealth", function(params)
            if params.player == CONSTANTS.ORDER_PLAYER then
                self.orderHealth = math.max(0, self.orderHealth + params.healthAmount)
            else
                self.chaosHealth = math.max(0, self.chaosHealth + params.healthAmount)
            end

            Log.info("[GameManager] Player " .. params.player .. " has gained " .. params.healthAmount .. " health.")
        end)
    end,

    Start = function(self, entity)
        Event.emitLocal("OnHealthChanged", {
            player = CONSTANTS.ORDER_PLAYER,
            curr = self.orderHealth,
            max = self.orderMaxHealth
        })
        Event.emitLocal("OnHealthChanged", {
            player = CONSTANTS.CHAOS_PLAYER,
            curr = self.chaosHealth,
            max = self.chaosMaxHealth
        })

        -- Tutorial: wait for TutorialManager to give the go-ahead before starting.
        -- TutorialManager emits OnTutorialGameStart after the intro overlays are dismissed.
        if Persistent.isTutorial then
            Event.subscribe("OnTutorialGameStart", function()
                Event.emitLocal("OnChangePhase", {})
                Event.emitLocal("PlayBGMCombat", {})
            end)
        else
            Event.emitLocal("OnChangePhase", {})
            Event.emitLocal("PlayBGMCombat", {})
        end
    end,

    Update = function(self, entity)
        -- E key shortcut disabled in tutorial to avoid bypassing steps
        if not Persistent.isTutorial then
            if Input.isKeyTriggered(69) then
                Log.info("[GameManager] ===== E PRESSED - ENDING ACTION =====")
                Event.emitLocal("OnEndActionButtonPressed", {})
            end
        end

        if self.selectedAbility > 0 then
            if AbilityList[self.selectedAbility] then 
                AbilityList[self.selectedAbility].Update(self, entity) 
            else
                self.selectedAbility = 0
            end
        end
    end,

    WinconCheck = function(self, entity)
        if self.orderHealth <= 0 and self.chaosHealth > 0 then
            return CONSTANTS.CHAOS_PLAYER
        elseif self.chaosHealth <= 0 and self.orderHealth > 0 then
            return CONSTANTS.ORDER_PLAYER
        elseif self.chaosHealth == 0 and self.orderHealth == 0 then
            -- Tie
            return 3
        else
            return 0
        end
    end
})

return class
