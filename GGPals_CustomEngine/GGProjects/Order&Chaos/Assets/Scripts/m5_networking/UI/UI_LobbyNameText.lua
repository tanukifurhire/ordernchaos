local class = DefineType("LobbyNameText", {
    UpdateDisplay = function(self, entity, roomID, roomName, orderJoined, chaosJoined, roomStatus)
        local textComponent = ECS.getTextComponent(entity)
        if not textComponent then return end

        textComponent.text = roomName

        ECS.setTextComponent(entity, textComponent)
    end
})

return class