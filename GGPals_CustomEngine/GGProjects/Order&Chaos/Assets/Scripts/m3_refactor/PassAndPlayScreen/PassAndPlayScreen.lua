-- For pass and play screen:
-- blocks screen before passing off to the next player

local class = DefineType("PassAndPlayScreen", 
{
    TextureAsset = {
        chaosToOrderScreen = 0,
        orderToChaosScreen = 0
    },

    Serializable = {
        -- off-screen position
        hidePosX = 0,
        hidePosY = -9999,

        lerpSpeed = 12.0
    },

    -- on-screen position
    displayPosX = 0,
    displayPosY = 0,

    currentX = nil,
    currentY = nil,

    targetX = nil,
    targetY = nil,

    visible = false,
    inResolution = false,

    Init = function(self, entity)
        Log.info("[PassAndPlayScreen] Initialized")

        -- start hidden
        self.currentX = self.hidePosX
        self.currentY = self.hidePosY
        self.targetX = self.hidePosX
        self.targetY = self.hidePosY

        self.visible = false
        self.inResolution = false

        local t = ECS.getTransform(entity)
        t.x = self.currentX
        t.y = self.currentY
        ECS.setTransform(entity, t)

        function self:ShowScreen()
            if self.visible then
                return
            end

            self.visible = true
            self.targetX = self.displayPosX
            self.targetY = self.displayPosY

            Log.info("[PassAndPlayScreen] Screen is being shown")
            Event.emitLocal("OnPassAndPlayDisplayed", {}) -- for disabling all other UI interactions
        end -- end of ShowScreen

        function self:HideScreen()
            if not self.visible then
                return
            end
            self.visible = false
            self.targetX = self.hidePosX
            self.targetY = self.hidePosY

            Log.info("[PassAndPlayScreen] Screen is hidden")
            Event.emitLocal("OnPassAndPlayHidden", {}) -- for enabling all other UI interactions
        end -- end of HideScreen

        Event.subscribe("InResolutionPhase", function()
            self.inResolution = true
            self:HideScreen()
        end)

        Event.subscribe("InPlanningPhase", function()
            self.inResolution = false
            self:HideScreen()
        end)

        -- display the screen once the player has finished their turn
        Event.subscribe("OnEndActionButtonPressed", function()
            -- skip if in resolution phase
            if self.inResolution then
                return
            end
            self:ShowScreen()
        end)

        -- hide when the player pressed "confirm"
        Event.subscribe("OnPassAndPlayContinue", function()
            self:HideScreen()
            Event.emitLocal("OnPassAndPlayConfirmed", {})
        end)

        Event.subscribe("TurnChanged", function(params)
            if params.currentPlayer == CONSTANTS.ORDER_PLAYER then
                -- self.currentPlayer = "order"
                local sprite = ECS.getSprite(entity)
                if sprite then
                    ECS.setSprite(entity, {
                        textureAssetID = self.chaosToOrderScreen
                    })
                end
            else
                -- self.currentPlayer = "chaos"
                local sprite = ECS.getSprite(entity)
                if sprite then
                    ECS.setSprite(entity, {
                        textureAssetID = self.orderToChaosScreen
                    })
                end
            end
        end)
    end, -- end of Init

    Update = function(self, entity)
        local dt = Time.getDeltaTime()

        local t = ECS.getTransform(entity)

        local dx = self.targetX - self.currentX
        local dy = self.targetY - self.currentY

        self.currentX = self.currentX + dx * self.lerpSpeed * dt
        self.currentY = self.currentY + dy * self.lerpSpeed * dt

        t.x = self.currentX
        t.y = self.currentY
        ECS.setTransform(entity, t)
    end -- end of Update

})

return class