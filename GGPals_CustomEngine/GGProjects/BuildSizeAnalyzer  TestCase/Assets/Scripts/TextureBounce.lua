-- TextureBounce.lua
-- Makes the texture jump up and return to the exact same position each cycle.
 
local TextureBounce = DefineType("TextureBounce",
{
    Serializable =
    {
        bounceHeight = 80,  -- Pixels to jump upward
        bounceSpeed  = 3.0, -- Oscillation speed (higher = faster)
    },
 
    originY = 0,
 
    Start = function(self, entity)
        local transform = ECS.getTransform(entity)
        if transform then
            self.originY = transform.y
            Log.info("TextureBounce started. Origin Y: " .. self.originY)
        else
            Log.error("TextureBounce: entity is missing a Transform component!")
        end
    end,
 
    Update = function(self, entity)
        local transform = ECS.getTransform(entity)
        if not transform then return end
 
        local t = Time.getTotalTime()
 
        -- abs(sin) always goes 0 -> 1 -> 0, so it starts and ends at origin
        local lift = math.abs(math.sin(t * self.bounceSpeed))
 
        -- Only update Y, leave sx/sy/rotation untouched via partial update
        ECS.setTransform(entity, { y = self.originY + lift * self.bounceHeight })
    end,
})
 
return TextureBounce