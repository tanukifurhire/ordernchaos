-- Global instance
AbilityTargetVisuals_instance = nil

local crosshairEntity = nil
local borderEntities = {}

local class = DefineType("AbilityTargetVisuals", {
    
    -- for ability grid preview
    Serializable = {
        hoverDuration = 1.5,
    },
    hoverTimer = 0,
    lastHoveredTile = nil,
    vfxFinishedPlaying = true,
    gridPreviewCall = "",
    ---------------------------

    TextureAsset = {
        emptyTexture = 0,
        crosshairTexture = 0,
        borderTexture = 0
    },
    
    isActive = false,
    currentAbilityID = 0,
    
    Init = function(self, entity)
        AbilityTargetVisuals_instance = self
        
        Event.subscribe("OnAbilitySelected", function(params)
            self.isActive = true
            self.currentAbilityID = params.ability
        end)
        
        Event.subscribe("OnUnitSelected", function(params)
            AbilityTargetVisuals.ClearVisuals(self)
        end)
        
        Event.subscribe("OnTileSelected", function(params)
            AbilityTargetVisuals.ClearVisuals(self)
        end)
        
        Event.subscribe("ShowPhaseOverlay", function(params)
            AbilityTargetVisuals.ClearVisuals(self)
        end)

        Event.subscribe("InPlanningPhase", function(params)
            AbilityTargetVisuals.ClearVisuals(self)
        end)

        Event.subscribe("OnChangePhase", function(params)
            AbilityTargetVisuals.ClearVisuals(self)
        end)

        Event.subscribe("PreviewVFXFinished", function(params)
            self.vfxFinishedPlaying = true
        end)

        Event.subscribe("AbilityChosen", function(params)
            self.gridPreviewCall = params.selectedAbility
            self.hoverTimer = 0
            self.lastHoveredTile = nil
        end)
    end,
    
    Start = function(self, entity)
    end,
    
    Update = function(self, entity)
        if not self.isActive or self.currentAbilityID == 0 then
            return
        end
        
        local targetGrid = IsAbilityOrder(self.currentAbilityID)
            and Grids_instance.ChaosGrid 
            or Grids_instance.OrderGrid
            
        if not targetGrid then return end
        
        local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
        local tilePos = Grid.worldToGrid(targetGrid.gridID, world.x, world.y)
        
        AbilityTargetVisuals.UpdateCrosshair(self, targetGrid, tilePos)
        AbilityTargetVisuals.UpdateBorders(self, targetGrid, tilePos)
    end,
    
    CreateCrosshair = function(self)
        if crosshairEntity then
            ECS.destroyEntity(crosshairEntity)
        end
        
        crosshairEntity = Prefab.spawn("AbilityCrosshair", -10000, -10000)
        
        if crosshairEntity then
            Log.info("[AbilityTargetVisuals] Crosshair spawned")
        else
            Log.error("[AbilityTargetVisuals] Failed to spawn AbilityCrosshair prefab")
        end
    end,
    
    UpdateCrosshair = function(self, grid, tilePos)
        if not tilePos then return end

        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                Grid.setTileTexture(grid.gridID, row, col, 1, self.emptyTexture)
            end
        end

        Grid.setTileTexture(grid.gridID, tilePos.y, tilePos.x, 1, self.crosshairTexture)

        -- for ability grid preview
        local lastTile = self.lastHoveredTile
        if not lastTile or lastTile.x ~= tilePos.x or lastTile.y ~= tilePos.y then
            self.hoverTimer = 0
            self.lastHoveredTile = {x = tilePos.x, y = tilePos.y}
            self.vfxFinishedPlaying = true
        end

        self.hoverTimer = self.hoverTimer + Time.getDeltaTime()

        -- preview selected ability when hovering on grid
        if self.vfxFinishedPlaying then
            self.hoverTimer = self.hoverTimer + Time.getDeltaTime()

            if self.hoverTimer >= self.hoverDuration then
                Event.emitLocal(self.gridPreviewCall, {
                    targetX = tilePos.x,
                    targetY = tilePos.y
                })
                self.vfxFinishedPlaying = false
                self.hoverTimer = 0
            end
        end
        ---------------------------
    end,
    
    UpdateBorders = function(self, grid, centerPos)
        local affectedTiles = AbilityTargetVisuals.GetAffectedTiles(self, centerPos)
        
        if not affectedTiles then return end

        -- Create border for each affected tile (excluding center where crosshair is)
        for _, tile in ipairs(affectedTiles) do
            if not (tile.x == centerPos.x and tile.y == centerPos.y) then
                Grid.setTileTexture(grid.gridID, tile.y, tile.x, 1, self.borderTexture)
            end
        end
        
        Log.info("[AbilityTargetVisuals] Spawned " .. #borderEntities .. " borders")
    end,
    
    GetAffectedTiles = function(self, centerPos)
        if not centerPos then return nil end

        local tiles = {}
        local abilityID = self.currentAbilityID
        
        -- Crystal Cascade has no targeting visuals
        if abilityID == CONSTANTS.ABILITIES.ORACLE_1 then
            return nil
            
        elseif abilityID == CONSTANTS.ABILITIES.DRAGOON_1 then
            table.insert(tiles, {x = centerPos.x, y = centerPos.y})
            if centerPos.y - 1 >= 0 then
                table.insert(tiles, {x = centerPos.x, y = centerPos.y - 1})
            end
            if centerPos.y + 1 < CONSTANTS.GRID_ROWS then
                table.insert(tiles, {x = centerPos.x, y = centerPos.y + 1})
            end
            
        elseif abilityID == CONSTANTS.ABILITIES.DRAGOON_2 then
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local x = centerPos.x + dx
                    local y = centerPos.y + dy
                    if x >= 0 and x < CONSTANTS.GRID_COLS and y >= 0 and y < CONSTANTS.GRID_ROWS then
                        table.insert(tiles, {x = x, y = y})
                    end
                end
            end
            
        elseif abilityID == CONSTANTS.ABILITIES.IMP_1 then
            table.insert(tiles, {x = centerPos.x, y = centerPos.y})
            if centerPos.y - 1 >= 0 then
                table.insert(tiles, {x = centerPos.x, y = centerPos.y - 1})
            end
            
        elseif abilityID == CONSTANTS.ABILITIES.EYE_1 then
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local x = centerPos.x + dx
                    local y = centerPos.y + dy
                    if x >= 0 and x < CONSTANTS.GRID_COLS and y >= 0 and y < CONSTANTS.GRID_ROWS then
                        table.insert(tiles, {x = x, y = y})
                    end
                end
            end
            
        elseif abilityID == CONSTANTS.ABILITIES.TIRION_1 then
            table.insert(tiles, {x = centerPos.x, y = centerPos.y})
            if centerPos.x + 1 < CONSTANTS.GRID_COLS and centerPos.y + 1 < CONSTANTS.GRID_ROWS then
                table.insert(tiles, {x = centerPos.x + 1, y = centerPos.y + 1})
            end
            
        elseif abilityID == CONSTANTS.ABILITIES.VIZOM_1 then
            table.insert(tiles, {x = centerPos.x, y = centerPos.y})
            if centerPos.x + 1 < CONSTANTS.GRID_COLS and centerPos.y + 1 < CONSTANTS.GRID_ROWS then
                table.insert(tiles, {x = centerPos.x + 1, y = centerPos.y + 1})
            end
        end
        
        return tiles
    end,
    
    ClearVisuals = function(self)
        self.isActive = false
        self.currentAbilityID = 0
        
        local grid = Grids_instance.OrderGrid

            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    Grid.setTileTexture(grid.gridID, row, col, 1, self.emptyTexture)
                end
            end

            grid = Grids_instance.ChaosGrid

            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    Grid.setTileTexture(grid.gridID, row, col, 1, self.emptyTexture)
                end
            end
    end
})

return class