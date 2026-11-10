local class = DefineType("ClientAbilityColorSegment",
{
    Serializable =
    {
        abilityType = 0,     -- which ability this belongs to (abilityID)
        segmentIndex = 1,   -- 1 / 2 / 3 / ...
        playerId = 1        -- 1 = Order, 2 = Chaos
    },

    TextureAsset =
    {
        redHighlight = 0,
        redUnhighlight = 0,

        blueHighlight = 0,
        blueUnhighlight = 0
    },

    resolvedAbilityType = 0,

    Init = function(self, entity)

        -- Listen for color count updates (filtered by player)
        Event.subscribe("AbilityColorCountsChanged", function(params)
            local gm = ClientGameManager_instance
            if not gm then return end

            local ability = gm:GetAbilityByIDGlobal(self.resolvedAbilityType)
            if not ability then return end

            local isOrderAbility = (ability.ownerFaction == CONSTANTS.ORDER_PLAYER)
            if isOrderAbility and params.player == 2 then return end
            if not isOrderAbility and params.player == 1 then return end

            ClientAbilityColorSegment.Refresh(self, entity, params.red, params.blue)
        end)

        -- Start hidden
        ClientAbilityColorSegment.Refresh(self, entity, 0, 0)
    end,

    Start = function(self, entity)
        -- Resolve original editor ability ID through arsenal map
        if ClientGameManager_instance then
            self.resolvedAbilityType = ClientGameManager_instance:ResolveArsenalAbility(self.abilityType)
        else
            self.resolvedAbilityType = self.abilityType
        end

        if self.resolvedAbilityType ~= self.abilityType then
            Log.info("[ClientAbilityColorSegment] Resolved ability " .. self.abilityType .. " -> " .. self.resolvedAbilityType)
        end
    end,

    Refresh = function(self, entity, redCount, blueCount)

        local gm = ClientGameManager_instance
        if not gm then return end

        local ability = gm:GetAbilityByIDGlobal(self.resolvedAbilityType)
        if not ability then return end

        ------------------------------------------------------------------
        -- Build layout: reds first, then blues
        ------------------------------------------------------------------
        local layout = {}

        for i = 1, ability.numRed do
            layout[#layout + 1] = "red"
        end

        for i = 1, ability.numBlue do
            layout[#layout + 1] = "blue"
        end

        local color = layout[self.segmentIndex]

        ------------------------------------------------------------------
        -- Segment not used -> invisible
        ------------------------------------------------------------------
        if not color then
            ECS.setSprite(entity, { textureAssetID = 0 })

            local ui = ECS.getUIComponent(entity)
            if ui then
                ui.colorMultiply.a = 0.0
                ECS.setUIComponent(entity, ui)
            end

            return
        end

        ------------------------------------------------------------------
        -- Segment is used -> restore visibility
        ------------------------------------------------------------------
        local ui = ECS.getUIComponent(entity)
        if ui then
            ui.colorMultiply.a = 1.0
            ECS.setUIComponent(entity, ui)
        end

        ------------------------------------------------------------------
        -- Determine if this segment is filled
        ------------------------------------------------------------------
        local filled = false

        if color == "red" then
            filled = self.segmentIndex <= redCount
        else
            -- Blue segments start AFTER red slots
            filled = (self.segmentIndex - ability.numRed) <= blueCount
        end

        ------------------------------------------------------------------
        -- Choose sprite
        ------------------------------------------------------------------
        local sprite = 0

        if color == "red" then
            sprite = filled and self.redHighlight or self.redUnhighlight
        else
            sprite = filled and self.blueHighlight or self.blueUnhighlight
        end

        ECS.setSprite(entity, {
            textureAssetID = sprite
        })
    end
})

return class
