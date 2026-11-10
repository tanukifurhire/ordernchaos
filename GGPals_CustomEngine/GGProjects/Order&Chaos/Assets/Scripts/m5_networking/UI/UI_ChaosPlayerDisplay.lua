local class = DefineType("ChaosPlayerDisplay", {
    Serializable = {},
    
    TextureAsset = {
        emptySprite = 0,
        playerSprite = 0,
        readySprite = 0,
        hoverSprite = 0
    },
    
    currentState = "empty",
    isHovering = false,
    
    Init = function(self, entity)
        Event.subscribe("OnLobbyRoomStateChanged", function(params)
            if params.chaosJoined then
                self.currentState = params.chaosReady and "ready" or "joined"
            else
                self.currentState = "empty"
            end
            
            self:UpdateVisual(entity)
        end)
        
        Event.subscribe("ChaosReadyButtonHover", function(params)
            self.isHovering = params.isHovering
            self:UpdateVisual(entity)
        end)
        
        self:UpdateVisual(entity)
    end,
    
    UpdateVisual = function(self, entity)
        local tex = self.emptySprite
        
        if self.currentState == "empty" then
            tex = self.emptySprite
        elseif self.currentState == "joined" then
            tex = self.playerSprite
        elseif self.currentState == "ready" then
            tex = self.isHovering and self.hoverSprite or self.readySprite
        end
        
        if tex > 0 then
            ECS.setSprite(entity, { textureAssetID = tex })
        end
    end
})

return class