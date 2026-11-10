--[[************************************************************************
  \file       TooltipName.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.1
  \brief
    Displays the ability name in the tooltip.
  \details
    Shows the name of the currently hovered ability.
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
    - 1.1 (03 April 2026): Fixed text update to only change when ability changes.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("TooltipName", {
  currentAbilityID = nil,
  lastUpdatedAbilityID = nil,
  isVisible = false,

  params = {},
  
  Start = function(self, entity)
    -- Hide immediately on scene start
    self.isVisible = false
    self.lastUpdatedAbilityID = nil
    ECS.setTextComponent(entity, { visible = false })

    Event.subscribe("ShowAbilityTooltip", function(params)
      self.currentAbilityID = params.abilityID
      self.isVisible = true
      -- Force text update on show
      self.lastUpdatedAbilityID = nil

      self.params = params
      ECS.setTextComponent(entity, { visible = true })
    end)
    
    Event.subscribe("HideAbilityTooltip", function()
      self.isVisible = false
      self.lastUpdatedAbilityID = nil
      ECS.setTextComponent(entity, { visible = false })
    end)
  end,
  
  Update = function(self, entity)
    if not self.isVisible then return end
    if not self.currentAbilityID then return end
    -- Only update text when ability changes
    if self.lastUpdatedAbilityID == self.currentAbilityID then return end
    
    if self.params.status == true then
      local status = StatusList[self.currentAbilityID]
      if not status then return end

      local name = (status.statusName or "Unknown Status")  .. " (" .. self.params.stacks .. " stacks)"
      ECS.setTextComponent(entity, {
        text = name,
        color = { r=255, g=255, b=255, a=255 }
      })
      
      self.lastUpdatedAbilityID = self.currentAbilityID
      return
    end

    local ability = AbilityList[self.currentAbilityID]
    if not ability then return end

    local name = ability.name or "Unknown Ability"
    ECS.setTextComponent(entity, {
      text = name,
      color = { r=255, g=255, b=255, a=255 }
    })
    
    self.lastUpdatedAbilityID = self.currentAbilityID
  end
})

return class