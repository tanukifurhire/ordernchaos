local class = DefineType("ClientTurnPhases", 
{
    Contains = 
    {
        deploymentPhase = "ClientDeploymentPhase",
        preparationPhase = "ClientPreparationPhase",
        resolutionPhase = "ClientResolutionPhase",
        endGamePhase = "ClientEndGamePhase"
    },

    currentPhase = nil,

    InitPhases = function(self)
        self.currentPhase = self.deploymentPhase
        self.currentPhase:Enter()

        Event.subscribe("ForceEndGame", function(params)
            local gm = ClientGameManager_instance
            if not gm then return end
            local winner = params.winner or 3
            Log.info("[ClientTurnPhases] ForceEndGame: winner=" .. winner)
            gm.resolutionData = gm.resolutionData or {}
            gm.resolutionData.winner = winner
            self:ChangePhase(self.endGamePhase)
        end)
    end,

    ChangePhase = function(self, phase)
        if self.currentPhase then
            self.currentPhase:Exit()
        end

        self.currentPhase = phase
        if self.currentPhase and self.currentPhase.Enter then
            self.currentPhase:Enter()
        else
            Log.error("[ClientTurnPhases] ChangePhase received invalid phase target")
        end
    end,

    UpdatePhase = function(self, entity)
        if self.currentPhase and self.currentPhase.Update then
            self.currentPhase:Update(entity)
        end
    end
})

return class
