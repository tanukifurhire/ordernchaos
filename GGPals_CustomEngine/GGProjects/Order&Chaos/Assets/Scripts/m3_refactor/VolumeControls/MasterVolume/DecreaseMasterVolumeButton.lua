-- For decreasing Master Volume
local class = DefineType("DecreaseMasterVolumeButton", 
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },
    
    Init = function(self, entity)
        Log.info("[-MasterVolumeButton] Initialized")
    end, -- end of Init

    OnHoverEnter = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.hoverSizeX,
            sy = self.hoverSizeY
        })
    end,

    OnHoverExit = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.normalSizeX,
            sy = self.normalSizeY
        })
    end,

    OnClick = function(self, entity, x, y)
        Log.info("[-MasterVolumeButton] OnClick called")

        local volume = Audio.getMasterVolume()

        -- return early if volume is already at 0
        if volume <= 0.0 then
            return
        end

        Event.emitLocal("OnDecreaseMasterVolume", {})
        
        Event.emitLocal("OnAdjustVolumeSettings", {})
    end -- end of OnClick
})

return class