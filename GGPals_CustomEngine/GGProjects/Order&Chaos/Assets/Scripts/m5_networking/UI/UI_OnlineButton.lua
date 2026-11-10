-- UI_OnlineButton.lua
-- Handles Online mode button with event-based hover

local class = DefineType("OnlineButton", {
    Serializable = {
        buttonState = 2  -- 2 = online
    },
    
    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },
    
    isHovering = false,
    
    Init = function(self, entity)
        -- Set initial texture
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
        
        Log.info("[OnlineButton] Initialized")
    end,
    
    OnHoverEnter = function(self, entity)
        self.isHovering = true
        
        Event.emitLocal("PlayMainMenuHoverBling", {})
        
        -- Switch to hover texture
        if self.hoverTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.hoverTexture })
        end
        
        -- Emit hover event for manager to handle
        Event.emitLocal("ModeSelectionHover", { state = self.buttonState })
        
        Log.info("[OnlineButton] Hover enter")
    end,
    
    OnHoverExit = function(self, entity)
        self.isHovering = false

        
        -- Switch back to normal texture
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
        
        -- Emit hover exit event
        Event.emitLocal("ModeSelectionHover", { state = 0 })
        
        Log.info("[OnlineButton] Hover exit")
    end,
    
    OnClick = function(self, entity, x, y)
        Log.info("[OnlineButton] Online mode selected")
        
        -- Game mode is determined by Persistent.connected (set by NetworkedGameClient)
        Event.emitLocal("PlayMainMenuClick", {})
        
        -- Navigate directly to lobby selection
        Event.emitLocal("RequestSceneTransition", {
            scene = "M5LobbySelectionScene"
        })
    end
})

return class