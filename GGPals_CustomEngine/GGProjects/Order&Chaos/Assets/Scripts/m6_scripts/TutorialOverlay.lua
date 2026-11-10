local class = DefineType("TutorialOverlay", {
    Serializable = {
        visibleX = 0.0,
        visibleY = 0.0,
        hiddenX  = -10000.0,
        hiddenY  = -10000.0,

        textX = 0.0,
        textY = 60.0,

        hintX = 0.0,
        hintY = -130.0,

        fontAssetID  = 1,
        fontSize     = 24,
        hintFontSize = 20,
    },

    TextureAsset = {
        overlayTexture = 0,
    },

    isVisible          = false,
    isClickAdvanceable = false,
    textEntity         = 0,
    hintEntity         = 0,
    openCooldown       = 0,

    Init = function(self, entity)
        if not Persistent.isTutorial then return end

        ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })

        -- Main message text
        self.textEntity = ECS.createEntity()
        ECS.addTransform(self.textEntity, self.textX, self.textY, 1, 1, 0)
        ECS.addTextComponent(self.textEntity, "", self.fontAssetID, self.fontSize)
        local tc = ECS.getTextComponent(self.textEntity)
        if tc then
            tc.visible           = false
            tc.alignment         = 1
            tc.verticalAlignment = 1
            tc.layer             = 501
            tc.maxWidth          = 700.0
            ECS.setTextComponent(self.textEntity, tc)
        end

        -- "Click anywhere to continue" hint
        self.hintEntity = ECS.createEntity()
        ECS.addTransform(self.hintEntity, self.hintX, self.hintY, 1, 1, 0)
        ECS.addTextComponent(self.hintEntity, "Click anywhere to continue",
                             self.fontAssetID, self.hintFontSize)
        local hc = ECS.getTextComponent(self.hintEntity)
        if hc then
            hc.visible           = false
            hc.alignment         = 1
            hc.verticalAlignment = 1
            hc.layer             = 501
            hc.color             = { r=0.65, g=0.65, b=0.65, a=1.0 }
            ECS.setTextComponent(self.hintEntity, hc)
        end

        Event.subscribe("OnTutorialShowOverlay", function(params)
            TutorialOverlay.Show(self, entity, params)
        end)

        Event.subscribe("OnTutorialHideOverlay", function()
            TutorialOverlay.Hide(self, entity)
        end)
    end,

    Show = function(self, entity, params)
        self.isVisible          = true
        self.isClickAdvanceable = (params.clickAdvanceable ~= false)
        self.openCooldown       = 10

        local showBg  = (params.showBackground ~= false)
        local offsetY = params.textOffsetY or 0
        local overlayScaleX = params.overlayScaleX or 1.0
        local overlayScaleY = params.overlayScaleY or 1.0
        local overlayOffsetX = params.overlayOffsetX or 0.0
        local overlayOffsetY = params.overlayOffsetY or 0.0
        local textOffsetX = params.textOffsetX or 0.0

        if showBg then
            if self.overlayTexture ~= 0 then
                ECS.setSprite(entity, { textureAssetID = self.overlayTexture })
            end
            ECS.setTransform(entity, { 
                x = self.visibleX + overlayOffsetX, 
                y = self.visibleY + overlayOffsetY
            })
        else
            ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })
        end

        local tc = ECS.getTextComponent(self.textEntity)
        if tc then
            tc.text    = params.text or ""
            if params.fontSize then
                tc.fontSize = params.fontSize 
            end
            tc.visible = true
            ECS.setTextComponent(self.textEntity, tc)
        end
        ECS.setTransform(self.textEntity, { x = self.textX + overlayOffsetX + textOffsetX, y = self.textY + overlayOffsetY + offsetY})

        local hc = ECS.getTextComponent(self.hintEntity)
        if hc then
            hc.visible = self.isClickAdvanceable
            ECS.setTextComponent(self.hintEntity, hc)
        end
        ECS.setTransform(self.hintEntity, { x = self.hintX + overlayOffsetX + textOffsetX, y = self.hintY + overlayOffsetY + offsetY })

        Log.info("[TutorialOverlay] Shown (clickAdvanceable=" ..
                 tostring(self.isClickAdvanceable) ..
                 ", showBg=" .. tostring(showBg) .. ")")
    end,

    Hide = function(self, entity)
        self.isVisible          = false
        self.isClickAdvanceable = false

        ECS.setTransform(entity, { x = self.hiddenX, y = self.hiddenY })

        local tc = ECS.getTextComponent(self.textEntity)
        if tc then
            tc.visible = false
            ECS.setTextComponent(self.textEntity, tc)
        end
        local hc = ECS.getTextComponent(self.hintEntity)
        if hc then
            hc.visible = false
            ECS.setTextComponent(self.hintEntity, hc)
        end

        Log.info("[TutorialOverlay] Hidden")
    end,

    Update = function(self, entity)
        if not self.isVisible then return end
        if not self.isClickAdvanceable then return end
        if self.openCooldown > 0 then
            self.openCooldown = self.openCooldown - 1
            return
        end
        if Input.isMouseButtonTriggered(MOUSE_LEFT) then
            TutorialOverlay.Hide(self, entity)
            Event.emitLocal("OnTutorialOverlayContinue", {})
        end
    end,
})

return class