local class = DefineType("Background",
{
    Serializable = 
    {
        isOrderBackground = false
    },

    Init = function(self, entity)
        if self.isOrderBackground then
            Event.subscribe("FocusOrderGrid", function(params)
                Log.info("Setting camera focus to Order")
                Camera.setFocusEntity(entity)
            end)
        else
            Event.subscribe("FocusChaosGrid", function(params)
                Log.info("Setting camera focus to Chaos")
                Camera.setFocusEntity(entity)
            end)
        end
    end,

    Start = function(self, entity)

    end,

    Update = function(self, entity)

    end
})

return class