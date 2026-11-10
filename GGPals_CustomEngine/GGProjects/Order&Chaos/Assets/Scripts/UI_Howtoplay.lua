local class = DefineType("HowToPlayPanel", {
    Serializable = {
        -- Panel position when visible/hidden
        visibleX = 0,
        visibleY = 0,
        hiddenX = -10000,
        hiddenY = -10000,
        
        -- Current page
        currentPage = 1,
        totalPages = 4
    },
    
    TextureAsset = {
        -- Tutorial page images
        page1 = 0,
        page2 = 0,
        page3 = 0,
        page4 = 0,
    },
    
    isVisible = false,
    
    Init = function(self, entity)
        -- Start hidden
        local t = ECS.getTransform(entity)
        if t then
            t.x = self.hiddenX
            t.y = self.hiddenY
            ECS.setTransform(entity, t)
        end
        self.isVisible = false

        Event.subscribe("OnHowToPlayOpen", function(params)
            HowToPlayPanel.Show(self, entity)
        end)

        Event.subscribe("OnHowToPlayClose", function(params)
            HowToPlayPanel.Hide(self, entity)
        end)
        
        Event.subscribe("OnHowToPlayNext", function(params)
            HowToPlayPanel.NextPage(self, entity)
        end)
        
        Event.subscribe("OnHowToPlayPrev", function(params)
            HowToPlayPanel.PrevPage(self, entity)
        end)
    end,
    
    Start = function(self, entity)
        HowToPlayPanel.UpdatePage(self, entity)
    end,
    
    Show = function(self, entity)
        local t = ECS.getTransform(entity)
        if t then
            t.x = self.visibleX
            t.y = self.visibleY
            ECS.setTransform(entity, t)
        end
        self.isVisible = true
        self.currentPage = 1
        HowToPlayPanel.UpdatePage(self, entity)
        Log.info("[HowToPlayPanel] Showing panel")
    end,
    
    Hide = function(self, entity)
        local t = ECS.getTransform(entity)
        if t then
            t.x = self.hiddenX
            t.y = self.hiddenY
            ECS.setTransform(entity, t)
        end
        self.isVisible = false
        Log.info("[HowToPlayPanel] Hiding panel")
    end,
    
    NextPage = function(self, entity)
        if self.currentPage < self.totalPages then
            self.currentPage = self.currentPage + 1
            HowToPlayPanel.UpdatePage(self, entity)
            Log.info("[HowToPlayPanel] Next page: " .. self.currentPage)
        end
    end,
    
    PrevPage = function(self, entity)
        if self.currentPage > 1 then
            self.currentPage = self.currentPage - 1
            HowToPlayPanel.UpdatePage(self, entity)
            Log.info("[HowToPlayPanel] Previous page: " .. self.currentPage)
        end
    end,
    
    UpdatePage = function(self, entity)
        local texture = 0
        if self.currentPage == 1 then texture = self.page1
        elseif self.currentPage == 2 then texture = self.page2
        elseif self.currentPage == 3 then texture = self.page3
        elseif self.currentPage == 4 then texture = self.page4
        end
        
        if texture ~= 0 then
            ECS.setSprite(entity, {
                textureAssetID = texture
            })
        end
    end,
    
    Update = function(self, entity)
    end
})

return class