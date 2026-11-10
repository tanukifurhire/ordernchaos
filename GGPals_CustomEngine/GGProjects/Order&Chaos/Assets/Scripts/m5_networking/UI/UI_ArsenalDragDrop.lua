ArsenalDragDrop_instance = nil

local class = DefineType("ArsenalDragDrop", {
    Serializable = {},
    
    isDragging = false,
    draggedAbilityIndex = 0,
    draggedIsPassive = false,
    draggedTexture = 0,
    
    Init = function(self, entity)
        ArsenalDragDrop_instance = self
        
        -- Listen for drag start from ability icons
        Event.subscribe("OnArsenalAbilityDragStart", function(params)
            ArsenalDragDrop.StartDrag(self, params)
        end)
        
        Log.info("[ArsenalDragDrop] Initialized")
    end,
    
    StartDrag = function(self, params)
        self.isDragging = true
        self.draggedAbilityIndex = params.abilityIndex
        self.draggedIsPassive = params.isPassive
        self.draggedTexture = params.texture
        
        Log.info("[ArsenalDragDrop] Started dragging " .. 
            (params.isPassive and "passive" or "ability") .. " " .. params.abilityIndex)
    end,
    
    EndDrag = function(self)
        Log.info("[ArsenalDragDrop] Ended drag")
        self.isDragging = false
        self.draggedAbilityIndex = 0
        self.draggedIsPassive = false
        self.draggedTexture = 0
    end
})

return class