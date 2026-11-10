local class = DefineType("MainMenuButton", {
    Serializable = {
        -- Scene to load when clicked
        targetSceneName = "MyScene",
    },

    TextureAsset =
    {
        -- Sprite asset IDs
        normalSprite    = 0,
        hoverSprite     = 0
    },

    Init = function(self, entity)
    end,

    Start = function(self, entity)
        if not self.targetSceneName or self.targetSceneName == "" then
            Log.warning("[MainMenuButton] targetSceneName is empty")
        end

        local sprite = ECS.getSprite(entity)
        if sprite then
            ECS.setSprite(entity, {
                textureAssetID = self.normalSprite
            })
        else
            Log.warning("[MainMenuButton] No Sprite component on entity")
        end
    end,

    Update = function(self, entity)
    end,

    OnHoverEnter = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            Log.info("[MainMenuButton] button hover enter")
            ECS.setSprite(entity, {
                textureAssetID = self.hoverSprite
            })
        end
    end,

    OnHoverExit = function(self, entity)
        local sprite = ECS.getSprite(entity)
        if sprite then
            Log.info("[MainMenuButton] button hover exit")
            ECS.setSprite(entity, {
                textureAssetID = self.normalSprite
            })
        end
    end,

    OnClick = function(self, entity, x, y)
        Log.info("[MainMenuButton] Click at " .. x .. ", " .. y)

        if self.targetSceneName and self.targetSceneName ~= "" then
            local ok = Scene.setActive(self.targetSceneName)
            if not ok then
                Log.error("[MainMenuButton] Failed to switch scene to " .. self.targetSceneName)
            end
        else
            Log.error("[MainMenuButton] targetSceneName not set; cannot switch scene")
        end
    end,
})

return class
