--[[************************************************************************
  \file       ChaosKnight.lua
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
 
-- Clears all dissolve state and effect. No event emitted — caller decides.
local function ResetDissolve(self, entity)
    self.isDissolving  = false
    self.dissolveTimer = 0.0
 
    local effect = ECS.getObjectEffectComponent(entity)
    effect.effectType = ECS.EffectType.NONE
    effect.loop       = false
    ECS.setObjectEffectComponent(entity, effect)
end

local function ForceFinishDissolve(self, entity)
    if not self.isDissolving then return end
 
    Log.info("[ChaosKnight] Force-finishing dissolve")
 
    self.isDissolving  = false
    self.dissolveTimer = 0.0
 
    local ui = ECS.getUIComponent(entity)
    ui.layer         = self.layerBack
    ui.colorMultiply = { r=1, g=1, b=1, a=0 }
    ECS.setUIComponent(entity, ui)
 
    local effect = ECS.getObjectEffectComponent(entity)
    effect.effectType = ECS.EffectType.NONE
    effect.loop       = false
    ECS.setObjectEffectComponent(entity, effect)
end
 
local class = DefineType("ChaosKnight", {
 
    Serializable = {
        layerFront       = 2,
        layerBack        = 1,
        dissolveDuration = 0.5,
    },
 
    dissolveTimer = 0.0,
    isDissolving  = false,
 
    
    Init = function(self, entity)
 
        -- Switching TO Chaos
        Event.subscribe("OnPlayModeSwitch", function(params)
            if params.playAsOrder then return end
 
            -- If we were dissolving away, snap to hidden and restart fresh.
            ForceFinishDissolve(self, entity)
 
            Log.info("[ChaosKnight] PlayModeSwitch->Chaos: stepping in behind")
 
            ResetDissolve(self, entity)
 
            local ui = ECS.getUIComponent(entity)
            ui.layer         = self.layerBack
            ui.colorMultiply = { r=1, g=1, b=1, a=1 }
            ECS.setUIComponent(entity, ui)
 
            Event.emitLocal("OnReadyForOrderDissolve", {})
        end)
 
        -- Order finished dissolving  we come to front 
        Event.subscribe("OnOrderDissolveDone", function(_)
            Log.info("[ChaosKnight] OrderDissolveDone: moving to front")
            local ui = ECS.getUIComponent(entity)
            ui.layer = self.layerFront
            ECS.setUIComponent(entity, ui)
        end)
 
        -- Order tells us to dissolve
        Event.subscribe("OnReadyForChaosDissolve", function(_)
            -- Force-finish any previous dissolve cleanly before starting new one.
            ForceFinishDissolve(self, entity)
 
            Log.info("[ChaosKnight] OnReadyForChaosDissolve: starting BURN_DISSOLVE")
 
            ResetDissolve(self, entity)
            self.isDissolving  = true
            self.dissolveTimer = 0.0
 
            local effect = ECS.getObjectEffectComponent(entity)
            effect.effectType     = ECS.EffectType.BURN_DISSOLVE
           
            effect.effectDuration = self.dissolveDuration
            effect.loop           = false
            ECS.setObjectEffectComponent(entity, effect)
 
            Log.info("[ChaosKnight] BURN_DISSOLVE armed: effectType=" .. tostring(effect.effectType))
        end)
    end,
 
    
    Start = function(self, entity)
        local ui = ECS.getUIComponent(entity)
        ui.layer         = self.layerBack
        ui.colorMultiply = { r=1, g=1, b=1, a=0 }
        ECS.setUIComponent(entity, ui)
        ResetDissolve(self, entity)
 
        Log.info("[ChaosKnight] Start: layer=" .. tostring(self.layerBack) .. " alpha=0")
    end,
 
   
    Update = function(self, entity)
        if not self.isDissolving then return end
 
        self.dissolveTimer = self.dissolveTimer + Time.getDeltaTime()
 
        -- Overflow guard
        if self.dissolveTimer > self.dissolveDuration + 0.1 then
            Log.info("[ChaosKnight] Timer overflow, forcing reset")
            ResetDissolve(self, entity)
            return
        end
 
        if self.dissolveTimer >= self.dissolveDuration then
            Log.info("[ChaosKnight] Dissolve finished: hiding self, firing OnChaosDissolveDone")
 
            ResetDissolve(self, entity)
 
            local ui = ECS.getUIComponent(entity)
            ui.layer         = self.layerBack
            ui.colorMultiply = { r=1, g=1, b=1, a=0 }
            ECS.setUIComponent(entity, ui)
 
            -- Natural finish so Order can come to front
            Event.emitLocal("OnChaosDissolveDone", {})
        end
    end,
})
 
return class
 