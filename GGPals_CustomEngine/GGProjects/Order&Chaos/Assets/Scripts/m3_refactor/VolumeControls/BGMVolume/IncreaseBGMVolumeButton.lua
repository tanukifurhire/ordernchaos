-- For increasing BGM Volume
local class = DefineType("IncreaseBGMVolumeButton",
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    volume = 100,
    
    Init = function(self, entity)
        Log.info("[+BGMVolumeButton] Initialized")

        -- get BGM AudioSource volume
        Event.subscribe("OnReturnBGMVolume", function(params)
            self.volume = params.bgmVolume;
        end)
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
        Log.info("[+BGMVolumeButton] OnClick called")

        Event.emitLocal("GetBGMVolume", {})

        -- return early if volume is already at max
        if self.volume >= 1.0 then
            return
        end

        Event.emitLocal("OnIncreaseBGMVolume", {})

        Event.emitLocal("OnAdjustVolumeSettings", {})
    end -- end of OnClick
})

return class