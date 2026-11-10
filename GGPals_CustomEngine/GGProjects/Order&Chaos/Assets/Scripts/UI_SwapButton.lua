local passAndPlayDisplayed = false

local class = DefineType("SwapGridButton", {
    Serializable = {
        visibleX = 0.0,
        visibleY = 0.0,
        hiddenX = -10000,
        hiddenY = -10000,
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    focusedOnOrder = true,

    Init = function(self, entity)
        -- Start hidden offscreen
        ECS.setTransform(entity, {
            x = self.hiddenX,
            y = self.hiddenY
        })

        Event.subscribe("ShowPhaseOverlay", function(params)
            if params.phase == "planphase" then
                ECS.setTransform(entity, {
                    x = self.visibleX,
                    y = self.visibleY
                })
            else
                ECS.setTransform(entity, {
                    x = self.hiddenX,
                    y = self.hiddenY
                })
            end
        end)

        Event.subscribe("OnPassAndPlayDisplayed", function()
            passAndPlayDisplayed = true
        end)

        Event.subscribe("OnPassAndPlayHidden", function()
            passAndPlayDisplayed = false
        end)
    end,

    Start = function(self, entity)
    end,

    Update = function(self, entity)
    end,

    OnHoverEnter = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_BUTTON_HOVER_MULTIPLY
        })
    end,

    OnHoverExit = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY
        })
    end,

    OnClick = function(self, entity, x, y)
        -- disable camera focus swaps onPassAndPlayDisplayed
        if passAndPlayDisplayed then
            return
        end
        
        if self.focusedOnOrder then
            Event.emitLocal("FocusChaosGrid", {})
            Log.info("[SwapGridButton] Swapping to Chaos grid")
        else
            Event.emitLocal("FocusOrderGrid", {})
            Log.info("[SwapGridButton] Swapping to Order grid")
        end

        self.focusedOnOrder = not self.focusedOnOrder
    end
})

return class