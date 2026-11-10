ArsenalUI_instance = nil

local class = DefineType("ArsenalUI", {
    Serializable = {},

    isOrder = false,
    currentFaction = 1,
    currentUnit = 0,
    units = {},

    -- Per-unit loadout: { active = {abilityId, abilityId}, passive = passiveId }
    loadouts = {},

    -- Cached ability selection from palette click (nil when nothing selected)
    lastSelectedAbility = nil,  -- { abilityIndex, isPassive, texture }
    lastSelectedSpawnedIcon = nil,  -- { abilityIndex, isPassive }

    -- Which active abilities each unit is allowed to equip (own + generics)
    unitOwnAbilities = {
        [1] = {1, 2},
        [2] = {3, 4},
        [3] = {9, 10},
        [4] = {5, 6},
        [5] = {7, 8},
        [6] = {11, 12},
    },

    -- Generic abilities any unit can equip in active slots
    genericAbilities = {14, 15},  -- CrimsonSlash, SapphireSpiral

    -- Default passive per unit for auto-fill
    -- 16 = Overclock Trigger, 17 = Isolation Execution, 18 = Hunter's Mark
    unitDefaultPassives = {
        [1] = 16,   -- ORACLE  → Overclock
        [2] = 18,   -- DRAGOON   → Hunter's Mark
        [3] = 17,   -- TIRION   → Isolation Execution
        [4] = 16,   -- IMP      → Overclock
        [5] = 18,   -- EYE      → Hunter's Mark
        [6] = 17,   -- VIZOM    → Isolation Execution
    },

    Init = function(self, entity)
        ArsenalUI_instance = self

        -- In PnP mode, always start from Order when entering the arsenal scene.
        -- The Order→Chaos switch happens in-scene via ResetForFaction.
        local isNetworked = Persistent.connected or false
        if not isNetworked then
            Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER
        elseif not Persistent.sessionFaction then
            Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER
        end
        self.currentFaction = Persistent.sessionFaction
        self.isOrder = (self.currentFaction == CONSTANTS.ORDER_PLAYER)

        Event.subscribe("OnClickArsenalConfirm", function()
            ArsenalUI.ConfirmArsenal(self, entity)
        end)

        Event.subscribe("OnClickArsenalBack", function()
            ArsenalUI.BackToMenu(self, entity)
        end)

        -- Track which unit is selected
        Event.subscribe("OnArsenalUnitSelected", function(params)
            self.currentUnit = params.unitType

            -- Clear current selection when switching units
            self.lastSelectedAbility = nil
            self.lastSelectedSpawnedIcon = nil
            Event.emitLocal("OnArsenalAbilityHighlighted", { abilityIndex = 0 })
            Event.emitLocal("OnArsenalSpawnedIconHighlighted", { abilityIndex = 0, isPassive = false })
            Event.emitLocal("OnArsenalClearVisualEffects", {})

            ArsenalUI.RefreshSlots(self, params.unitType)
            ArsenalUI.EmitLoadoutChanged(self)
            Log.info("[ArsenalUI] Selected unit " .. params.unitType)
        end)

        -- Cache ability selection from palette icon clicks
        Event.subscribe("OnArsenalAbilityDragStart", function(params)
            ArsenalUI.SelectAbility(self, params)
        end)

        -- Handle spawned icon clicks (replacement flow)
        Event.subscribe("OnArsenalRemoveAbility", function(params)
            ArsenalUI.ReplaceAbility(self, params)
        end)
        
        -- Handle spawned icon selection (reverse flow)
        Event.subscribe("OnArsenalSpawnedIconSelected", function(params)
            ArsenalUI.SelectSpawnedIcon(self, params)
        end)
        
        -- Handle ability icon click when spawned icon is selected (reverse flow)
        Event.subscribe("OnArsenalAbilityIconToSpawnedIcon", function(params)
            ArsenalUI.ReplaceWithAbilityIcon(self, params)
        end)

        -- Handle slot clear requests
        Event.subscribe("OnArsenalSlotCleared", function(params)
            ArsenalUI.ClearSlot(self, params.isPassiveSlot, params.slotIndex)
        end)

        -- Networked: server confirmed our arsenal
        Event.subscribe("OnArsenalServerConfirmed", function(params)
            ArsenalUI.OnServerConfirmed(self, entity, params.arsenalMap)
        end)

        -- Networked: server rejected our arsenal
        Event.subscribe("OnArsenalServerRejected", function(params)
            Log.warning("[ArsenalUI] Server rejected arsenal: " .. params.reason)
            -- Re-enable confirm button so player can fix and retry
            Event.emitLocal("OnArsenalConfirmEnabled", { enabled = true })
        end)

        -- Networked: both players confirmed, transition to game
        Event.subscribe("OnArsenalBothReady", function(params)
            Log.info("[ArsenalUI] Both players ready, transitioning to game scene")
            Event.emitLocal("Transitionfade",{})
            Event.subscribe("TransitionComplete",function()
            Scene.setActive("M6NetworkGame")
            end)
        end)

        -- Handle slot-to-slot swap: player selected from a slot, now clicks another slot
        Event.subscribe("OnArsenalSlotClicked", function(params)
            ArsenalUI.HandleSlotClick(self, params)
        end)

        Log.info("[ArsenalUI] Initialized")
    end,

    Start = function(self, entity)
        -- Set faction based on isOrder (determined in Init from Persistent.sessionFaction)
        self.currentFaction = self.isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER

        -- Set up units based on faction
        if self.isOrder then
            self.units = {
                CONSTANTS.UNIT.DRAGOON,
                CONSTANTS.UNIT.ORACLE,
                CONSTANTS.UNIT.TIRION
            }
        else
            self.units = {
                CONSTANTS.UNIT.IMP,
                CONSTANTS.UNIT.EYE,
                CONSTANTS.UNIT.VIZOM
            }
        end

        -- Initialize loadouts for each unit
        for _, unitType in ipairs(self.units) do
            self.loadouts[unitType] = { active = {0, 0}, passive = 0 }
        end

        -- Restore persisted loadouts
        for _, unitType in ipairs(self.units) do
            local a1 = Persistent["arsenal_" .. unitType .. "_a1"]
            local a2 = Persistent["arsenal_" .. unitType .. "_a2"]
            local p  = Persistent["arsenal_" .. unitType .. "_passive"]

            if a1 then self.loadouts[unitType].active[1] = a1 end
            if a2 then self.loadouts[unitType].active[2] = a2 end
            if p  then self.loadouts[unitType].passive = p end

            if ArsenalData_instance and a1 and a2 then
                ArsenalData_instance:SetUnitAbilities(unitType, a1, a2)
            end
        end

        -- Auto-fill empty slots with defaults
        for _, unitType in ipairs(self.units) do
            local loadout = self.loadouts[unitType]
            local own = self.unitOwnAbilities[unitType]
            if own then
                if loadout.active[1] == 0 then loadout.active[1] = own[1] end
                if loadout.active[2] == 0 then loadout.active[2] = own[2] end
            end
            local defaultPassive = self.unitDefaultPassives[unitType]
            if loadout.passive == 0 and defaultPassive then
                loadout.passive = defaultPassive
            end
        end

        -- Default to first unit
        self.currentUnit = self.units[1]

        -- Inform all listeners of the current player's faction
        Event.emitLocal("OnArsenalFactionReady", {
            faction = self.currentFaction,
            isOrder = self.isOrder
        })

        -- Refresh all unit slots (sidebar UnitAbilitySlots filter by unitType)
        for _, unitType in ipairs(self.units) do
            ArsenalUI.RefreshSlots(self, unitType)
        end
        -- Re-emit for current unit so center ArsenalAbilitySlots show correct data
        ArsenalUI.RefreshSlots(self, self.currentUnit)

        -- Emit initial unit selection so all UI elements update
        Event.emitLocal("OnArsenalUnitSelected", {
            unitType = self.currentUnit
        })

        Log.info("[ArsenalUI] Ready for faction " .. self.currentFaction ..
            " (" .. (self.isOrder and "Order" or "Chaos") .. ")")
    end,
    
    -- Update: check for right-click to cancel selection
    Update = function(self, entity)
        if Input.isKeyTriggered(MOUSE_RIGHT) then
            if self.lastSelectedAbility or self.lastSelectedSpawnedIcon then
                ArsenalUI.ClearSelection(self)
            end
        end
    end,
    
    -- Clear any active selection
    ClearSelection = function(self)
        self.lastSelectedAbility = nil
        self.lastSelectedSpawnedIcon = nil
        
        Event.emitLocal("OnArsenalAbilityHighlighted", { abilityIndex = 0 })
        Event.emitLocal("OnArsenalSpawnedIconHighlighted", { abilityIndex = 0, isPassive = false })
        Event.emitLocal("OnArsenalClearVisualEffects", {})
        
        Log.info("[ArsenalUI] Selection cleared")
    end,

    -- Check if an ability ID is a valid active ability for a unit
    IsValidActiveAbility = function(self, unitType, abilityIndex)
        -- Check unit's own abilities
        local own = self.unitOwnAbilities[unitType]
        if own then
            for _, id in ipairs(own) do
                if id == abilityIndex then return true end
            end
        end

        -- Check generic abilities
        for _, id in ipairs(self.genericAbilities) do
            if id == abilityIndex then return true end
        end

        return false
    end,

    -- Cache a palette icon selection (does NOT assign to slot directly)
    SelectAbility = function(self, params)
        if self.currentUnit == 0 then
            Log.warning("[ArsenalUI] No unit selected")
            return
        end

        -- Validate active abilities for current unit
        if not params.isPassive then
            if not ArsenalUI.IsValidActiveAbility(self, self.currentUnit, params.abilityIndex) then
                Log.warning("[ArsenalUI] Ability " .. params.abilityIndex ..
                    " is not valid for unit " .. self.currentUnit)
                return
            end
        end
        
        -- Clear any spawned icon selection
        self.lastSelectedSpawnedIcon = nil

        self.lastSelectedAbility = {
            abilityIndex = params.abilityIndex,
            isPassive = params.isPassive,
            texture = params.texture
        }

        Event.emitLocal("OnArsenalAbilityHighlighted", {
            abilityIndex = params.abilityIndex
        })
        
        Event.emitLocal("OnArsenalSpawnedIconHighlighted", { 
            abilityIndex = 0, 
            isPassive = false 
        })
        
        -- Emit visual state update for darkening/blinking
        Event.emitLocal("OnArsenalVisualStateUpdate", {
            selectedAbilityIcon = {
                abilityIndex = params.abilityIndex,
                isPassive = params.isPassive
            },
            selectedSpawnedIcon = nil,
            currentUnit = self.currentUnit
        })

        Log.info("[ArsenalUI] Selected ability " .. params.abilityIndex ..
            " (passive=" .. tostring(params.isPassive) .. ")")
    end,
    
    -- Select a spawned icon (reverse flow)
    SelectSpawnedIcon = function(self, params)
        if self.currentUnit == 0 then
            Log.warning("[ArsenalUI] No unit selected")
            return
        end
        
        -- Clear any ability icon selection
        self.lastSelectedAbility = nil
        
        self.lastSelectedSpawnedIcon = {
            abilityIndex = params.abilityIndex,
            isPassive = params.isPassive
        }
        
        Event.emitLocal("OnArsenalSpawnedIconHighlighted", {
            abilityIndex = params.abilityIndex,
            isPassive = params.isPassive
        })
        
        Event.emitLocal("OnArsenalAbilityHighlighted", { 
            abilityIndex = 0 
        })
        
        -- Emit visual state update for darkening/blinking
        Event.emitLocal("OnArsenalVisualStateUpdate", {
            selectedAbilityIcon = nil,
            selectedSpawnedIcon = {
                abilityIndex = params.abilityIndex,
                isPassive = params.isPassive
            },
            currentUnit = self.currentUnit
        })
        
        Log.info("[ArsenalUI] Selected spawned icon " .. params.abilityIndex ..
            " (passive=" .. tostring(params.isPassive) .. ")")
    end,
    
    -- Replace spawned icon with selected ability icon (reverse flow)
    ReplaceWithAbilityIcon = function(self, params)
        if not self.lastSelectedSpawnedIcon then
            Log.info("[ArsenalUI] No spawned icon selected to replace")
            return
        end
        if self.currentUnit == 0 then return end

        local loadout = self.loadouts[self.currentUnit]
        if not loadout then return end

        local selectedSpawned = self.lastSelectedSpawnedIcon

        -- Passive/active type must match
        if params.isPassive ~= selectedSpawned.isPassive then
            Log.warning("[ArsenalUI] Cannot replace: passive/active type mismatch")
            return
        end
        
        -- Validate active abilities for current unit
        if not params.isPassive then
            if not ArsenalUI.IsValidActiveAbility(self, self.currentUnit, params.abilityIndex) then
                Log.warning("[ArsenalUI] Ability " .. params.abilityIndex ..
                    " is not valid for unit " .. self.currentUnit)
                return
            end
        end

        if params.isPassive then
            -- Replace passive slot
            ArsenalUI.UpdateSlot(self, self.currentUnit, true, 1, params.abilityIndex)
        else
            -- Find which active slot the spawned ability occupies
            local targetSlot = nil
            for i = 1, 2 do
                if loadout.active[i] == selectedSpawned.abilityIndex then
                    targetSlot = i
                    break
                end
            end

            if not targetSlot then
                Log.warning("[ArsenalUI] Ability " .. selectedSpawned.abilityIndex ..
                    " not found in current unit's active slots")
                return
            end

            ArsenalUI.UpdateSlot(self, self.currentUnit, false, targetSlot, params.abilityIndex)
        end

        -- Clear selection after replacement
        self.lastSelectedSpawnedIcon = nil
        Event.emitLocal("OnArsenalSpawnedIconHighlighted", { abilityIndex = 0, isPassive = false })
        Event.emitLocal("OnArsenalClearVisualEffects", {})
        ArsenalUI.EmitLoadoutChanged(self)

        Log.info("[ArsenalUI] Replaced spawned icon " .. selectedSpawned.abilityIndex ..
            " with ability " .. params.abilityIndex .. " for unit " .. self.currentUnit)
    end,

    -- Replace a slot's ability with the cached selection
    ReplaceAbility = function(self, params)
        if not self.lastSelectedAbility then
            Log.info("[ArsenalUI] No ability selected to replace with")
            return
        end
        if self.currentUnit == 0 then return end

        local loadout = self.loadouts[self.currentUnit]
        if not loadout then return end

        local selected = self.lastSelectedAbility

        -- Passive/active type must match
        if selected.isPassive ~= params.isPassive then
            Log.warning("[ArsenalUI] Cannot replace: passive/active type mismatch")
            return
        end

        if selected.isPassive then
            -- Replace passive slot
            ArsenalUI.UpdateSlot(self, self.currentUnit, true, 1, selected.abilityIndex)
        else
            -- Find which active slot the clicked ability occupies
            local targetSlot = nil
            for i = 1, 2 do
                if loadout.active[i] == params.abilityIndex then
                    targetSlot = i
                    break
                end
            end

            if not targetSlot then
                Log.warning("[ArsenalUI] Ability " .. params.abilityIndex ..
                    " not found in current unit's active slots")
                return
            end

            ArsenalUI.UpdateSlot(self, self.currentUnit, false, targetSlot, selected.abilityIndex)
        end

        -- Clear selection after replacement
        self.lastSelectedAbility = nil
        Event.emitLocal("OnArsenalAbilityHighlighted", { abilityIndex = 0 })
        Event.emitLocal("OnArsenalClearVisualEffects", {})
        ArsenalUI.EmitLoadoutChanged(self)

        Log.info("[ArsenalUI] Replaced ability " .. params.abilityIndex ..
            " with " .. selected.abilityIndex .. " for unit " .. self.currentUnit)
    end,

    -- Helper: update a single slot in the loadout and emit events
    UpdateSlot = function(self, unitType, isPassiveSlot, slotIndex, abilityIndex)
        local loadout = self.loadouts[unitType]
        if not loadout then return end

        if isPassiveSlot then
            loadout.passive = abilityIndex
        else
            loadout.active[slotIndex] = abilityIndex
        end

        Event.emitLocal("OnArsenalSlotUpdated", {
            unitType = unitType,
            isPassiveSlot = isPassiveSlot,
            slotIndex = slotIndex,
            abilityIndex = abilityIndex,
            texture = 0
        })

        -- Notify sidebar unit bar
        if isPassiveSlot then
            Event.emitLocal("OnAssignPassiveToCurrentUnit", {
                unitType = unitType,
                texture = 0
            })
        else
            Event.emitLocal("OnAssignAbilityToCurrentUnit", {
                unitType = unitType,
                slotIndex = slotIndex,
                texture = 0
            })
        end

        -- Sync with ArsenalData
        if not isPassiveSlot and ArsenalData_instance then
            ArsenalData_instance:SetUnitAbilities(unitType,
                loadout.active[1], loadout.active[2])
        end
    end,

    -- Clear a specific slot for the current unit
    ClearSlot = function(self, isPassiveSlot, slotIndex)
        if self.currentUnit == 0 then return end

        ArsenalUI.UpdateSlot(self, self.currentUnit, isPassiveSlot, slotIndex, 0)
        ArsenalUI.EmitLoadoutChanged(self)

        Log.info("[ArsenalUI] Cleared " .. (isPassiveSlot and "passive" or "active") ..
            " slot " .. slotIndex .. " for unit " .. self.currentUnit)
    end,

    -- Emit the current unit's full loadout so icons can update their in-loadout state
    EmitLoadoutChanged = function(self)
        if self.currentUnit == 0 then return end

        local loadout = self.loadouts[self.currentUnit]
        if not loadout then return end

        Event.emitLocal("OnArsenalLoadoutChanged", {
            unitType = self.currentUnit,
            active = { loadout.active[1], loadout.active[2] },
            passive = loadout.passive
        })
    end,

    -- Emit slot updates so slots refresh when switching units
    RefreshSlots = function(self, unitType)
        local loadout = self.loadouts[unitType]
        if not loadout then return end

        -- Refresh active slots
        for i = 1, 2 do
            Event.emitLocal("OnArsenalSlotUpdated", {
                unitType = unitType,
                isPassiveSlot = false,
                slotIndex = i,
                abilityIndex = loadout.active[i],
                texture = 0
            })
        end

        -- Refresh passive slot
        Event.emitLocal("OnArsenalSlotUpdated", {
            unitType = unitType,
            isPassiveSlot = true,
            slotIndex = 1,
            abilityIndex = loadout.passive,
            texture = 0
        })
    end,

    -- Get the current loadout for a unit (for external queries)
    GetLoadout = function(self, unitType)
        return self.loadouts[unitType]
    end,

    -- Maps unit type to the original (editor-assigned) ability IDs for that unit's two slots
    -- These are the IDs that AbilityButton and AbilityColorSegment have in their abilityType
    unitSlotOriginalIDs = {
        [1] = {1, 2},    -- ORACLE:  ORACLE_1=1,  ORACLE_2=2
        [2] = {3, 4},    -- DRAGOON: DRAGOON_1=3, DRAGOON_2=4
        [3] = {9, 10},   -- TIRION:  TIRION_1=9,  TIRION_2=10
        [4] = {5, 6},    -- IMP:     IMP_1=5,     IMP_2=6
        [5] = {7, 8},    -- EYE:     EYE_1=7,     EYE_2=8
        [6] = {11, 12},  -- VIZOM:   VIZOM_1=11,  VIZOM_2=12
    },

    -- Build the arsenal map from current loadouts (original editor ID -> equipped ID)
    BuildArsenalMap = function(self)
        local arsenalMap = {}
        for _, unitType in ipairs(self.units) do
            local loadout = self.loadouts[unitType]
            local originalIDs = self.unitSlotOriginalIDs[unitType]
            if originalIDs and loadout then
                arsenalMap[originalIDs[1]] = loadout.active[1]
                arsenalMap[originalIDs[2]] = loadout.active[2]
                -- Include passive using virtual slot (originalIDs[1] + 100)
                -- BuildPassiveLookup maps slot-100 through ABILITY_SLOT_TO_UNIT to find the unit
                if loadout.passive and loadout.passive > 0 then
                    arsenalMap[originalIDs[1] + 100] = loadout.passive
                end
            end
        end
        return arsenalMap
    end,

    -- Persist loadouts and arsenal map for this faction
    PersistArsenal = function(self, arsenalMap)
        for _, unitType in ipairs(self.units) do
            local loadout = self.loadouts[unitType]
            Persistent["arsenal_" .. unitType .. "_a1"] = loadout.active[1]
            Persistent["arsenal_" .. unitType .. "_a2"] = loadout.active[2]
            Persistent["arsenal_" .. unitType .. "_passive"] = loadout.passive

            if ArsenalData_instance then
                ArsenalData_instance:SetUnitAbilities(unitType,
                    loadout.active[1], loadout.active[2])
            end

            Log.info("[ArsenalUI] Unit " .. unitType .. ": slot1=" .. loadout.active[1] ..
                " slot2=" .. loadout.active[2] .. " passive=" .. loadout.passive)
        end

        local factionKey = self.isOrder and "order" or "chaos"
        Persistent["arsenalMap_" .. factionKey] = arsenalMap
        Log.info("[ArsenalUI] Arsenal persisted for " .. factionKey)
    end,

    ConfirmArsenal = function(self, entity)
        for _, unitType in ipairs(self.units) do
            local loadout = self.loadouts[unitType]
            if not loadout or loadout.active[1] == 0 or loadout.active[2] == 0 then
                Log.warning("[ArsenalUI] All units must have 2 active abilities selected")
                return
            end
        end

        local arsenalMap = ArsenalUI.BuildArsenalMap(self)
        local isNetworked = Persistent.connected or false

        if not isNetworked then
            -- PnP: persist immediately (no server validation needed)
            
            ArsenalUI.PersistArsenal(self, arsenalMap)
            
            
            if self.isOrder then
                -- Order done: reset scene state for Chaos without reloading
                Persistent.sessionFaction = CONSTANTS.CHAOS_PLAYER
                ArsenalUI.ResetForFaction(self, entity, CONSTANTS.CHAOS_PLAYER)
            else
                Event.emitLocal("Transitionfade", {})
                Event.subscribe("TransitionComplete", function()
                -- Chaos done: both factions confirmed, go to game
                Scene.setActive("M6PassAndPlay")
                end)
            end
        else
            -- Networked: send arsenal to server for validation
            -- Disable confirm button while waiting
            Event.emitLocal("OnArsenalConfirmEnabled", { enabled = false })

            local roomID = Persistent.sessionRoomID or -1
            if NetworkedGameClient_instance then
                NetworkedGameClient_instance:SubmitArsenal(roomID, arsenalMap)
            else
                Log.error("[ArsenalUI] No NetworkedGameClient_instance to submit arsenal")
                Event.emitLocal("OnArsenalConfirmEnabled", { enabled = true })
            end
        end
    end,

    -- Called when server confirms our arsenal (networked only)
    -- Persists but does NOT transition — waits for OnArsenalBothReady
    OnServerConfirmed = function(self, entity, arsenalMap)
        ArsenalUI.PersistArsenal(self, arsenalMap)
        Log.info("[ArsenalUI] Server confirmed our arsenal, waiting for opponent...")
    end,

    -- Reset entire arsenal UI for a new faction without scene reload (PnP Order->Chaos)
    ResetForFaction = function(self, entity, newFaction)
        Log.info("[ArsenalUI] Resetting arsenal for faction " .. newFaction)

        self.currentFaction = newFaction
        self.isOrder = (newFaction == CONSTANTS.ORDER_PLAYER)
        self.currentUnit = 0
        self.lastSelectedAbility = nil
        self.lastSelectedSpawnedIcon = nil

        -- Set up units for new faction
        if self.isOrder then
            self.units = {
                CONSTANTS.UNIT.DRAGOON,
                CONSTANTS.UNIT.ORACLE,
                CONSTANTS.UNIT.TIRION
            }
        else
            self.units = {
                CONSTANTS.UNIT.IMP,
                CONSTANTS.UNIT.EYE,
                CONSTANTS.UNIT.VIZOM
            }
        end

        -- Initialize fresh loadouts
        for _, unitType in ipairs(self.units) do
            self.loadouts[unitType] = { active = {0, 0}, passive = 0 }
        end

        -- Restore any persisted loadouts for this faction
        for _, unitType in ipairs(self.units) do
            local a1 = Persistent["arsenal_" .. unitType .. "_a1"]
            local a2 = Persistent["arsenal_" .. unitType .. "_a2"]
            local p  = Persistent["arsenal_" .. unitType .. "_passive"]
            if a1 then self.loadouts[unitType].active[1] = a1 end
            if a2 then self.loadouts[unitType].active[2] = a2 end
            if p  then self.loadouts[unitType].passive = p end
        end

        -- Auto-fill empty slots with defaults
        for _, unitType in ipairs(self.units) do
            local loadout = self.loadouts[unitType]
            local own = self.unitOwnAbilities[unitType]
            if own then
                if loadout.active[1] == 0 then loadout.active[1] = own[1] end
                if loadout.active[2] == 0 then loadout.active[2] = own[2] end
            end
            local defaultPassive = self.unitDefaultPassives[unitType]
            if loadout.passive == 0 and defaultPassive then
                loadout.passive = defaultPassive
            end
        end

        -- Default to first unit of new faction
        self.currentUnit = self.units[1]

        -- Notify all listeners of faction change (resets icons, unit display, slots, etc.)
        Event.emitLocal("OnArsenalFactionReady", {
            faction = self.currentFaction,
            isOrder = self.isOrder
        })

        -- Refresh all unit slots
        for _, unitType in ipairs(self.units) do
            ArsenalUI.RefreshSlots(self, unitType)
        end
        ArsenalUI.RefreshSlots(self, self.currentUnit)

        -- Emit unit selection so UI updates
        Event.emitLocal("OnArsenalUnitSelected", {
            unitType = self.currentUnit
        })

        Log.info("[ArsenalUI] Reset complete for " ..
            (self.isOrder and "Order" or "Chaos"))
    end,

    BackToMenu = function(self, entity)
        local isNetworked = Persistent.connected or false

        if isNetworked then
            -- Networked: go back to lobby room (server handles the other player)
            Event.emitLocal("Transitionfade",{})
            Event.subscribe("TransitionComplete",function()
                Scene.setActive("M5LobbyRoomScene")
            end)
        else
            -- Pass and Play
            if self.isOrder then
                -- Order selecting: go back to mode selection
                Event.emitLocal("Transitionfade",{})
                Event.subscribe("TransitionComplete",function()
                    Scene.setActive("M5ModeSelectScene")
                end)
            else
                -- Chaos selecting: go back to Order arsenal select
                Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER
                ArsenalUI.ResetForFaction(self, entity, CONSTANTS.ORDER_PLAYER)
            end
        end
    end,

    HandleSlotClick = function(self, params)
        if not self.lastSelectedAbility then return end
        if self.currentUnit == 0 then return end

        local loadout = self.loadouts[self.currentUnit]
        if not loadout then return end

        local selected = self.lastSelectedAbility

        if selected.isPassive ~= params.isPassiveSlot then
            Log.warning("[ArsenalUI] Cannot swap: passive/active mismatch")
            return
        end

        local existingAbility = 0
        if params.isPassiveSlot then
            existingAbility = loadout.passive
        else
            existingAbility = loadout.active[params.slotIndex]
        end

        ArsenalUI.UpdateSlot(self, self.currentUnit, params.isPassiveSlot, params.slotIndex, selected.abilityIndex)

        if params.sourceSlotIndex and existingAbility > 0 then
            ArsenalUI.UpdateSlot(self, self.currentUnit, params.isPassiveSlot, params.sourceSlotIndex, existingAbility)
        end

        self.lastSelectedAbility = nil
        Event.emitLocal("OnArsenalAbilityHighlighted", { abilityIndex = 0 })
        Event.emitLocal("OnArsenalClearVisualEffects", {})
        ArsenalUI.EmitLoadoutChanged(self)

        Log.info("[ArsenalUI] Swapped slot " .. params.slotIndex .. " with selected ability " .. selected.abilityIndex)
    end,
})

return class
