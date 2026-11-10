local class = DefineType("HealthBarNumbers", {
    Serializable = {
        isOrder = true,
        lerpSpeed = 10.0,
        snapEpsilon = 0.01
    },

    targetHealth = 0,
    currentHealth = 0,
    currentMaxHealth = 0,
    currentText = "",
    initialized = false,

    TryBootstrapFromGameManager = function(self, entity)
        local gm = ClientGameManager_instance
        if not gm or not gm.players then return false end

        local playerData = self.isOrder and gm.players.orderPlayer or gm.players.chaosPlayer
        if not playerData then return false end

        local curr = playerData.currentHealth or 0
        local maxv = playerData.maxHealth or CONSTANTS.MAX_HP

        -- Avoid showing 0/MAX before server-side deployment init has populated health.
        if curr <= 0 or maxv <= 0 then return false end

        self.targetHealth = curr
        self.currentMaxHealth = maxv
        self.currentHealth = curr
        self.initialized = true
        self:UpdateText(entity, self.currentHealth, self.currentMaxHealth)
        return true
    end,

    Init = function(self, entity)
        Event.subscribe("OnHealthChanged", function(params)
            local expectedPlayer = self.isOrder and CONSTANTS.ORDER_PLAYER or CONSTANTS.CHAOS_PLAYER
            if params.player == expectedPlayer then
                self.targetHealth = params.curr or 0
                self.currentMaxHealth = params.max or 0

                if not self.initialized then
                    self.currentHealth = self.targetHealth
                    self.initialized = true
                    self:UpdateText(entity, self.currentHealth, self.currentMaxHealth)
                end
            end
        end)
    end,

    Start = function(self, entity)
        self:TryBootstrapFromGameManager(entity)
    end,

    UpdateText = function(self, entity, curr, maxv)
        local nextText = tostring(curr)
        if nextText ~= self.currentText then
            self.currentText = nextText
            ECS.setTextComponent(entity, { text = nextText })
        end
    end,

    Update = function(self, entity)
        if not self.initialized then
            self:TryBootstrapFromGameManager(entity)
            if not self.initialized then return end
        end

        local dt = Time.getDeltaTime()
        local diff = self.targetHealth - self.currentHealth

        if math.abs(diff) > self.snapEpsilon then
            self.currentHealth = self.currentHealth + diff * self.lerpSpeed * dt
        else
            if self.currentHealth ~= self.targetHealth then
                self.currentHealth = self.targetHealth
            end
        end

        local displayHealth = math.floor(self.currentHealth + 0.5)
        if displayHealth < 0 then displayHealth = 0 end
        if self.currentMaxHealth < 0 then self.currentMaxHealth = 0 end
        if self.currentMaxHealth > 0 and displayHealth > self.currentMaxHealth then
            displayHealth = self.currentMaxHealth
        end
        self:UpdateText(entity, displayHealth, self.currentMaxHealth)
    end
})

return class
