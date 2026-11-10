--[[************************************************************************
  \file       AbilityTooltip.lua
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       20 February 2026
  \version    1.1
  \brief
    Ability tooltip that displays information when hovering over abilities.
  \details
    Shows tooltip near the hovered ability, automatically clamping position
    to keep the tooltip visible within screen boundaries.
  \dependencies
    
  \see
    
  \history
    - 1.0 (20 February 2026): Created this script.
    - 1.1 (03 April 2026): Updated to use System.getScreenDimensions() and
                           clamp tooltip position to screen boundaries.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
************************************************************************--]]
StatusList = {
    [2] = {
      statusName = "Shield",
      statusDescription = "This unit blocks up to X damage. X is the number of Shield stacks."
    },

    [3] = {
      statusName = "Burn",
      statusDescription = "This unit takes 1 more damage from all sources."
    },

    [8] = {
      statusName = "Wrath",
      statusDescription = "The next ability (and its recasts) by this unit's faction deal 2 more damage."
    },

    [9] = {
      statusName = "Fervor",
      statusDescription = "Luminous Cleave deals X increased damage. X is the number of Fervor stacks."
    },

    [11] = {
      statusName = "Berserk",
      statusDescription = "The next Umbral Cleave will recast X times. X is the number of Berserk stacks."
    },

    [12] = {
      statusName = "Bleed",
      statusDescription = "This unit takes 1 damage each tile it travels."
    },

    [17] = {
      statusName = "Marked",
      statusDescription = "This unit takes additional damage from opponent units with Hunter's Mark."
    }
  }

local class = DefineType("AbilityTooltip", {
  Serializable = {
    offsetX = 0,
    offsetY = 80,
    screenPadding = 10
  },
  currentAbilityID = nil,
  isVisible = false,

  -- for description2 transform.y
  twoLine = 0.00,
  threeLine = -0.08,
  fourLine = -0.16,
  fiveLine = -0.24,

  fadeTimer = 0,
  fadeDuration = 0.2,

  isFadingIn = false,
  isFadingOut = false,

  canHover = true,

  disableTimer = 0,
  disableDuration = 1.0,

  Start = function(self, entity)
    Event.subscribe("ShowAbilityTooltip", function(params)
      AbilityTooltip.Show(self, entity, params.abilityID, params.x, params.y)
    end)

    Event.subscribe("HideAbilityTooltip", function()
      AbilityTooltip.Hide(self, entity)
    end) 

    Event.subscribe("OnPassAndPlayDisplayed", function()
        self.canHover = false
        AbilityTooltip.Hide(self, entity)
    end)

    Event.subscribe("OnPassAndPlayHidden", function()
        self.canHover = true
    end)

    Event.subscribe("OnPause", function()
        self.canHover = false
        self.isVisible = false
        self.currentAbilityID = nil
        self.isFadingIn = false
        self.isFadingOut = false

        ECS.setTransform(entity, {
            x = -100000,
            y = -100000
        }) 
    end)

    Event.subscribe("OnResume", function()
        self.canHover = true
    end)
    
    AbilityTooltip.Hide(self, entity)
  end,

  Show = function(self, entity, abilityID, x, y)
    if not self.canHover then return end
    self.currentAbilityID = nil
    self.currentAbilityID = abilityID
    self.isVisible = true

    self.fadeTimer = 0
    self.isFadingIn = true
    self.isFadingOut = false

    ECS.setUIComponent(entity, {
      colorMultiply = { r = 1.0, g = 1.0, b = 1.0, a = 0.0}
    })

    if x and y then
      local finalX = x + self.offsetX
      local finalY = y + self.offsetY

      -- Get screen dimensions and tooltip scale to clamp position
      local screen = System.getScreenDimensions()
      local transform = ECS.getTransform(entity)
      
      if screen and transform then
        local halfScreenW = screen.x * 0.5
        local halfScreenH = screen.y * 0.5
        local tooltipW = transform.sx
        local halfTooltipH = transform.sy * 0.5

        -- Calculate tooltip edges
        -- X: left-aligned (anchor at left edge)
        local tooltipLeft = finalX
        local tooltipRight = finalX + tooltipW
        -- Y: center-aligned (anchor at center)
        local tooltipBottom = finalY - halfTooltipH
        local tooltipTop = finalY + halfTooltipH

        -- Screen bounds with padding
        local screenLeft = -halfScreenW + self.screenPadding
        local screenRight = halfScreenW - self.screenPadding
        local screenBottom = -halfScreenH + self.screenPadding
        local screenTop = halfScreenH - self.screenPadding

        -- Adjust X if tooltip exceeds horizontal bounds
        if tooltipLeft < screenLeft then
          finalX = finalX + (screenLeft - tooltipLeft)
        elseif tooltipRight > screenRight then
          finalX = finalX - (tooltipRight - screenRight)
        end

        -- Adjust Y if tooltip exceeds vertical bounds
        if tooltipBottom < screenBottom then
          finalY = finalY + (screenBottom - tooltipBottom)
        elseif tooltipTop > screenTop then
          finalY = finalY - (tooltipTop - screenTop)
        end
      end

      ECS.setTransform(entity, {
        x = finalX,
        y = finalY
      })
    end
  end,

  Hide = function(self, entity)
    self.isFadingOut = true
    self.isFadingIn = false
    self.fadeTimer = 0
  end,

  Update = function(self, entity)
    if not self.isVisible then return end
    if not self.currentAbilityID then return end

    local ui = ECS.getUIComponent(entity)
    if not ui then return end

    --------------------------------------------------
    -- FADE IN
    --------------------------------------------------
    if self.isFadingIn then
      self.fadeTimer = self.fadeTimer + Time.getDeltaTime()
      local t = self.fadeTimer / self.fadeDuration

      if t >= 1 then
        t = 1
        self.isFadingIn = false
      end

      ui.colorMultiply = { r=1, g=1, b=1, a=0.9 * t }
      ECS.setUIComponent(entity, ui)
    end

    --------------------------------------------------
    -- FADE OUT
    --------------------------------------------------
    if self.isFadingOut then
      self.fadeTimer = self.fadeTimer + Time.getDeltaTime()
      local t = self.fadeTimer / self.fadeDuration

      if t >= 1 then
        t = 1
        self.isFadingOut = false
        self.isVisible = false

        ECS.setTransform(entity, {
          x = -100000,
          y = -100000
        })
      end

      ui.colorMultiply = { r=1, g=1, b=1, a=0.9 * (1 - t) }
      ECS.setUIComponent(entity, ui)
    end

    local ability = AbilityList[self.currentAbilityID]
    if not ability then return end

    -- ORACLE_1 (Crystal Cascade)
    if self.currentAbilityID == 1 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })
    end

    -- ORACLE_2 (Fiberglass Finesse)
    if self.currentAbilityID == 2 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })
    end

    -- DRAGOON_1 (Azure Blaze)
    if self.currentAbilityID == 3 then
      Event.emitLocal("Decription2Transform", {
        posY = self.threeLine
      })
    end
    
    -- DRAGOON_2 (Feather Cyclone)
    if self.currentAbilityID == 4 then
      Event.emitLocal("Decription2Transform", {
        posY = self.fiveLine
      })
    end

    -- IMP_1 (Imp-Pact)
    if self.currentAbilityID == 5 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })
    end

    -- IMP_2 (Wicked Shift)
    if self.currentAbilityID == 6 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })
    end

    -- EYE_1 (Ruinous Gaze)
    if self.currentAbilityID == 7 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })
    end

    -- EYE_2 (Fractured Earth)
    if self.currentAbilityID == 8 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })    
    end

    -- TIRION_1 (Luminous Cleave)
    if self.currentAbilityID == 9 then
      Event.emitLocal("Decription2Transform", {
        posY = self.fourLine
      })    
    end

    -- TIRION_2 (Sigil of Consecration)
    if self.currentAbilityID == 10 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })    
    end

    -- VIZOM_1 (Umbral Cleave)
    if self.currentAbilityID == 11 then
      Event.emitLocal("Decription2Transform", {
        posY = self.twoLine
      })    
    end

    -- VIZOM_2 (Sigil of Malevolence)
    if self.currentAbilityID == 12 then
      Event.emitLocal("Decription2Transform", {
        posY = self.fourLine
      })    
    end
  end
})

return class