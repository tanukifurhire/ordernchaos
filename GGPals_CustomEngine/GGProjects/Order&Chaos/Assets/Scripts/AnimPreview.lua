local class = DefineType("AnimPreview", {
    Serializable = {
        displayDuration = 2.0,
        lerpDuration = 0.5,
        startPosX = -1000.0,
        displayPosX = 0.0,
        endPosX = 1000.0,
        posY = 0.0
    },
    
    onLerpIn = false,
    onDisplay = false,
    onLerpOut = false,
    displayTimer = 0.0,
    lerpTimer = 0.0,
    transitionRequested = false,
    
    startPos = {
        x = 0,
        y = 0
    },
    targetPos = {
        x = 0,
        y = 0
    },
    
    pendingTrigger = nil,  -- queued trigger when a new one arrives while already active

    eventHandlers = {},

    Init = function(self, entity)
        Log.info("AnimPreview: Init called")

        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerOracle1AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Oracle_Ability1", 1) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerOracle2AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Oracle_Ability2", 2) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerDragoon1AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Dragoon_Ability1", 3) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerDragoon2AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Dragoon_Ability2", 4) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerImp1AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Imp_Ability1", 5) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerImp2AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Imp_Ability2", 6) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerEye1AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Eye_Ability1", 7) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerEye2AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Eye_Ability2", 8) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerTirion1AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Tirion_Ability1", 9) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerTirion2AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Tirion_Ability2", 10) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerVizom1AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Vizom_Ability1", 11) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerVizom2AnimPreview", function() AnimPreview.OnTrigger(self, entity, "Vizom_Ability2", 12) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerGeneric1AnimPreview", function(params) AnimPreview.OnTriggerGeneric(self, entity, 14, params.faction) end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerGeneric2AnimPreview", function(params) AnimPreview.OnTriggerGeneric(self, entity, 15, params.faction) end)

        Log.info("AnimPreview: Subscribed to all trigger events")
    end,

    OnTrigger = function(self, entity, animStateName, abilityID)
        Log.info("AnimPreview: OnTrigger called with animation: " .. animStateName)
        Event.emitLocal("PlayCombatAnimPreviewOpen", {})

        if self.onLerpIn or self.onDisplay or self.onLerpOut then
            -- Queue this trigger and start lerping out the current one
            Log.info("AnimPreview: Already active, queuing trigger and starting lerp out")
            self.pendingTrigger = { animStateName = animStateName, abilityID = abilityID }
            if self.onLerpIn or self.onDisplay then
                AnimPreview.StartLerpOut(self, entity)
            end
            return
        end
        
        self.onLerpIn = true
        self.lerpTimer = 0.0
        self.transitionRequested = false
        
        self.startPos.x = self.startPosX
        self.startPos.y = self.posY
        self.targetPos.x = self.displayPosX
        self.targetPos.y = self.posY
        
        local transform = ECS.getTransform(entity)
        transform.x = self.startPos.x
        transform.y = self.startPos.y
        ECS.setTransform(entity, transform)
        
        local uiComponent = ECS.getUIComponent(entity)
        if uiComponent then
            uiComponent.colorMultiply.a = 0.0
            ECS.setUIComponent(entity, uiComponent)
        end
        
        Event.emitLocal("AnimPreview_StartLerpIn", {
            lerpDuration = self.lerpDuration
        })
        
        AnimPreview.PlayAnimation(self, entity, animStateName)
        Log.info("AnimPreview: Started lerping in from x=" .. transform.x .. " to x=" .. self.targetPos.x)

        local ability = AbilityList[abilityID]
        if not ability then
            Log.error("Ability not found for ID: " .. tostring(abilityID))
            return
        end

        Log.info("Triggered ability: " .. ability.name)

        -- Order abilities
        -- Oracle_Ability1 (Crystal Cascade)
        if abilityID == 1 then
            Event.emitLocal("PlayPreviewVFX",{
                animationName = "Oracle_Ability1_BEAM",
                posX = self.displayPosX,
                posY = self.posY
            })

            Event.emitLocal("Order", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXOracle1", {})
        end

        -- Oracle_Ability2 (Fiberglass Finesse)
        if abilityID == 2 then
            Event.emitLocal("Order", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXOracle2", {})
        end   
        
        -- Dragoon_Ability1 (Azure Blaze)
        if abilityID == 3 then
            Event.emitLocal("Order", {
                ability = animStateName
            })

            Event.emitLocal("oneLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXDragon1", {})
        end 

        -- Dragoon_Ability2 (Feather Cyclone)
        if abilityID == 4 then
            Event.emitLocal("Order", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXDragon2", {})
        end 

        -- Tirion_Ability2 (Luminous Cleave)
        if abilityID == 9 then
            Event.emitLocal("Order", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXTirion1", {})
        end 

        -- Tirion_Ability1 (Sigil of Consecration)
        if abilityID == 10 then
            Event.emitLocal("Order", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXTirion2", {})
        end 

        -- Chaos abilities
        -- Imp_Ability1 (Imp-Pact)
        if abilityID == 5 then
            Event.emitLocal("Chaos", {
                ability = animStateName
            })

            Event.emitLocal("oneLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXImp1", {})
        end 

        -- Imp_Ability2 (Wicked Shift)
        if abilityID == 6 then
            Event.emitLocal("Chaos", {
                ability = animStateName
            })

            Event.emitLocal("oneLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXImp2", {})
        end 

        -- Eye_Ability1 (Ruinous Gaze)
        if abilityID == 7 then
            Event.emitLocal("Chaos", {
                ability = animStateName
            })

            Event.emitLocal("oneLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXEye1", {})
        end
        
        --Eye_Ability2
        if abilityID == 8 then
            Event.emitLocal("Chaos", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXEye2", {})
        end   

        --Vizom_Ability1
        if abilityID == 11 then
            Event.emitLocal("Chaos", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXVizom1", {})
        end 

        --Vizom_Ability1
        if abilityID == 12 then
            Event.emitLocal("Chaos", {
                ability = animStateName
            })

            Event.emitLocal("twoLine", {})

            -- SFXManager
            Event.emitLocal("PlayAnimSFXVizom2", {})
        end 
    end,

    OnTriggerGeneric = function(self, entity, abilityID, faction)
        Event.emitLocal("PlayCombatAnimPreviewOpen", {})
        -- Generics use different animation names based on faction
        local animName = ""
        local factionEvent = ""
        local sfxEvent = ""
        
        if abilityID == 14 then  -- GENERIC_1 (Crimson Slash)
            animName = "None"
            sfxEvent = "PlayAnimSFXGeneric1"
        elseif abilityID == 15 then  -- GENERIC_2 (Sapphire Swirl)
            animName = "None"  
            sfxEvent = "PlayAnimSFXGeneric2"
        end

        -- Use faction to determine Order vs Chaos event
        if faction == CONSTANTS.ORDER_PLAYER then
            factionEvent = "Order"
        else
            factionEvent = "Chaos"
        end

        Log.info("AnimPreview: OnTriggerGeneric called with ability: " .. animName .. " for faction: " .. factionEvent)
        
        if self.onLerpIn or self.onDisplay or self.onLerpOut then
            Log.info("AnimPreview: Already active, queuing generic trigger and starting lerp out")
            self.pendingTrigger = { animStateName = animName, abilityID = abilityID, isGeneric = true, faction = faction }
            if self.onLerpIn or self.onDisplay then
                AnimPreview.StartLerpOut(self, entity)
            end
            return
        end

        self.onLerpIn = true
        self.lerpTimer = 0.0
        self.transitionRequested = false

        self.startPos.x = self.startPosX
        self.startPos.y = self.posY
        self.targetPos.x = self.displayPosX
        self.targetPos.y = self.posY

        local transform = ECS.getTransform(entity)
        transform.x = self.startPos.x
        transform.y = self.startPos.y
        ECS.setTransform(entity, transform)

        local uiComponent = ECS.getUIComponent(entity)
        if uiComponent then
            uiComponent.colorMultiply.a = 0.0
            ECS.setUIComponent(entity, uiComponent)
        end

        Event.emitLocal("AnimPreview_StartLerpIn", {
            lerpDuration = self.lerpDuration
        })

        AnimPreview.PlayAnimation(self, entity, animName)

        -- Emit faction event
        Event.emitLocal(factionEvent, {
            ability = animName
        })

        Event.emitLocal("oneLine", {})

        -- Play SFX
        if sfxEvent ~= "" then
            Event.emitLocal(sfxEvent, {})
        end

        Log.info("AnimPreview: Started generic animation preview")
    end,

    Update = function(self, entity)
        if not self.onLerpIn and not self.onDisplay and not self.onLerpOut then
            return
        end

        local transform = ECS.getTransform(entity)

        if self.onLerpIn then
            self.lerpTimer = self.lerpTimer + Time.getDeltaTime()
            local t = math.min(self.lerpTimer / self.lerpDuration, 1.0)
            
            transform.x = self.startPos.x + (self.targetPos.x - self.startPos.x) * t
            transform.y = self.startPos.y + (self.targetPos.y - self.startPos.y) * t
            ECS.setTransform(entity, transform)
            
            local uiComponent = ECS.getUIComponent(entity)
            if uiComponent then
                uiComponent.colorMultiply.a = t
                ECS.setUIComponent(entity, uiComponent)
            end
            
            if t >= 1.0 then
                self.onLerpIn = false
                self.onDisplay = true
                self.displayTimer = 0.0
                Log.info("AnimPreview: Reached display position, now displaying")
                
                if self.transitionRequested then
                    Log.info("AnimPreview: Transition was already requested, starting lerp out immediately")
                    AnimPreview.StartLerpOut(self, entity)
                end
            end
            
        elseif self.onDisplay then
            self.displayTimer = self.displayTimer + Time.getDeltaTime()
            
            if self.transitionRequested or self.displayTimer >= self.displayDuration then
                Log.info("AnimPreview: Display duration complete or transition requested, starting lerp out")
                AnimPreview.StartLerpOut(self, entity)
            end
            
        elseif self.onLerpOut then
            self.lerpTimer = self.lerpTimer + Time.getDeltaTime()
            local t = math.min(self.lerpTimer / self.lerpDuration, 1.0)
            
            transform.x = self.displayPosX + (self.targetPos.x - self.displayPosX) * t
            transform.y = self.posY + (self.targetPos.y - self.posY) * t
            ECS.setTransform(entity, transform)
            
            local uiComponent = ECS.getUIComponent(entity)
            if uiComponent then
                uiComponent.colorMultiply.a = 1.0 - t
                ECS.setUIComponent(entity, uiComponent)
            end
            
            if t >= 1.0 then
                self.onLerpOut = false
                self.transitionRequested = false
                Log.info("AnimPreview: Completed lerp out, back to idle")
                self:PlayAnimation(entity, "None")

                -- Fire queued trigger if one is pending
                if self.pendingTrigger then
                    local pending = self.pendingTrigger
                    self.pendingTrigger = nil
                    if pending.isGeneric then
                        Log.info("AnimPreview: Firing queued generic trigger: " .. tostring(pending.abilityID))
                        AnimPreview.OnTriggerGeneric(self, entity, pending.abilityID, pending.faction)
                    else
                        Log.info("AnimPreview: Firing queued trigger: " .. pending.animStateName)
                        AnimPreview.OnTrigger(self, entity, pending.animStateName, pending.abilityID)
                    end
                end
            end
        end
    end,

    PlayAnimation = function(self, entity, stateName)
        local animInfo = ECS.getAnimation(entity)
        if not animInfo then
            return
        end
        
        ECS.setAnimation(entity, stateName)
    end,

    StartLerpOut = function(self, entity)
        self.onDisplay = false
        self.onLerpOut = true
        self.lerpTimer = 0.0
        self.targetPos.x = self.endPosX
        self.targetPos.y = self.posY
        
        Event.emitLocal("AnimPreview_StartLerpOut", {
            lerpDuration = self.lerpDuration
        })
        
        Log.info("AnimPreview: Starting lerp out to x=" .. self.targetPos.x)
    end,

    OnAnimationTransition = function(self, entity)
        Log.info("AnimPreview: OnAnimationTransition called")
        
        if self.onLerpIn then
            Log.info("AnimPreview: Still lerping in, marking transition as requested")
            self.transitionRequested = true
        elseif self.onDisplay then
            AnimPreview.StartLerpOut(self, entity)
        end
    end,

    Destroy = function(self, entity)
        for _, handler in ipairs(self.eventHandlers) do
            Event.unsubscribe(handler)
        end
    end
})

return class
