local class = DefineType("UnitStatus", {
    Serializable = {
        statusType = 0,
        stackFontAssetID = 0,
        stackFontSize = 48,
        stackTextOffsetX = 0.5,
        stackTextOffsetY = 0.5,
        stackTextScaleX = 0.015,
        stackTextScaleY = 0.015,
        blinkMinAlpha = 0.35,
        blinkMaxAlpha = 1.0,
        blinkSpeed = 8.0
    },

    ownerUnit = 0,
    appliedUnit = 0,
    stackCount = 1,
    isExpiring = false,
    _stackTextEntity = nil,

    EnsureStackText = function(self, entity)
        local textChild = nil

        local children = ECS.getChildren(entity)

        for _, child in ipairs(children) do
            local text = ECS.getTextComponent(child)

            if text then textChild = child end
        end

        self._stackTextEntity = textChild
    end,

    SyncStackTextTransform = function(self, entity)
        if not self._stackTextEntity or self._stackTextEntity == 0 then return end
        local rootTransform = ECS.getTransform(entity)
        if not rootTransform then return end
        ECS.setTransform(self._stackTextEntity, {
            x = self.stackTextOffsetX,
            y = self.stackTextOffsetY,
            scaleX = self.stackTextScaleX,
            scaleY = self.stackTextScaleY,
            rotation = 0.0
        })
    end,

    UpdateStackText = function(self)
        if not self._stackTextEntity or self._stackTextEntity == 0 then return end

        local shouldShow = (self.stackCount or 1) > 1
        if shouldShow then
            ECS.setTextComponent(self._stackTextEntity, {
                text = tostring(self.stackCount),
                visible = true
            })
        else
            ECS.setTextComponent(self._stackTextEntity, { visible = false })
        end
    end,

    ShowDisplay = function(self, entity, ownerUnit, targetUnit, statusType, stackCount, isExpiring)
        if statusType ~= self.statusType then return end

        self.appliedUnit = targetUnit
        self.ownerUnit = ownerUnit
        self.stackCount = math.max(1, tonumber(stackCount) or 1)
        self.isExpiring = isExpiring and true or false

        self:EnsureStackText(entity)
        self:UpdateStackText()

        local children = ECS.getChildren(entity)
        for _, e in ipairs(children) do
            local scriptInstance = ECS.getScriptInstance(e)

            if scriptInstance and scriptInstance.ShowDisplay then
                scriptInstance:ShowDisplay(e, self.ownerUnit, self.stackCount, self.isExpiring)
            end
        end
    end,

    Update = function(self, entity)
        self:SyncStackTextTransform(entity)

        local uiComponent = ECS.getUIComponent(entity)
        if not uiComponent then return end

        if self.isExpiring then
            local t = Time.getTotalTime() * self.blinkSpeed
            local phase = (math.sin(t) + 1.0) * 0.5
            local alpha = self.blinkMinAlpha + (self.blinkMaxAlpha - self.blinkMinAlpha) * phase
            uiComponent.colorMultiply.a = alpha
        else
            uiComponent.colorMultiply.a = 1.0
        end
        ECS.setUIComponent(entity, uiComponent)
    end,

    Destroy = function(self, entity, targetUnit, statusType)
        if (self.statusType == statusType) and
            (targetUnit == nil or self.appliedUnit == targetUnit) then
            local uiComponent = ECS.getUIComponent(entity)
            if uiComponent then
                uiComponent.colorMultiply.a = 1.0
                ECS.setUIComponent(entity, uiComponent)
            end
            if self._stackTextEntity and self._stackTextEntity ~= 0 then
                ECS.destroyEntityDeferred(self._stackTextEntity)
                self._stackTextEntity = nil
            end
            ECS.destroyEntityDeferred(entity)
        end
    end,

    OnHoverEnter = function(self, entity)
        local t = ECS.getWorldPosition(entity)

        if t then
            Event.emitLocal("ShowAbilityTooltip", {
                abilityID = self.statusType,
                x = t.x,
                y = t.y,
                status = true,
                expiring = self.isExpiring,
                stacks = self.stackCount
            })
        end
    end,

    OnHoverExit = function(self, entity)
        Event.emitLocal("HideAbilityTooltip", {})
    end
})

return class
