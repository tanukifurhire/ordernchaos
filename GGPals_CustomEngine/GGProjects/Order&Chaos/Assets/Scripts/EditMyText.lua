--[[
  EditMyText.lua
  
  Edits the TextComponent on the same entity this script is attached to.
  Attach this to the entity with "HELLO" text.
  
  Uses Log.info() to print to the editor console.
]]

local class = DefineType("EditMyText", {
    counter = 0,
    lastTime = 0,
    
    Init = function(self, entity)
        Log.info("EditMyText: Init on entity " .. tostring(entity))
        
        -- Try to get current text (will error if no TextComponent exists)
        local success, textComp = pcall(function()
            return ECS.getTextComponent(entity)
        end)
        
        if success then
            Log.info("EditMyText: Current text = '" .. textComp.text .. "'")
        else
            Log.error("This entity doesn't have a TextComponent!")
            Log.error(tostring(textComp))
        end
    end,
    
    Update = function(self, entity)
        local time = Time.getTotalTime()
        
        -- Update every second
        if time - self.lastTime >= 1.0 then
            self.lastTime = time
            self.counter = self.counter + 1
            
            -- Edit the TextComponent on THIS entity
            ECS.setTextComponent(entity, {
                text = "Count: " .. self.counter
            })
            
            Log.info("EditMyText: Updated text to " .. self.counter)
        end
    end
})

return class