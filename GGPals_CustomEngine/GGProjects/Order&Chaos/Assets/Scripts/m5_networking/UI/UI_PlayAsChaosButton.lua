local class = DefineType("PlayAsChaosButton", {
    Serializable = {},
    
    OnClick = function(self, entity, x, y)
        Persistent.sessionFaction = CONSTANTS.CHAOS_PLAYER
        Log.info("[PlayAsChaosButton] Selected Chaos faction")
        Scene.setActive("M5ModeSelectScene")
    end
})

return class