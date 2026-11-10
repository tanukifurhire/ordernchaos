local class = DefineType("TileBreakVFX", {
    Serializable = {
        offsetX = 0.08,
        offsetY = 0.08
    },

    Init = function(self, entity)
    end,

    Start = function(self, entity)
        ECS.setMeshRenderer(entity, {
            perspectiveFactor = CONSTANTS.TILEMAP_PERSPECTIVE_FACTOR
        })
    end,

    OnVFXAnimationFinished = function(self, entity)
        ECS.destroyEntityDeferred(entity)
    end
})

return class