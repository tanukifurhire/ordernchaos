local class = DefineType("ReadyButton", {
    Serializable = {
        isOrderSide = true
    },

    TextureAsset = {
        normalTexture = 0,
        glowTexture = 0
    },

    isReady = false,
    isHovering = false,
    canInteract = false,

    greyTint   = { r = 0.4, g = 0.4, b = 0.4, a = 1.0 },
    darkenTint = { r = 0.6, g = 0.6, b = 0.6, a = 1.0 },
    fullTint   = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 },

    Init = function(self, entity)
        Event.subscribe("OnLobbyRoomStateChanged", function(params)
            if self.isOrderSide then
                self.isReady = params.orderReady
            else
                self.isReady = params.chaosReady
            end
            self:UpdateVisual(entity)
        end)

        -- Start greyed out (not ready)
        if self.normalTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.normalTexture, colorMultiply = self.greyTint })
        end
    end,

    Start = function(self, entity)
        local myFaction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
        self.canInteract = (self.isOrderSide and myFaction == CONSTANTS.ORDER_PLAYER) or
                           (not self.isOrderSide and myFaction == CONSTANTS.CHAOS_PLAYER)
    end,

    UpdateVisual = function(self, entity)
        if self.normalTexture == 0 then return end

        local tex = self.isReady and self.glowTexture or self.normalTexture
        local tint = self.isHovering and self.darkenTint or self.fullTint

        ECS.setSprite(entity, { textureAssetID = tex })
        ECS.setUIComponent(entity, { colorMultiply = tint })
    end,

    OnHoverEnter = function(self, entity)
        if not self.canInteract then return end
        Event.emitLocal("PlayMainMenuHoverBling", {})
        self.isHovering = true
        self:UpdateVisual(entity)
    end,

    OnHoverExit = function(self, entity)
        if not self.canInteract then return end
        self.isHovering = false
        self:UpdateVisual(entity)
    end,

    OnClick = function(self, entity, x, y)
        if not self.canInteract then
            Log.warning("[ReadyButton] Cannot interact with the opposing faction's button.")
            return
        end
        Event.emitLocal("PlayMainMenuClick", {})

        self.isReady = not self.isReady

        if NetworkedGameClient_instance then
            local roomID = Persistent.sessionRoomID
            NetworkedGameClient_instance:SetReady(roomID, self.isReady)
        end

        self:UpdateVisual(entity)
    end
})

return class