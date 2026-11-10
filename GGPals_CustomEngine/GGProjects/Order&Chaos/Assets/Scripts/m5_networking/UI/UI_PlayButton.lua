local class = DefineType("PlayButton", {
    Serializable = {},

    TextureAsset = {
        normalTexture = 0,
        hoverTexture = 0
    },

    isHovering = false,
    bothReady = false,

    hiddenTint  = { r = 1.0, g = 1.0, b = 1.0, a = 0.0 },
    darkenTint  = { r = 0.6, g = 0.6, b = 0.6, a = 1.0 },
    fullTint    = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 },

    Init = function(self, entity)
        Event.subscribe("OnLobbyRoomStateChanged", function(params)
            self.bothReady = params.orderReady and params.chaosReady and params.orderJoined and params.chaosJoined
            self:UpdateVisual(entity)
        end)

        -- Hidden by default until both players are ready
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
            ECS.setUIComponent(entity, { colorMultiply = self.darkenTint })
        end
    end,

    UpdateVisual = function(self, entity)
        if self.normalTexture == 0 then return end

        if not self.bothReady then
            ECS.setUIComponent(entity, { colorMultiply = self.darkenTint })
        elseif self.isHovering then
            ECS.setSprite(entity, { textureAssetID = self.hoverTexture })
            ECS.setUIComponent(entity, { colorMultiply = self.fullTint })
        else
            ECS.setSprite(entity, { textureAssetID = self.normalTexture })
            ECS.setUIComponent(entity, { colorMultiply = self.fullTint })
        end
    end,

    OnHoverEnter = function(self, entity)
        if not self.bothReady then return end
        Event.emitLocal("PlayMainMenuHoverBling", {})
        self.isHovering = true
        self:UpdateVisual(entity)
    end,

    OnHoverExit = function(self, entity)
        self.isHovering = false
        self:UpdateVisual(entity)
    end,

    OnClick = function(self, entity, x, y)
        if not self.bothReady then return end
        Event.emitLocal("PlayMainMenuClick", {})

        Event.emitLocal("Transitionfade",{})
        Event.subscribe("TransitionComplete",function()

        if NetworkedGameClient_instance then
            local roomID = Persistent.sessionRoomID or -1
            NetworkedGameClient_instance:StartGame(roomID)
        else
            Scene.setActive("M5NetworkGameScene")
        end
        end)
    end
})

return class