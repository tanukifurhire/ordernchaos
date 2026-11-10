--[[
  TestText.lua
  
  Test for TextComponent Lua bindings.
  Creates a text that changes every second.
]]

local class = DefineType("TestText", {
    textEntity = nil,
    counter = 0,
    lastTime = 0,
    
    Init = function(self, entity)
        print("TestText: Init() called")
        
        -- Create text entity
        self.textEntity = ECS.createEntity()
        print("TestText: Created entity: " .. tostring(self.textEntity))
        
        -- Add transform (x, y, sx, sy, rotation)
        ECS.addTransform(self.textEntity, 0, 5, 1, 1, 0)
        print("TestText: Transform added")
        
        -- Add text
        ECS.addTextComponent(self.textEntity, "Test: 0", 24)
        print("TestText: TextComponent added successfully!")
    end,
    
    Update = function(self, entity)
        local time = Time.getTotalTime()
        
        -- Update every second
        if time - self.lastTime >= 1.0 then
            self.lastTime = time
            self.counter = self.counter + 1
            
            -- Change the text
            ECS.setTextComponent(self.textEntity, {
                text = "Test: " .. self.counter
            })
            
            print("TestText: Updated to " .. self.counter)
        end
    end
})

return class