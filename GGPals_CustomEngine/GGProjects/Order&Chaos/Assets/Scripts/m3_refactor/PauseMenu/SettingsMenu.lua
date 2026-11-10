local class = DefineType("SettingsMenu",
{   
    Serializable = {
        -- position when closed
        closedX = 2000.0,
        closedY = 0.0,

        -- position when open
        openX = 250.0, -- will lerp to this position
        openY = 0.0,

        easeOpen = 0.18,
        easeClosed = 0.05,
        snapDistance = 0.5
    },

    settingsMenuOpen = false,
    targetX = 0.0,
    targetY = 0.0,
    currentEase = 0.18,

    Init = function(self, entity)
        Log.info("[SettingsMenu] Initialized")

        self.targetX = self.closedX
        self.targetY = self.closedY
        self.currentEase = self.easeClosed

        local t = ECS.getTransform(entity)
        t.x = self.closedX
        t.y = self.closedY
        ECS.setTransform(entity, t)

        -- only one can be active at a time,
        -- if settings menu active, how to play is disabled (and vice versa)
        Event.subscribe("OnOpenSettingsMenu", function()
            self.settingsMenuOpen = true
            self.targetX = self.openX
            self.targetY = self.openY
            self.currentEase = self.easeOpen
        end)

        Event.subscribe("OnHowToPlayOpen", function()
            self.settingsMenuOpen = false
            self.targetX = self.closedX
            self.targetY = self.closedY
            self.currentEase = self.easeClosed
        end)

        -- default behaviour: settings open by default on pause
        Event.subscribe("OnPause", function()
            self.settingsMenuOpen = true
            self.targetX = self.openX
            self.targetY = self.openY
            self.currentEase = self.easeOpen
        end)

        -- if resume, disable settings menu
        Event.subscribe("OnResume", function()
            if self.settingsMenuOpen then
                self.targetX = self.closedX
                self.targetY = self.closedY
                self.currentEase = self.easeClosed
                self.settingsMenuOpen = false
            end
        end)

        -- for main menu
        Event.subscribe("OnCloseSettingsMenu", function()
            if self.settingsMenuOpen then
                self.targetX = self.closedX
                self.targetY = self.closedY
                self.currentEase = self.easeClosed
                self.settingsMenuOpen = false
            end
        end)
    end, -- end of Init

    Update = function(self, entity)
        local t = ECS.getTransform(entity)

        local dx = self.targetX - t.x
        local dy = self.targetY - t.y
        local distSq = dx * dx + dy * dy
        local snapSq = self.snapDistance * self.snapDistance

        if distSq <= snapSq then
            -- snap into position
            t.x = self.targetX
            t.y = self.targetY
        else
            -- lerp into position
            t.x = t.x + dx * self.currentEase
            t.y = t.y + dy * self.currentEase
        end

        ECS.setTransform(entity, t)
    end -- end of Update
})

return class