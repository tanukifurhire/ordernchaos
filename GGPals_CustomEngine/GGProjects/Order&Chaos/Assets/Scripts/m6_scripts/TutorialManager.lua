Persistent.isTutorial = true

local class = DefineType("TutorialManager", {
    Serializable = {
        -- Match these to the abilityType set on the relevant AbilityButton entities
        tutorialAbility1 = 1,
        tutorialAbility3 = 3,
    },

    currentStep   = 0,
    currentPlayer = 0,
    deployedCount = 0,
    autoTimer     = -1,
    pendingStep   = 0,
    gameStarted   = false,

    Init = function(self, entity)
        Persistent.isTutorial = true
        Persistent.isTutorialTimer = true
        self.currentStep   = 0
        self.currentPlayer = CONSTANTS.ORDER_PLAYER
        self.deployedCount = 0
        self.autoTimer     = -1
        self.pendingStep   = 0
        self.gameStarted   = false

        Event.subscribe("TurnChanged", function(params)
            self.currentPlayer = params.currentPlayer
        end)

        Event.subscribe("OnTutorialOverlayContinue", function()
            TutorialManager.OnOverlayContinue(self, entity)
        end)

        -- PassAndPlay screen visible after Order ends deployment turn
        Event.subscribe("OnPassAndPlayDisplayed", function()
            if self.currentStep == 9 then
                TutorialManager.GoToStep(self, entity, 10)
            end
        end)

        -- Player presses Continue on PassAndPlay screen
        Event.subscribe("OnPassAndPlayContinue", function()
            if self.currentStep == 10 then
                TutorialManager.GoToStep(self, entity, 11)
            elseif self.currentStep == 12 then
                TutorialManager.GoToStep(self, entity, 13)
            end
        end)

        -- Order planning phase starts (fires synchronously inside Chaos EndTurn chain)
        Event.subscribe("InPlanningPhase", function(params)
            if params.player == CONSTANTS.ORDER_PLAYER then
                if self.currentStep == 11 or self.currentStep == 12 then
                    TutorialManager.GoToStep(self, entity, 12)
                end
            end
        end)

        -- Unit deployed on Order grid
        Event.subscribe("OnUnitDeployed", function(params)
            if not params.isOrder then return end
            if self.currentStep == 6 then
                if params.unitType == CONSTANTS.UNIT.ORACLE then
                    TutorialManager.GoToStep(self, entity, 7)
                end
            elseif self.currentStep == 8 then
                self.deployedCount = self.deployedCount + 1
                Log.info("[TutorialManager] Deploy count: " .. self.deployedCount .. "/3")
                if self.deployedCount >= 3 then
                    TutorialManager.GoToStep(self, entity, 9)
                end
            end
        end)

        -- EndTurn pressed
        Event.subscribe("OnEndActionButtonPressed", function()
            if self.currentStep == 11 and self.currentPlayer == CONSTANTS.CHAOS_PLAYER then
                TutorialManager.GoToStep(self, entity, 12)
            end
        end)

        -- Ability selected
        Event.subscribe("OnAbilitySelected", function(params)
            if self.currentStep == 19 and params.ability == self.tutorialAbility3 then
                TutorialManager.GoToStep(self, entity, 20)
            elseif self.currentStep == 24 and params.ability == self.tutorialAbility1 then
                TutorialManager.GoToStep(self, entity, 25)
            end
        end)

        Event.subscribe("Tutorial_Set_Unit_Move_Targets", function(params)
            TutorialAllowedTiles = {}

            for _, tile in ipairs(params.tiles) do
                local key = tile.x .. "," .. tile.y
                TutorialAllowedTiles[key] = true
            end
        end)

        Event.subscribe("Tutorial_Clear_Unit_Move_Targets", function()
            TutorialAllowedTiles = {}
        end)
    end,

    Start = function(self, entity)
        TutorialManager.GoToStep(self, entity, 1)
        Event.emitLocal("TutorialTimer", {})
    end,

    Update = function(self, entity)
        -- Allow skipping tutorial with right-click
        if Input.isMouseButtonTriggered(MOUSE_RIGHT) then
            TutorialManager.GoToStep(self, entity, 28)
            return
        end

        -- Handle ability hotkeys during tutorial
        if Input.isKeyTriggered(KEY_1) then
            Event.emitLocal("OnAbilityHotkey", { slot = 1, player = self.currentPlayer })
        elseif Input.isKeyTriggered(KEY_3) then
            Event.emitLocal("OnAbilityHotkey", { slot = 3, player = self.currentPlayer })
        end

        if self.autoTimer < 0 then return end
        self.autoTimer = self.autoTimer - 1
        if self.autoTimer == 0 then
            self.autoTimer = -1
            local next = self.pendingStep
            self.pendingStep = 0
            if next > 0 then
                TutorialManager.GoToStep(self, entity, next)
            end
        end
    end,

    OnOverlayContinue = function(self, entity)
        if self.currentStep == 3 then
            if self.gameStarted then return end
            self.gameStarted = true
            Event.emitLocal("OnTutorialHideOverlay", {})
            Event.emitLocal("OnTutorialGameStart", {})
            self.pendingStep = 4
            self.autoTimer   = 60
            return
        end
        local clickAdvance = {
            [1]=true, [2]=true,
            [4]=true, [5]=true, [7]=true,
            [10]=true,
            [13]=true, [14]=true,
            [15]=true, [16]=true, [17]=true, [18]=true,
            [21]=true, [22]=true,
            [26]=true, [27]=true
        }
        if clickAdvance[self.currentStep] then
            TutorialManager.GoToStep(self, entity, self.currentStep + 1)
        end
    end,

    -- ================================================================
    -- STEP DRIVER
    -- ================================================================

    GoToStep = function(self, entity, step)
        self.currentStep        = step
        Persistent.tutorialStep = step
        Log.info("[TutorialManager] === Step " .. step .. " ===")

        Event.emitLocal("Darken_All", {})

        -- Emit OnTutorialStepChanged with spotlight flags.
        -- Each entity script with isTutorial=true subscribes and
        -- spotlights/restores itself based on its own flag.
        local function uiState(endTurnEnabled, allowedAbility, s)
            s = s or {}
            Event.emitLocal("OnTutorialStepChanged", {
                step                     = step,
                endTurnEnabled           = endTurnEnabled,
                endTurnBright            = endTurnEnabled,
                allowedAbility           = allowedAbility or 0,
                -- Pedestal spotlight flags (AutoHidePedestals reads these)
                spotlightOraclePedestal  = s.oraclePedestal  or false,
                spotlightDragoonPedestal = s.dragoonPedestal or false,
                spotlightTirionPedestal  = s.tirionPedestal  or false,
                -- Grid spotlight (GridManager reads this)
                spotlightGrid            = s.grid            or false,
                -- AP Crystal spotlight (APCrystal reads this)
                spotlightCrystals        = s.crystals        or false,
                -- Ability spotlight (AbilityButton reads these against its abilityType)
                spotlightAbilityA        = s.abilityA        or 0,
                spotlightAbilityB        = s.abilityB        or 0,
                -- EndTurn spotlight (EndTurnButton reads this)
                spotlightEndTurn         = s.endTurn         or false,
            })
        end

        local function show(text, clickAdvanceable, showBackground, textOffsetY, overlayScaleX, overlayScaleY, overlayOffsetX, overlayOffsetY, textOffsetX)
            Event.emitLocal("OnTutorialShowOverlay", {
                text             = text,
                clickAdvanceable = (clickAdvanceable ~= false),
                showBackground   = (showBackground ~= false),
                textOffsetY      = textOffsetY or 0,
                overlayScaleX    = overlayScaleX or 1.0,
                overlayScaleY    = overlayScaleY or 1.0,
                overlayOffsetX   = overlayOffsetX or 0.0,
                overlayOffsetY   = overlayOffsetY or 0.0,
                textOffsetX      = textOffsetX or 0.0
            })
        end
        local function hide()
            Event.emitLocal("OnTutorialHideOverlay", {})
        end

        local function darkenTile(gridX, gridY)
            Event.emitLocal("Darken_Tile", { gridX = gridX, gridY = gridY })
        end

        local function lightenTile(gridX, gridY)
            Event.emitLocal("Lighten_Tile", { gridX = gridX, gridY = gridY })
        end

        local function lightenOraclePedestal()
            Event.emitLocal("Lighten_UI_OraclePedestal", {})
        end

        local function lightenDragoonPedestal()
            Event.emitLocal("Lighten_UI_DragoonPedestal", {})
        end

        local function lightenTirionPedestal()
            Event.emitLocal("Lighten_UI_TirionPedestal", {})
        end

        local function lightenEyePedestal()
            Event.emitLocal("Lighten_UI_EyePedestal", {})
        end

        local function lightenVizomPedestal()
            Event.emitLocal("Lighten_UI_VizomPedestal", {})
        end

        local function lightenImpPedestal()
            Event.emitLocal("Lighten_UI_ImpPedestal", {})
        end

        local function disablePlacementTile(gridX, gridY)
            Event.emitLocal("Disable_Placement_Tile", { gridX = gridX, gridY = gridY })
        end

        local function enablePlacementTile(gridX, gridY)
            Event.emitLocal("Enable_Placement_Tile", { gridX = gridX, gridY = gridY })
        end

        local function clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    lightenTile(col, row)
                end
            end
        end

        local function disableAllPlacement()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    disablePlacementTile(col, row)
                end
            end
        end

        local function enableAllPlacement()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    enablePlacementTile(col, row)
                end
            end
        end

        local function darkenUnit(unitType)
            Event.emitLocal("Darken_Unit", { unitType = unitType })
        end

        local function lightenUnit(unitType)
            Event.emitLocal("Lighten_Unit", { unitType = unitType })
        end

        local function darkenAllUnits()
            for unitType = 1, 6 do
                darkenUnit(unitType)
            end
        end

        local function lightenAllUnits()
            for unitType = 1, 6 do
                lightenUnit(unitType)
            end
        end

        local function setAllowedMovementTiles(unitType, tiles)
            Event.emitLocal("Tutorial_Set_Unit_Move_Targets", {
                unitType = unitType or 0,
                tiles = tiles or {}
            })
        end

        local function clearAllowedMovementTargets()
            Event.emitLocal("Tutorial_Clear_Unit_Move_Targets", {})
        end

        -- -------------------------------------------------------
        -- PRE-GAME
        -- -------------------------------------------------------

        if step == 1 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            uiState(false, 0, { grid = false })
            show("Welcome to Order N Chaos!\n\n"..
                 "In this two-player strategy game,\n"..
                 "one of you will command Order\n"..
                 "while the other will control Chaos.",
                 true, true, 0, 0, 0, 0, 0)

        elseif step == 2 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            uiState(false, 0, { grid = false })
            show("From the Main Menu, after choosing your side,\n"..
                 "you can select a game mode.\n\n"..
                 "Pass And Play, Play together on the same device\n"..
                 "with your opponent sitting beside you.\n\n"..
                 "Online Multiplayer, Create or join rooms\n"..
                 "to battle other players online.\n\n",
                 true, true, -20, 0, 0, 0, 0)

        elseif step == 3 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            uiState(false, 0)
            -- Clicking emits OnTutorialGameStart (see OnOverlayContinue)
            show("Order N Chaos is a game full of tactics\n"..
                 "and bombardment.\n\n"..
                 "Launch attacks into your opponent's grid to\n"..
                 "destroy their tiles and damage their units.\n\n"..
                 "Order commands elemental magic with a control playstyle.\n"..
                 "Chaos thrive on evil scheme with gimmick abilities.\n\n",
                 true, true, -20, 0, 0, 0, 0)

        -- -------------------------------------------------------
        -- ORDER DEPLOYMENT
        -- -------------------------------------------------------

        elseif step == 4 then
            lightenAllUnits()
            lightenOraclePedestal()
            lightenDragoonPedestal()
            lightenTirionPedestal()
            Event.emitLocal("Lighten_ClientUnit", {})
            Event.emitLocal("Lighten_GAME_Grid", {})
            -- ensure grid is tutorial-dimmed early for step 4/5
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            -- SECTION 5: Order units — spotlight all three pedestals
            uiState(false, 0, {
                oraclePedestal  = true,
                dragoonPedestal = true,
                tirionPedestal  = true,
                grid = false
            })
            show("Each player commands three units.\n\n"..
                 "These units move across the grid\n"..
                 "and cast powerful abilities.",
                 true, true, 0, 0, 0, 0, 0)

        elseif step == 5 then
            darkenAllUnits()
            -- SECTION 6: Grid — spotlight Order grid 
            clearAllTutorialTileDarken()
            uiState(false, 0, { grid = true })
            show("This is your grid.\n\n"..
                 "Each player's grid has 12 red tiles and 12 blue tiles —\n"..
                 "24 tiles in total.\n\n"..
                 "Your positioning on these tiles is key\n"..
                 "to using your abilities effectively.",
                 true, true, 0, 0, 0, 470, 210)

        elseif step == 6 then
            darkenAllUnits()
            lightenUnit(CONSTANTS.UNIT.ORACLE)
            Event.emitLocal("Lighten_ClientUnit", {})
            lightenOraclePedestal()
            clearAllTutorialTileDarken()
            -- darken all grid tiles as a base and highlight one tile for placement focus
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            lightenTile(0, 3)  -- recommended Oracle start position

            disableAllPlacement()
            enablePlacementTile(0, 3)  -- only allow placement on recommended tile for tutorial focus
            -- SECTION 7: Deploy Oracle — spotlight Oracle pedestal + grid
            self.deployedCount = 0
            uiState(false, 0, { oraclePedestal = true, grid = true })
            show("At the start of the game you can deploy\n"..
                 "your units anywhere on the grid.\n\n"..
                 "Left click and drag the top unit, the Oracle,\n"..
                 "the red tile on your grid.", false,
                 true, 0, 0, 0, 470, 210)

        elseif step == 7 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            disableAllPlacement()
            -- SECTION 8: Ability colour — spotlight grid + crystals + ability3
            uiState(false, 0, {
                grid     = true,
                crystals = true,
                abilityA = self.tutorialAbility3
            })
            show("Each unit stands on a red or blue tile.\n\n"..
                 "These tiles contribute to your ability colour requirements.\n\n"..
                 "As your units move, colour requirements update\n"..
                 "based on where they collectively stand.\n\n"..
                 "Standing on one red tile lights up abilities\n"..
                 "that require them.",
                 true, true, -20, 0, 0, 470, 210)

        elseif step == 8 then
            darkenAllUnits()
            lightenUnit(CONSTANTS.UNIT.DRAGOON)
            lightenUnit(CONSTANTS.UNIT.TIRION)

            clearAllTutorialTileDarken()
            -- darken all grid tiles as a base and highlight one tile for placement focus
            Event.emitLocal("Lighten_GAME_Grid", {})
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            lightenTile(1, 0) 
            lightenTile(3, 0)

            disableAllPlacement()
            enablePlacementTile(1, 0)
            enablePlacementTile(3, 0)  -- only allow placement on two recommended tiles

            lightenDragoonPedestal()
            lightenTirionPedestal()
            -- SECTION 9: Deploy remaining — spotlight Dragoon+Tirion+grid+both abilities
            self.deployedCount = 1  -- Oracle already placed at step 6
            uiState(false, 0, {
                dragoonPedestal = true,
                tirionPedestal  = true,
                grid            = true,
                abilityA        = self.tutorialAbility1,
                abilityB        = self.tutorialAbility3
            })
            show("Now deploy your two remaining units\n"..
                 "to the highlighted tiles.\n\n"..
                 "Watch how the colour requirements light up\n"..
                 "as you place each unit.\n\n"..
                 "As long as your units collectively stand on\n"..
                 "the required colours, you can cast the ability.", false,
                 true, -60, 0, 0, 470, 210)

        elseif step == 9 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            -- SECTION 10: End Turn — spotlight EndTurn, unfreeze timer
            uiState(true, 0, { endTurn = true })
            Event.emitLocal("OnTutorialEnableTimer", {})
            show("During a normal game the End Turn button\n"..
                 "has a circular timer.\n\n"..
                 "If the bar empties the turn ends automatically\n"..
                 "and undeployed units are placed for you.\n\n"..
                 "Press End Turn when you are ready!", false,
                 true, 0, 0, 0, 470, 210)

        -- -------------------------------------------------------
        -- PASS AND PLAY INTRO
        -- -------------------------------------------------------

        elseif step == 10 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end

            disableAllPlacement()

            uiState(false, 0)
            -- showBackground=false: PnP screen is already covering the screen
            show("In Pass And Play this screen appears each turn.\n\n"..
                 "Pass the device to the next player and\n"..
                 "do not look during your opponent's turn.\n\n"..
                 "For this tutorial, both players may watch.\n\n"..
                 "In real matches please play fair!",
                 true, true, 0, 0, 0, 470, 210)

        -- -------------------------------------------------------
        -- CHAOS DEPLOYMENT
        -- -------------------------------------------------------

        elseif step == 11 then
            lightenEyePedestal()
            lightenVizomPedestal()
            lightenImpPedestal()
            lightenAllUnits()
            clearAllTutorialTileDarken()
            enableAllPlacement()
            uiState(true, 0)
            show("Chaos player!\n\n"..
                 "Deploy your three units in whichever way you like.\n\n"..
                 "Notice how your ability colour requirements\n"..
                 "light up as you place each unit.\n\n"..
                 "Once all three units are deployed,\n"..
                 "press End Turn.", false,
                 true, -20, 0, 0, -470, 210)

        elseif step == 12 then
            -- Waiting for Order to press Continue on PassAndPlay
            enableAllPlacement()
            hide()
            uiState(false, 0)

        -- -------------------------------------------------------
        -- ORDER PLANNING
        -- -------------------------------------------------------

        elseif step == 13 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            enableAllPlacement()
            Event.emitLocal("Lighten_UI_AbilityButton", {})
            -- SECTION 12: Ability overview — crystals + both abilities
            uiState(false, 0, {
                crystals = true,
                abilityA = self.tutorialAbility1,
                abilityB = self.tutorialAbility3,
            })
            show("During the planning phase you can either\n"..
                 "cast abilities or move your units.\n\n"..
                 "Each of your units can bring two abilities,\n"..
                 "chosen before the game starts.",
                 true, true, 0, 0, 0, -470, -130)

        elseif step == 14 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            -- SECTION 13: Mana crystals — crystals + ability3
            uiState(false, 0, {
                crystals = true,
                abilityA = self.tutorialAbility3
            })
            show("When your turn starts your six mana crystals\n"..
                 "are refreshed.\n\n"..
                 "These crystals are spent to cast abilities.\n"..
                 "Each ability costs crystals equal to its segment count.",
                 true, true, 0, 0, 0, -470, -130)

        -- -------------------------------------------------------
        -- GAMEPLAY INTRODUCTION
        -- -------------------------------------------------------

        elseif step == 15 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            uiState(false, 0)
            show("Order N Chaos is full of strategic mind games.\n\n"..
                 "Predict your opponent's moves while unleashing\n"..
                 "a variety of abilities to outsmart them!",
                 true, true, 0, 0, 0, 0, 0)

        elseif step == 16 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            disableAllPlacement()
            uiState(false, 0)
            show("Hello Order player!\n\n"..
                 "I will now teach you how to play the game.",
                 true, true, 0, 0, 0, 0, 0)

        elseif step == 17 then
            lightenAllUnits()
            clearAllTutorialTileDarken()
            disableAllPlacement()
            uiState(false, 0)
            show("Each of your vessels stands on red or blue tiles.\n\n"..
                 "You can cast abilities by matching tile colours\n"..
                 "with ability colour requirements.",
                 true, true, 0, 0, 0, 470, 210)

        elseif step == 18 then
            lightenAllUnits()
            clearAllTutorialTileDarken()
            -- darken all grid tiles as a base and highlight one tile for placement focus
            Event.emitLocal("Lighten_GAME_Grid", {})
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            lightenTile(1, 0) 
            lightenTile(3, 0)
            lightenTile(0, 3)
            disableAllPlacement()
            uiState(false, 0)
            show("Right now all your vessels are on red tiles.\n\n"..
                 "This lets you cast the third ability\n"..
                 "which requires 1 red tiles.\n\n"..
                 "You consume mana crystals equal to the segment count.",
                 true, true, 0, 0, 0, 470, 210)

        elseif step == 19 then
            lightenAllUnits()
            clearAllTutorialTileDarken()
            disableAllPlacement()
            Event.emitLocal("Lighten_UI_AbilityButton", {})
            -- Try ability 3 — spotlight ability3
            uiState(false, self.tutorialAbility3, { abilityA = self.tutorialAbility3 })
            show("Try casting the highlighted third ability!\n\n"..
                 "Left click it, or press '3' on your keyboard.\n\n"..
                 "The camera will pan to the opponent's grid.", false,
                 true, 0, 0, 0, 470, 210)

        elseif step == 20 then
            lightenAllUnits()
            clearAllTutorialTileDarken()
            enableAllPlacement()
            -- Let player click tiles; auto-advance 5s
            uiState(false, self.tutorialAbility3)
            show("Please left click on the tiles you would\n"..
                 "like to fire the projectile at.\n"..
                 "(Please Wait After Done)", false,
                 true, 0, 0, 0, 470, 210)
            self.pendingStep = 21
            self.autoTimer   = 300

        elseif step == 21 then
            lightenAllUnits()
            clearAllTutorialTileDarken()
            enableAllPlacement()
            uiState(false, 0)
            show("Abilities have a target indicator showing\n"..
                 "which tiles you can affect.\n\n"..
                 "If a projectile hits a tile, the tile cracks and takes damage.\n"..
                 "If it hits a vessel, the vessel takes damage\n"..
                 "and the tile below is spared.",
                 true, true, -10, 0, 0, 470, 210)

        elseif step == 22 then
            lightenAllUnits()
            uiState(false, 0)
            clearAllTutorialTileDarken()
            -- darken all grid tiles as a base and highlight one tile for placement focus
            Event.emitLocal("Lighten_GAME_Grid", {})
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            lightenTile(2, 0) 
            lightenTile(4, 0)
            lightenTile(1, 3)

            disableAllPlacement()
            enablePlacementTile(2, 0)
            enablePlacementTile(4, 0)
            enablePlacementTile(1, 3)  -- only allow placement on three blue tiles for tutorial focus
            setAllowedMovementTiles(0, {
                { x = 2, y = 0 },
                { x = 4, y = 0 },
                { x = 1, y = 3 }
            })

            show("Your first ability requires three blue tiles.\n\n"..
                 "You can move each vessel once per turn\n"..
                 "to reach the tile colours you need.\n\n"..
                 "Left click a vessel to see movement options,\n"..
                 "then left click a tile to move it.\n\n",
                 true, true, -20, 0, 0, 470, 210)

        elseif step == 23 then
            -- Restrict movement to the three blue target tiles for all units during this tutorial step
            setAllowedMovementTiles(0, {
                { x = 2, y = 0 },
                { x = 4, y = 0 },
                { x = 1, y = 3 }
            })
            -- Free movement window; auto-advance after 10s
            uiState(false, 0)
            self.pendingStep = 24
            self.autoTimer   = 600

            show("Your first ability requires three blue tiles.\n\n"..
                 "You can move each vessel once per turn\n"..
                 "to reach the tile colours you need.\n\n"..
                 "Left click a vessel to see movement options,\n"..
                 "then left click a tile to move it.\n\n"..
                 "Please move your vessels to blue tiles!\n",
                 true, true, -20, 0, 0, 470, 210)

        elseif step == 24 then
            -- Clear tutorial movement restriction after the forced move window
            clearAllowedMovementTargets()
            lightenAllUnits()
            clearAllTutorialTileDarken()
            enableAllPlacement()
            Event.emitLocal("Lighten_UI_AbilityButton", {})
            -- Try ability 1 — spotlight ability1
            uiState(false, self.tutorialAbility1, { abilityA = self.tutorialAbility1 })
            show("Now try the highlighted first ability!\n\n"..
                 "Left click it, or press '1' on your keyboard.\n\n"..
                 "This ability fires automatically at random tiles\n"..
                 "and does not need a target.", false,
                 true, 0, 0, 0, 470, 210)

        elseif step == 25 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            enableAllPlacement()
            uiState(false, self.tutorialAbility1)
            show("Well done!\n\n"..
                 "(Please Wait After Done)", false)
            self.pendingStep = 26
            self.autoTimer   = 180

        elseif step == 26 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            uiState(false, 0)
            show("This game is simultaneous turn-based.\n\n"..
                 "Both players plan their actions before\n"..
                 "they resolve together.\n\n"..
                 "As you are playing side by side, take turns\n"..
                 "looking at the screen according to the prompts.\n\n"..
                 "You can cast abilities up to the number of\n"..
                 "crystals you have available each turn.",
                 true, true, -40, 0, 0, 0, 0)

        elseif step == 27 then
            darkenAllUnits()
            clearAllTutorialTileDarken()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    darkenTile(col, row)
                end
            end
            uiState(true, -1)   -- all UI unlocked
            show("Well done! You have completed the tutorial.\n\n"..
                 "You are now ready to play Order N Chaos!\n\n"..
                 "Click anywhere to return to the main menu.")
            Persistent.isTutorial   = false
            Persistent.isTutorialTimer = false

        elseif step == 28 then
            Persistent.isTutorial   = false
            Persistent.isTutorialTimer = false
            Persistent.tutorialStep = 0
            Event.emitLocal("Transitionfade",{})
            Event.subscribe("TransitionComplete",function()
            Scene.setActive("M5MainMenuScene")
            end)
        end
    end,
})

return class