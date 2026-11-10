local class = DefineType("ArsenalBackButton", {
    Serializable = {},

    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
        ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY })
    end,
    
    OnHoverExit = function(self, entity)
        Event.emitLocal("PlayMainMenuClick", {})
        ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY })
    end,
    
    OnClick = function(self, entity, x, y)
        Event.emitLocal("OnClickArsenalBack", {})
    end
})

return class