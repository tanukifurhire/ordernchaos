local class = DefineType("StoryNavButton", {
    Serializable = {
        isNextButton = true  -- true = right arrow (next), false = left arrow (prev)
    },
    
    Init = function(self, entity)
    end,
    
    Start = function(self, entity)
    end,
    
    Update = function(self, entity)
    end,
    
    OnClick = function(self, entity, x, y)
        if self.isNextButton then
            Log.info("[StoryNavButton] Next page")
            Event.emitLocal("OnStoryNext", {})
        else
            Log.info("[StoryNavButton] Previous page")
            Event.emitLocal("OnStoryPrev", {})
        end
    end
})

return class