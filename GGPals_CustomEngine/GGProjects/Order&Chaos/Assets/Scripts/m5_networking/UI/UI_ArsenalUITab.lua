local class = DefineType("ArsenalUnitBar", {
    Serializable = {
        unitType = 1  -- 1-6
    },
    
    TextureAsset = {
        normalBarTexture = 0,
        selectedBarTexture = 0,
        unitIconTexture = 0
    },
    
    isSelected = false,
    ability1Texture = 0,
    ability2Texture = 0,
    passiveTexture = 0,
    
    -- Child slot entities (set these in editor)
    ability1SlotEntity = 0,
    ability2SlotEntity = 0,
    passiveSlotEntity = 0,
    
    Init = function(self, entity)
        -- Listen for selection
        Event.subscribe("OnArsenalUnitSelected", function(params)
            if params.unitType == self.unitType then
                ArsenalUnitBar.Select(self, entity)
            else
                ArsenalUnitBar.Deselect(self, entity)
            end
        end)
        
        -- Listen for ability assignments to THIS unit
        Event.subscribe("OnAssignAbilityToCurrentUnit", function(params)
            if params.unitType == self.unitType then
                ArsenalUnitBar.UpdateSlotDisplay(self, params.slotIndex, params.texture)
            end
        end)
        
        Event.subscribe("OnAssignPassiveToCurrentUnit", function(params)
            if params.unitType == self.unitType then
                ArsenalUnitBar.UpdatePassiveDisplay(self, params.texture)
            end
        end)
    end,
    
    Start = function(self, entity)
        local faction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
        local defaultUnit = (faction == CONSTANTS.ORDER_PLAYER) and CONSTANTS.UNIT.DRAGOON or CONSTANTS.UNIT.IMP
        
        if self.unitType == defaultUnit then
            ArsenalUnitBar.Select(self, entity)
        else
            ArsenalUnitBar.Deselect(self, entity)
        end
    end,
    
    Select = function(self, entity)
        self.isSelected = true
        ECS.setSprite(entity, { textureAssetID = self.selectedBarTexture })
    end,
    
    Deselect = function(self, entity)
        self.isSelected = false
        ECS.setSprite(entity, { textureAssetID = self.normalBarTexture })
    end,
    
    UpdateSlotDisplay = function(self, slotIndex, texture)
        if slotIndex == 1 then
            self.ability1Texture = texture
            if self.ability1SlotEntity > 0 then
                ECS.setSprite(self.ability1SlotEntity, { textureAssetID = texture })
            end
        elseif slotIndex == 2 then
            self.ability2Texture = texture
            if self.ability2SlotEntity > 0 then
                ECS.setSprite(self.ability2SlotEntity, { textureAssetID = texture })
            end
        end
    end,
    
    UpdatePassiveDisplay = function(self, texture)
        self.passiveTexture = texture
        if self.passiveSlotEntity > 0 then
            ECS.setSprite(self.passiveSlotEntity, { textureAssetID = texture })
        end
    end,
    
    OnClick = function(self, entity, x, y)
        Event.emitLocal("OnArsenalUnitSelected", {
            unitType = self.unitType
        })
    end
})

return class