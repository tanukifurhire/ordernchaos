local class = DefineType("PlayerHealthNameDisplay", {
    Serializable = 
    {
        isOrder = true
    },

    Init = function(self, entity)
        -- Will be set in Start once Persistent is available
    end,

    Start = function(self, entity)
        PlayerNameDisplay.UpdateName(self, entity)
        
        -- Listen for room state updates (when opponent joins/leaves)
        Event.subscribe("OnRoomStateUpdate", function(params)
            PlayerNameDisplay.UpdateNameFromState(self, entity, params)
        end)
    end,
    
    UpdateNameFromState = function(self, entity, state)
        local displayName = ""
        
        if self.isOrder then
            displayName = state.orderName or ""
        else
            displayName = state.chaosName or ""
        end
        
        -- Fallback if empty
        if displayName == "" then
            displayName = self.isOrder and "Order" or "Chaos"
        end
        
        ECS.setTextComponent(entity, { text = displayName })
    end,
    
    UpdateName = function(self, entity)
        local displayName = ""
        
        if self.isOrder then
            -- Display Order player's name (our name if we're Order, opponent if we're Chaos)
            if Persistent.sessionFaction == CONSTANTS.ORDER_PLAYER then
                displayName = Persistent.sessionPlayerName or "Order"
            else
                displayName = Persistent.sessionOpponentName or "Order"
            end
        else
            -- Display Chaos player's name (our name if we're Chaos, opponent if we're Order)
            if Persistent.sessionFaction == CONSTANTS.CHAOS_PLAYER then
                displayName = Persistent.sessionPlayerName or "Chaos"
            else
                displayName = Persistent.sessionOpponentName or "Chaos"
            end
        end
        
        -- Fallback if empty
        if displayName == "" then
            displayName = self.isOrder and "Order" or "Chaos"
        end
        
        ECS.setTextComponent(entity, { text = displayName })
    end
})

return class