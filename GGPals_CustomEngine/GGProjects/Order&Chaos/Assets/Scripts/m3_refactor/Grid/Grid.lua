-- Singleton instance references to GridManager instances
Grids_instance = Grids_instance or { OrderGrid = nil, ChaosGrid = nil }

local function EmitAbilityColorCounts()
    local orderGrid = Grids_instance.OrderGrid
    local chaosGrid = Grids_instance.ChaosGrid

    local orderRed, orderBlue = 0, 0
    local chaosRed, chaosBlue = 0, 0

    -- Count only Order grid tiles
    if orderGrid then
        for i = 1, #orderGrid.Tiles do
            local tile = orderGrid.Tiles[i]
            if tile.occupied then
                if tile.color == CONSTANTS.TILE_RED then orderRed = orderRed + 1 end
                if tile.color == CONSTANTS.TILE_BLUE then orderBlue = orderBlue + 1 end
            end
        end
    end

    -- Count only Chaos grid tiles
    if chaosGrid then
        for i = 1, #chaosGrid.Tiles do
            local tile = chaosGrid.Tiles[i]
            if tile.occupied then
                if tile.color == CONSTANTS.TILE_RED then chaosRed = chaosRed + 1 end
                if tile.color == CONSTANTS.TILE_BLUE then chaosBlue = chaosBlue + 1 end
            end
        end
    end

    -- Emit counts **separately per player**
    Event.emitLocal("AbilityColorCountsChanged", {
        red = orderRed,
        blue = orderBlue,
        player = 1
    })

    Event.emitLocal("AbilityColorCountsChanged", {
        red = chaosRed,
        blue = chaosBlue,
        player = 2
    })
end

local function CreateGrid(gridID, rows, cols, texRed, texBlue)
    local Tiles = {}

    for row = 0, rows - 1 do
        for col = 0, cols - 1 do
            local index = row * cols + col + 1
            local odd = (row + col) % 2

            Tiles[index] = {}
            Tiles[index].health = 5
            Tiles[index].occupied = false
            Tiles[index].unit = CONSTANTS.UNIT.NONE
            Tiles[index].colorAdd = CONSTANTS.COLOR_NONE
            Tiles[index].hasTrap = false
            Tiles[index].trapArmed = false
            Tiles[index].trapDamage = 0
            Tiles[index].trapVFXEntity = 0

            if odd == 1 then
                Tiles[index].color = CONSTANTS.TILE_RED
                Grid.setTileTexture(gridID, row, col, 0, texRed)
            else
                Tiles[index].color = CONSTANTS.TILE_BLUE
                Grid.setTileTexture(gridID, row, col, 0, texBlue)
            end
        end
    end

    return Tiles
end

local function HighlightValidTilePlacements(instance, isOrder)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return end

    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            local index = row * CONSTANTS.GRID_COLS + col + 1

            if (not grid.Tiles[index].occupied) and (grid.Tiles[index].health > 0) then
                Grid.setTileColorAdd(grid.gridID,
                    row, col, 0, CONSTANTS.COLOR_PLACEMENT_HIGHLIGHT
                )
            end
        end
    end
end

local function HighlightMovementRange(instance, isOrder, gridX, gridY, range)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return end

    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            local index = row * CONSTANTS.GRID_COLS + col + 1
            
            Grid.setTileColorAdd(grid.gridID,
                row, col, 0, CONSTANTS.COLOR_NONE
            )
        end
    end

    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            local index = row * CONSTANTS.GRID_COLS + col + 1
            local distance = math.abs(col - gridX) + math.abs(row - gridY)

            if distance > 0 and distance <= range and 
            (not grid.Tiles[index].occupied) and 
            grid.Tiles[index].health > 0 then
                Grid.setTileColorAdd(grid.gridID,
                    row, col, 0, CONSTANTS.COLOR_MOVEMENT_HIGHLIGHT
                )
            end
        end
    end
end

function ValidateUnitMovement(instance, isOrder, fromX, fromY, toX, toY, range)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return nil end

    local fromIndex = GetTileIndex(fromX, fromY)
    local toIndex = GetTileIndex(toX, toY)

    -- Return nil if same tile
    if fromX == toX and fromY == toY then return nil end

    -- Return nil if either index is invalid
    if fromIndex < 0 or toIndex < 0 then return nil end

    -- Return nil if tile already occupied
    if grid.Tiles[toIndex].occupied then return nil end

    if grid.Tiles[toIndex].health <= 0 then return nil end

    -- Return array of tiles
    return Grid.validateMovement(grid.gridID, fromX, fromY, toX, toY, range)
end

local function ClearAllHighlights(instance, isOrder)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return end

    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            local index = row * CONSTANTS.GRID_COLS + col + 1
            Grid.setTileColorAdd(grid.gridID, row, col, 0, CONSTANTS.COLOR_NONE)
        end
    end
end

function TrySetTileOccupied(instance, isOrder, gridX, gridY, unitType)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return false end

    local index = GetTileIndex(gridX, gridY)
    if index < 0 then
        Log.error("[TrySetTileOccupied] Invalid gridX " .. gridX .. " gridY " .. gridY)
        return false
    end

    if grid.Tiles[index].health <= 0 then
        Log.warning("[TrySetTileOccupied] Cannot occupy destroyed tile at " .. gridX .. ", " .. gridY)
        return false
    end

    Log.info("[SetTileOccupied] " .. gridX .. ", " .. gridY)

    if grid.Tiles[index].occupied then return false end

    grid.Tiles[index].occupied = true
    grid.Tiles[index].unit = unitType

    EmitAbilityColorCounts()

    return true
end

function SetTileUnoccupied(instance, isOrder, gridX, gridY)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return false end

    local index = GetTileIndex(gridX, gridY)
    if index < 0 then
        Log.error("[SetTileOccupied] Invalid gridX " .. gridX .. " gridY " .. gridY)
        return false
    end

    grid.Tiles[index].occupied = false
    grid.Tiles[index].unit = CONSTANTS.UNIT.NONE

    EmitAbilityColorCounts()

    return true
end

function GetTileIndex(gridX, gridY)
    if gridX >= CONSTANTS.GRID_COLS or gridX < 0
    or gridY >= CONSTANTS.GRID_ROWS or gridY < 0 then return -1 end
    
    return gridY * CONSTANTS.GRID_COLS + gridX + 1  -- row * cols + col + 1
end

function PlaceTrap(isOrder, gridX, gridY, damage, vfxEntity)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return false end

    local index = GetTileIndex(gridX, gridY)
    if index < 0 then return false end

    grid.Tiles[index].hasTrap = true
    grid.Tiles[index].trapArmed = false
    grid.Tiles[index].trapDamage = damage
    grid.Tiles[index].trapVFXEntity = vfxEntity

    Log.info("[PlaceTrap] Placed trap at " .. gridX .. ", " .. gridY)
    return true
end

function ArmTrap(isOrder, gridX, gridY)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return false end

    local index = GetTileIndex(gridX, gridY)
    if index < 0 then return false end

    if grid.Tiles[index].hasTrap then
        grid.Tiles[index].trapArmed = true
        Log.info("[ArmTrap] Armed trap at " .. gridX .. ", " .. gridY)
        return true
    end

    return false
end

function CheckAndTriggerTrap(isOrder, gridX, gridY)
    local grid = isOrder and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return nil end

    local index = GetTileIndex(gridX, gridY)
    if index < 0 then return nil end

    if grid.Tiles[index].hasTrap and grid.Tiles[index].trapArmed then
        Log.info("[CheckAndTriggerTrap] Trap triggered at " .. gridX .. ", " .. gridY)
        
        local trapData = {
            damage = grid.Tiles[index].trapDamage,
            vfxEntity = grid.Tiles[index].trapVFXEntity,
            gridX = gridX,
            gridY = gridY
        }

        grid.Tiles[index].hasTrap = false
        grid.Tiles[index].trapArmed = false
        grid.Tiles[index].trapDamage = 0
        grid.Tiles[index].trapVFXEntity = 0

        return trapData
    end

    return nil
end

local function GetHoveredTilePos(grid, tilemap)
    local mouse = Input.getMousePosition()
    local world = Grid.getMouseWorldPerspective(tilemap.perspectiveFactor)
    local tilePos = Grid.worldToGrid(grid.gridID, world.x, world.y)
    if not tilePos then return nil end
    return tilePos
end

local function UpdateTileMouseHover(grid, tilePos)
    if not tilePos then tilePos = { x = -1, y = -1 } end

    for row = 0, CONSTANTS.GRID_ROWS - 1 do
        for col = 0, CONSTANTS.GRID_COLS - 1 do
            if tilePos.x == col and tilePos.y == row then
                Grid.setTileColorMultiply(grid.gridID, row, col, 0, CONSTANTS.COLOR_HOVER_MULTIPLY)
            else
                Grid.setTileColorMultiply(grid.gridID, row, col, 0, CONSTANTS.COLOR_NORMAL_MULTIPLY)
            end
        end
    end
end

local function CheckMouseClick(isOrderGrid, tilePos)
    if not tilePos then return end

    local isClicked = Input.isKeyTriggered(MOUSE_LEFT)

    if not isClicked then return end

    local grid = isOrderGrid and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
    if not grid then return end

    local index = GetTileIndex(tilePos.x, tilePos.y)

    if index > 0 then

        --if grid.Tiles[index].health <= 0 then
            --return
        --end

        if grid.Tiles[index].occupied then
            Event.emitLocal("PlayGridConfirmTile", {})
            Event.emitLocal("OnUnitSelected", {
                isOrder = isOrderGrid,
                unitType = grid.Tiles[index].unit,
                gridX = tilePos.x,
                gridY = tilePos.y
            })
        else
            Event.emitLocal("PlayGridConfirmTile", {})
            Event.emitLocal("OnTileSelected",{
                isOrder = isOrderGrid,
                gridX = tilePos.x,
                gridY = tilePos.y
            })
        end
    end
end

local class = DefineType("GridManager", 
{
    Serializable = 
    {
        isOrderGrid = true
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

        blueTileDestroyed = 0,
        blueTileNormal = 0,
        blueTileDamaged1 = 0,
        blueTileDamaged2 = 0,
        blueTileDamaged3 = 0,
        blueTileDamaged4 = 0,
        blueTileDamaged5 = 0
    },

    Tiles = {},

    gridID = 0,
    unitFlags = 0,

    RedTileAssets = {},
    BlueTileAssets = {},

    Init = function(self, entity)
        GridManager.InitializeGrid(self, entity)

        -- Assign this instance to the global singleton reference
        if self.isOrderGrid then
            Grids_instance.OrderGrid = self
        else
            Grids_instance.ChaosGrid = self
        end

        Event.subscribe("HighlightValidPlacements", function(params)
            local targetGrid = params.playerId == 1 and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
            if targetGrid and targetGrid.gridID == self.gridID then
                -- Highlight all valid tile placements
                HighlightValidTilePlacements(-1, self.isOrderGrid)
            end
        end)

        Event.subscribe("ShowUnitMovement", function(params)
            local targetGrid = params.unitType < CONSTANTS.UNIT.IMP and Grids_instance.OrderGrid or Grids_instance.ChaosGrid
            if targetGrid and targetGrid.gridID == self.gridID then
                -- Highlight valid tiles in movement range
                HighlightMovementRange(-1, self.isOrderGrid, params.gridX, params.gridY, params.range)
            end
        end)

        Event.subscribe("ClearHighlights", function(params)
            ClearAllHighlights(-1, self.isOrderGrid)
        end)

        Event.subscribe("UpdateGridTextures", function(params)
            local targetGrid = (params.playerId == CONSTANTS.ORDER_PLAYER) 
                and Grids_instance.OrderGrid 
                or Grids_instance.ChaosGrid

            if targetGrid then
                GridManager.UpdateGridTextures(self, entity, targetGrid)
            end
        end)

        Event.subscribe("InResolutionPhase", function()
            for row = 0, CONSTANTS.GRID_ROWS - 1 do
                for col = 0, CONSTANTS.GRID_COLS - 1 do
                    local index = GetTileIndex(col, row)
                    if self.Tiles[index].hasTrap and not self.Tiles[index].trapArmed then
                        ArmTrap(self.isOrderGrid, col, row)
                    end
                end
            end
        end)

        self.RedTileAssets[0] = self.redTileDestroyed
        self.RedTileAssets[1] = self.redTileDamaged5
        self.RedTileAssets[2] = self.redTileDamaged4
        self.RedTileAssets[3] = self.redTileDamaged3
        self.RedTileAssets[4] = self.redTileDamaged2
        self.RedTileAssets[5] = self.redTileDamaged1
        self.RedTileAssets[6] = self.redTileNormal

        self.BlueTileAssets[0] = self.blueTileDestroyed
        self.BlueTileAssets[1] = self.blueTileDamaged5
        self.BlueTileAssets[2] = self.blueTileDamaged4
        self.BlueTileAssets[3] = self.blueTileDamaged3
        self.BlueTileAssets[4] = self.blueTileDamaged2
        self.BlueTileAssets[5] = self.blueTileDamaged1
        self.BlueTileAssets[6] = self.blueTileNormal
    end,

    Start = function(self, entity)

    end,

    InitializeGrid = function(self, entity)
        local grid = ECS.getGridComponent(entity)
        if grid then
            self.gridID = grid.gridID
            self.unitFlags = 0
            self.Tiles = CreateGrid(grid.gridID, CONSTANTS.GRID_ROWS, CONSTANTS.GRID_COLS, self.redTileNormal, self.blueTileNormal)

            if self.isOrderGrid then
                Log.info("[Grid] Order Grid ID: " .. self.gridID)
            else
                Log.info("[Grid] Chaos Grid ID: " .. self.gridID)
            end

            Log.info("[Grid] Created Grid of " .. CONSTANTS.GRID_ROWS .. " rows and " .. CONSTANTS.GRID_COLS .. " columns")
        else
            Log.info("[Grid] No Grid Component found! Is this a Networked Grid?")
        end
    end,

    Update = function(self, entity)
        local grid = ECS.getGridComponent(entity)
        local tilemap = ECS.getTilemapComponent(entity)
        if not grid or not tilemap then return end

        local tilePos = GetHoveredTilePos(grid, tilemap)
        UpdateTileMouseHover(grid, tilePos)
        CheckMouseClick(self.isOrderGrid, tilePos)
    end,

    UpdateGridTextures = function(self, entity, grid)
        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = GetTileIndex(col, row)

                local health = grid.Tiles[index].health
                if health < 0 then
                    Log.error("[UpdateGridTextures] Negative health at index " .. index .. ": " .. health)
                    health = 0
                elseif health > 5 then
                    Log.error("[UpdateGridTextures] Health too high at index " .. index .. ": " .. health)
                    health = 5
                end

                -- Determine texture index
                local textureIndex
                if health == 0 then
                    textureIndex = 0  -- Use destroyed texture
                else
                    textureIndex = health + 1  -- Normal indexing for health 1-5
                end

                if grid.Tiles[index].color == CONSTANTS.TILE_BLUE then
                    Grid.setTileTexture(grid.gridID, row, col, 0, 
                        self.BlueTileAssets[textureIndex]
                    )
                else
                    Grid.setTileTexture(grid.gridID, row, col, 0, 
                        self.RedTileAssets[textureIndex]
                    )
                end
            end
        end
    end
})

return class