local class = DefineType("ClientDeploymentPhase",
{
    Serializable = {
        phaseEnterDelay = 0.5,      -- Delay after overlay before enabling input
        phaseOverlayDuration = 1.0   -- How long the phase overlay displays
    },

    endActionButtonHandler = 0,
    phaseDelayTimer = 0,
    phaseReady = false,

    Enter = function(self)
        Event.emitLocal("ShowPhaseOverlay", { phase = "deploy" })

        Log.info("Entered Deployment Phase!")
        
        -- Start phase delay timer
        self.phaseDelayTimer = 0
        self.phaseReady = false

        local isOrder = (ClientGameManager_instance.playerFaction == CONSTANTS.ORDER_PLAYER)

        -- Play announcer SFX for deployment phase
        if isOrder then
            Event.emitLocal("PlayAnnouncerOrderDeploy", {})
        else
            Event.emitLocal("PlayAnnouncerChaosDeploy", {})
        end

        if isOrder then
            Event.emitLocal("FocusOrderGrid", {})
            Event.emitLocal("HideChaosAbilities", {})
        else
            Event.emitLocal("FocusChaosGrid", {})
            Event.emitLocal("HideOrderAbilities", {})
        end

        Event.emitLocal("OnGameplayFactionReady", { isOrder = isOrder })

        self.endActionButtonHandler = 
        Event.subscribe("OnEndActionButtonPressed", function(params)
            if not self.phaseReady then return end
            if ClientGameManager_instance.awaitingServer then return end

            local bit = require("bit")

            local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid
            local flags = grid.unitFlags
            
            local allUnitsFlags = isOrder
                and bit.bor(bit.lshift(1, CONSTANTS.UNIT.DRAGOON),
                            bit.lshift(1, CONSTANTS.UNIT.ORACLE),
                            bit.lshift(1, CONSTANTS.UNIT.TIRION))
                or  bit.bor(bit.lshift(1, CONSTANTS.UNIT.EYE),
                            bit.lshift(1, CONSTANTS.UNIT.IMP),
                            bit.lshift(1, CONSTANTS.UNIT.VIZOM))

            if bit.band(flags, allUnitsFlags) == allUnitsFlags then
                -- We can now try to end our turn and pass to server
                Log.info("[DeploymentPhase] Ending turn..")
                ClientGameManager_instance:PassToServer()
                RPC.call("ServerRPC_OnClientEndDeploymentPhase", NetworkedGameClient_instance.assignedClientID)
            else
                -- We won't be able to visually end our turn!
            end
        end)
    end,

    Exit = function(self)
        Event.unsubscribe(self.endActionButtonHandler)
    end,

    Update = function(self, entity)
        -- Handle phase enter delay
        if not self.phaseReady then
            self.phaseDelayTimer = self.phaseDelayTimer + Time.getDeltaTime()
            if self.phaseDelayTimer >= self.phaseEnterDelay then
                self.phaseReady = true
            end
            return
        end
        
        if ClientGameManager_instance.awaitingServer then return end

        -- Ctrl+D: Randomly deploy all undeployed units
        if Input.isKeyPressed(KEY_CTRL) and Input.isKeyTriggered(KEY_D) then
            Log.info("[DeploymentPhase] Ctrl+D pressed - randomly deploying undeployed units")
            ClientGameManager_instance:RandomlyDeployUndeployedUnits()
        end

        -- E: End turn (same as pressing the End Turn button)
        if Input.isKeyTriggered(69) then  -- E key (keycode 69)
            -- Check if all units are deployed before allowing turn end
            local gm = ClientGameManager_instance
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
                    Log.info("[DeploymentPhase] E pressed - ending turn")
                    Event.emitLocal("OnEndActionButtonPressed", {})
                    Event.emitLocal("PlayUIEndTurn", {})
                else
                    Log.info("[DeploymentPhase] E pressed but units not deployed")
                    Event.emitLocal("SpawnErrorText", { message = "Deploy all units before ending turn" })
                end
            end
        end
    end
})

return class