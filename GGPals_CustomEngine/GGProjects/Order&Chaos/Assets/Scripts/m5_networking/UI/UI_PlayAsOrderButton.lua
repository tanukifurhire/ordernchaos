local class = DefineType("PlayAsOrderButton", {
    Serializable = {},
    
    OnClick = function(self, entity, x, y)
        Persistent.sessionFaction = CONSTANTS.ORDER_PLAYER
        Log.info("[PlayAsOrderButton] Selected Order faction")
        Scene.setActive("M5ModeSelectScene")
    end
})

return class