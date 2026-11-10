local class = DefineType("Imp_Ability1_VFX", {
    Init = function(self, entity)
        
    end,

    Start = function(self, entity)
        Imp_Ability1_VFX.PlayAnimation(self, entity, "Fire")
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
        Event.emitLocal("Imp_Ability1_VFX_Transition", {})
    end,

    OnVFXAnimationFinished = function(self, entity)
        Event.emitLocal("PreviewVFXFinished", {})
        Event.emitLocal("TriggerVFXFinished", {})
        ECS.destroyEntityDeferred(entity)
    end
})

return class