local class = DefineType("ClientGridLink", 
{
    Serializable =
    {
        isOrderGrid = true,
        isTutorial = false
    },

    Start = function(self, entity)
        local gridComponent = ECS.getGridComponent(entity)

        if gridComponent then
            Event.emitLocal("SetupClientGrid",
            {
                isOrderGrid = self.isOrderGrid,
                isTutorial = self.isTutorial,
                grid = { gridID = gridComponent.gridID }
            })
        end
    end,
})

return class