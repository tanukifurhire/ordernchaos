local class = DefineType("SidePanelToggle", 
{
    Serializable = {
        normalSizeX = 0.001,
        normalSizeY = 0.001,
        hoverSizeX = 0.001,
        hoverSizeY = 0.001
    },

    panelOpen = false,

    Init = function(self, entity)
        Log.info("[SidePanelToggle] Initialized")

    end, -- end of Init

    OnHoverEnter = function(self, entity)
        Event.emitLocal("PlayMainMenuHoverBling", {})
    end,

    OnHoverExit = function(self, entity)
        
    end,

    OnClick = function(self, entity)
        Event.emitLocal("PlayMainMenuClick", {})
        if not self.panelOpen then
            Event.emitLocal("OpenPanel", {})
            self.panelOpen = true
        else 
            Event.emitLocal("ClosePanel", {})
            self.panelOpen = false
        end

    end,

    Update = function(self, entity)
        
    end -- end of Update

})

return class