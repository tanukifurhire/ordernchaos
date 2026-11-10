local class = DefineType("AnimPreviewChildTextAlpha", {
    abilityNames = {
        Oracle_Ability1 = "Crystal Cascade",
        Oracle_Ability2 = "Fiberglass Finesse",
        Dragoon_Ability1 = "Azure Blaze",
        Dragoon_Ability2 = "Feather Cyclone",
        Imp_Ability1 = "Imp-Pact!",
        Imp_Ability2 = "Wicked Shift",
        Eye_Ability1 = "Ruinous Gaze",
        Eye_Ability2 = "Fractured Earth",
        Tirion_Ability1 = "Luminous Cleave",
        Tirion_Ability2 = "Sigil of Consecration",
        Vizom_Ability1 = "Umbral Cleave",
        Vizom_Ability2 = "Sigil of Malevolence",
        CrimsonSlash = "Crimson Slash",
        SapphireSwirl = "Sapphire Swirl"
    },
    
    eventHandlers = {},

    Init = function(self, entity)
        Log.info("AnimPreviewChildTextAlpha: Init called")
        
        -- Subscribe to lerp events for visibility
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("AnimPreview_StartLerpIn", function(params)
            AnimPreviewChildTextAlpha.OnStartLerpIn(self, entity)
        end)
        
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("AnimPreview_StartLerpOut", function(params)
            AnimPreviewChildTextAlpha.OnStartLerpOut(self, entity)
        end)
        
        -- Subscribe to trigger events for text updates
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerOracle1AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Oracle_Ability1") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerOracle2AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Oracle_Ability2") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerDragoon1AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Dragoon_Ability1") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerDragoon2AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Dragoon_Ability2") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerImp1AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Imp_Ability1") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerImp2AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Imp_Ability2") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerEye1AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Eye_Ability1") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerEye2AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Eye_Ability2") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerTirion1AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Tirion_Ability1") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerTirion2AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Tirion_Ability2") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerVizom1AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Vizom_Ability1") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerVizom2AnimPreview", function() AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "Vizom_Ability2") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerGeneric1AnimPreview", function(params) AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "CrimsonSlash") end)
        self.eventHandlers[#self.eventHandlers + 1] = Event.subscribe("TriggerGeneric2AnimPreview", function(params) AnimPreviewChildTextAlpha.SetAbilityName(self, entity, "SapphireSwirl") end)
        
        Log.info("AnimPreviewChildTextAlpha: Subscribed to all events")

        Event.subscribe("oneLine", function (params)
            local transform = ECS.getTransform(entity)
        
            if transform then
                transform.y = -0.28
                ECS.setTransform(entity,transform)
            end
        end)

        Event.subscribe("twoLine", function (params)
            local transform = ECS.getTransform(entity)
        
            if transform then
                transform.y = -0.22
                ECS.setTransform(entity,transform)
            end
        end)        
    end,

    SetAbilityName = function(self, entity, abilityKey)
        local abilityName = self.abilityNames[abilityKey] or abilityKey
        
        Log.info("AnimPreviewChildTextAlpha: Setting text to '" .. abilityName .. "'")
        
        local textComponent = ECS.getTextComponent(entity)
        if textComponent then
            textComponent.text = abilityName
            ECS.setTextComponent(entity, textComponent)
        end
    end,

    OnStartLerpIn = function(self, entity)
        Log.info("AnimPreviewChildTextAlpha: Setting text visible")
        
        local textComponent = ECS.getTextComponent(entity)
        if textComponent then
            textComponent.visible = true
            ECS.setTextComponent(entity, textComponent)
        end
    end,

    OnStartLerpOut = function(self, entity)
        Log.info("AnimPreviewChildTextAlpha: Setting text invisible")
        
        local textComponent = ECS.getTextComponent(entity)
        if textComponent then
            textComponent.visible = false
            ECS.setTextComponent(entity, textComponent)
        end
    end,

    Destroy = function(self, entity)
        for _, handler in ipairs(self.eventHandlers) do
            Event.unsubscribe(handler)
        end
    end
})

return class
