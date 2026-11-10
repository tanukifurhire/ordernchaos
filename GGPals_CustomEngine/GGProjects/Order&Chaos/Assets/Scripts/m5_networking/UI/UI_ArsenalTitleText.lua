local class = DefineType("UI_ArsenalTitleText", {
    -- Funny phrases synonymous with "Customize Your Loadout"
    TITLE_PHRASES = {
        -- Order N Chaos style
        "Abilities N Passives!",
        "Skills N Thrills!",
        "Powers N Perks!",
        "Lock N Load!",
        
        -- Hype/Battle
        "Prepare for Battle!",
        "Gear Up, Champion!",
        "Ready Your Arsenal!",
        "An IQ Too High..?",
        "Is He Cooking?",
        
        -- Gaming culture
        "Pick Your Poison!",
        "Min-Max This!",
        "Built Different..?",
        "Meta or Meme?",
        "Skill Issue Incoming",
        
        -- Yu-Gi-Oh references
        "D-D-D-DUEL!",
        "Heart of the Cards!",
        "No Pathetic Cards..?",
        "Third Rate Duelist.",
        "Screw the Rules!",
        "MONSTA CARDO!",
        
        -- Dramatic
        "Destiny Awaits...",
        "Forge Your Legend!",
        "The Choice is Yours",
        "Embrace Your Power!",
        
        -- Silly
        "Ooh, Shiny Buttons!",
        "Malevolent Kitchen...",
        "No Refunds!",
        "Warranty Void If Used",
        "Side Effects May Vary",
        "Dis is Krezi Guys",
        
        -- Encouraging
        "You Got This!",
        "Make It Count!",
        "Show 'Em What You Got!",
        "Believe in Something",
        
        -- Self-aware
        "UI_ArsenalTitleText.lua",
        "Insert Title Here",
        "[PLACEHOLDER TEXT]"
    },

    Init = function(self, entity)
        self.entity = entity
        
        -- Seed RNG with current time
        math.randomseed(os.time())
        
        -- Pick a random phrase on init
        local idx = math.random(1, #self.TITLE_PHRASES)
        local phrase = self.TITLE_PHRASES[idx]
        
        -- Set the text
        local text = ECS.getTextComponent(entity)
        if not text then return end
        text.text = phrase
        ECS.setTextComponent(entity, text)
        
        Log.info("[ArsenalTitle] " .. phrase)
    end
})

return class
