local class = DefineType("AbilityBarSlide", {
    Serializable = {
        isOrderBar = true,
        
        -- Positions
        hiddenY = -500.00001,
        visibleY = 0.00001,
        xPosition = 0.00001,
        
        -- Animation
        lerpSpeed = 8.00001
    },
    
    targetY = 0.00001,
    currentY = 0.00001,
    
    Init = function(self, entity)
        Event.subscribe("TurnChanged", function(params)
            AbilityBarSlide.CheckPlayerSwitch(self, entity, params.currentPlayer)
        end)

        Event.subscribe("ShowOrderAbilities", function(params)
            if self.isOrderBar then
                self.targetY = self.visibleY
            end
        end)

        Event.subscribe("HideOrderAbilities", function(params)
            if self.isOrderBar then
                self.targetY = self.hiddenY
            end
        end)

        Event.subscribe("ShowChaosAbilities", function(params)
            if not self.isOrderBar then
                self.targetY = self.visibleY
            end
        end)

        Event.subscribe("HideChaosAbilities", function(params)
            if not self.isOrderBar then
                self.targetY = self.hiddenY
            end
        end)
    end,

    Start = function(self, entity)
        ECS.setTransform(entity, {
            x = self.xPosition,
            y = self.currentY
        })
        
        Log.info("[AbilityBarSlide] " .. (self.isOrderBar and "Order" or "Chaos") .. " bar initialized")
    end,
    
    Update = function(self, entity)
        local dt = Time.getDeltaTime()
        
        if math.abs(self.currentY - self.targetY) > 0.5 then
            self.currentY = self.currentY + (self.targetY - self.currentY) * self.lerpSpeed * dt
            
            ECS.setTransform(entity, {
                x = self.xPosition,
                y = self.currentY
            })
        else
            if self.currentY ~= self.targetY then
                self.currentY = self.targetY
                
                ECS.setTransform(entity, {
                    x = self.xPosition,
                    y = self.currentY
                })
            end
        end
    end,
    
    CheckPlayerSwitch = function(self, entity, currentPlayer)
        -- Order bar (P1) shows when it's P1's turn
        -- Chaos bar (P2) shows when it's P2's turn
        local shouldShow = (self.isOrderBar and currentPlayer == 1) or (not self.isOrderBar and currentPlayer == 2)

        self.targetY = shouldShow and self.visibleY or self.hiddenY
        
        Log.info("[AbilityBarSlide] " .. (self.isOrderBar and "Order" or "Chaos") .. 
                " bar switching to " .. (shouldShow and "visible" or "hidden") .. 
                " for player " .. currentPlayer)
    end
})

return class