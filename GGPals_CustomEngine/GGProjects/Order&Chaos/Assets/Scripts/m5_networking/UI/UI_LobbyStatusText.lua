local class = DefineType("LobbyStatusText", {
    UpdateDisplay = function(self, entity, roomID, roomName, orderJoined, chaosJoined, roomStatus)
        local textComponent = ECS.getTextComponent(entity)
        if not textComponent then return end

        textComponent.text = roomStatus

        ECS.setTextComponent(entity, textComponent)
    end
})

return class