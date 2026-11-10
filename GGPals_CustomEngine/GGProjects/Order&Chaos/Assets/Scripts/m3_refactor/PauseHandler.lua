local class = DefineType("PauseHandler", {
    Serializable = 
    {
        pausedPosX = 0.001,
        pausedPosY = 0.001,
        resumePosX = 0.001,
        resumePosY = 0.001
    },

    paused = false,

    Init = function(self, entity)
        Event.subscribe("OnPause", function(params)
            self.paused = true
            Event.emitLocal("PlayUIMenuPauseOpen", {})

            if not NetworkedGameClient_instance then
                System.pause()
            end

            Log.info("Pause!")

            
            ECS.setTransform(entity, {
                x = self.pausedPosX,
                y = self.pausedPosY
            })
        end)

        Event.subscribe("OnResume", function(params)
            self.paused = false
            System.resume()
            Log.info("Resume!")
            ECS.setTransform(entity, {
                x = self.resumePosX,
                y = self.resumePosY
            })
        end)
    end,

    Start = function(self, entity)
        
    end,

    Update = function(self, entity)
        if Input.isKeyTriggered(256) and self.paused then
            Event.emitLocal("OnResume", {})
        elseif Input.isKeyTriggered(256) and not self.paused then
            Event.emitLocal("OnPause", {})
        end
    end
})

return class
