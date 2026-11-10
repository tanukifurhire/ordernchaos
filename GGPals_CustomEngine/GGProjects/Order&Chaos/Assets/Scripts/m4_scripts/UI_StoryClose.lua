local class = DefineType("StoryCloseButton", {
    Serializable = {},
    
    Init = function(self, entity)
    end,
    
    Start = function(self, entity)
    end,
    
    Update = function(self, entity)
    end,
    
    OnClick = function(self, entity, x, y)
        Log.info("[StoryCloseButton] Closing Story")
        Event.emitLocal("OnStoryClose", {})
    end
})

return class