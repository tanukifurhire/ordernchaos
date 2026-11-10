local class = DefineType("ModeSelectionBackButton", {
    Serializable = {},
    
    OnClick = function(self, entity, x, y)
        Event.emitLocal("PlayMainMenuClick", {})
        
        Log.info("[ModeSelectionBackButton] Going back to main menu")
        Event.emitLocal("OnClickModeBack", {})
    end,

    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})

        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY
        })
    end,
    
    OnHoverExit = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY
        })
    end,
})

return class
