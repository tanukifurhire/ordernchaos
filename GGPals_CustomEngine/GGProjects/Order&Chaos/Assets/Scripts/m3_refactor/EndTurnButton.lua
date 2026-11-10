local SPOTLIGHT_LAYER = 12
local bit = require("bit")

local class = DefineType("EndTurnButton", 
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX  = 0.001,
        hoverSizeY  = 0.001,

        phaseTimeLimit  = 60,

        textFontAssetID = 0,
        timerFontSize   = 24,
        endTurnFontSize = 16,

        timerOffsetX   = 0.0,
        timerOffsetY   = -50.0,
        endTurnOffsetX = 0.0,
        endTurnOffsetY = -30.0,
        textSpacing    = 30.0,

        isTutorial = false,   -- tick TRUE in inspector on the EndTurn button entity
        -- Timer countdown offset (idle only)
        textTimerOffsetX = 0.0,
        textTimerOffsetY = 0.0,

        -- Greyed out color multiplier
        disabledColorMultiply = 0x808080FF,
        enabledColorMultiply = 0xFFFFFFFF,
    },

    textEntityTimer = 0,
    textEntityEnd   = 0,
    textEntityTurn  = 0,

    isTutorialLocked     = false,
    tutorialTimerEnabled = false,
    originalLayer        = 0,
    isSpotlighted        = false,

    elapsedTime = 0.0,
    isTimerActive = false,
    isHovering = false,
    isDisabled = false,
    isInvalid = false,
    hasPlayedTimerLowSFX = false,
    
    -- Event handlers
    turnChangedHandler = 0,
    phaseChangeHandler = 0,

    Init = function(self, entity)
        local transform = ECS.getTransform(entity)

        self.textEntityTimer = ECS.createEntity()
        ECS.addTransform(self.textEntityTimer,
            transform.x + self.timerOffsetX,
            transform.y + self.timerOffsetY, 2, 2, 0)
        ECS.addTextComponent(self.textEntityTimer, "", self.textFontAssetID, self.timerFontSize)
        ECS.setTextComponent(self.textEntityTimer, {
            layer = 3, ySort = false, visible = true,
            alignment = 1, verticalAlignment = 1, maxWidth = 200.0
        })

        self.textEntityEnd = ECS.createEntity()
        ECS.addTransform(self.textEntityEnd,
            transform.x + self.endTurnOffsetX,
            transform.y + self.endTurnOffsetY - (self.textSpacing / 2), 2, 2, 0)
        ECS.addTextComponent(self.textEntityEnd, "", self.textFontAssetID, self.endTurnFontSize)
        ECS.setTextComponent(self.textEntityEnd, {
            layer = 3, ySort = false, visible = false,
            alignment = 1, verticalAlignment = 1, maxWidth = 200.0
        })

        self.textEntityTurn = ECS.createEntity()
        ECS.addTransform(self.textEntityTurn,
            transform.x + self.endTurnOffsetX,
            transform.y + self.endTurnOffsetY + (self.textSpacing / 2), 2, 2, 0)
        ECS.addTextComponent(self.textEntityTurn, "", self.textFontAssetID, self.endTurnFontSize)
        ECS.setTextComponent(self.textEntityTurn, {
            layer = 3, ySort = false, visible = false,
            alignment = 1, verticalAlignment = 1, maxWidth = 200.0
        })

        self.turnChangedHandler = Event.subscribe("TurnChanged", function(params)
            -- Only start timer when not in tutorial or timer explicitly enabled
            if self.isTutorial and not self.tutorialTimerEnabled then return end
            self.elapsedTime   = 0.0
            self.isTimerActive = true
            if Persistent.isTutorialTimer then
                self.phaseTimeLimit = 99999
            end
        end)

        self.phaseChangeHandler = Event.subscribe("ShowPhaseOverlay", function(params)
            if params.phase == "resphase" then
                self.isTimerActive = false
                if not (self.isTutorial and not self.tutorialTimerEnabled) then
                    ECS.setTextComponent(self.textEntityTimer, { text = "", visible = true })
                end
                ECS.setTextComponent(self.textEntityEnd,   { visible = false })
                ECS.setTextComponent(self.textEntityTurn,  { visible = false })
            end
            self:UpdateValidState(entity)
        end)

        Event.subscribe("SetUIBottomBarEnabled", function(params)
            self.isDisabled = not params.enabled
            ECS.setUIComponent(entity, { colorMultiply = params.colorMultiply })
        end)

        Event.subscribe("OnPassAndPlayHidden", function()
            self.passAndPlayDisplayed = false
        end)

        -- Subscribe to events that could change validity
        Event.subscribe("OnUnitDeployed", function()
            self:UpdateValidState(entity)
        end)

        Event.subscribe("OnUnitRemoved", function()
            self:UpdateValidState(entity)
        end)

        if self.isTutorial then
            -- Save original layer
            local ok = pcall(function()
                local ui = ECS.getUIComponent(entity)
                self.originalLayer = ui.layer
            end)
            if not ok then
                self.originalLayer = 0
            end

            -- Start locked and dimmed
            self.isTutorialLocked     = true
            self.tutorialTimerEnabled = false
            ECS.setUIComponent(entity, { colorMultiply = { r=0.3, g=0.3, b=0.3, a=0.5 } })

            local function setSpotlight(shouldSpotlight)
                if shouldSpotlight == self.isSpotlighted then return end
                self.isSpotlighted = shouldSpotlight
                local targetLayer = shouldSpotlight and SPOTLIGHT_LAYER or self.originalLayer
                pcall(function() ECS.setUIComponent(entity, { layer = targetLayer }) end)
            end

            Event.subscribe("OnTutorialStepChanged", function(params)
                -- Lock/unlock
                if params.endTurnEnabled then
                    self.isTutorialLocked = false
                    ECS.setUIComponent(entity, { colorMultiply = { r=1.0, g=1.0, b=1.0, a=1.0 } })
                else
                    self.isTutorialLocked = true
                    ECS.setUIComponent(entity, { colorMultiply = { r=0.3, g=0.3, b=0.3, a=0.5 } })
                end

                -- Spotlight
                setSpotlight(params.spotlightEndTurn or false)
            end)

            -- Step 9 unfreezes the timer
            Event.subscribe("OnTutorialEnableTimer", function()
                self.tutorialTimerEnabled = true
                self.elapsedTime          = 0.0
                self.isTimerActive        = true
                Log.info("[EndTurnButton] Tutorial timer enabled")
            end)
        end
    end,

    -- Check if the button should be valid (clickable)
    IsValid = function(self)
        if not ClientGameManager_instance then return false end
        
        local turnPhases = ClientGameManager_instance.turnPhases
        if not turnPhases then return false end
        
        local currentPhase = turnPhases.currentPhase
        if not currentPhase then return false end

        -- Invalid during resolution phase
        if currentPhase == turnPhases.resolutionPhase then
            return false
        end

        -- During deployment phase, check if all units are deployed
        if currentPhase == turnPhases.deploymentPhase then
            local isOrder = (ClientGameManager_instance.playerFaction == CONSTANTS.ORDER_PLAYER)
            local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid
            if not grid then return false end
            
            local flags = grid.unitFlags or 0
            local allUnitsFlags = isOrder
                and bit.bor(bit.lshift(1, CONSTANTS.UNIT.DRAGOON),
                            bit.lshift(1, CONSTANTS.UNIT.ORACLE),
                            bit.lshift(1, CONSTANTS.UNIT.TIRION))
                or  bit.bor(bit.lshift(1, CONSTANTS.UNIT.EYE),
                            bit.lshift(1, CONSTANTS.UNIT.IMP),
                            bit.lshift(1, CONSTANTS.UNIT.VIZOM))

            Log.info("FLAGS = " .. tostring(flags) .. " EXPECT = " .. tostring(allUnitsFlags))
            if bit.band(flags, allUnitsFlags) ~= allUnitsFlags then
                return false
            end
        end

        return true
    end,

    -- Update visual state based on validity
    UpdateValidState = function(self, entity)
        local wasInvalid = self.isInvalid
        self.isInvalid = not self:IsValid()

        if wasInvalid ~= self.isInvalid then
            local colorMultiply = self.isInvalid and self.disabledColorMultiply or self.enabledColorMultiply
            ECS.setUIComponent(entity, { colorMultiply = colorMultiply })
        end
    end,

    Start = function(self, entity)
        self.elapsedTime = 0.0
        self.isTimerActive = true
        self.isHovering = false
        ECS.setTextComponent(self.textEntityEnd, { text = "END", visible = true })
        ECS.setTextComponent(self.textEntityTurn, { text = "TURN", visible = true })
        self:UpdateValidState(entity)
    end,

    Update = function(self, entity)
        if self.isTutorial and not self.tutorialTimerEnabled then
                ECS.setTextComponent(self.textEntityTimer, { visible = false })
            return
        end
        if not self.isTimerActive then return end
        if self.textEntityTimer == 0 then return end

        self.elapsedTime = self.elapsedTime + Time.getDeltaTime()
        local timeRemaining = math.max(0.0, self.phaseTimeLimit - self.elapsedTime)
        local seconds = math.floor(timeRemaining)

        if not self.isHovering and not self.isTutorial then
            ECS.setTextComponent(self.textEntityTimer, { text = tostring(seconds), visible = true })
            ECS.setTextComponent(self.textEntityEnd,   { visible = false })
            ECS.setTextComponent(self.textEntityTurn,  { visible = false })
        end

        if seconds <= 10 and not self.hasPlayedTimerLowSFX then
            self.hasPlayedTimerLowSFX = true
            Event.emitLocal("PlayCombatEndTurnTimerLow", {})
        end

        if self.elapsedTime >= self.phaseTimeLimit then
            self.isTimerActive = false
            if self.isTutorial then return end  -- no auto-fire during tutorial

            -- If in deployment phase and not all units deployed, randomly deploy them first
            if ClientGameManager_instance and ClientGameManager_instance.turnPhases then
                local turnPhases = ClientGameManager_instance.turnPhases
                if turnPhases.currentPhase == turnPhases.deploymentPhase then
                    if not self:IsValid() then
                        ClientGameManager_instance:RandomlyDeployUndeployedUnits()
                    end
                end
            end

            Event.emitLocal("OnEndActionButtonPressed", {})
            Event.emitLocal("PlayUIEndTurn", {})
        end
    end,

    OnHoverEnter = function(self, entity)
        if self.isDisabled or self.isInvalid then return end
        if self.isTutorialLocked then return end
        self.isHovering = true
        local transform = ECS.getTransform(entity)
        if transform then
            ECS.setTransform(entity, {
                x = transform.x, y = transform.y,
                sx = self.hoverSizeX, sy = self.hoverSizeY
            })
        end
        ECS.setTextComponent(self.textEntityTimer, { visible = false })
        ECS.setTextComponent(self.textEntityEnd,  { text = "END",  visible = true })
        ECS.setTextComponent(self.textEntityTurn, { text = "TURN", visible = true })
    end,

    OnHoverExit = function(self, entity)
        if self.isDisabled or self.isInvalid then return end
        if self.isTutorialLocked then return end
        self.isHovering = false
        local transform = ECS.getTransform(entity)
        if transform then
            ECS.setTransform(entity, {
                x = transform.x, y = transform.y,
                sx = self.normalSizeX, sy = self.normalSizeY
            })
        end
        if not (self.isTutorial and not self.tutorialTimerEnabled) then
            ECS.setTextComponent(self.textEntityTimer, { visible = true })
        end
        ECS.setTextComponent(self.textEntityEnd,  { visible = false })
        ECS.setTextComponent(self.textEntityTurn, { visible = false })
    end,

    OnClick = function(self, entity, x, y)
        if self.isDisabled then return end
        self:UpdateValidState(entity)
        -- Show error message if invalid during deployment phase
        if self.isInvalid then
            if ClientGameManager_instance and ClientGameManager_instance.turnPhases then
                local turnPhases = ClientGameManager_instance.turnPhases
                if turnPhases.currentPhase == turnPhases.deploymentPhase then
                    Event.emitLocal("SpawnErrorText", { message = "Deploy all units before ending turn" })
                end
            end
            return
        end
        if self.isTutorialLocked then return end

        Event.emitLocal("OnEndActionButtonPressed", {})
        Event.emitLocal("PlayUIEndTurn", {})
    end
})

return class