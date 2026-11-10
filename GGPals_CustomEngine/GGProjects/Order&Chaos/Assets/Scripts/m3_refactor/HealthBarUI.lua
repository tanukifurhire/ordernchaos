local class = DefineType("HealthBarUI", {
    Serializable = {
        player = 0,

        -- Positions
        hiddenX = 0.0001,
        visibleX = 1.0001,
        
        -- Animation
        lerpSpeed = 8.0,

        anchorRight = 1
    },
    
    targetInterpValue = 1.0,
    currentInterpValue = 1.0,

    originalScaleX = 1.0,

    currentRawValue = 0,
    currentMaxValue = 0,

    Init = function(self, entity)
        Event.subscribe("OnHealthChanged", function(params)
            if params.player == self.player then
                Log.info("Update Healthbar! Curr Health: " .. params.curr .. " Max Health: " .. params.max)
                self.currentRawValue = params.curr
                self.currentMaxValue = params.max
            end
        end)

        Event.subscribe("PlayHealthBarAnimations", function(params)
            if self.currentMaxValue > 0 then
                local ratio = self.currentRawValue / self.currentMaxValue
                ratio = math.max(0, math.min(1, ratio))
                -- Remap [0,1] health ratio into [hiddenX, visibleX] to skip whitespace in the image
                self.targetInterpValue = self.hiddenX + ratio * (self.visibleX - self.hiddenX)
            end
        end)
    end,

    Start = function(self, entity)
        local transform = ECS.getTransform(entity)

        if transform then
            self.originalScaleX = transform.sx
        end
    end,
    
    Update = function(self, entity)
        local dt = Time.getDeltaTime()

        local transform = ECS.getTransform(entity)

        if not transform then return end

        if math.abs(self.currentInterpValue - self.targetInterpValue) > 0.01 then
            self.currentInterpValue = self.currentInterpValue + (self.targetInterpValue - self.currentInterpValue) * self.lerpSpeed * dt
            
            ECS.setTransform(entity, {
                x = self.anchorRight * (self.originalScaleX - (self.currentInterpValue * 1.0)) * 0.5,
                sx = self.currentInterpValue * 1.0
            })

            if self.anchorRight > 0 then
                ECS.setSprite(entity, {
                    uvBounds = {
                        sMin = 1.0 - self.currentInterpValue,
                        sMax = 1.0,
                        tMax = 1.0
                    }
                })
            else
                ECS.setSprite(entity, {
                    uvBounds = {
                        sMax = self.currentInterpValue,
                        tMax = 1.0
                    }
                })
            end
        else
            if self.currentInterpValue ~= self.targetInterpValue then
                self.currentInterpValue = self.targetInterpValue
                
                ECS.setTransform(entity, {
                    x = self.anchorRight * (self.originalScaleX - (self.currentInterpValue * 1.0)) * 0.5,
                    sx = self.currentInterpValue * 1.0
                })

                if self.anchorRight > 0 then
                    ECS.setSprite(entity, {
                        uvBounds = {
                            sMin = 1.0 - self.currentInterpValue,
                            sMax = 1.0,
                            tMax = 1.0
                        }
                    })
                else
                    ECS.setSprite(entity, {
                        uvBounds = {
                            sMax = self.currentInterpValue,
                            tMax = 1.0
                        }
                    })
                end
            end
        end
    end
})

return class