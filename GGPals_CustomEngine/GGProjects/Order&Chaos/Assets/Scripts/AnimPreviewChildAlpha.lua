local class = DefineType("AnimPreviewChildAlpha", {
    TextureAsset = {
        orderAsset = 0,
        chaosAsset = 0
    },

    onLerpIn = false,
    onLerpOut = false,
    lerpTimer = 0.0,
    lerpDuration = 0.5,
    
    lerpInHandler = 0,
    lerpOutHandler = 0,

    Init = function(self, entity)
        Log.info("AnimPreviewChildAlpha: Init called")
        
        self.lerpInHandler = Event.subscribe("AnimPreview_StartLerpIn", function(params)
            AnimPreviewChildAlpha.OnStartLerpIn(self, entity, params.lerpDuration)
        end)
        
        self.lerpOutHandler = Event.subscribe("AnimPreview_StartLerpOut", function(params)
            AnimPreviewChildAlpha.OnStartLerpOut(self, entity, params.lerpDuration)
        end)
        
        Log.info("AnimPreviewChildAlpha: Subscribed to AnimPreview events")


        Event.subscribe("Order", function(params)
            local sprite = ECS.getSprite(entity)
            if sprite then
                ECS.setSprite(entity,{
                    textureAssetID = self.orderAsset
                })
            end
        end)
        Event.subscribe("Chaos", function (params)
            local sprite = ECS.getSprite(entity)
            if sprite then
                ECS.setSprite(entity,{
                    textureAssetID = self.chaosAsset
                })
            end
        end)
    end,

    OnStartLerpIn = function(self, entity, duration)
        Log.info("AnimPreviewChildAlpha: Starting lerp in with duration " .. duration)
        self.onLerpIn = true
        self.onLerpOut = false
        self.lerpTimer = 0.0
        self.lerpDuration = duration
        
        local uiComponent = ECS.getUIComponent(entity)
        if uiComponent then
            uiComponent.colorMultiply.a = 0.0
            ECS.setUIComponent(entity, uiComponent)
        end
    end,

    OnStartLerpOut = function(self, entity, duration)
        Log.info("AnimPreviewChildAlpha: Starting lerp out with duration " .. duration)
        self.onLerpIn = false
        self.onLerpOut = true
        self.lerpTimer = 0.0
        self.lerpDuration = duration
    end,

    Update = function(self, entity)
        if not self.onLerpIn and not self.onLerpOut then
            return
        end

        self.lerpTimer = self.lerpTimer + Time.getDeltaTime()
        local t = math.min(self.lerpTimer / self.lerpDuration, 1.0)
        
        local uiComponent = ECS.getUIComponent(entity)
        if not uiComponent then
            return
        end

        if self.onLerpIn then
            uiComponent.colorMultiply.a = t
            ECS.setUIComponent(entity, uiComponent)
            
            if t >= 1.0 then
                self.onLerpIn = false
                Log.info("AnimPreviewChildAlpha: Lerp in complete")
            end
            
        elseif self.onLerpOut then
            uiComponent.colorMultiply.a = 1.0 - t
            ECS.setUIComponent(entity, uiComponent)
            
            if t >= 1.0 then
                self.onLerpOut = false
                Log.info("AnimPreviewChildAlpha: Lerp out complete")
            end
        end
    end,

    Destroy = function(self, entity)
        Event.unsubscribe(self.lerpInHandler)
        Event.unsubscribe(self.lerpOutHandler)
    end
})

return class
