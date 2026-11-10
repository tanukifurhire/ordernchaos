-- Reflects current Master Volume
local class = DefineType("MasterVolumePercentage", 
{
    percentage = 100,
    increment = 10,

    -- subscription handles
    _increaseVolume = 0,
    _decreaseVolume = 0,

    Start = function(self, entity)
        local audioConfig = Audio.getAudioConfig();
        self.percentage = (audioConfig.masterVolume * 100)
        ECS.setTextComponent(entity, {
            text = tostring(self.percentage)
        })
    end,

    Init = function(self, entity)
        Log.info("[MasterVolumePercentage] Initialized")

        -- unsubscribe any leftover subscriptions
        if self._increaseVolume ~= 0 then
            Event.unsubscribe(self._increaseVolume)
        end
        if self._decreaseVolume ~= 0 then
            Event.unsubscribe(self._decreaseVolume)
        end

        self._increaseVolume =
        Event.subscribe("OnIncreaseMasterVolume", function()
            local text = ECS.getTextComponent(entity)

            -- skip if current value is already at max
            if tonumber(text.text) >= 100 then
                self.percentage = 100 -- clamp percentage at 100
                return
            end

            self.percentage = tonumber(text.text) + self.increment
            ECS.setTextComponent(entity, {
                text = tostring(self.percentage),
                color = text.color,
                fontSize = text.fontSize,
                visible = text.visible
            })
        end)

        self._decreaseVolume =
        Event.subscribe("OnDecreaseMasterVolume", function()
            local text = ECS.getTextComponent(entity)

            -- skip is current value is already at 0
            if tonumber(text.text) <= 0 then
                self.percentage = 0 -- clamp percentage at 0
                return
            end

            self.percentage = tonumber(text.text) - self.increment
            ECS.setTextComponent(entity, {
                text = tostring(self.percentage),
                color = text.color,
                fontSize = text.fontSize,
                visible = text.visible
            })
        end)

        -- return back to the caller (MuteToggleButton)
        Event.subscribe("GetMasterVolumePercentage", function()
            local text = ECS.getTextComponent(entity)
            self.percentage = tonumber(text.text)

            Event.emitLocal("OnReturnMasterVolumePercentage", {
                masterVolumePercentage = self.percentage 
            })
        end)
    end -- end of Init
})

return class