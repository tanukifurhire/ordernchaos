local class = DefineType("HowToPlayNavButton", {
    Serializable = {
        isNextButton = true  -- true = next, false = prev
    },
    
    Init = function(self, entity)
    end,
    
    Start = function(self, entity)
    end,
    
    Update = function(self, entity)
    end,
    
    OnClick = function(self, entity, x, y)
        if self.isNextButton then
            Log.info("[HowToPlayNavButton] Next page")
            Event.emitLocal("OnHowToPlayNext", {})
        else
            Log.info("[HowToPlayNavButton] Previous page")
            Event.emitLocal("OnHowToPlayPrev", {})
        end
    end
})

return class