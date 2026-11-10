--[[
    One faction's seat in the lobby room: is somebody sitting in it, and have
    they readied up.

    This used to answer neither question. It read Persistent.sessionFaction
    once in Start and lit whichever book was the local player's, which is the
    one thing in this scene nobody needs telling - and it never subscribed to
    OnLobbyRoomStateChanged, so the opponent's book looked identical whether
    they had joined, readied, or never arrived.

    Three states, out of the two textures the scene supplies:

        empty            normal texture, greyed
        joined           normal texture, full colour
        ready            glow texture

    The local player's own seat is occupied by definition, so it lands on
    "joined" immediately and needs no separate highlight.
]]
local class = DefineType("LobbyBook", {
    Serializable = {
        isOrderSide = true
    },

    TextureAsset = {
        normalTexture = 0,
        glowTexture = 0
    },

    joined = false,
    ready = false,

    greyTint = { r = 0.4, g = 0.4, b = 0.4, a = 1.0 },
    fullTint = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 },

    Init = function(self, entity)
        Event.subscribe("OnLobbyRoomStateChanged", function(params)
            if self.isOrderSide then
                self.joined = params.orderJoined and true or false
                self.ready  = params.orderReady and true or false
            else
                self.joined = params.chaosJoined and true or false
                self.ready  = params.chaosReady and true or false
            end

            self:UpdateVisual(entity)
        end)

        self:UpdateVisual(entity)
    end,

    Start = function(self, entity)
        -- Our own seat is filled the moment we are in the room, so show that
        -- much straight away rather than sitting greyed out until the first
        -- broadcast lands. The opponent's seat stays empty until the server
        -- says otherwise.
        local myFaction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
        local isMySeat = (self.isOrderSide and myFaction == CONSTANTS.ORDER_PLAYER) or
                         (not self.isOrderSide and myFaction == CONSTANTS.CHAOS_PLAYER)

        if isMySeat and not self.joined then
            self.joined = true
            self:UpdateVisual(entity)
        end
    end,

    UpdateVisual = function(self, entity)
        if self.normalTexture == 0 then return end

        if self.ready and self.glowTexture > 0 then
            ECS.setSprite(entity, { textureAssetID = self.glowTexture })
            ECS.setUIComponent(entity, { colorMultiply = self.fullTint })
            return
        end

        ECS.setSprite(entity, { textureAssetID = self.normalTexture })
        ECS.setUIComponent(entity, {
            colorMultiply = self.joined and self.fullTint or self.greyTint
        })
    end
})

return class
