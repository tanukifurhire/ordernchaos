-- For side panel
-- view opponent's casted abilities

local class = DefineType("SidePanelBar", 
{
    TextureAsset = {
        orderPanel = 0,
        chaosPanel = 0,
    },

    Init = function(self, entity)
        Log.info("[SidePanelBar] Initialized")
    end, -- end of Init

    Update = function(self, entity)
        local gm = ClientGameManager_instance

        local sprite = ECS.getSprite(entity)
        if not sprite then
            return
        end

        if gm.playerFaction == CONSTANTS.ORDER_PLAYER then
            ECS.setSprite(entity, {
                textureAssetID = self.chaosPanel
            })
        else
            ECS.setSprite(entity, {
                textureAssetID = self.orderPanel
            })
        end
    end -- end of Update

})

return class