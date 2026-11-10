local class = DefineType("SFXManager",
{
    AudioAsset = {
        -- Grid / UI interactions
        gridConfirmTile    = 0,
        gridHoverTile      = 0,
        gridPickup         = 0,
        gridPlaceGrid      = 0,

        uiAbilityInvalid   = 0,
        uiAbilityValid     = 0,
        uiEndTurn          = 0,

        -- Main Menu and Pause Menu
        mainMenuClick      = 0,
        mainMenuHoverBling = 0,
        mainMenuHoverChaos = 0,
        mainMenuHoverOrder = 0,
        uiMenuPauseOpen        = 0, 

        -- Units
        unitEyeCry         = 0,
        unitEyeMove        = 0,
        unitImpCry         = 0,
        unitImpMove        = 0,
        unitOracleCry      = 0,
        unitOracleMove     = 0,
        unitWyvernCry      = 0,
        unitWyvernMove     = 0,
        unitTirionCry      = 0,       
        unitTirionMove     = 0,      
        unitVizomCry       = 0,     
        unitVizomMove      = 0,       

        -- VFX
        vfxWyvernA1        = 0,
        vfxWyvernA2        = 0,
        vfxWyvernFeatherWind  = 0,
        vfxWyvernFeatherSlash = 0,
        vfxImpA1           = 0,
        vfxImpA2           = 0,
        vfxEyeA1           = 0,
        vfxEyeA2           = 0,
        vfxOracleA1        = 0,
        vfxOracleA2        = 0,
        vfxTirionA1Attack      = 0,
        vfxTirionA2TrapHit     = 0,
        vfxVizomA1Attack       = 0,
        vfxVizomA2TrapHit      = 0,
        vfxGeneric1       = 0,

        -- Animation Peview
        animWyvern1        = 0,
        animWyvern2        = 0,
        animImp1           = 0,
        animImp2           = 0,
        animEye1           = 0,
        animEye2           = 0,
        animOracle1        = 0,
        animOracle2        = 0,
        animTirion1        = 0,
        animTirion2        = 0,
        animVizom1         = 0,
        animVizom2         = 0,

        combatNoAP             = 0, 
        combatChangeGrid       = 0,  
        combatTileDeselect     = 0, 
        combatEndTurnTimerLow  = 0,  
 
        combatOrderWin         = 0,   
        combatChaosWin         = 0,
        combatDraw             = 0,
        combatAnimPreviewOpen  = 0,
        combatUnitHit1         = 0,
        combatUnitHit2         = 0,
        combatUnitHit3         = 0,
        combatTileCrack        = 0, 
        combatTileDestroyed    = 0,

        -- Announcer SFX (Order)
        announcerOrderDeploy1     = 0,
        announcerOrderDeploy2     = 0,
        announcerOrderDeploy3     = 0,
        announcerOrderPlan1       = 0,
        announcerOrderPlan2       = 0,
        announcerOrderPlan3       = 0,
        announcerOrderLowHPPlan1  = 0,
        announcerOrderLowHPPlan2  = 0,
        announcerOrderResolution1 = 0,
        announcerOrderResolution2 = 0,
        announcerOrderResolution3 = 0,
        announcerOrderWins        = 0,

        -- Announcer SFX (Chaos)
        announcerChaosDeploy1     = 0,
        announcerChaosDeploy2     = 0,
        announcerChaosDeploy3     = 0,
        announcerChaosPlan1       = 0,
        announcerChaosPlan2       = 0,
        announcerChaosPlan3       = 0,
        announcerChaosLowHPPlan1  = 0,
        announcerChaosLowHPPlan2  = 0,
        announcerChaosResolution1 = 0,
        announcerChaosResolution2 = 0,
        announcerChaosResolution3 = 0,
        announcerChaosWins        = 0,

        -- Announcer SFX (Neutral)
        announcerDraw             = 0
    },

    volumeIncrement = 0.1,

    Start = function(self, entity)
        local audioConfig = Audio.getAudioConfig()
        if audioConfig and audioConfig.sfxVolume then
            Audio.setAudioSourceVolume(self.entity, audioConfig.sfxVolume)
            Log.info("[SFXManager] Volume set to " .. tostring(audioConfig.sfxVolume))
        end
    end,

    Init = function(self, entity)
        self.entity = entity
        Log.info("[SFXManager] Initialized")

        -- Helper to play a random SFX from a list of assets (one-shot)
        local function playRandomSFX(assets)
            local validAssets = {}
            for _, asset in ipairs(assets) do
                if asset ~= 0 then
                    table.insert(validAssets, asset)
                end
            end
            if #validAssets > 0 then
                local idx = math.random(1, #validAssets)
                Audio.triggerSoundOneShot(self.entity, validAssets[idx])
            end
        end

        ----------------------------------------------------------------
        -- Combat interactions
        ----------------------------------------------------------------
        -- NOTE: Most SFX use triggerSoundOneShot so they don't kill the
        -- announcer channel (triggerSound stops runtimeChannel first).
        -- Only hover uses triggerSound since it fires rapidly and we want
        -- to replace rather than stack hover sounds.
        Event.subscribe("PlayGridConfirmTile", function()
            Audio.triggerSoundOneShot(self.entity, self.gridConfirmTile)
        end)

        Event.subscribe("PlayGridHoverTile", function()
            Audio.triggerSound(self.entity, self.gridHoverTile)
        end)

        Event.subscribe("PlayGridPickup", function()
            Audio.triggerSoundOneShot(self.entity, self.gridPickup)
        end)

        Event.subscribe("PlayGridPlaceGrid", function()
            Audio.triggerSoundOneShot(self.entity, self.gridPlaceGrid)
        end)

        Event.subscribe("PlayUIAbilityInvalid", function()
            Audio.triggerSoundOneShot(self.entity, self.uiAbilityInvalid)
        end)

        Event.subscribe("PlayUIAbilityValid", function()
            Audio.triggerSoundOneShot(self.entity, self.uiAbilityValid)
        end)

        Event.subscribe("PlayUIEndTurn", function()
            Audio.triggerSoundOneShot(self.entity, self.uiEndTurn)
        end)

        Event.subscribe("PlayCombatNoAP", function() Audio.triggerSoundOneShot(self.entity, self.combatNoAP) end)
        Event.subscribe("PlayCombatTileDeselect", function() Audio.triggerSoundOneShot(self.entity, self.combatTileDeselect) end)
        Event.subscribe("PlayCombatChangeGrid", function() Audio.triggerSoundOneShot(self.entity, self.combatChangeGrid) end)
        Event.subscribe("PlayCombatEndTurnTimerLow", function() Audio.triggerSoundOneShot(self.entity, self.combatEndTurnTimerLow) end)
        Event.subscribe("PlayCombatOrderWin", function() Audio.triggerSoundOneShot(self.entity, self.combatOrderWin) end)
        Event.subscribe("PlayCombatChaosWin", function() Audio.triggerSoundOneShot(self.entity, self.combatChaosWin) end)
        Event.subscribe("PlayCombatDraw", function() Audio.triggerSoundOneShot(self.entity, self.combatDraw) end)
        Event.subscribe("PlayCombatAnimPreviewOpen", function()
            if self.combatAnimPreviewOpen ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.combatAnimPreviewOpen)
            end
        end)
        Event.subscribe("PlayCombatUnitHit", function()
            playRandomSFX({self.combatUnitHit1, self.combatUnitHit2, self.combatUnitHit3})
        end)
        Event.subscribe("PlayCombatTileCrack", function() Audio.triggerSoundOneShot(self.entity, self.combatTileCrack) end)
        Event.subscribe("PlayCombatTileDestroyed", function() Audio.triggerSoundOneShot(self.entity, self.combatTileDestroyed) end)

        Event.subscribe("OnAdjustVolumeSettings", function()
            Audio.triggerSoundOneShot(self.entity, self.uiEndTurn) -- using this sound for now, change later
        end)

        ----------------------------------------------------------------
        -- Main menu and Pause Menu
        ----------------------------------------------------------------
        Event.subscribe("PlayMainMenuClick", function()
            Audio.triggerSoundOneShot(self.entity, self.mainMenuClick)
        end)

        Event.subscribe("PlayMainMenuHoverBling", function()
            Audio.triggerSound(self.entity, self.mainMenuHoverBling)
        end)

        Event.subscribe("PlayMainMenuHoverChaos", function()
            Audio.triggerSound(self.entity, self.mainMenuHoverChaos)
        end)

        Event.subscribe("PlayMainMenuHoverOrder", function()
            Audio.triggerSound(self.entity, self.mainMenuHoverOrder)
        end)

        Event.subscribe("PlayUIMenuPauseOpen", function()
            Audio.triggerSoundOneShot(self.entity, self.uiMenuPauseOpen)
        end)

        ----------------------------------------------------------------
        -- Units
        ----------------------------------------------------------------
        Event.subscribe("PlayUnitEyeCry", function()
            Audio.triggerSoundOneShot(self.entity, self.unitEyeCry)
        end)

        Event.subscribe("PlayUnitEyeMove", function()
            Audio.triggerSoundOneShot(self.entity, self.unitEyeMove)
        end)

        Event.subscribe("PlayUnitImpCry", function()
            Audio.triggerSoundOneShot(self.entity, self.unitImpCry)
        end)

        Event.subscribe("PlayUnitImpMove", function()
            Audio.triggerSoundOneShot(self.entity, self.unitImpMove)
        end)

        Event.subscribe("PlayUnitOracleCry", function()
            Audio.triggerSoundOneShot(self.entity, self.unitOracleCry)
        end)

        Event.subscribe("PlayUnitOracleMove", function()
            Audio.triggerSoundOneShot(self.entity, self.unitOracleMove)
        end)

        Event.subscribe("PlayUnitWyvernCry", function()
            Audio.triggerSoundOneShot(self.entity, self.unitWyvernCry)
        end)

        Event.subscribe("PlayUnitWyvernMove", function()
            Audio.triggerSoundOneShot(self.entity, self.unitWyvernMove)
        end)

        Event.subscribe("PlayUnitTirionCry", function()
            Audio.triggerSoundOneShot(self.entity, self.unitTirionCry)
        end)

        Event.subscribe("PlayUnitTirionMove", function()
            Audio.triggerSoundOneShot(self.entity, self.unitTirionMove)
        end)

        Event.subscribe("PlayUnitVizomCry", function()
            Audio.triggerSoundOneShot(self.entity, self.unitVizomCry)
        end)

        Event.subscribe("PlayUnitVizomMove", function()
            Audio.triggerSoundOneShot(self.entity, self.unitVizomMove)
        end)

        -- Play the selected unit's cry when clicked during planning phase
        Event.subscribe("OnUnitSelected", function(params)
            if not params or not params.unitType then return end
            if params.unitType == CONSTANTS.UNIT.DRAGOON then
                Audio.triggerSoundOneShot(self.entity, self.unitWyvernCry)
            elseif params.unitType == CONSTANTS.UNIT.ORACLE then
                Audio.triggerSoundOneShot(self.entity, self.unitOracleCry)
            elseif params.unitType == CONSTANTS.UNIT.TIRION then
                Audio.triggerSoundOneShot(self.entity, self.unitTirionCry)
            elseif params.unitType == CONSTANTS.UNIT.IMP then
                Audio.triggerSoundOneShot(self.entity, self.unitImpCry)
            elseif params.unitType == CONSTANTS.UNIT.EYE then
                Audio.triggerSoundOneShot(self.entity, self.unitEyeCry)
            elseif params.unitType == CONSTANTS.UNIT.VIZOM then
                Audio.triggerSoundOneShot(self.entity, self.unitVizomCry)
            end
        end)

        ----------------------------------------------------------------
        -- VFX
        ----------------------------------------------------------------
        Event.subscribe("PlayVFXDragonA1", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxWyvernA1)
        end)

        Event.subscribe("PlayVFXDragonA2", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxWyvernA2)
        end)

        Event.subscribe("PlaySFXFeatherLaunch", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxWyvernFeatherWind)
        end)

        Event.subscribe("PlaySFXFeatherHit", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxWyvernFeatherSlash)
        end)

        Event.subscribe("PlayVFXImpA1", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxImpA1)
        end)

        Event.subscribe("PlayVFXImpA2", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxImpA2)
        end)

        Event.subscribe("PlayVFXEyeA1", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxEyeA1)
        end)

        Event.subscribe("PlayVFXEyeA2", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxEyeA2)
        end)

        Event.subscribe("PlayVFXOracleA1", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxOracleA1)
        end)

        Event.subscribe("PlayVFXOracleA2", function()
            Audio.triggerSoundOneShot(self.entity, self.vfxOracleA2)
        end)

        Event.subscribe("PlayVFXTirionA1", function()
            if self.vfxTirionA1Attack ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxTirionA1Attack)
            end
        end)

        Event.subscribe("PlayVFXTirionA2", function()
            if self.vfxTirionA2TrapHit ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxTirionA2TrapHit)
            end
        end)

        Event.subscribe("PlayVFXVizomA1", function()
            if self.vfxVizomA1Attack ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxVizomA1Attack)
            end
        end)

        Event.subscribe("PlayVFXVizomA2", function()
            if self.vfxVizomA2TrapHit ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxVizomA2TrapHit)
            end
        end)

        Event.subscribe("PlayVFXGeneric1", function()
            if self.vfxGeneric1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxGeneric1)
            end
        end)

        -- Combined Tirion/Vizom events (VFXPlayer uses shared prefabs)
        Event.subscribe("PlayVFXTirionVizomA1", function()
            if self.vfxTirionA1Attack ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxTirionA1Attack)
            end
        end)

        Event.subscribe("PlayVFXTirionVizomA2", function()
            if self.vfxTirionA2TrapHit ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.vfxTirionA2TrapHit)
            end
        end)

        ----------------------------------------------------------------
        -- Animation Preview
        ----------------------------------------------------------------
        Event.subscribe("PlayAnimSFXDragon1", function()
            if self.animWyvern1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animWyvern1)
            else
                Log.info("Dragon1 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXDragon2", function()
            if self.animWyvern2 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animWyvern2)
            else
                Log.info("Dragon2 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXImp1", function()
            if self.animImp1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animImp1)
            else
                Log.info("Imp1 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXImp2", function()
            if self.animImp2 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animImp2)
            else
                Log.info("Imp2 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXEye1", function()
            if self.animEye1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animEye1)
            else
                Log.info("Eye1 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXEye2", function()
            if self.animEye2 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animEye2)
            else
                Log.info("Eye2 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXOracle1", function()
            if self.animOracle1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animOracle1)
            else
                Log.info("Oracle1 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXOracle2", function()
            if self.animOracle2 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animOracle2)
            else
                Log.info("Oracle2 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXTirion1", function()
            if self.animTirion1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animTirion1)
            else
                Log.info("Tirion1 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXTirion2", function()
            if self.animTirion2 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animTirion2)
            else
                Log.info("Tirion2 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXVizom1", function()
            if self.animVizom1 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animVizom1)
            else
                Log.info("Vizom1 SFX not assigned")
            end
        end)

        Event.subscribe("PlayAnimSFXVizom2", function()
            if self.animVizom2 ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.animVizom2)
            else
                Log.info("Vizom2 SFX not assigned")
            end
        end)

        ----------------------------------------------------------------
        -- SFX Audio Controls
        ----------------------------------------------------------------
        Event.subscribe("GetSFXVolume", function()
            local volume = Audio.getAudioSourceVolume(self.entity)
            Event.emitLocal("OnReturnSFXVolume", {
                sfxVolume = volume;
            })
        end)

        Event.subscribe("OnIncreaseSFXVolume", function()
            local currentVolume = Audio.getAudioSourceVolume(self.entity)

            -- skip is already at max volume
            if currentVolume >= 1.0 then
                return
            end

            local newVolume = currentVolume + self.volumeIncrement
            Audio.setAudioSourceVolume(self.entity, newVolume)
        end)

        Event.subscribe("OnDecreaseSFXVolume", function()
            local currentVolume = Audio.getAudioSourceVolume(self.entity)

            -- skip if already at volume 0
            if currentVolume <= 0.0 then
                return
            end

            local newVolume = currentVolume - self.volumeIncrement
            Audio.setAudioSourceVolume(self.entity, newVolume)
        end)

        Event.subscribe("InPlanningPhase", function()
            Audio.setAudioSourceStereoPan(self.entity, 0)
        end)

        Event.subscribe("SetStereoPan", function(params)
            Audio.setAudioSourceStereoPan(self.entity, params.pan)
        end)

        ----------------------------------------------------------------
        -- Announcer SFX (use OneShot so they don't get cut off by
        -- other SFX firing on the same entity right after)
        ----------------------------------------------------------------
        -- Order Deployment Phase
        Event.subscribe("PlayAnnouncerOrderDeploy", function()
            playRandomSFX({
                self.announcerOrderDeploy1,
                self.announcerOrderDeploy2,
                self.announcerOrderDeploy3
            })
        end)

        -- Chaos Deployment Phase
        Event.subscribe("PlayAnnouncerChaosDeploy", function()
            playRandomSFX({
                self.announcerChaosDeploy1,
                self.announcerChaosDeploy2,
                self.announcerChaosDeploy3
            })
        end)

        -- Order Planning Phase (normal)
        Event.subscribe("PlayAnnouncerOrderPlan", function()
            playRandomSFX({
                self.announcerOrderPlan1,
                self.announcerOrderPlan2,
                self.announcerOrderPlan3
            })
        end)

        -- Order Planning Phase (low HP)
        Event.subscribe("PlayAnnouncerOrderLowHPPlan", function()
            playRandomSFX({
                self.announcerOrderLowHPPlan1,
                self.announcerOrderLowHPPlan2
            })
        end)

        -- Chaos Planning Phase (normal)
        Event.subscribe("PlayAnnouncerChaosPlan", function()
            playRandomSFX({
                self.announcerChaosPlan1,
                self.announcerChaosPlan2,
                self.announcerChaosPlan3
            })
        end)

        -- Chaos Planning Phase (low HP)
        Event.subscribe("PlayAnnouncerChaosLowHPPlan", function()
            playRandomSFX({
                self.announcerChaosLowHPPlan1,
                self.announcerChaosLowHPPlan2
            })
        end)

        -- Order Resolution Phase
        Event.subscribe("PlayAnnouncerOrderResolution", function()
            playRandomSFX({
                self.announcerOrderResolution1,
                self.announcerOrderResolution2,
                self.announcerOrderResolution3
            })
        end)

        -- Chaos Resolution Phase
        Event.subscribe("PlayAnnouncerChaosResolution", function()
            playRandomSFX({
                self.announcerChaosResolution1,
                self.announcerChaosResolution2,
                self.announcerChaosResolution3
            })
        end)

        -- End of Game
        Event.subscribe("PlayAnnouncerOrderWins", function()
            if self.announcerOrderWins ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.announcerOrderWins)
            end
        end)

        Event.subscribe("PlayAnnouncerChaosWins", function()
            if self.announcerChaosWins ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.announcerChaosWins)
            end
        end)

        Event.subscribe("PlayAnnouncerDraw", function()
            if self.announcerDraw ~= 0 then
                Audio.triggerSoundOneShot(self.entity, self.announcerDraw)
            end
        end)

    end -- end of Init
})

return class
