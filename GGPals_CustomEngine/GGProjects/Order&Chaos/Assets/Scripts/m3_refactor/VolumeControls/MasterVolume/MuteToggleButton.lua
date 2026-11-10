-- For Mute Settings Toggling
local function norm(value)
    return value / 255.0
end

local class = DefineType("MuteToggleButton",
{
    Serializable = {
        -- enabled RGB
        enabledR = 61,
        enabledG = 107,
        enabledB = 168,

        -- disabled RGB
        disabledR = 182,
        disabledG = 0,
        disabledB = 14
    },

    unmuteVolume = 100,
    receivedMasterVolume = false,

    Init = function(self, entity)
        Log.info("[MuteToggleButton] Initialized")

        -- get the unmute volume
        Event.subscribe("OnReturnMasterVolumePercentage", function(params)
            self.unmuteVolume = params.masterVolumePercentage
            
            Event.emitLocal("OnUnmuted", {
                newVolume = (self.unmuteVolume / 100)
            })
            Log.info("[MuteToggleButton] Master Volume Unmuted")
        end)
    end, -- end of Init

    OnClick = function(self, entity, x, y)
        Log.info("[MuteToggleButton] OnClick called")

        local volume = Audio.getMasterVolume()

        -- if volume not muted, then mute
        if volume > 0.0 then
            local ui = ECS.getUIComponent(entity)
            if not ui then
                return
            end

            ui.colorAdd.r = norm(self.enabledR)
            ui.colorAdd.g = norm(self.enabledG)
            ui.colorAdd.b = norm(self.enabledB)
            
            ECS.setUIComponent(entity, ui)

            Event.emitLocal("OnMuted", {})
            Log.info("[MuteToggleButton] Master Volume Muted")
        end

        -- if volume already muted, then unmute
        if volume <= 0.0 then
            local ui = ECS.getUIComponent(entity)
            if not ui then
                return
            end

            ui.colorAdd.r = norm(self.disabledR)
            ui.colorAdd.g = norm(self.disabledG)
            ui.colorAdd.b = norm(self.disabledB)

            ECS.setUIComponent(entity, ui)

            -- get the master volume percentage first, then call unmute
            Event.emitLocal("GetMasterVolumePercentage", {})
        end
    end -- end of OnClick
})

return class