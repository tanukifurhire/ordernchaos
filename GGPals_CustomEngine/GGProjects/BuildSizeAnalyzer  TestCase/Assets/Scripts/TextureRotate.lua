-- TextureRotate.lua
-- Makes the texture spin non-stop using total time, with an optional pulse.
 
local TextureRotate = DefineType("TextureRotate",
{
    Serializable =
    {
        degreesPerSecond = 120,  -- Rotation speed (negative = counter-clockwise)
        pulseAmount      = 0.08, -- Scale pulse while spinning (0 = disabled)
        pulseSpeed       = 4.0,  -- How fast the scale pulses
    },
 
    Start = function(self, entity)
        local transform = ECS.getTransform(entity)
        if transform then
            Log.info("TextureRotate started.")
        else
            Log.error("TextureRotate: entity is missing a Transform component!")
        end
    end,
 
    Update = function(self, entity)
        local transform = ECS.getTransform(entity)
        if not transform then return end
 
        local t = Time.getTotalTime()
 
        -- Drive rotation directly from total time so it never drifts
        transform.rotation = (t * self.degreesPerSecond) % 360
 
        -- Optional breathing pulse on scale
        if self.pulseAmount > 0 then
            local pulse = 8.0 + math.sin(t * self.pulseSpeed) * self.pulseAmount
            transform.sx = pulse
            transform.sy = pulse
        end
 
        ECS.setTransform(entity, transform)
    end,
})
 
return TextureRotate