-- For increasing Master Volume
local class = DefineType("IncreaseMasterVolumeButton",
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },
    
    Init = function(self, entity)
        Log.info("[+MasterVolumeButton] Initialized")
    end, -- end of Init

    OnHoverEnter = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.hoverSizeX,
            sy = self.hoverSizeY
        })
    end, -- end of OnHoverEnter

    OnHoverExit = function(self, entity)
        ECS.setTransform(entity, {
            sx = self.normalSizeX,
            sy = self.normalSizeY
        })
    end, -- end of OnHoverExit

    OnClick = function(self, entity, x, y)
        Log.info("[+MasterVolumeButton] OnClick called")

        local volume = Audio.getMasterVolume()

        -- return early if volume is already at max
        if volume >= 1.0 then
            return
        end

        Event.emitLocal("OnIncreaseMasterVolume", {})

        Event.emitLocal("OnAdjustVolumeSettings", {})
    end -- end of OnClick
})

return class