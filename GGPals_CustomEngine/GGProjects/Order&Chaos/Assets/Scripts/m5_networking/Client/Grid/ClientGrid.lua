local class = DefineType("ClientGrid", 
{
    Serializable = 
    {
        isOrderGrid = true,
        isTutorial = false,
        crossFadeDuration = 1.5
    },

    TextureAsset =
    {
        redTileDestroyed = 0,
        redTileNormal = 0,
        redTileDamaged1 = 0,
        redTileDamaged2 = 0,
        redTileDamaged3 = 0,
        redTileDamaged4 = 0,
        redTileDamaged5 = 0,
        redTileDamaged6 = 0,

        blueTileDestroyed = 0,
        blueTileNormal = 0,
        blueTileDamaged1 = 0,
        blueTileDamaged2 = 0,
        blueTileDamaged3 = 0,
        blueTileDamaged4 = 0,
        blueTileDamaged5 = 0,
        blueTileDamaged6 = 0
    },

    Tiles = {},
    
    gridID = 0,
    unitFlags = 0,

    RedTileAssets = {},
    BlueTileAssets = {},

    Init = function(self, entity)

        local _SPOTLIGHT_LAYER = 12
        local _originalLayer   = 0
        local _isSpotlighted   = false

        if self.isTutorial and self.isOrderGrid then
            local ok = pcall(function()
                local mr = ECS.getMeshRenderer(entity)
                _originalLayer = mr.layer
            end)

            local function setSpotlight(shouldSpotlight)
                if shouldSpotlight == _isSpotlighted then return end
                _isSpotlighted = shouldSpotlight
                local targetLayer = shouldSpotlight and _SPOTLIGHT_LAYER or _originalLayer
                pcall(function() ECS.setMeshRenderer(entity, { layer = targetLayer }) end)
            end

            Event.subscribe("OnTutorialStepChanged", function(params)
                setSpotlight(params.spotlightGrid or false)
            end)
        end
        
        -- Create per-instance tables to avoid shared references between orderGrid and chaosGrid        
        self.Tiles = {}
        self.RedTileAssets = {}
        self.BlueTileAssets = {}
        self.previewedTiles = {}
        self.crossFadingTiles = {}

        self.RedTileAssets[0] = self.redTileDestroyed
        self.RedTileAssets[1] = self.redTileDamaged6
        self.RedTileAssets[2] = self.redTileDamaged5
        self.RedTileAssets[3] = self.redTileDamaged4
        self.RedTileAssets[4] = self.redTileDamaged3
        self.RedTileAssets[5] = self.redTileDamaged2
        self.RedTileAssets[6] = self.redTileDamaged1
        self.RedTileAssets[7] = self.redTileNormal

        self.BlueTileAssets[0] = self.blueTileDestroyed
        self.BlueTileAssets[1] = self.blueTileDamaged6
        self.BlueTileAssets[2] = self.blueTileDamaged5
        self.BlueTileAssets[3] = self.blueTileDamaged4
        self.BlueTileAssets[4] = self.blueTileDamaged3
        self.BlueTileAssets[5] = self.blueTileDamaged2
        self.BlueTileAssets[6] = self.blueTileDamaged1
        self.BlueTileAssets[7] = self.blueTileNormal

        Event.subscribe("ClearHighlights", function(params)
            Log.info("Clearing all highlights")
            self:ClearAllHighlights()
        end)

        Event.subscribe("SetupClientGrid", function(params)
            if self.isOrderGrid == params.isOrderGrid then
                self.gridID = params.grid.gridID
                self:CreateGrid()
            end
        end)

        Event.subscribe("RevertPreviewedTileTextures", function(params)
            self:RevertPreviewedTileTextures()
        end)

        Event.subscribe("PreviewTileTexture", function(params)
            self:PreviewTileTexture(params.tileIdx, params.damage)
        end)

        -- Tutorial-specific per-tile darken/lighten control
        Event.subscribe("Darken_Tile", function(params)
            if not params or not params.gridX or not params.gridY then return end
            local index = self:GetTileIndex(params.gridX, params.gridY)
            if index < 1 or not self.Tiles[index] then return end
            self.Tiles[index].tutorialDarkened = true
            Grid.setTileColorMultiply(self.gridID, params.gridY, params.gridX, 0, {
                r = 0.2, g = 0.2, b = 0.2, a = 1.0
            })
        end)

        Event.subscribe("Lighten_Tile", function(params)
            if not params or not params.gridX or not params.gridY then return end
            local index = self:GetTileIndex(params.gridX, params.gridY)
            if index < 1 or not self.Tiles[index] then return end
            self.Tiles[index].tutorialDarkened = false
            Grid.setTileColorMultiply(self.gridID, params.gridY, params.gridX, 0, CONSTANTS.COLOR_NORMAL_MULTIPLY)
        end)

        Event.subscribe("Darken_All", function()
            if not self.isTutorial then return end
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    self.Tiles[index].tutorialDarkened = true
                    Grid.setTileColorMultiply(self.gridID, row, col, 0, { r = 0.2, g = 0.2, b = 0.2, a = 1.0 })
                end
            end
        end)

        Event.subscribe("Lighten_GAME_Grid", function()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = row * CONSTANTS.GRID_COLS + col + 1
                    if self.Tiles[index] then
                        self.Tiles[index].tutorialDarkened = false
                        Grid.setTileColorMultiply(self.gridID, row, col, 0, CONSTANTS.COLOR_NORMAL_MULTIPLY)
                    end
                end
            end
        end)

        Event.subscribe("Disable_Placement_Tile", function(params)
            if not params or not params.gridX or not params.gridY then return end
            local index = self:GetTileIndex(params.gridX, params.gridY)
            if index > 0 and self.Tiles[index] then
                self.Tiles[index].tutorialPlacementDisabled = true
            end
        end)

        Event.subscribe("Enable_Placement_Tile", function(params)
            if not params or not params.gridX or not params.gridY then return end
            local index = self:GetTileIndex(params.gridX, params.gridY)
            if index > 0 and self.Tiles[index] then
                self.Tiles[index].tutorialPlacementDisabled = false
            end
        end)
    end,

    Start = function(self, entity)
    end,

    CreateGrid = function(self)
        self.tileLastHealth = {}
        local rows = CONSTANTS.GRID_ROWS
        local cols = CONSTANTS.GRID_COLS

        for row = 0, rows - 1 do
            for col = 0, cols - 1 do
                local index = row * cols + col + 1
                local odd = (row + col) % 2

                self.Tiles[index] = {}
                self.Tiles[index].health = 7
                self.Tiles[index].occupied = false
                self.Tiles[index].unit = nil
                self.Tiles[index].colorAdd = CONSTANTS.COLOR_NONE
                self.Tiles[index].trap = nil
                self.Tiles[index].tutorialPlacementDisabled = false
                self.Tiles[index].wasDestroyed = false
                
                if odd == 1 then
                    self.Tiles[index].color = CONSTANTS.TILE_RED
                    Grid.setTileTexture(self.gridID, row, col, 0, self.redTileNormal)
                else
                    self.Tiles[index].color = CONSTANTS.TILE_BLUE
                    Grid.setTileTexture(self.gridID, row, col, 0, self.blueTileNormal)
                end
            end
        end
    end,

    -- Add to class fields:
    tileLastHealth = {},

    UpdateTileTexture = function(self, tileIdx)
        local tile = self.Tiles[tileIdx]
        if not tile then return end

        local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
        local col = (tileIdx - 1) % CONSTANTS.GRID_COLS
        local health = tile.health

        local assetIndex = math.max(0, math.min(health, CONSTANTS.TILE_MAX_HEALTH))
        local texture
        if tile.color == CONSTANTS.TILE_RED then
            texture = self.RedTileAssets[assetIndex]
        else
            texture = self.BlueTileAssets[assetIndex]
        end

        if texture then
            Grid.setTileTexture(self.gridID, row, col, 0, texture)
        end

        -- Play tile crack SFX when tile takes damage but isn't destroyed
        local prevHealth = self.tileLastHealth[tileIdx]
        if prevHealth and health < prevHealth and health > 0 then
            Event.emitLocal("PlayCombatTileCrack", {})
        end
        self.tileLastHealth[tileIdx] = health

        -- Spawn break VFX only once when tile first becomes destroyed
        if health <= 0 and not tile.wasDestroyed then
            tile.wasDestroyed = true
            ClientGrid.SpawnBreakVFX(self, row, col, tile.color)
            Event.emitLocal("PlayCombatTileDestroyed", {})
        end
    end,

    SpawnBreakVFX = function(self, row, col, tileColor)
        local world = Grid.gridToWorld(self.gridID, col, row)
        if not world then return end

        local prefabName = (tileColor == CONSTANTS.TILE_RED)
            and "TileBreakVFX_Red"
            or "TileBreakVFX_Blue"

        local rowOffsetY = { [0] = 0.04, [1] = 0.1, [2] = 0.08, [3] = 0.00 }

        local tileXCorrection = {
            [0] = { [0] = -0.02,   [1] = 0.03,  [2] = 0.09,  [3] = 0.13,  [4] = 0.19,  [5] = 0.21  },
            [1] = { [0] = 0.08,   [1] = 0.09,  [2] = 0.11,  [3] = 0.11,  [4] = 0.11,  [5] = 0.11  },
            [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02   },
            [3] = { [0] = 0.25, [1] = 0.19, [2] = 0.12, [3] = 0.06, [4] = 0.00,   [5] = -0.06 },
        }

        local xOffset = tileXCorrection[row] and (tileXCorrection[row][col] or 0) or 0

        local spawnX = world.x + 0.12 -- + xOffset
        local spawnY = world.y + 0.12 -- + (rowOffsetY[row] or 0)

        local vfx = Prefab.spawn(prefabName, spawnX, spawnY)
        if vfx then
            ECS.setMeshRenderer(vfx, {
                perspectiveFactor = CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR
            })
            Log.info("[ClientGrid] Spawned " .. prefabName .. " at tile (" .. col .. ", " .. row .. ")")
        else
            Log.warning("[ClientGrid] Failed to spawn tile break VFX: " .. tostring(prefabName))
        end
    end,

    -----------------------------------------------------------------------------------------------------------
    --- Tile damage previews and cross fades
    -----------------------------------------------------------------------------------------------------------
    StartCrossFade = function(self, tileIdx, row, col, texture, duration)
        duration = duration or 0.25
    
        -- clear previous tracked tiles
        self.crossFadingTiles[tileIdx] = nil

        -- add new layer on top and set alpha to 0 first
        Grid.setTileTexture(self.gridID, row, col, 1, texture)
        Grid.setTileColorMultiply(self.gridID, row, col, 1, {
            r = 1.0,
            g = 1.0,
            b = 1.0,
            a = 0.0
        })

        self.crossFadingTiles[tileIdx] = {
            time = 0,
            duration = duration,
            row = row,
            col = col,
            newTexture = texture
        }
    end,

    UpdateCrossFades = function(self)
        local dt = Time.getDeltaTime()

        local count = 0
        for _ in pairs(self.crossFadingTiles) do count = count + 1 end

        for tileIdx, fade in pairs(self.crossFadingTiles) do
            fade.time = fade.time + dt
            local t = math.min(fade.time / fade.duration, 1)
            local alpha = t * t * (3 - 2 * t)

            Grid.setTileColorMultiply(self.gridID, fade.row, fade.col, 1, {
                r = 1.0,
                g = 1.0,
                b = 1.0,
                a = alpha
            })

            if t >= 1 then
                self.crossFadingTiles[tileIdx] = nil
            end
        end
    end,

    PreviewTileTexture = function(self, tileIdx, damage)
        local tile = self.Tiles[tileIdx]
        if not tile then
            return
        end

        if self.crossFadingTiles[tileIdx] then
            return
        end

        -- save original health before preview
        if not self.previewedTiles[tileIdx] then
            self.previewedTiles[tileIdx] = tile.health
        end

        local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
        local col = (tileIdx - 1) % CONSTANTS.GRID_COLS

        local previewHealth = math.max(0, (tile.health - damage))
        local assetIndex = math.max(0, math.min(previewHealth, 6))
        
        local texture
        if tile.color == CONSTANTS.TILE_RED then
            texture = self.RedTileAssets[assetIndex]
        else
            texture = self.BlueTileAssets[assetIndex]
        end

        if texture then
            -- Grid.setTileTexture(self.gridID, row, col, 0, texture)
            self:StartCrossFade(tileIdx, row, col, texture, self.crossFadeDuration)
        end
    end,

    RevertPreviewedTileTextures = function(self)
        for tileIdx, savedHealth in pairs(self.previewedTiles) do
            local tile = self.Tiles[tileIdx]
            if tile then
                local row = math.floor((tileIdx - 1) / CONSTANTS.GRID_COLS)
                local col = (tileIdx - 1) % CONSTANTS.GRID_COLS

                -- clear layer 1
                local gm = ClientGameManager_instance
                if gm and gm.emptyTexture then
                    Grid.setTileTexture(self.gridID, row, col, 1, gm.emptyTexture)
                end
                
                self.crossFadingTiles[tileIdx] = nil
            end
        end

        self.previewedTiles = {}
    end,
    -----------------------------------------------------------------------------------------------------------
    -----------------------------------------------------------------------------------------------------------

    TrySetTileOccupied = function(self, gridX, gridY, unit)
        local index = self:GetTileIndex(gridX, gridY)
        if index < 0 then
            Log.error("[TrySetTileOccupied] Invalid gridX " .. gridX .. " gridY " .. gridY)
            return false 
        end

        if self.Tiles[index].health <= 0 then
            Log.warning("[TrySetTileOccupied] Cannot occupy destroyed tile at " .. gridX .. ", " .. gridY)
            return false
        end

        Log.info("[TrySetTileOccupied] Set tile " .. gridX .. ", " .. gridY .. " occupied")

        if self.Tiles[index].occupied then
            Log.warning("[TrySetTileOccupied] Tile " .. gridX .. ", " .. gridY .. " is already occupied")
            return false
        end

        if self.Tiles[index].tutorialPlacementDisabled then
            Log.info("[TrySetTileOccupied] Placement disabled on tile " .. gridX .. ", " .. gridY)
            return false
        end

        self.Tiles[index].occupied = true
        self.Tiles[index].unit = unit

        self:EmitAbilityColorCounts()
        Event.emitLocal("OnUnitDeployed", {
            isOrder  = self.isOrderGrid,
            unitType = unit
        })
        return true
    end,

    SetTileUnoccupied = function(self, gridX, gridY)
        local index = self:GetTileIndex(gridX, gridY)
        if index < 0 then
            Log.error("[SetTileUnoccupied] Invalid gridX " .. gridX .. " gridY " .. gridY)
            return false 
        end

        self.Tiles[index].occupied = false
        self.Tiles[index].unit = nil

        self:EmitAbilityColorCounts()

        return true
    end,

    EmitAbilityColorCounts = function(self)
        local red, blue = 0, 0

        for i = 1, #self.Tiles do
            local tile = self.Tiles[i]
            if tile.occupied then
                if tile.color == CONSTANTS.TILE_RED then red = red + 1 end
                if tile.color == CONSTANTS.TILE_BLUE then blue = blue + 1 end
            end
        end

        Event.emitLocal("AbilityColorCountsChanged", {
            red = red,
            blue = blue,
            player = self.isOrderGrid and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
        })
    end,

    GetHoveredTilePos = function(self)
        local mouse = Input.getMousePosition()
        local world = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
        local tilePos = Grid.worldToGrid(self.gridID, world.x, world.y)
        if not tilePos then return nil end
        return tilePos
    end,

    UpdateTileMouseHover = function(self, tilePos)
        if not tilePos then tilePos = { x = -1, y = -1 } end

        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1
                local tile = self.Tiles[index]
                if tile and tile.tutorialDarkened then
                    Grid.setTileColorMultiply(self.gridID, row, col, 0, { r = 0.2, g = 0.2, b = 0.2, a = 1.0 })
                elseif tilePos.x == col and tilePos.y == row then
                    Grid.setTileColorMultiply(self.gridID, row, col, 0, CONSTANTS.COLOR_HOVER_MULTIPLY)
                else
                    Grid.setTileColorMultiply(self.gridID, row, col, 0, CONSTANTS.COLOR_NORMAL_MULTIPLY)
                end
            end
        end
    end,

    HighlightValidTilePlacements = function(self)
        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1

                if (not self.Tiles[index].occupied) and (self.Tiles[index].health > 0) then
                    Grid.setTileColorAdd(self.gridID,
                        row, col, 0, CONSTANTS.COLOR_PLACEMENT_HIGHLIGHT
                    )
                end
            end
        end
    end,

    ClearAllHighlights = function(self)
        local gm = ClientGameManager_instance
        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                Grid.setTileColorAdd(self.gridID,
                    row, col, 0, CONSTANTS.COLOR_NONE
                )
                -- Also clear layer 2 targeting textures (borders, crosshairs)
                if gm and gm.emptyTexture then
                    Grid.setTileTexture(self.gridID, row, col, 2, gm.emptyTexture)
                end
            end
        end
    end,

    GetTileIndex = function(self, gridX, gridY)
        if gridX >= CONSTANTS.GRID_COLS or gridX < 0
        or gridY >= CONSTANTS.GRID_ROWS or gridY < 0 then return -1 end

        return gridY * CONSTANTS.GRID_COLS + gridX + 1
    end,

    CheckMouseClick = function(self)
        local tilePos = self:GetHoveredTilePos()
        if not tilePos then return end

        local isClicked = Input.isKeyTriggered(MOUSE_LEFT)
        if not isClicked then return end

        local index = self:GetTileIndex(tilePos.x, tilePos.y)
        if index > 0 then
            if self.Tiles[index].occupied then
                Event.emitLocal("PlayGridConfirmTile", {})
                Event.emitLocal("OnUnitSelected", {
                    isOrder = self.isOrderGrid,
                    unitType = self.Tiles[index].unit,
                    gridX = tilePos.x,
                    gridY = tilePos.y
                })
            else
                Event.emitLocal("PlayGridConfirmTile", {})
                Event.emitLocal("OnTileSelected", {
                    isOrder = self.isOrderGrid,
                    gridX = tilePos.x,
                    gridY = tilePos.y
                })
            end

            -- revert previewed tile damage textures after click
            Event.emitLocal("RevertPreviewedTileTextures", {})
        end
    end,

    HighlightMovementRange = function(self, gridX, gridY, range)
        self:ClearAllHighlights()

        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1
                local distance = math.abs(col - gridX) + math.abs(row - gridY)

                if distance > 0 and distance <= range and
                   (not self.Tiles[index].occupied) and
                   self.Tiles[index].health > 0 then
                    Grid.setTileColorAdd(self.gridID,
                        row, col, 0, CONSTANTS.COLOR_MOVEMENT_HIGHLIGHT
                    )
                end
            end
        end
    end,

    ShowOwnTraps = function(self, playerFaction)
        for i = 1, CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS do
            local tile = self.Tiles[i]
            if tile and tile.trap and tile.trap.owner == playerFaction then
                local row = math.floor((i - 1) / CONSTANTS.GRID_COLS)
                local col = (i - 1) % CONSTANTS.GRID_COLS
                Grid.setTileColorAdd(self.gridID, row, col, 0, CONSTANTS.COLOR_TRAP_INDICATOR)
                if tile.trap.vfxEntity then
                    if tile.trap.armed then
                        -- Armed trap: full opacity, undarkened
                        ECS.setMeshRenderer(tile.trap.vfxEntity, {
                            colorMultiply = { r = 1, g = 1, b = 1, a = 1 }
                        })
                    else
                        -- Unarmed trap: darkened + lower alpha
                        ECS.setMeshRenderer(tile.trap.vfxEntity, {
                            colorMultiply = { r = 0.4, g = 0.4, b = 0.4, a = 0.5 }
                        })
                    end
                end
            end
        end
    end,

    HideAllTraps = function(self)
        for i = 1, CONSTANTS.GRID_ROWS * CONSTANTS.GRID_COLS do
            local tile = self.Tiles[i]
            if tile and tile.trap then
                local row = math.floor((i - 1) / CONSTANTS.GRID_COLS)
                local col = (i - 1) % CONSTANTS.GRID_COLS
                Grid.setTileColorAdd(self.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
                if tile.trap.vfxEntity then
                    ECS.setMeshRenderer(tile.trap.vfxEntity, {
                        colorMultiply = CONSTANTS.COLOR_NONE
                    })
                end
            end
        end
    end,

    UpdateGrid = function(self)
        local tilePos = self:GetHoveredTilePos()

        local hoverChanged = (tilePos == nil) ~= (self._lastHoverPos == nil) or
            (tilePos and self._lastHoverPos and
            (tilePos.x ~= self._lastHoverPos.x or tilePos.y ~= self._lastHoverPos.y))

        if hoverChanged then
            self._lastHoverPos = tilePos
            self:RevertPreviewedTileTextures()
            if tilePos then
                Event.emitLocal("PlayGridHoverTile", {})
            end
        end

        self:UpdateTileMouseHover(tilePos)
        self:CheckMouseClick()

        self:UpdateCrossFades()
    end
})

return class
