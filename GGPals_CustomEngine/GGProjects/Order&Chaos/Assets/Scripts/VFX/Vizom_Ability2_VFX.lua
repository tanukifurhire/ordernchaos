local class = DefineType("Vizom_Ability2_VFX", {
    subscriptions = {},
    
    Init = function(self, entity)
        self.subscriptions = {}
        
        self.subscriptions[#self.subscriptions + 1] = 
        Event.subscribe("InPlanningPhase", function(params)
            if params.player ~= CONSTANTS.CHAOS_PLAYER then 
                Vizom_Ability2_VFX.Hide(self, entity)
                return 
            end

            Vizom_Ability2_VFX.Show(self, entity)
        end)

        self.subscriptions[#self.subscriptions + 1] = 
        Event.subscribe("OnTrapTriggered", function(params)
            if params.vfxEntity == entity then
                Vizom_Ability2_VFX.Show(self, entity)
                Vizom_Ability2_VFX.PlayAnimation(self, entity, "Trigger")
            end
        end)

        self.subscriptions[#self.subscriptions + 1] = 
        Event.subscribe("InResolutionPhase", function()
            Vizom_Ability2_VFX.Hide(self, entity)
        end)
    end,
    
    OnDestroy = function(self, entity)
        for _, subId in ipairs(self.subscriptions) do
            Event.unsubscribe(subId)
        end
        self.subscriptions = {}
    end,

    Start = function(self, entity)
        Vizom_Ability2_VFX.PlayAnimation(self, entity, "Fire")
    end,

    PlayAnimation = function(self, entity, stateName)
        -- Check if entity has animator component
        local animInfo = ECS.getAnimation(entity)
        if not animInfo then
            -- No animation component, skip silently
            return
        end
        
        -- Don't switch if already playing this state
        if animInfo.currentState == stateName then
            return
        end
        
        -- Set the new animation state
        ECS.setAnimation(entity, stateName)
    end,

    OnVFXAnimationTransition = function(self, entity)
        Event.emitLocal("PlayVFXTirionVizomA2", {})
        Event.emitLocal("FocusChaosGrid", {})
    end,

    OnVFXAnimationFinished = function(self, entity)
        ECS.destroyEntityDeferred(entity)
    end,

    Show = function(self, entity)
        ECS.setMeshRenderer(entity, {
            colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY
        })
    end,

    Hide = function(self, entity)
        ECS.setMeshRenderer(entity, {
            colorMultiply = CONSTANTS.COLOR_NONE
        })
    end
})

return class
