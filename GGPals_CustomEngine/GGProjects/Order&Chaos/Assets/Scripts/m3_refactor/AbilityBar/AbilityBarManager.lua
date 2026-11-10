local class = DefineType("AbilityBarManager",
{
    Serializable =
    {
        playerId = 1, -- 1 = Order, 2 = Chaos
    },

    Init = function(self, entity)

        Event.subscribe("OnUnitDeployed", function()
            AbilityBarManager.Recount(self)
        end)

        Event.subscribe("TryMoveCommand", function()
            AbilityBarManager.Recount(self)
        end)

        Event.subscribe("InPlanningPhase", function()
            AbilityBarManager.Recount(self)
        end)

    end,

    Recount = function(self)
        local orderGrid = Grids_instance.OrderGrid
        local chaosGrid = Grids_instance.ChaosGrid

        local red, blue = 0, 0
        local grid = self.playerId == 1 and orderGrid or chaosGrid

        if grid then
            for i = 1, #grid.Tiles do
                local tile = grid.Tiles[i]
                if tile.occupied then
                    if tile.color == CONSTANTS.TILE_RED then red = red + 1 end
                    if tile.color == CONSTANTS.TILE_BLUE then blue = blue + 1 end
                end
            end
        end

        Event.emitLocal("AbilityColorCountsChanged", {
            red = red,
            blue = blue,
            player = self.playerId
        })
    end
})

return class
