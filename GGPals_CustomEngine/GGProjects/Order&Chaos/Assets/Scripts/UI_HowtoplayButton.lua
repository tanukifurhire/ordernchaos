local class = DefineType("UI_HowToPlayButton", {
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    Init = function(self, entity)
    end,
    
    Start = function(self, entity)
    end,
    
    Update = function(self, entity)
    end,

    OnHoverEnter = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.hoverSizeX,
            sy = self.hoverSizeY
        })
    end,

    OnHoverExit = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.normalSizeX,
            sy = self.normalSizeY
        })
    end,
    
    OnClick = function(self, entity, x, y)
        Log.info("[HowToPlayButton] Opening How To Play")
        Event.emitLocal("OnHowToPlayOpen", {})
    end
})

return class