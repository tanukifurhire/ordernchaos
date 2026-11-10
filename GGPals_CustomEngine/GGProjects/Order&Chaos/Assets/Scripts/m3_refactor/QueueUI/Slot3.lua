local class = DefineType("Slot3",
{    
    slotIndex = 3,

    enabled = true,

    Init = function(self, entity)
        local function register()
            Event.emitLocal("RegisterAbilitySlot", {
                index = self.slotIndex,
                entity = entity
            })
        end

        register()

        -- try registering again
        Event.subscribe("AbilityQueueUIReady", function()
            register()
        end)

        Event.subscribe("EnableSlots", function()
            self.enabled = true
        end)

        Event.subscribe("DisableSlots", function()
            self.enabled = false
        end)
    end, -- end of Init

    -- Previewing a queued ability is a click, not a hover. Hovering fired
    -- previews the player never asked for while the cursor crossed the queue.
    -- Clicking this slot again while it is previewing closes it; that dismissal
    -- lives in AbilityQueueUI, which is the side that knows what is playing.
    OnClick = function(self, entity)
        if not self.enabled then return end
        Log.info("[Slot3] OnClick called")
        Event.emitLocal("Slot3Selected", {})
    end
})

return class