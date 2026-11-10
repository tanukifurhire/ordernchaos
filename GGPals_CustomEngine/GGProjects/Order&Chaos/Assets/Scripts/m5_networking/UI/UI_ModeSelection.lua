-- ModeSelectionUI.lua - Choose Pass & Play or Online Multiplayer
local class = DefineType("ModeSelectionUI", {
    Serializable = {},
    
    Init = function(self, entity)
        Event.subscribe("OnClickPassAndPlay", function()
            ModeSelectionUI.SelectPassAndPlay(self, entity)
        end)
        
        Event.subscribe("OnClickOnlineMultiplayer", function()
            ModeSelectionUI.SelectOnlineMultiplayer(self, entity)
        end)
        
        Event.subscribe("OnClickModeBack", function()
            Event.emitLocal("Transitionfade",{})
            Event.subscribe("TransitionComplete",function()
            Scene.setActive("M5MainMenuScene")
            end)
        end)
    end,
    
    --[[SelectPassAndPlay = function(self, entity)
        Log.info("[ModeSelection] Pass & Play selected")
        Persistent.sessionGameMode = "passandplay"
        Scene.setActive("M5ArsenalScene")
    end,

    SelectOnlineMultiplayer = function(self, entity)
        Log.info("[ModeSelection] Online Multiplayer selected")
        Persistent.sessionGameMode = "online"
        Scene.setActive("M5LobbySelectionScene")
    end]]
})

return class