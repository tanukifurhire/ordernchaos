local class = DefineType("LobbyAvailableFaction", {
    Serializable = {
        isOrder = true
    },

    TextureAsset = {
        chaosActive = 0,
        orderActive = 0,
        inactive = 0
    },

    UpdateDisplay = function(self, entity, roomID, roomName, orderJoined, chaosJoined, roomStatus)
        Log.info("Updating Lobby Display!")
        if self.isOrder then
            if orderJoined then
                Log.info("Order joined!")
                ECS.setSprite(entity, { textureAssetID = self.orderActive })
            else
                ECS.setSprite(entity, { textureAssetID = self.inactive })
            end
        else
            if chaosJoined then
                Log.info("Chaos joined!")
                ECS.setSprite(entity, { textureAssetID = self.chaosActive })
            else
                ECS.setSprite(entity, { textureAssetID = self.inactive })
            end
        end
    end
})

return class