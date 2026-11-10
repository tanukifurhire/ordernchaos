local class = DefineType("NetworkTestButton", {
    Serializable = {
        isServer = false
    },

    connected = false,

    Init = function(self, entity)
        if self.isServer then
            Event.subscribe("OnClientConnected", function(params)
                Log.info("[NetworkTestButton] Client connected!")
            end)
        else
            Event.subscribe("OnConnectedToServer", function(params)
                Log.info("[NetworkTestButton] Connected to Server!")
            end)
        end
    end,

    OnClick = function(self, entity, x, y)
        Log.info("[NetworkTestButton] Start Network Test!")
        if self.isServer then
            NetworkTestButton.StartServer(self, entity)
        else
            NetworkTestButton.StartClient(self, entity)
        end
    end,

    StartServer = function(self, entity)
        if RPC.startServer() then
            Log.info("[NetworkTestButton] Server started successfully")
            self.connected = true
        else
            Log.error("[NetworkTestButton] Failed to start server")
        end
    end,

    StartClient = function(self, entity)
        if RPC.connectToServer("onclocal.ddns.net", 9081) then
            Log.info("[NetworkTestButton] Attempting to establish connection with Server..")
        else
            Log.error("[NetworkTestButton] Failed to begin connection request.")
        end
    end
})

return class