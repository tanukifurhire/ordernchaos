-- M4 RPC Test Handler
-- Handles RPC calls for testing timeout/reliability improvements

local class = DefineType("M4RPCHandler", {
    Serializable = {
        enableLogging = true
    },
    
    messageCount = 0,
    lastMessageTime = 0,
    
    Init = function(self, entity)
        Log.info("[M4RPC] Handler initialized")
    end,
    
    Start = function(self, entity)
        Log.info("[M4RPC] Registering RPC handlers")
        
        -- Register test handlers
        Event.subscribe("M4Test_Ping", function(params)
            M4RPCHandler.HandlePing(self, entity, params)
        end)
        
        Event.subscribe("M4Test_Echo", function(params)
            M4RPCHandler.HandleEcho(self, entity, params)
        end)
        
        Event.subscribe("M4Test_Stress", function(params)
            M4RPCHandler.HandleStress(self, entity, params)
        end)
    end,
    
    Update = function(self, entity)
        -- Monitor for timeout issues
    end,
    
    HandlePing = function(self, entity, params)
        self.messageCount = self.messageCount + 1
        
        if self.enableLogging then
            Log.info("[M4RPC] Ping received (#" .. self.messageCount .. ")")
            
            -- Echo back if params exist
            if params and params.message then
                Log.info("[M4RPC]   Message: " .. params.message)
            end
        end
        
        -- Send response
        RPC.call("M4Test_Pong", "Pong from server!")
    end,
    
    HandleEcho = function(self, entity, params)
        if self.enableLogging and params and params.data then
            Log.info("[M4RPC] Echo: " .. params.data)
        end
        
        -- Echo back the data
        if params and params.data then
            RPC.call("M4Test_EchoResponse", params.data)
        end
    end,
    
    HandleStress = function(self, entity, params)
        -- Handle rapid RPC
        self.messageCount = self.messageCount + 1
        
        if self.messageCount % 100 == 0 then
            Log.info("[M4RPC] Stress test messages: " .. self.messageCount)
        end
    end
})

return class
