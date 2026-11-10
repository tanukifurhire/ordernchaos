local class = DefineType("ClientDeployState", 
{
    stateMachine = nil,

    selected = false,

    Serializable = 
    {
        pedestalPosX = 0.0001,
        pedestalPosY = 0.0001
    },

    Init = function(self, stateMachine, entity)
        self.stateMachine = stateMachine
    end,

    Enter = function(self, entity)

    end,

    Exit = function(self, entity)

    end,

    HandleInput = function(self, entity)

    end,

    OnClick = function(self, entity)
        Event.emitLocal("PlayGridPickup", {})
        self.selected = true

        local isOrder = (self.stateMachine.unit.ownerPlayer == 1)
        local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

        grid:HighlightValidTilePlacements()
    end,

    OnDrop = function(self, entity)
        -- It really doesn't matter which unit's OnDrop was called..
        local isOrder = (self.stateMachine.unit.ownerPlayer == 1)
        local grid = isOrder and ClientGameManager_instance.orderGrid or ClientGameManager_instance.chaosGrid

        if not grid then 
            Log.warning("No grid found!")
            return 
        end
        
        grid:ClearAllHighlights()

        if self.selected then
            self.selected = false

            local worldPos = Grid.getMouseWorldPerspective(CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR)
            local tilePos = Grid.worldToGrid(grid.gridID, worldPos.x, worldPos.y)


            if tilePos then
                -- Deploy unit on client side first, storing deployment history
                Log.info("Deploying at x: " .. tilePos.x .. " y: " .. tilePos.y)
                if grid:TrySetTileOccupied(tilePos.x, tilePos.y, self.stateMachine.unit.unitType) then
                    Event.emitLocal("PlayGridPlaceGrid", {})

                    self.stateMachine.unit.gridPos.x = tilePos.x
                    self.stateMachine.unit.gridPos.y = tilePos.y

                    self.stateMachine.unit:RecalculateGridOffset()

                    local gridPos = Grid.gridToWorld(grid.gridID, tilePos.x, tilePos.y)

                    ECS.setTransform(entity, {
                        x = gridPos.x + self.stateMachine.unit.gridOffsetX,
                        y = gridPos.y + self.stateMachine.unit.gridOffsetY
                    })

                    local bit = require("bit")

                    grid.unitFlags = bit.bor(grid.unitFlags, bit.lshift(1, self.stateMachine.unit.unitType))

                    if PassAndPlayServer_instance then
                        PassAndPlayServer_instance:OnDeployUnit(self.stateMachine.unit.unitType, tilePos.x, tilePos.y)
                    else
                        RPC.call("ServerRPC_OnDeployUnit",
                            NetworkedGameClient_instance.assignedClientID,
                            self.stateMachine.unit.unitType,
                            tilePos.x,
                            tilePos.y
                        )
                        ClientGameManager_instance.awaitingServer = true
                    end
                    
                    self.stateMachine:ChangeState(entity, self.stateMachine.states.readyState)
                    return
                end
            end

        end

        ECS.setTransform(entity, {
            x = self.pedestalPosX,
            y = self.pedestalPosY
        })

        self.selected = false
    end,

    Update = function(self, entity)
        if self.selected then
            local worldPos = Input.getMouseWorldPosition()
            ECS.setTransform(entity, {
                x = worldPos.x,
                y = worldPos.y
            })
        end
    end
})

return class