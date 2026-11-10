local class = DefineType("PnPTurnPhases",
{
    Contains =
    {
        deploymentPhase = "PnPDeploymentPhase",
        preparationPhase = "PnPPreparationPhase",
        resolutionPhase = "ClientResolutionPhase",
        endGamePhase = "PnPEndGamePhase"
    },
    currentPhase = nil,
    InitPhases = function(self)
        Log.info("[PnPTurnPhases] InitPhases called -- starting PnP deployment phase")
        self.currentPhase = self.deploymentPhase
        self.currentPhase:Enter()

        Event.subscribe("ForceEndGame", function(params)
            local gm = ClientGameManager_instance
            if not gm then return end
            local winner = params.winner or 3
            Log.info("[PnPTurnPhases] ForceEndGame: winner=" .. winner)
            -- PnPEndGamePhase reads from server gameData or resolutionData
            local server = PassAndPlayServer_instance
            if server and server.gameData then
                server.gameData.winner = winner
            end
            gm.resolutionData = gm.resolutionData or {}
            gm.resolutionData.winner = winner
            self:ChangePhase(self.endGamePhase)
        end)
    end,
    ChangePhase = function(self, phase)
        Log.info("[PnPTurnPhases] ChangePhase called")
        if self.currentPhase then
            self.currentPhase:Exit()
        end

        self.currentPhase = phase
        if self.currentPhase and self.currentPhase.Enter then
            self.currentPhase:Enter()
        else
            Log.error("[PnPTurnPhases] ChangePhase received invalid phase target")
        end
    end,
    UpdatePhase = function(self, entity)
        if self.currentPhase and self.currentPhase.Update then
            self.currentPhase:Update(entity)
        end
    end
})

return class
