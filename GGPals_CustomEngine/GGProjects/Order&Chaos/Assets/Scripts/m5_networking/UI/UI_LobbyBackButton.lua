local class = DefineType("LobbyBackButton", {
    Serializable = {
        textInputName = "MyTextInput"
    },
    
    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },
    
    Init = function(self, entity)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,
    
    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_HOVER_MULTIPLY
        })

        if self.hoverTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.hoverTexture })
        end
    end,
    
    OnHoverExit = function(self, entity)
        ECS.setUIComponent(entity, {
            colorMultiply = CONSTANTS.COLOR_NORMAL_MULTIPLY
        })
        
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
    end,
    
    OnClick = function(self, entity, x, y)
        Event.emitLocal("PlayMainMenuClick", {})
        Log.info("[LobbyBackButton] Disconnecting and returning to mode selection")

        Event.emitLocal("TextInputDeactivate", {
            target = self.textInputName
        })

        -- Tear down in the reverse of the order it came up, and tear ALL of it
        -- down: on a listen server this same process owns a client, a server
        -- AND a Steam lobby, and leaving any of them running strands the lobby
        -- in everyone's browser with no UI attached to it.
        --
        -- Through the shared helper rather than step by step. This button used
        -- to open-code the sequence, which is how it drifted out of step with
        -- the other exits - it left the server's room list behind, so the next
        -- room this player hosted was found sitting in the old one.
        NetworkSessionTeardown("left the lobby browser")

        Event.emitLocal("RequestSceneTransition", {
            scene = "M5ModeSelectScene"
        })
    end
})

return class