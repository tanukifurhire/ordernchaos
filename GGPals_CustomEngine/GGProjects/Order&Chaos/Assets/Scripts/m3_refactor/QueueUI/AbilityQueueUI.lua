-- UI layer that mirrors the game's ability queue state

local passAndPlayDisplayed = false

-- For AbilityQueue slot UI
local class = DefineType("AbilityQueueUI",
{
    Serializable = {
        normalIconSizeX = 1.000,
        normalIconSizeY = 1.000,
        hoverIconSizeX = 1.000,
        hoverIconSizeY = 1.000,

        normalSlotSizeX = 1.000,
        normalSlotSizeY = 1.000,
        hoverSlotSizeX = 1.000,
        hoverSlotSizeY = 1.000,
    },
    
    slots = nil,
    queues = nil,
    activePlayer = nil,
    visible = nil,
    inResolution = nil,
    lastQueueFrame = -1,
    
    -- for ability playbacks --
    abilityPlaybackIdx = 0,
    playbackTriggered = false,
    activePlaybackIdx = 0, -- tracks the slot idx that is triggered

    playbackScaleTimer = 0,
    playbackScaleDirection = 0, -- 1 = scale up, -1 = scale down, 0 = idle
    playbackScaleDuration = 0.2,
    playbackScaleAmount = 15.0,
    
    targetSlotScaleX = 1.0,
    targetSlotScaleY = 1.0,
    targetIconScaleX = 1.0,
    targetIconScaleY = 1.0,
    playbackVFX = nil, -- store the playback entity to delete on hover exit

    -- disable the ability hover functionality when
    -- an ability is in the middle of being casted!
    abilitySelected = false,
    tirionVizomAbility = nil,
    ---------------------------

    -- subscription handles
    _planningPhase           = 0,
    _resolutionPhase         = 0,
    _turnChanged             = 0,
    _focusOrder              = 0,
    _focusChaos              = 0,
    _changePhase             = 0,
    _registerSlot            = 0,
    _abilityQueued           = 0,
    _abilityResolved         = 0,
    _abilityAnimFinished     = 0,
    _passAndPlayDisplayed    = 0,
    _passAndPlayHidden       = 0,
    _slot1Selected           = 0,
    _slot2Selected           = 0,
    _slot3Selected           = 0,
    _slot4Selected           = 0,
    _slot5Selected           = 0,
    _slot6Selected           = 0,

    iconPrefabs = {
        [1] = "UI_Oracle_1",
        [2] = "UI_Oracle_2",
        [3] = "UI_Dragoon_1",
        [4] = "UI_Dragoon_2",
        [7] = "UI_Eye_1",
        [8] = "UI_Eye_2",
        [5] = "UI_Imp_1",
        [6] = "UI_Imp_2",
        [9] = "UI_Tirion_1",
        [10] = "UI_Tirion_2",
        [11] = "UI_Vizom_1",
        [12] = "UI_Vizom_2",
        [14] = "UI_CrimsonSlash",
        [15] = "UI_SapphireSwirl"
    },

    Init = function(self, entity)
        Log.info("[AbilityQueueUI] Initialized")

        -- unsubscribe any leftover subscriptions (to prevent multiple queue bug)
        if self._planningPhase ~= 0 then
            Event.unsubscribe(self._planningPhase)
        end
        if self._resolutionPhase ~= 0 then
            Event.unsubscribe(self._resolutionPhase)
        end
        if self._turnChanged ~= 0 then
            Event.unsubscribe(self._turnChanged)
        end
        if self._focusOrder ~= 0 then
            Event.unsubscribe(self._focusOrder)
        end
        if self._focusChaos ~= 0 then
            Event.unsubscribe(self._focusChaos)
        end
        if self._changePhase ~= 0 then
            Event.unsubscribe(self._changePhase)
        end
        if self._registerSlot ~= 0 then
            Event.unsubscribe(self._registerSlot)
        end
        if self._abilityQueued ~= 0 then
            Event.unsubscribe(self._abilityQueued)
        end
        if self._abilityResolved ~= 0 then
            Event.unsubscribe(self._abilityResolved)
        end
        if self._abilityAnimFinished ~= 0 then
            Event.unsubscribe(self._abilityAnimFinished)
        end
        if self._passAndPlayDisplayed ~= 0 then
            Event.unsubscribe(self._passAndPlayDisplayed)
        end
        if self._passAndPlayHidden ~= 0 then
            Event.unsubscribe(self._passAndPlayHidden)
        end
        if self._slot1Selected ~= 0 then
            Event.unsubscribe(self._slot1Selected)
        end
        if self._slot2Selected ~= 0 then
            Event.unsubscribe(self._slot2Selected)
        end
        if self._slot3Selected ~= 0 then
            Event.unsubscribe(self._slot3Selected)
        end
        if self._slot4Selected ~= 0 then
            Event.unsubscribe(self._slot4Selected)
        end
        if self._slot5Selected ~= 0 then
            Event.unsubscribe(self._slot5Selected)
        end
        if self._slot6Selected ~= 0 then
            Event.unsubscribe(self._slot6Selected)
        end

        self.slots = {}

        self.queues = {
            [CONSTANTS.ORDER_PLAYER] = {},
            [CONSTANTS.CHAOS_PLAYER] = {}
        }

        self.activePlayer = CONSTANTS.CHAOS_PLAYER
        self.visible = false
        self.inResolution = false

        function self:SetActivePlayer(player)
            -- hide all queues at first
            for playerID, queue in pairs(self.queues) do
                for _, entry in ipairs(queue) do
                    entry.targetX = -9999
                    entry.targetY = -9999
                end
            end

            -- then get active player and show their queue
            self.activePlayer = player
            self.visible = true
            self:RefreshSlots()
        end

        function self:Hide()
            self.visible = false

            -- hide all queues
            for playerID, queue in pairs(self.queues) do
                for _, entry in ipairs(queue) do
                    entry.targetX = -9999
                    entry.targetY = -9999
                end
            end
        end

        function self:RefreshSlots()
            local queue = self.queues[self.activePlayer]
            if not queue then return end

            for _, entry in ipairs(queue) do
                entry.targetX = -9999
                entry.targetY = -9999
            end

            for index, entry in ipairs(queue) do
                local slot = self.slots[index]
                if slot then
                    local slotT = ECS.getTransform(slot)
                    Log.info("[AbilityQueueUI] Slot " .. index .. " pos: (" .. slotT.x .. ", " .. slotT.y .. ")")
                    entry.targetX = slotT.x
                    entry.targetY = slotT.y
                else
                    Log.info("[AbilityQueueUI] Slot " .. index .. " is nil!")
                end
            end
        end

        local refresh = function()
            self:RefreshSlots()
        end

        function self:PreviewQueuedAbility()

        end

        -- listen for UI events
        self._planningPhase =
        Event.subscribe("InPlanningPhase", function()
            self.inResolution = false
            self.playbackTriggered = false -- safety net
        end)

        self._resolutionPhase =
        Event.subscribe("InResolutionPhase", function()
            self.inResolution = true
            self.visible = true

            -- PnP: clear all queued entries so the resolution loop can repopulate fresh
            if PassAndPlayServer_instance then
                for _, queue in pairs(self.queues) do
                    for i = #queue, 1, -1 do
                        ECS.destroyEntityDeferred(queue[i].icon)
                        table.remove(queue, i)
                    end
                end
            end
        end)

        self._turnChanged =
        Event.subscribe("TurnChanged", function(params)
            if not self.inResolution then
                self:SetActivePlayer(params.currentPlayer)
            end
        end)

        -- in resolution phase, use the camera Focus to determine 
        -- which queue is displayed
        self._focusOrder =
        Event.subscribe("FocusOrderGrid", function()
            if self.inResolution then
                self:SetActivePlayer(CONSTANTS.CHAOS_PLAYER)
                self:RefreshSlots()
            end
        end)

        self._focusChaos =
        Event.subscribe("FocusChaosGrid", function()
            if self.inResolution then
                self:SetActivePlayer(CONSTANTS.ORDER_PLAYER)
                self:RefreshSlots()
            end
        end)

        self._changePhase =
        Event.subscribe("OnChangePhase", function()
            if PassAndPlayServer_instance then
                -- PnP: clear ALL entries (queued entries linger across faction switches otherwise)
                for _, queue in pairs(self.queues) do
                    for i = #queue, 1, -1 do
                        ECS.destroyEntityDeferred(queue[i].icon)
                        table.remove(queue, i)
                    end
                end
            else
                -- Networked: only clean up resolved/activating abilities
                for _, queue in pairs(self.queues) do
                    for i = #queue, 1, -1 do
                        local entry = queue[i]
                        if entry.state == "resolved" or entry.state == "activating" then
                            ECS.destroyEntityDeferred(entry.icon)
                            table.remove(queue, i)
                        end
                    end
                end
            end

            self:Hide() -- hide UI once resolution is complete
        end)

        self._registerSlot =
        Event.subscribe("RegisterAbilitySlot", function(params)
            self.slots[params.index] = params.entity
            Log.info("[AbilityQueueUI] Slot registered: " .. params.index)
            refresh()
        end)

        Event.subscribe("OnAbilitySelectedValid", function(params)
            self.abilitySelected = true
        end)

        self._abilityQueued =
        Event.subscribe("OnAbilityQueued", function(params)
            self.abilitySelected = false -- reset to false

            if passAndPlayDisplayed then
                return
            end

            if params.abilityID == CONSTANTS.ABILITIES.ORACLE_2 or
                params.abilityID == CONSTANTS.ABILITIES.EYE_2 or
                params.abilityID == CONSTANTS.ABILITIES.TIRION_2 or
                params.abilityID == CONSTANTS.ABILITIES.VIZOM_2 then
                return
            end

            -- prevent duplicate spawns within the same frame (prep phase only)
            -- During resolution, the loop adds all abilities at once in one frame
            local frame = Time.getFrameCount()
            Log.info("[AbilityQueueUI] Ability queued frame: " .. frame)

            if not self.inResolution and self.lastQueueFrame == frame then
                return
            end
            self.lastQueueFrame = frame
            ----------------------------------------------------------------------

            local playerQueue = self.queues[params.player] -- playerQueue references self.queues
            if not playerQueue then return end

            local prefab = self.iconPrefabs[params.abilityID]
            if not prefab then
                Log.info("[AbilityQueueUI] No prefab for ability " .. params.abilityID)
                return
            end

            local icon = Prefab.spawn(prefab, -9999, -9999)
            if not icon then
                Log.info("[AbilityQueueUI] Failed to spawn prefab " .. prefab)
                return
            end
            
            table.insert(playerQueue, {
                abilityID = params.abilityID,
                icon = icon,
                targetX = -9999,
                targetY = -9999,
                state = "queued", -- states: queued, activating, resolved
                fadeT = 0,
                fadeDuration = 0.5,

                gridTargetX = params.targetX,
                gridTargetY = params.targetY,
                param1 = params.param1,
                param2 = params.param2
            })

            self:RefreshSlots()
        end)

        self._abilityResolved =
        Event.subscribe("OnAbilityResolved", function(params)
            if passAndPlayDisplayed then
                return
            end

            -- mark to be removed
            local queue = self.queues[params.player]
            if not queue or not queue[1] then
                return
            end

            -- find first queued ability
            local entry = nil
            for _, e in ipairs(queue) do
                if e.state == "queued" then
                    entry = e
                    break
                end
            end

            if not entry then
                return
            end

            -- fade ability icons to visualize queue ability activation
            if params.isQueuedAbility then
                entry.state = "activating"
                entry.fadeT = 0
                entry.fadeDuration = 0.5
            end
        end)
        
        self._abilityAnimFinished =
        Event.subscribe("AbilityAnimationFinished", function(params)
            if passAndPlayDisplayed then
                return
            end

            local queue = self.queues[params.player]
            if not queue then
                return
            end
            
            -- mark as resolved but keep in queue
            for _, entry in ipairs(queue) do
                if entry.state == "activating" then
                    entry.state = "resolved"
                    break
                end
            end

            if self.visible and self.activePlayer == params.player then
                self:RefreshSlots()
            end
        end)

        self._passAndPlayDisplayed =
        Event.subscribe("OnPassAndPlayDisplayed", function()
            passAndPlayDisplayed = true
        end)

        self._passAndPlayHidden =
        Event.subscribe("OnPassAndPlayHidden", function()
            passAndPlayDisplayed = false
        end)

        -- Click a queued slot to review its effect on the grid.
        --
        -- This used to run off OnHoverEnter with a 0.5s dwell timer. Sweeping the
        -- mouse across the queue fired previews nobody asked for, so the six
        -- slots now emit SlotNSelected from OnClick instead and the preview
        -- starts on the click. Clicking the slot that is already previewing
        -- dismisses it -- OnHoverExit no longer does that, because a preview you
        -- opened deliberately should not vanish the moment the cursor drifts off.
        --
        -- One handler body for all six slots: they only ever differed by index,
        -- and keeping them as six copies meant six places to keep in step.
        for slotIdx = 1, 6 do
            self["_slot" .. slotIdx .. "Selected"] =
            Event.subscribe("Slot" .. slotIdx .. "Selected", function(params)
                -- don't trigger in resolution!
                if self.inResolution then
                    return
                end

                -- clicking the slot that is currently previewing closes it
                if self.playbackTriggered then
                    if self.activePlaybackIdx == slotIdx then
                        Log.info("[AbilityQueueUI] Slot " .. slotIdx .. " clicked again, dismissing playback")
                        Event.emitLocal("ExitSlotHover", {})
                    end
                    return
                end

                -- if any other scaling operations are occuring, don't trigger first
                if self.playbackScaleDirection ~= 0 then
                    return
                end

                Log.info("[AbilityQueueUI] Slot " .. slotIdx .. " selected!")
                self.abilityPlaybackIdx = slotIdx

                -- only emit TriggerPlayback if slot is not empty
                local activeQueue = self.queues[self.activePlayer]
                if activeQueue and activeQueue[self.abilityPlaybackIdx] then
                    Event.emitLocal("TriggerPlayback", {
                        playbackIndex = self.abilityPlaybackIdx
                    })
                end
            end)
        end

        -- store the VFX spawned by playback trigger so it can be deleted on ExitSlotHover
        Event.subscribe("AbilityVFXSpawned", function(params)
            if self.inResolution then
               return
            end

            if not self.playbackTriggred then
                return
            end

            self.playbackVFX = params.vfxEntity
        end)

        -- if end turn button pressed, 
        -- reset any icons/slots that are in the middle of scaling back to defaults
        Event.subscribe("OnEndActionButtonPressed", function()
            local activeQueue = self.queues[self.activePlayer]
            if not activeQueue then
                return
            end

            local entry = activeQueue[self.activePlaybackIdx]
            if not entry then
                return
            end

            -- reset icon to defaults
            ECS.setTransform(entry.icon, {
                sx = self.normalIconSizeX,
                sy = self.normalIconSizeY
            })

            -- reset slot to defaults
            ECS.setTransform(self.slots[self.activePlaybackIdx], {
                sx = self.normalSlotSizeX,
                sy = self.normalSlotSizeY
            })
        end)

        -- on exit slot hover, 
        -- 1. restore slot properties
        -- 2. destroy any playing VFX
        -- 3. change back the grid focus
        Event.subscribe("ExitSlotHover", function()
            -- prevent grid swapping while an ability is in the middle of casting
            if self.abilitySelected then
                return
            end

            if self.abilityPlaybackIdx ~= self.activePlaybackIdx then
                return
            end
            
            local activeQueue = self.queues[self.activePlayer]
            if not activeQueue then
                return
            end

            local entry = activeQueue[self.activePlaybackIdx]
            if not entry then
                return
            end

            -- restore slot and icon properties after playbackTriggered finished
            local icon = entry.icon
            if icon then
                -- remove shine effect (reset to defaults)
                ECS.setObjectEffectComponent(icon, {
                    intensity = 1.0,
                    color = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 },
                    effectDuration = 1.0,
                    effectType = 0, -- no effect
                    startTime = 0.0
                })

                ECS.setUIComponent(icon, {
                    colorMultiply = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 }
                })
            end

            -- start scaling back down
            self.targetSlotScaleX = self.normalSlotSizeX
            self.targetSlotScaleY = self.normalSlotSizeY
            self.targetIconScaleX = self.normalIconSizeX
            self.targetIconScaleY = self.normalIconSizeY
            self.playbackScaleTimer = 0
            self.playbackScaleDirection = -1

            -- destroy any playing VFX
            if self.playbackVFX then
                ECS.destroyEntityDeferred(self.playbackVFX)
            end

            -- for tirion and vizom ability_1 animations (to stop updating)
            self.tirionVizomAbility = nil

            -- once the VFX has been destroyed then change back the grid focus
            if not self.playbackVFX then
                if self.activePlayer == CONSTANTS.ORDER_PLAYER then
                    Event.emitLocal("FocusOrderGrid", {})
                else
                    Event.emitLocal("FocusChaosGrid", {})
                end

                self.playbackTriggered = false
            end
        end)

        Event.subscribe("TriggerVFXFinished", function(params)
            -- prevent interference with resolution phase
            if not self.playbackTriggered then
                return
            end

            -- switch back to active player's grid after playback
            if self.activePlayer == CONSTANTS.ORDER_PLAYER then
                Event.emitLocal("FocusOrderGrid", {})
            else
                Event.emitLocal("FocusChaosGrid", {})
            end

            -- restore slot and icon properties after playbackTriggered finished
            local activeQueue = self.queues[self.activePlayer]
            if activeQueue and activeQueue[self.activePlaybackIdx] then
                local icon = activeQueue[self.activePlaybackIdx].icon
                if icon then
                    -- remove shine effect (reset to defaults)
                    ECS.setObjectEffectComponent(icon, {
                        intensity = 1.0,
                        color = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 },
                        effectDuration = 1.0,
                        effectType = 0, -- no effect
                        startTime = 0.0
                    })
                end
            end

            -- start scaling back down
            self.playbackScaleTimer = 0
            self.playbackScaleDirection = -1

            if not self.playbackVFX then
                self.playbackTriggered = false
            end
        end)

        Event.subscribe("TriggerPlayback", function(params)
            -- prevent trigger while an ability is in the middle of casting
            if self.abilitySelected then
                return
            end

            -- prevent retrigger while in the middle of playback
            if self.playbackTriggered then
                return
            end

            -- if there is already a VFX playing, don't trigger again!
            if self.playbackVFX then
                return
            end

            self.activePlaybackIdx = params.playbackIndex

            Log.info("[AbilityQueueUI] TriggerPlayback event fired for slot " .. tostring(self.activePlaybackIdx))

            -- access the current caster's queue
            local queue = self.queues[self.activePlayer]
            if not queue then
                Log.info("[AbilityQueueUI] No queue exists!")
                return
            end

            local abilityToPlayback = queue[params.playbackIndex]
            if not abilityToPlayback then
                Log.info("[AbilityQueueUI] abilityToPlayback is NIL at index " .. tostring(params.playbackIndex))
                return
            end

            local abilityID = abilityToPlayback.abilityID
            local targetX = abilityToPlayback.gridTargetX
            local targetY = abilityToPlayback.gridTargetY
            local param1 = abilityToPlayback.param1
            local param2 = abilityToPlayback.param2

            -- update icon and slot scaling and effects --
            local activeQueue = self.queues[self.activePlayer]
            local entry = activeQueue[self.activePlaybackIdx]
            if activeQueue and entry then
                local slot = self.slots[self.activePlaybackIdx]
                if slot then
                    self.targetSlotScaleX = self.hoverSlotSizeX
                    self.targetSlotScaleY = self.hoverSlotSizeY
                end

                local icon = activeQueue[self.activePlaybackIdx].icon
                if icon then
                    local iconT = ECS.getTransform(icon)
                    self.targetIconScaleX = self.hoverIconSizeX
                    self.targetIconScaleY = self.hoverIconSizeY

                    -- add shine effect
                    ECS.setObjectEffectComponent(icon, {
                        intensity = 0.5,
                        color = { r = 1.0, g = 1.0, b = 1.0, a = 0.5 },
                        effectDuration = 1.0,
                        effectType = 6, -- shine
                        startTime = 0.0
                    })

                    -- for abilities with random targetting
                    if abilityID == CONSTANTS.ABILITIES.ORACLE_1 or     -- crystal cascade
                        abilityID == CONSTANTS.ABILITIES.DRAGOON_2 then -- feather cyclone
                        
                        ECS.setUIComponent(icon, {
                            colorMultiply = { r = 0.5, g = 0.5, b = 0.5, a = 0.8 }
                        })
                    end
                end
            end
            ----------------------------------------------

            -- start scaling up
            self.playbackScaleTimer = 0
            self.playbackScaleDirection = 1

            self.playbackTriggered = true

            -- skip playback for abilities with random tile targetting
            if abilityID == CONSTANTS.ABILITIES.ORACLE_1 or     -- crystal cascade
                abilityID == CONSTANTS.ABILITIES.DRAGOON_2 then -- feather cyclone
                Log.info("[AbilityQueueUI] No playback exists abilities with random tile targetting!")
                return
            end

            if self.activePlayer == CONSTANTS.ORDER_PLAYER then
                Event.emitLocal("FocusChaosGrid", {})
            else
                Event.emitLocal("FocusOrderGrid", {})
            end

            local gm = ClientGameManager_instance
            local ability = gm:GetAbilityByID(abilityID)
            if not ability then
                Log.info("[AbilityQueueUI] No ability found for ID = " .. tostring(abilityID))
                return
            end

            local orderGrid = gm.orderGrid
            local chaosGrid = gm.chaosGrid

            -- special VFX effects for playback in AbilityQueueUI.lua
            if abilityID == CONSTANTS.ABILITIES.TIRION_1 or
                abilityID == CONSTANTS.ABILITIES.VIZOM_1 then
                ability:TriggerPlayback(orderGrid, chaosGrid, targetX, targetY, param1, param2, self.activePlayer)
                self.tirionVizomAbility = ability
            elseif abilityID == CONSTANTS.ABILITIES.GENERIC_1 or
                    abilityID == CONSTANTS.ABILITIES.GENERIC_2 then
                ability:TriggerPlayback(orderGrid, chaosGrid, targetX, targetY, param1, param2, self.activePlayer)
            else
                -- follow normal resolution trigger VFX
                ability:Trigger(orderGrid, chaosGrid, targetX, targetY, param1, param2)
            end

            Log.info("[AbilityQueueUI] Triggered playback for ability ID = " .. tostring(abilityID))
        end)
    end, -- end of Init

    Start = function(self, entity)
        Event.emitLocal("AbilityQueueUIReady", {})
    end,

    -- for interpolation and snapping
    Update = function(self, entity)
        if passAndPlayDisplayed then
            return
        end

        local dt = Time.getDeltaTime()
        local speed = 14.0
        local snapDistance = 0.5
        local snapSq = snapDistance * snapDistance

        for playerID, queue in pairs(self.queues) do
            for _, entry in ipairs(queue) do
                local icon = entry.icon
                if icon and entry.targetX and entry.targetY then

                    -- resolution phase (skip all interpolation)
                    if self.inResolution then
                        local t = ECS.getTransform(icon)
                        t.x = entry.targetX
                        t.y = entry.targetY
                        ECS.setTransform(icon, t)
                    
                    -- planning phase
                    else 
                        local t = ECS.getTransform(icon)

                        local dx = entry.targetX - t.x
                        local dy = entry.targetY - t.y
                        local distSq = dx * dx + dy * dy

                        if distSq <= snapSq then
                            -- snap into queue slots
                            t.x = entry.targetX
                            t.y = entry.targetY
                        else
                            -- interpolate to queue slots
                            t.x = t.x + dx * speed * dt
                            t.y = t.y + dy * speed * dt
                        end

                        ECS.setTransform(icon, t)
                    end

                    -- ability icons fade out effect (when being activated or resolved)
                    if (entry.state == "activating" or entry.state == "resolved") and self.inResolution then
                        if entry.state == "activating" then
                            entry.fadeT = math.min(entry.fadeT + dt, entry.fadeDuration)
                        end
                        
                        local t = entry.fadeT / entry.fadeDuration

                        local ui = ECS.getUIComponent(icon)
                        if ui then
                            -- ease out fade
                            ui.colorMultiply.a = 1.0 - 0.65 * t
                            ECS.setUIComponent(icon, ui)
                        end
                    end

                end
            end
        end

        -- check scale direction (-1, 0, 1)
        if self.playbackScaleDirection ~= 0 then
            self.playbackScaleTimer = math.min(self.playbackScaleTimer + dt / self.playbackScaleDuration, 1.0)

            local progress = self.playbackScaleTimer
            local activeQueue = self.queues[self.activePlayer]

            if activeQueue and activeQueue[self.activePlaybackIdx] then
                local slot = self.slots[self.activePlaybackIdx]
                if slot then
                    local slotT = ECS.getTransform(slot)
                    -- save base scale first
                    local baseScaleX = slotT.sx
                    local baseScaleY = slotT.sy
                    -- scale
                    if self.playbackScaleDirection == 1 or self.playbackScaleDirection == -1 then
                        slotT.sx = baseScaleX + (self.targetSlotScaleX - baseScaleX) * progress
                        slotT.sy = baseScaleY + (self.targetSlotScaleY - baseScaleY) * progress
                    end
                    ECS.setTransform(slot, slotT)
                end

                local icon = activeQueue[self.activePlaybackIdx].icon
                if icon then
                    local iconT = ECS.getTransform(icon)
                    -- save base scale first
                    local baseScaleX = iconT.sx
                    local baseScaleY = iconT.sy
                    -- scale
                    if self.playbackScaleDirection == 1 or self.playbackScaleDirection == -1 then
                        iconT.sx = baseScaleX + (self.targetIconScaleX - baseScaleX) * progress
                        iconT.sy = baseScaleY + (self.targetIconScaleY - baseScaleY) * progress
                    end
                    ECS.setTransform(icon, iconT)
                end

            end

            if self.playbackScaleTimer >= 1.0 then
                self.playbackScaleDirection = 0
            end
        end

        -- update tirion/vizom ability_1 ability animations
        if self.tirionVizomAbility then
            self.tirionVizomAbility:UpdateTrigger(dt)
        end
    end
})

return class