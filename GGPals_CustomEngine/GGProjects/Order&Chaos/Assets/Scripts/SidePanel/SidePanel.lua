-- For side panel
-- view opponent's casted abilities

local class = DefineType("SidePanel", 
{
    Serializable = {
        closeX = 0.0,
        openX = 0.0,

        closeOpenY = 0,

        lerpSpeed = 12.0
    },

    panelOpen = false;
    targetX = 0.0,
    currentX = -900.0,

    Init = function(self, entity)
        Log.info("[SidePanel] Initialized")

        -- start closed
        self.currentX = self.closeX
        self.targetX = self.closeX

        local t = ECS.getTransform(entity)
        t.x = self.currentX
        t.y = self.closeOpenY
        ECS.setTransform(entity, t)

        Event.subscribe("OpenPanel", function(params)
            self.targetX = self.openX
            self.panelOpen = true
            Event.emitLocal("PlayUIMenuPauseOpen", {})

            Log.info("[SidePanel] Panel opened")
        end)

        Event.subscribe("ClosePanel", function(params)
            self.targetX = self.closeX
            self.panelOpen = false
            Event.emitLocal("PlayUIMenuPauseOpen", {})

            Log.info("[SidePanel] Panel closed")
        end)

        -- close the panel if turn changed
        Event.subscribe("TurnChanged", function(params)
            self.currentX = self.closeX
            self.targetX = self.closeX
            self.panelOpen = false
            
            local t = ECS.getTransform(entity)
            t.x = self.closeX
            t.y = self.closeOpenY
            ECS.setTransform(entity, t)
        end)
    end, -- end of Init

    Update = function(self, entity)
        if Input.isKeyTriggered(258) then
            if not self.panelOpen then
                Event.emitLocal("OpenPanel", {})
            else
                Event.emitLocal("ClosePanel", {})
            end
        end

        local dt = Time.getDeltaTime()
        local t = ECS.getTransform(entity)
        local dx = self.targetX - self.currentX
        self.currentX = self.currentX + dx * self.lerpSpeed * dt
        t.x = self.currentX
        t.y = self.closeOpenY
        ECS.setTransform(entity, t)
        
    end -- end of Update

})

return class