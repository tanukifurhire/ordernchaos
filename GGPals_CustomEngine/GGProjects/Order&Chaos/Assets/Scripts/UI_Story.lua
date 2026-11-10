local class = DefineType("StoryOverlay", {
    Serializable = {},
    
    TextureAsset = {
        storyPage1 = 0,
        storyPage2 = 0,
        storyPage3 = 0
    },
    
    currentPage = 1,
    totalPages = 3,
    isVisible = false,
    
    Init = function(self, entity)
        Event.subscribe("OnStoryOpen", function()
            self.isVisible = true
            self.currentPage = 1
            StoryOverlay.ShowPage(self, entity, 1)
            StoryOverlay.SetVisible(self, entity, true)
        end)
        
        Event.subscribe("OnStoryClose", function()
            self.isVisible = false
            StoryOverlay.SetVisible(self, entity, false)
        end)
        
        Event.subscribe("OnStoryNext", function()
            if self.currentPage < self.totalPages then
                self.currentPage = self.currentPage + 1
                StoryOverlay.ShowPage(self, entity, self.currentPage)
            end
        end)
        
        Event.subscribe("OnStoryPrev", function()
            if self.currentPage > 1 then
                self.currentPage = self.currentPage - 1
                StoryOverlay.ShowPage(self, entity, self.currentPage)
            end
        end)
    end,
    
    Start = function(self, entity)
        -- Hide overlay on start
        StoryOverlay.SetVisible(self, entity, false)
    end,
    
    Update = function(self, entity)
    end,
    
    ShowPage = function(self, entity, pageNum)
        local textureID = 0
        
        if pageNum == 1 then
            textureID = self.storyPage1
        elseif pageNum == 2 then
            textureID = self.storyPage2
        elseif pageNum == 3 then
            textureID = self.storyPage3
        end
        
        if textureID > 0 then
            ECS.setSprite(entity, {
                textureAssetID = textureID
            })
            Log.info("[StoryOverlay] Showing page " .. pageNum)
        end
    end,
    
    SetVisible = function(self, entity, visible)
        if visible then
            ECS.setTransform(entity, {
                x = 0,
                y = 0
            })
        else
            ECS.setTransform(entity, {
                x = -10000,
                y = -10000
            })
        end
    end
})

return class