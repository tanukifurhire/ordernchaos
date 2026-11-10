-- Continue button for Pass And Play screen

local class = DefineType("PassAndPlayContinueButton", 
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    Init = function(self, entity)
        Log.info("[PassAndPlayContinueButton] Initialized")
    end, -- end of Init

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
        -- confirm pass and play continue
        Event.emitLocal("OnPassAndPlayContinue", {})
    end
})

return class