local class = DefineType("PassAndPlayScreen2",
{
    ------------------------------------------------------------------
    TextureAsset =
    {
        orderSprite = 0, -- Order just played → pass to Chaos
        chaosSprite = 0, -- Chaos just played → pass to Order
        plainSprite = 0  -- Optional: default sprite
    },

    hidePosX = -9999,
    hidePosY = -9999,
    displayPosX = 0,
    displayPosY = 0,

    currentX = 0,
    currentY = 0,
    targetX = 0,
    targetY = 0,
    lerpSpeed = 12.0,

    visible = false,
    inResolution = false,

    lastPlayer = CONSTANTS.ORDER_PLAYER, -- track player who just finished
    currentPlayer = CONSTANTS.ORDER_PLAYER, -- current turn player

    ------------------------------------------------------------------
    Init = function(self, entity)

        -- Initialize offscreen
        self.currentX = self.hidePosX
        self.currentY = self.hidePosY
        self.targetX = self.hidePosX
        self.targetY = self.hidePosY
        ECS.setTransform(entity, { x = self.currentX, y = self.currentY })

        ------------------------------------------------------------------
        -- Helper: update sprite based on last player
        ------------------------------------------------------------------
        local function UpdateSprite()
            local sprite =
                (self.lastPlayer == CONSTANTS.ORDER_PLAYER)
                    and self.orderSprite
                    or self.chaosSprite

            ECS.setSprite(entity, { textureAssetID = sprite })
        end

        ------------------------------------------------------------------
        -- Show / hide
        ------------------------------------------------------------------
        function self:Show()
            if self.visible then return end
            self.visible = true
            self.targetX = self.displayPosX
            self.targetY = self.displayPosY
            UpdateSprite()
            Event.emitLocal("OnPassAndPlayDisplayed", {})
        end

        function self:Hide()
            if not self.visible then return end
            self.visible = false
            self.targetX = self.hidePosX
            self.targetY = self.hidePosY
            Event.emitLocal("OnPassAndPlayHidden", {})
        end

        ------------------------------------------------------------------
        -- Track current player
        ------------------------------------------------------------------
        Event.subscribe("TurnChanged", function(params)
            if params and params.currentPlayer then
                -- Save who just finished (opposite of new current player)
                self.lastPlayer = self.currentPlayer
                self.currentPlayer = params.currentPlayer
            end
        end)

        ------------------------------------------------------------------
        -- Resolution phase flag
        ------------------------------------------------------------------
        Event.subscribe("InResolutionPhase", function()
            self.inResolution = true
            self:Hide()
        end)

        Event.subscribe("InPlanningPhase", function()
            self.inResolution = false
        end)

        ------------------------------------------------------------------
        -- Normal End Turn: show pass screen
        ------------------------------------------------------------------
        Event.subscribe("OnEndActionButtonPressed", function()
            if self.inResolution then return end
            self:Show()
        end)

        ------------------------------------------------------------------
        -- After resolution ends: show pass screen before deployment
        ------------------------------------------------------------------
        Event.subscribe("OnChangePhase", function(params)
            -- Show pass screen when transitioning FROM resolution TO deployment
            if self.inResolution then
                -- Resolution just ended, about to start deployment
                self.inResolution = false
                self:Show()
            end
        end)

        ------------------------------------------------------------------
        Event.subscribe("OnPassAndPlayContinue", function()
            self:Hide()
            Event.emitLocal("OnPassAndPlayConfirmed", {})
        end)

        ------------------------------------------------------------------
        -- Initial sprite
        ------------------------------------------------------------------
        ECS.setSprite(entity, { textureAssetID = self.plainSprite or self.orderSprite })
    end,

    ------------------------------------------------------------------
    Update = function(self, entity)
        local dt = Time.getDeltaTime()
        local t = ECS.getTransform(entity)

        self.currentX = self.currentX + (self.targetX - self.currentX) * self.lerpSpeed * dt
        self.currentY = self.currentY + (self.targetY - self.currentY) * self.lerpSpeed * dt

        t.x = self.currentX
        t.y = self.currentY
        ECS.setTransform(entity, t)
    end
})

return class