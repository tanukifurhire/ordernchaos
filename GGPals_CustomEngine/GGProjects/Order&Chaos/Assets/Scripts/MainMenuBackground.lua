Event.define("MainMenuHover", {
    state = "integer"
})

local class = DefineType("MainMenuBackground", {

    TextureAsset =
    {
        plainSprite    = 0,  -- default background (no hover)

        storySprite    = 0,
        orderSprite    = 0,
        chaosSprite    = 0,
        howToSprite    = 0,
        settingsSprite = 0,
        quitSprite     = 0
    },

    Init = function(self, entity)
        Event.subscribe("MainMenuHover", function(params)
            MainMenuBackground.Refresh(self, entity, params.state)
        end)
    end,

    Start = function(self, entity)
        -- Start on plain background
        MainMenuBackground.Refresh(self, entity, 0)
    end,

    Refresh = function(self, entity, state)
        local sprite = 0

        if state == 0 then
            sprite = self.plainSprite
        elseif state == 1 then
            sprite = self.storySprite
        elseif state == 2 then
            sprite = self.orderSprite
        elseif state == 3 then
            sprite = self.chaosSprite
        elseif state == 4 then
            sprite = self.howToSprite
        elseif state == 5 then
            sprite = self.settingsSprite
        elseif state == 6 then
            sprite = self.quitSprite
        end

        if sprite ~= 0 then
            ECS.setSprite(entity, {
                textureAssetID = sprite
            })
        end
    end
})

return class
