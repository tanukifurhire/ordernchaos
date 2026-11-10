-- For increasing Master Volume
local class = DefineType("DecreaseSFXVolumeButton",
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    volume = 100,
    
    Init = function(self, entity)
        Log.info("[-SFXVolumeButton] Initialized")

        -- get SFX AudioSource volume
        Event.subscribe("OnReturnSFXVolume", function(params)
            self.volume = params.sfxVolume;
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
        Log.info("[-SFXVolumeButton] OnClick called")

        Event.emitLocal("GetSFXVolume", {})

        -- return early if volume is already at 0
        if self.volume <= 0.0 then
            return
        end

        Event.emitLocal("OnDecreaseSFXVolume", {})

        Event.emitLocal("OnAdjustVolumeSettings", {})
    end -- end of OnClick
})

return class