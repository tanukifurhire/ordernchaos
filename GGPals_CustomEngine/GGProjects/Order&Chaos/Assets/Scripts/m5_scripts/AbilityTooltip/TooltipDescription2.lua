--[[************************************************************************
  \file       TooltipDescription.lua
  \author     
  \par        Email: 
  \date       01 February 2026
  \version    1.1
  \brief
    Secondary description for ability tooltips.
  \details
    Displays additional description text (description2) for abilities.
  \dependencies
    
  \see
    
  \history
    - 1.0 (01 February 2026): Created this script.
    - 1.1 (03 April 2026): Fixed text update to only change when ability changes.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
local class = DefineType("TooltipDescription2", {
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

    Event.subscribe("Decription2Transform", function (params)
      if not params then
        return
      end

      local transform = ECS.getTransform(entity)
      if transform then
        transform.y = params.posY
        ECS.setTransform(entity,transform)
      end
    end)

  end,

  Update = function(self, entity)
    if not self.isVisible then return end
    if not self.currentAbilityID then return end
    -- Only update text when ability changes
    if self.lastUpdatedAbilityID == self.currentAbilityID then return end

    local ability = AbilityList[self.currentAbilityID]
    if not ability then return end

    local text = ""
    if ability.description2 then
      for _, line in ipairs(ability.description2) do
        text = text .. line .. "\n"
      end
    else
      text = ""
    end

    ECS.setTextComponent(entity, {
      text = text,
      color = { r=220, g=220, b=220, a=255}
    })
    
    self.lastUpdatedAbilityID = self.currentAbilityID
  end
})

return class