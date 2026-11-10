-- UI_PassAndPlayButton.lua
-- Handles Pass and Play mode button with event-based hover

local class = DefineType("PassAndPlayButton", {
    Serializable = {
        buttonState = 1,  -- 1 = pass and play
        targetSceneName = ""
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
        
        Log.info("[PassAndPlayButton] Initialized")
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
        
        Log.info("[PassAndPlayButton] Hover enter")
    end,
    
    OnHoverExit = function(self, entity)
        self.isHovering = false
        
        -- Switch back to normal texture
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        end
        
        -- Emit hover exit event
        Event.emitLocal("ModeSelectionHover", { state = 0 })
        
        Log.info("[PassAndPlayButton] Hover exit")
    end,
    
    OnClick = function(self, entity, x, y)
        Log.info("[PassAndPlayButton] Pass and Play mode selected")
        
        -- PnP mode: Persistent.connected is false (no server connection)
        Event.emitLocal("PlayMainMenuClick", {})
        
        -- Navigate directly to M5GameScene
        if self.targetSceneName ~= "" then
            Event.emitLocal("RequestSceneTransition", {
                scene = self.targetSceneName
            })
        end
    end
})

return class