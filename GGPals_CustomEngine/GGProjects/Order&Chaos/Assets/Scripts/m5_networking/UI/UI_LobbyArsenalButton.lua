local class = DefineType("LobbyArsenalButton", {
    Serializable = {},
    
    OnClick = function(self, entity, x, y)
        Log.info("[LobbyArsenalButton] Opening arsenal from lobby")
        Event.emitLocal("OnClickLobbyArsenal", {})
    end
})

return class
