local class = DefineType("PnPEndGamePhase",
{
    Serializable = {
        phaseEnterDelay = 1.0,           -- Delay before starting tile destruction
        delayBetweenTiles = 0.1,         -- Delay between each tile breaking
        delayBeforeBanner = 1.5,         -- Delay after last tile before showing banner
        cameraPanDuration = 0.5          -- How long camera pan takes (for timing)
    },

    winner = 0,
    losingGrid = nil,
    tileQueue = {},
    currentTileIndex = 0,
    stateTimer = 0,
    phase = "idle",  -- idle, panning, destroying, waiting_banner, done

    Enter = function(self)
        Log.info("[PnPEndGamePhase] Entered End Game Phase!")
        
        local gm = ClientGameManager_instance
        if not gm then
            Log.error("[PnPEndGamePhase] No ClientGameManager_instance!")
            return
        end

        -- Get winner from PnP server
        local server = PassAndPlayServer_instance
        self.winner = 0
        
        if server and server.gameData and server.gameData.winner then
            self.winner = server.gameData.winner
        elseif gm.resolutionData and gm.resolutionData.winner then
            self.winner = gm.resolutionData.winner
        end
        
        if self.winner == 0 then
            Log.warning("[PnPEndGamePhase] No winner set, showing draw")
            self.winner = 3  -- Draw
        end

        -- Determine losing grid
        if self.winner == CONSTANTS.ORDER_PLAYER then
            self.losingGrid = gm.chaosGrid
            Log.info("[PnPEndGamePhase] Order wins - destroying Chaos grid")
        elseif self.winner == CONSTANTS.CHAOS_PLAYER then
            self.losingGrid = gm.orderGrid
            Log.info("[PnPEndGamePhase] Chaos wins - destroying Order grid")
        else
            -- Draw - pick one grid (or could do both)
            self.losingGrid = gm.chaosGrid
            Log.info("[PnPEndGamePhase] Draw - destroying Chaos grid for effect")
        end

        -- Build queue of unbroken tiles
        self:BuildTileQueue()

        -- Start phase
        self.stateTimer = 0
        self.currentTileIndex = 0
        self.phase = "panning"

        -- Pan camera to losing side
        if self.winner == CONSTANTS.ORDER_PLAYER then
            Event.emitLocal("FocusChaosGrid", {})
        elseif self.winner == CONSTANTS.CHAOS_PLAYER then
            Event.emitLocal("FocusOrderGrid", {})
        else
            Event.emitLocal("FocusChaosGrid", {})
        end
    end,

    Exit = function(self)
        self.phase = "idle"
        self.tileQueue = {}
        self.currentTileIndex = 0
    end,

    BuildTileQueue = function(self)
        self.tileQueue = {}
        
        if not self.losingGrid or not self.losingGrid.Tiles then
            Log.warning("[PnPEndGamePhase] No losing grid tiles to destroy")
            return
        end

        -- Iterate through tiles and queue unbroken ones
        -- Go row by row for a sweeping effect
        for row = 0, CONSTANTS.GRID_ROWS - 1 do
            for col = 0, CONSTANTS.GRID_COLS - 1 do
                local index = row * CONSTANTS.GRID_COLS + col + 1
                local tile = self.losingGrid.Tiles[index]
                
                if tile and tile.health and tile.health > 0 then
                    table.insert(self.tileQueue, {
                        index = index,
                        row = row,
                        col = col,
                        color = tile.color or CONSTANTS.TILE_BLUE
                    })
                end
            end
        end

        Log.info("[PnPEndGamePhase] Queued " .. #self.tileQueue .. " tiles for destruction")
    end,

    SpawnBreakVFXAt = function(self, row, col, tileColor)
        if not self.losingGrid then return end

        local world = Grid.gridToWorld(self.losingGrid.gridID, col, row)
        if not world then return end

        local prefabName = (tileColor == CONSTANTS.TILE_RED)
            and "TileBreakVFX_Red"
            or "TileBreakVFX_Blue"

        -- Offset tables from ClientGrid.lua
        local rowOffsetY = { [0] = 0.04, [1] = 0.1, [2] = 0.08, [3] = 0.00 }
        local tileXCorrection = {
            [0] = { [0] = -0.02, [1] = 0.03, [2] = 0.09, [3] = 0.13, [4] = 0.19, [5] = 0.21 },
            [1] = { [0] = 0.08, [1] = 0.09, [2] = 0.11, [3] = 0.11, [4] = 0.11, [5] = 0.11 },
            [2] = { [0] = 0.16, [1] = 0.14, [2] = 0.12, [3] = 0.08, [4] = 0.06, [5] = 0.02 },
            [3] = { [0] = 0.25, [1] = 0.19, [2] = 0.12, [3] = 0.06, [4] = 0.00, [5] = -0.06 },
        }

        local xOffset = tileXCorrection[row] and (tileXCorrection[row][col] or 0) or 0
        local spawnX = world.x + 0.12 --xOffset
        local spawnY = world.y + 0.12 --(rowOffsetY[row] or 0)

        local vfx = Prefab.spawn(prefabName, spawnX, spawnY)
        if vfx then
            ECS.setMeshRenderer(vfx, {
                perspectiveFactor = CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR
            })
        end

        -- Play tile break SFX
        Event.emitLocal("PlayCombatTileDestroyed", {})
    end,

    SetTileBroken = function(self, row, col, index, tileColor)
        if not self.losingGrid then return end

        -- Get broken texture based on tile color (index 0 = broken)
        local brokenTexture
        if tileColor == CONSTANTS.TILE_RED then
            brokenTexture = self.losingGrid.RedTileAssets and self.losingGrid.RedTileAssets[0]
        else
            brokenTexture = self.losingGrid.BlueTileAssets and self.losingGrid.BlueTileAssets[0]
        end

        if brokenTexture then
            Grid.setTileTexture(self.losingGrid.gridID, row, col, 0, brokenTexture)
        end

        -- Mark tile as destroyed
        if self.losingGrid.Tiles[index] then
            self.losingGrid.Tiles[index].health = 0
            self.losingGrid.Tiles[index].wasDestroyed = true
        end
    end,

    ShowWinnerBanner = function(self)
        if self.winner == CONSTANTS.ORDER_PLAYER then
            Log.info("[PnPEndGamePhase] === ORDER WINS ===")
            Event.emitLocal("ShowPhaseOverlay", { phase = "OrderWin" })
            Event.emitLocal("PlayCombatOrderWin", {})
            Event.emitLocal("PlayAnnouncerOrderWins", {})
        elseif self.winner == CONSTANTS.CHAOS_PLAYER then
            Log.info("[PnPEndGamePhase] === CHAOS WINS ===")
            Event.emitLocal("ShowPhaseOverlay", { phase = "ChaosWin" })
            Event.emitLocal("PlayCombatChaosWin", {})
            Event.emitLocal("PlayAnnouncerChaosWins", {})
        else
            Log.info("[PnPEndGamePhase] === DRAW ===")
            Event.emitLocal("ShowPhaseOverlay", { phase = "Draw" })
            Event.emitLocal("PlayCombatDraw", {})
            Event.emitLocal("PlayAnnouncerDraw", {})
        end

        -- Clear resolution data
        local gm = ClientGameManager_instance
        if gm then
            gm.resolutionData = nil
        end
    end,

    Update = function(self, entity)
        if self.phase == "idle" or self.phase == "done" then
            return
        end

        local dt = Time.getDeltaTime()
        self.stateTimer = self.stateTimer + dt

        if self.phase == "panning" then
            -- Wait for camera pan + enter delay
            if self.stateTimer >= (self.cameraPanDuration + self.phaseEnterDelay) then
                self.phase = "destroying"
                self.stateTimer = 0
                self.currentTileIndex = 0
            end

        elseif self.phase == "destroying" then
            -- Sequentially destroy tiles
            if self.currentTileIndex < #self.tileQueue then
                if self.stateTimer >= self.delayBetweenTiles then
                    self.currentTileIndex = self.currentTileIndex + 1
                    local tileData = self.tileQueue[self.currentTileIndex]
                    
                    if tileData then
                        -- Check if a unit is standing on this tile
                        local tile = self.losingGrid.Tiles[tileData.index]
                        if tile and tile.unit then
                            Event.emitLocal("UnitDeath", { unitType = tile.unit })
                        end
                        -- Set tile to broken texture
                        self:SetTileBroken(tileData.row, tileData.col, tileData.index, tileData.color)
                        -- Spawn break VFX
                        self:SpawnBreakVFXAt(tileData.row, tileData.col, tileData.color)
                        -- Camera shake
                        Camera.startShake(0.5, 0.1)
                    end
                    
                    self.stateTimer = 0
                end
            else
                -- All tiles destroyed, wait before showing banner
                self.phase = "waiting_banner"
                self.stateTimer = 0
            end

        elseif self.phase == "waiting_banner" then
            if self.stateTimer >= self.delayBeforeBanner then
                self:ShowWinnerBanner()
                self.phase = "done"
            end
        end
    end
})

return class
