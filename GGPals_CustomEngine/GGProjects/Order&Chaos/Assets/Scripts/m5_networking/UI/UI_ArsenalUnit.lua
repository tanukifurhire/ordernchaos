ArsenalUnitDisplay_instance = nil

-- Not referenced by any scene or prefab. Renamed off "ArsenalUnitDisplay",
-- which UI_ArsenalUnitDisplay.lua (the one the arsenal scenes use) declares -
-- metadata is keyed by type name, so two files sharing one is a coin flip
-- decided by load order. It happens to land the right way round today.
local class = DefineType("ArsenalUnit", {
    Serializable = {},
    
    TextureAsset = {
        dragoonSprite = 0,
        oracleSprite = 0,
        tirionSprite = 0,
        impSprite = 0,
        eyeSprite = 0,
        vizomSprite = 0
    },
    
    currentUnitType = 1,
    
    Init = function(self, entity)
        ArsenalUnitDisplay_instance = self
        
        -- Verify entity has Sprite component
        local sprite = ECS.getSprite(entity)
        if not sprite then
            Log.error("[ArsenalUnitDisplay] Entity " .. entity .. " does not have Sprite component! Add one in the editor.")
            return
        end
        
        -- Listen for unit selection changes
        Event.subscribe("OnArsenalUnitSelected", function(params)
            ArsenalUnitDisplay.ShowUnit(self, entity, params.unitType)
        end)
        
        Log.info("[ArsenalUnitDisplay] Initialized")
    end,
    
    Start = function(self, entity)
        -- Show first unit by default
        local faction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
        local defaultUnit = (faction == CONSTANTS.ORDER_PLAYER) and CONSTANTS.UNIT.DRAGOON or CONSTANTS.UNIT.IMP
        ArsenalUnitDisplay.ShowUnit(self, entity, defaultUnit)
    end,
    
    ShowUnit = function(self, entity, unitType)
        self.currentUnitType = unitType
        
        local textureID = 0
        
        if unitType == CONSTANTS.UNIT.DRAGOON then
            textureID = self.dragoonSprite
        elseif unitType == CONSTANTS.UNIT.ORACLE then
            textureID = self.oracleSprite
        elseif unitType == CONSTANTS.UNIT.TIRION then
            textureID = self.tirionSprite
        elseif unitType == CONSTANTS.UNIT.IMP then
            textureID = self.impSprite
        elseif unitType == CONSTANTS.UNIT.EYE then
            textureID = self.eyeSprite
        elseif unitType == CONSTANTS.UNIT.VIZOM then
            textureID = self.vizomSprite
        end
        
        -- Check sprite component exists
        local sprite = ECS.getSprite(entity)
        if not sprite then
            Log.error("[ArsenalUnitDisplay] Cannot set sprite - entity has no Sprite component")
            return
        end
        
        ECS.setSprite(entity, { textureAssetID = textureID })
        
        Log.info("[ArsenalUnitDisplay] Showing unit " .. unitType)
    end
})

return class