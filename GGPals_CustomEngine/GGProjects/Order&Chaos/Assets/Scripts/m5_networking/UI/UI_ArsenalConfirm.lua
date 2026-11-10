local COLOR_DISABLED = { r = 0.4, g = 0.4, b = 0.4, a = 1.0 }

local class = DefineType("ArsenalConfirm", {
    Serializable = {},

    confirmed = false,

    Init = function(self, entity)
        -- Enable/disable from ArsenalUI (networked confirm flow)
        Event.subscribe("OnArsenalConfirmEnabled", function(params)
            if params.enabled then
                ArsenalConfirm.Enable(self, entity)
            else
                ArsenalConfirm.Disable(self, entity)
            end
        end)

        -- Reset on faction switch (PnP Order->Chaos)
        Event.subscribe("OnArsenalFactionReady", function(params)
            ArsenalConfirm.Enable(self, entity)
        end)
    end,

    Enable = function(self, entity)
        self.confirmed = false
        ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY })
    end,

    Disable = function(self, entity)
        self.confirmed = true
        ECS.setUIComponent(entity, { colorMultiply = COLOR_DISABLED })
    end,

    OnHoverEnter = function(self, entity)
        if self.confirmed then return end
        Event.emitLocal("PlayMainMenuHoverBling", {})
        ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY })
    end,

    OnHoverExit = function(self, entity)
        if self.confirmed then return end
        ECS.setUIComponent(entity, { colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY })
    end,

    OnClick = function(self, entity, x, y)
        if self.confirmed then return end
        Event.emitLocal("PlayMainMenuClick", {})
        Log.info("[ArsenalConfirm] Confirm clicked")
        Event.emitLocal("OnClickArsenalConfirm", {})
    end
})

return class