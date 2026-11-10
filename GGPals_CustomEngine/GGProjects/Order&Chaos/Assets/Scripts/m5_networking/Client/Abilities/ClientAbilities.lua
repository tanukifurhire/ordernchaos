local class = DefineType("ClientAbilities", 
{
    Contains = 
    {
        -- Order Abilities
        oracleAbility1 = "ClientOracleAbility1",
        oracleAbility2 = "ClientOracleAbility2",
        dragoonAbility1 = "ClientDragoonAbility1",
        dragoonAbility2 = "ClientDragoonAbility2",
        tirionAbility1 = "ClientTirionAbility1",
        tirionAbility2 = "ClientTirionAbility2",

        -- Chaos abilities
        impAbility1 = "ClientImpAbility1",
        impAbility2 = "ClientImpAbility2",
        eyeAbility1 = "ClientEyeAbility1",
        eyeAbility2 = "ClientEyeAbility2",
        vizomAbility1 = "ClientVizomAbility1",
        vizomAbility2 = "ClientVizomAbility2",

        -- Generic abilities
        crimsonSlash = "CrimsonSlash",
        sapphireSwirl = "SapphireSwirl"
    },

    Init = function(self, entity)
        self.oracleAbility1:Init(entity)
        self.oracleAbility2:Init(entity)
        self.dragoonAbility1:Init(entity)
        self.dragoonAbility2:Init(entity)
        self.tirionAbility1:Init(entity)
        self.tirionAbility2:Init(entity)
        self.impAbility1:Init(entity)
        self.impAbility2:Init(entity)
        self.eyeAbility1:Init(entity)
        self.eyeAbility2:Init(entity)
        self.vizomAbility1:Init(entity)
        self.vizomAbility2:Init(entity)
        self.crimsonSlash:Init(entity)
        self.sapphireSwirl:Init(entity)
    end,

    Start = function(self, entity)
    
    end,
})

return class