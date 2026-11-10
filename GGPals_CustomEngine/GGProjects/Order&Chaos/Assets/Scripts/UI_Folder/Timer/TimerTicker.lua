--[[************************************************************************
  \file       TimerTicker.lua
  \author     Nelson Tok
  \par        Email: t.duanjienelson@digipen.edu
  \date       09 March 2026
  \version    1.0
  \brief
    Restarts the timer animation when a new turn begins.
  \details
    The animation asset handles sync and freeze automatically:
     frameTime set to (phaseTimeLimit / totalFrames) e.g. 60.0 / 452 ≈ 0.1327
     looping disabled so it freezes on the last frame
  \history
    - 1.0 (01 February 2026): Created this script.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("TimerTicker",
{
    Serializable = {
        countdownState = "Ticking",
        finishedState  = "End",
    },

    turnChanged             = 0,
    endActionButtonPressed  = 0,
    passAndPlayDisplayedHandler = 0,
    passAndPlayHiddenHandler    = 0,

    passAndPlayDisplayed = false,

    Init = function(self, entity)
        
        if self.turnChanged ~= 0 then
            Event.unsubscribe(self.turnChanged)
        end
        if self.endActionButtonPressed ~= 0 then
            Event.unsubscribe(self.endActionButtonPressed)
        end
        if self.passAndPlayDisplayedHandler ~= 0 then
            Event.unsubscribe(self.passAndPlayDisplayedHandler)
        end
        if self.passAndPlayHiddenHandler ~= 0 then
            Event.unsubscribe(self.passAndPlayHiddenHandler)
        end

        self.turnChanged = Event.subscribe("TurnChanged", function(params)
            if not self.passAndPlayDisplayed then
                TimerTicker.PlayAnimation(self, entity, self.countdownState)
                Log.info("[TimerTicker] TurnChanged -> countdown started")
            end
        end)

        self.endActionButtonPressed = Event.subscribe("OnEndActionButtonPressed", function(params)
            TimerTicker.PlayAnimation(self, entity, self.finishedState)
            Log.info("[TimerTicker] EndTurn -> finished state")
        end)

        self.passAndPlayDisplayedHandler = Event.subscribe("OnPassAndPlayDisplayed", function()
            self.passAndPlayDisplayed = true
            TimerTicker.PlayAnimation(self, entity, self.finishedState)
            Log.info("[TimerTicker] PassAndPlay shown -> pausing ticker")
        end)

        self.passAndPlayHiddenHandler = Event.subscribe("OnPassAndPlayHidden", function()
            self.passAndPlayDisplayed = false
            TimerTicker.PlayAnimation(self, entity, self.countdownState)
            Log.info("[TimerTicker] PassAndPlay hidden -> resuming ticker")
        end)

        self.showPhaseOverlayHandler = Event.subscribe("ShowPhaseOverlay", function(params)
            if params.phase == "resphase" then
                TimerTicker.PlayAnimation(self, entity, self.finishedState)
            end
        end)


    end,

    Start = function(self, entity)
        TimerTicker.PlayAnimation(self, entity, self.countdownState)
        Log.info("[TimerTicker] Start -> countdown started")
    end,

    OnCountdownFinished = function(self, entity)
        TimerTicker.PlayAnimation(self, entity, self.finishedState)
        Log.info("[TimerTicker] Countdown elapsed -> finished state")
    end,

    PlayAnimation = function(self, entity, stateName)
        local animInfo = ECS.getAnimation(entity)
        if animInfo and animInfo.currentState ~= stateName then
            ECS.setAnimation(entity, stateName)
        end
    end,
})

return class