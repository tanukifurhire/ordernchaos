local class = DefineType("APDisplay", {
    TextureAsset = 
    {
        ap_2 = 0,
        ap_1 = 0,
        ap_0 = 0
    },
    
    Init = function(self, entity)
        Event.subscribe("OnAPUpdate", function(params)
            if params.value == 2 then
                ECS.setSprite(entity, {
                    textureAssetID = self.ap_2
                })
            elseif params.value == 1 then
                ECS.setSprite(entity, {
                    textureAssetID = self.ap_1
                })
            else
                ECS.setSprite(entity, {
                    textureAssetID = self.ap_0
                })
            end
        end)

        Event.subscribe("HideAP", function(params)
            ECS.setUIComponent(entity, {
                colorMultiply = {r = 0.0, g = 0.0, b = 0.0, a = 0.0}
            })
        end)

        Event.subscribe("ShowAP", function(params)
            ECS.setUIComponent(entity, {
                colorMultiply = {r = 1.0, g = 1.0, b = 1.0, a = 1.0}
            })
        end)

        Event.subscribe("SetUIBottomBarEnabled", function(params)
            ECS.setUIComponent(entity, {
                colorMultiply = params.colorMultiply
            })
        end)
    end
})

return class