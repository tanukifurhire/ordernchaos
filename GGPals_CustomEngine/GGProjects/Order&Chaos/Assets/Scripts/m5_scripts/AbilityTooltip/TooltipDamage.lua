--[[************************************************************************
  \file       TooltipDamage.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.1
  \brief
    Displays the ability damage in the tooltip.
  \details
    Shows the base damage and bonus damage of the hovered ability.
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
    - 1.1 (03 April 2026): Fixed text update to only change when ability changes.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("TooltipDamage", {
  currentAbilityID = nil,
  lastUpdatedAbilityID = nil,
  isVisible = false,

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
      ECS.setTextComponent(entity, { visible = true})
    end)

    Event.subscribe("HideAbilityTooltip", function()
      self.isVisible = false
      self.lastUpdatedAbilityID = nil
      ECS.setTextComponent(entity, { visible = false})
    end)
  end,

  Update = function(self, entity)
    if not self.isVisible then return end
    if not self.currentAbilityID then return end
    -- Only update text when ability changes
    if self.lastUpdatedAbilityID == self.currentAbilityID then return end

    local ability = AbilityList[self.currentAbilityID]
    if not ability then return end

    local damageBase = ability.unitDamage or 0
    local damageBonus = GetDamageBonus(self.currentAbilityID) or 0

    local damageText = damageBase .. "(+" .. damageBonus .. ")"
    ECS.setTextComponent(entity, {
      text = "Damage: " .. damageText,
      color = { r=100, g=255, b=100, a=255}
    })

    self.lastUpdatedAbilityID = self.currentAbilityID
  end
})

return class