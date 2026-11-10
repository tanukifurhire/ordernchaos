--[[************************************************************************
  \file       AnimPreviewVFXPlayer.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.0
  \brief
    EDIT YOUR BRIEF HERE
  \details
    EDIT YOUR DETAILS HERE
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("AnimPreviewVFXPlayer",
{
  isPlaying = false,
  currentState = nil,
  eventHandler = 0,
  offsetX = 0,
  offsetY = 0,

  Init = function(self, entity)
    Log.info("AnimPreviewVFXPlayer: Init")

    -- Listen for anim preview VFX requests
    self.eventHandler = Event.subscribe("PlayPreviewVFX", function(params)
      AnimPreviewVFXPlayer.OnPlayRequested(self, entity, params)
    end)

    -- subscribe to animation transition event
    self.eventHandlers = self.eventHandlers or {}
    table.insert(self.eventHandlers, Event.subscribe("AnimationTransition", function()
      AnimPreviewVFXPlayer.OnAnimationTransition(self, entity)
    end))
  end,

  OnPlayRequested = function (self, entity, params)
    if not params or not params.animationName then
      Log.warning("AnimPreviewVFXPlayer: Missing animationName")
      return
    end

    Log.info("AnimPreviewVFXPlayer: Request for animation: " .. params.animationName)
    Log.info("AnimPreviewVFXPlayer: Current playing state: " .. tostring(self.isPlaying))
    

    if params.animationName == "Oracle_Ability1_BEAM" then
      Log.info("Spawning Ability Prefab!")
      self.offsetX = 160
      self.offsetY = 180
      Prefab.spawn("Oracle_Ability1_BEAM", params.posX + self.offsetX, params.posY + self.offsetY)
      self.isPlaying = true
    end
  end,

  -- Called from animationTransition event
  OnAnimationTransition = function(self, entity) 
    if self.isPlaying then
      Log.info("AnimPreviewVFXPlayer: Animation finished")
      self.isPlaying = false
      self.currentState = nil
    end
  end,

  Destroy = function(self, entity)
    Log.info("AnimPreviewVFXPlayer: Destroy")
    if self.eventHandler ~= 0 then
      Event.unsubscribe(self.eventHandler)
    end
  end
})
return class
