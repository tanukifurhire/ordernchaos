--[[************************************************************************
  \file       Oracle_Ability1_BEAM.lua
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
local class = DefineType("Oracle_Ability1_BEAM", {
  Init = function(self, entity) 
  end,

  Start = function(self, entity)
    Oracle_Ability1_BEAM.PlayAnimation(self, entity, "Idle")
  end,

  PlayAnimation = function(self, entity, stateName)
    local animInfo = ECS.getAnimation(entity)
    if not animInfo then
      --N0 animation component
    end

    if animInfo.currentState == stateName then
      return
    end

    ECS.setAnimation(entity, stateName)
  end,
})

return class