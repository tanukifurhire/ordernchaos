local class = DefineType("Vizom_Ability2_VFX_Preview", {    
    Init = function(self, entity)
        Log.info("[Vizom_Ability2_VFX_Preview] Playing Vizom_Ability2 Preview")
    end,

    Start = function(self, entity)
        Vizom_Ability2_VFX_Preview.PlayAnimation(self, entity, "Fire")
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

    OnPreviewVFXAnimationFinished = function(self, entity)
        Event.emitLocal("PreviewVFXFinished", {})
        ECS.destroyEntityDeferred(entity)
    end
})

return class