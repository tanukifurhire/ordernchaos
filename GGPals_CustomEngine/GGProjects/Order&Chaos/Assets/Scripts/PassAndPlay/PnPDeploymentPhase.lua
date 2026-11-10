local class = DefineType("PnPDeploymentPhase",
{
    Serializable = {
        phaseEnterDelay = 0.5,       -- Delay after overlay before enabling input
        phaseOverlayDuration = 1.0   -- How long the phase overlay displays
    },

    endActionButtonHandler = 0,
    passAndPlayHandler = 0,
    bothReady = false,
    phaseDelayTimer = 0,
    phaseReady = false,

    Enter = function(self)
        Log.info("[PnPDeploy] ====== ENTER PnP Deployment Phase ======")
        self.bothReady = false
        
        -- Start phase delay timer
        self.phaseDelayTimer = 0
        self.phaseReady = false

        local gm = ClientGameManager_instance
        gm.playerFaction = CONSTANTS.ORDER_PLAYER
        if Persistent then Persistent.playerFaction = CONSTANTS.ORDER_PLAYER end
        gm.awaitingServer = false

        Event.emitLocal("ShowPhaseOverlay", { phase = "deploy" })
        
        -- Play announcer SFX for deployment phase (Order first in PnP)
        Event.emitLocal("PlayAnnouncerOrderDeploy", {})
        
        Event.emitLocal("FocusOrderGrid", {})
        Event.emitLocal("HideChaosAbilities", {})

        Event.emitLocal("TurnChanged", { currentPlayer = CONSTANTS.ORDER_PLAYER })
        Event.emitLocal("OnGameplayFactionReady", { isOrder = true })

        self.endActionButtonHandler = Event.subscribe("OnEndActionButtonPressed", function(params)
            if not self.phaseReady then return end
            if gm.awaitingServer then return end

            local bit = require("bit")
            local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
            local grid = isOrder and gm.orderGrid or gm.chaosGrid
            local flags = grid.unitFlags

            local allUnitsFlags = isOrder
                and bit.bor(bit.lshift(1, CONSTANTS.UNIT.DRAGOON),
                            bit.lshift(1, CONSTANTS.UNIT.ORACLE),
                            bit.lshift(1, CONSTANTS.UNIT.TIRION))
                or  bit.bor(bit.lshift(1, CONSTANTS.UNIT.EYE),
                            bit.lshift(1, CONSTANTS.UNIT.IMP),
                            bit.lshift(1, CONSTANTS.UNIT.VIZOM))

            if bit.band(flags, allUnitsFlags) ~= allUnitsFlags then
                Log.info("[PnPDeploy] Not all units deployed yet")
                return
            end

            Log.info("[PnPDeploy] " .. (isOrder and "Order" or "Chaos") .. " ending deployment")
            self.bothReady = PassAndPlayServer_instance:OnEndDeployment(gm.playerFaction)
            gm:PassToServer()
            -- PassAndPlayScreen2 will show the overlay (it listens to OnEndActionButtonPressed)
            -- OnPassAndPlayConfirmed will handle the transition
        end)

        self.passAndPlayHandler = Event.subscribe("OnPassAndPlayConfirmed", function()
            Log.info("[PnPDeploy] OnPassAndPlayConfirmed received, bothReady=" .. tostring(self.bothReady))
            gm.awaitingServer = false

            if self.bothReady then
                -- Both deployed: sync positions, init health, transition to prep
                Log.info("[PnPDeploy] Both factions deployed! Transitioning to prep")
                local server = PassAndPlayServer_instance
                for i = 1, 6 do
                    Event.emitLocal("UpdatePosForce", {
                        unitType = i,
                        x = server.gameData.units[i].gridPos.x,
                        y = server.gameData.units[i].gridPos.y
                    })
                end
                gm:InitializeHealth()
                gm.playerFaction = CONSTANTS.ORDER_PLAYER
                if Persistent then Persistent.playerFaction = CONSTANTS.ORDER_PLAYER end
                gm.turnPhases:ChangePhase(gm.turnPhases.preparationPhase)
            else
                -- Switch to other faction
                if gm.playerFaction == CONSTANTS.ORDER_PLAYER then
                    gm.playerFaction = CONSTANTS.CHAOS_PLAYER
                    if Persistent then Persistent.playerFaction = CONSTANTS.CHAOS_PLAYER end
                    Log.info("[PnPDeploy] Switching to Chaos")
                    Event.emitLocal("PlayAnnouncerChaosDeploy", {})
                    Event.emitLocal("FocusChaosGrid", {})
                    Event.emitLocal("ShowChaosAbilities", {})
                    Event.emitLocal("HideOrderAbilities", {})
                else
                    gm.playerFaction = CONSTANTS.ORDER_PLAYER
                    if Persistent then Persistent.playerFaction = CONSTANTS.ORDER_PLAYER end
                    Log.info("[PnPDeploy] Switching to Order")
                    Event.emitLocal("PlayAnnouncerOrderDeploy", {})
                    Event.emitLocal("FocusOrderGrid", {})
                    Event.emitLocal("ShowOrderAbilities", {})
                    Event.emitLocal("HideChaosAbilities", {})
                end
                Event.emitLocal("ShowPhaseOverlay", { phase = "deploy" })
                Event.emitLocal("TurnChanged", { currentPlayer = gm.playerFaction })
                Event.emitLocal("OnGameplayFactionReady", { isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER) })
            end
        end)
    end,

    Exit = function(self)
        Log.info("[PnPDeploy] ====== EXIT PnP Deployment Phase ======")
        if self.endActionButtonHandler ~= 0 then
            Event.unsubscribe(self.endActionButtonHandler)
            self.endActionButtonHandler = 0
        end
        if self.passAndPlayHandler ~= 0 then
            Event.unsubscribe(self.passAndPlayHandler)
            self.passAndPlayHandler = 0
        end
    end,

    Update = function(self, entity)
        local gm = ClientGameManager_instance
        
        -- Handle phase enter delay
        if not self.phaseReady then
            self.phaseDelayTimer = self.phaseDelayTimer + Time.getDeltaTime()
            if self.phaseDelayTimer >= self.phaseEnterDelay then
                self.phaseReady = true
            end
            return
        end
        
        if gm.awaitingServer then return end

        -- Ctrl+D: Randomly deploy all undeployed units for current faction
        if Input.isKeyPressed(KEY_CTRL) and Input.isKeyTriggered(KEY_D) then
            Log.info("[PnPDeploy] Ctrl+D pressed - randomly deploying undeployed units")
            gm:RandomlyDeployUndeployedUnits()
        end

        -- E: End turn (same as pressing the End Turn button)
        if Input.isKeyTriggered(69) then  -- E key (keycode 69)
            -- Check if all units are deployed before allowing turn end
            local isOrder = (gm.playerFaction == CONSTANTS.ORDER_PLAYER)
            local grid = isOrder and gm.orderGrid or gm.chaosGrid
            
            if grid then
                local flags = grid.unitFlags or 0
                local allUnitsFlags = isOrder
                    and bit.bor(bit.lshift(1, CONSTANTS.UNIT.DRAGOON),
                                bit.lshift(1, CONSTANTS.UNIT.ORACLE),
                                bit.lshift(1, CONSTANTS.UNIT.TIRION))
                    or  bit.bor(bit.lshift(1, CONSTANTS.UNIT.EYE),
                                bit.lshift(1, CONSTANTS.UNIT.IMP),
                                bit.lshift(1, CONSTANTS.UNIT.VIZOM))

                if bit.band(flags, allUnitsFlags) == allUnitsFlags then
                    Log.info("[PnPDeploy] E pressed - ending turn")
                    Event.emitLocal("OnEndActionButtonPressed", {})
                    Event.emitLocal("PlayUIEndTurn", {})
                else
                    Log.info("[PnPDeploy] E pressed but units not deployed")
                    Event.emitLocal("SpawnErrorText", { message = "Deploy all units before ending turn" })
                end
            end
        end
    end
})

return class
