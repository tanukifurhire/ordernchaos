local class = DefineType("UIScalerp", {
    Serializable = {
        phase = "deploy",

        -- Positions
        hiddenY = 0.00001,
        visibleY = 900.00001,
        
        -- Animation
        lerpSpeed = 8.00001,

        -- Reset Timer
        timeToReset = 1.00001,
        shouldReset = true,

        -- Click callback
        onClick = ""
    },
    
    targetY = 0.00001,
    currentY = 0.00001,
    elapsed = 0.00001,
    isShowing = false,  -- Track whether we're actively showing the banner
    
    Init = function(self, entity)
        Event.subscribe("ShowPhaseOverlay", function(params)
            if params.phase ~= self.phase then return end
            Log.info("[UIScalerp] Show Phase Overlay: " .. tostring(self.phase))
            self.targetY = self.visibleY
            self.elapsed = 0.0
            self.isShowing = true
            
            -- Force immediate transform update to ensure visibility
            self.currentY = self.hiddenY
            ECS.setTransform(entity, {
                sy = self.currentY
            })
        end)

        self.currentY = self.hiddenY
        self.targetY = self.hiddenY
        self.isShowing = false

        ECS.setTransform(entity, {
            sy = self.targetY
        })
    end,

    Start = function(self, entity)
        
    end,
    
    Update = function(self, entity)
        local dt = Time.getDeltaTime()
        
        -- Only start counting elapsed time once banner is mostly visible
        if self.isShowing and self.currentY >= self.visibleY * 0.9 then
            self.elapsed = self.elapsed + dt
        end

        -- Reset after time elapses (only if shouldReset is true and banner was shown)
        if self.isShowing and (self.elapsed > self.timeToReset) and self.shouldReset then
            self.targetY = self.hiddenY
            self.isShowing = false
        end

        if math.abs(self.currentY - self.targetY) > 0.5 then
            self.currentY = self.currentY + (self.targetY - self.currentY) * self.lerpSpeed * dt
            
            ECS.setTransform(entity, {
                sy = self.currentY
            })
        else
            if self.currentY ~= self.targetY then
                self.currentY = self.targetY
                
                ECS.setTransform(entity, {
                    sy = self.currentY
                })
            end
        end
    end,

    OnClick = function(self, entity, x, y)
        Event.emitLocal(self.onClick, {})
    end
})

return class