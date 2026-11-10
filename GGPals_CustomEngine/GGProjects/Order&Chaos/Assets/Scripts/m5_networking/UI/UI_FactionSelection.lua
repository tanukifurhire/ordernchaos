-- FactionSelectionUI.lua - Main menu faction choice
local class = DefineType("FactionSelectionUI", {
    Serializable = {},
    
    Init = function(self, entity)
        Event.subscribe("OnClickPlayAsOrder", function()
            FactionSelectionUI.SelectOrder(self, entity)
        end)
        
        Event.subscribe("OnClickPlayAsChaos", function()
            FactionSelectionUI.SelectChaos(self, entity)
        end)
    end,
    
    SelectOrder = function(self, entity)
        Log.info("[FactionSelection] Play as Order selected")
        
        Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER

        -- Go to mode selection
        Scene.setActive("M5ModeSelectScene")
    end,

    SelectChaos = function(self, entity)
        Log.info("[FactionSelection] Play as Chaos selected")

        Persistent.sessionFaction = CONSTANTS.CHAOS_PLAYER

        -- Go to mode selection
        Scene.setActive("M5ModeSelectScene")
    end
})

return class