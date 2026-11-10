--[[
    MouseWheelTest.lua
    Simple test script to verify Input.getMouseWheelDelta() functionality
    Attach to any entity to test mouse wheel scrolling
]]

local class = DefineType("MouseWheelTest", 
{
    scrollValue = 0,
    
    Init = function(self, entity)
        Log.info("[MouseWheelTest] Initialized - scroll to test wheel detection")
    end,
    
    Update = function(self, entity)
        local delta = Input.getMouseWheelDelta()
        
        if delta ~= 0 then
            self.scrollValue = self.scrollValue + delta
            
            if delta > 0 then
                Log.info("[MouseWheelTest] Scrolled UP - Delta: " .. delta .. " | Total: " .. self.scrollValue)
            else
                Log.info("[MouseWheelTest] Scrolled DOWN - Delta: " .. delta .. " | Total: " .. self.scrollValue)
            end
        end
    end
})

return class