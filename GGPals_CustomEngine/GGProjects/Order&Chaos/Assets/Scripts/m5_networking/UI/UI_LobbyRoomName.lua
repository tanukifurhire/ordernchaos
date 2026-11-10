local class = DefineType("LobbyRoomName", {
    Init = function(self, entity)
        local name = Persistent.sessionRoomName or "Room"
        ECS.setTextComponent(entity, { text = name })
    end
})

return class
