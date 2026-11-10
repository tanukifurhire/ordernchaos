local class = DefineType("UI_TextInputButton", {
    Serializable = {
        textInputName = "MyTextInput"
    },

    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_TEXT_FIELD_HOVER_MULTIPLY
        })
    end,

    OnHoverExit = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_TEXT_FIELD_NORMAL_MULTIPLY
        })
    end,

    OnClick = function(self, entity, x, y)
        Event.emitLocal("PlayMainMenuClick", {})
        Event.emitLocal("TextInputActivate", {
            target = self.textInputName
        })
    end
})

return class
