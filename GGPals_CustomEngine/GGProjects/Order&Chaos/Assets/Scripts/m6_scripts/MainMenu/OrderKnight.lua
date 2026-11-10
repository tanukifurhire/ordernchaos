--[[************************************************************************
  \file       OrderKnight.lua
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
Event.define("OnPlayModeSwitch",        { playAsOrder = "boolean" })
Event.define("OnReadyForOrderDissolve", {})
Event.define("OnOrderDissolveDone",     {})
Event.define("OnReadyForChaosDissolve", {})
Event.define("OnChaosDissolveDone",     {})
 

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
 
    Log.info("[OrderKnight] Force-finishing dissolve")
 
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
 
local class = DefineType("OrderKnight", {
 
    Serializable = {
        layerFront       = 2,
        layerBack        = 1,
        dissolveDuration = 0.5,
    },
 
    dissolveTimer = 0.0,
    isDissolving  = false,
 
   
    Init = function(self, entity)
 
        
        Event.subscribe("OnPlayModeSwitch", function(params)
            if not params.playAsOrder then return end
 
           
            ForceFinishDissolve(self, entity)
 
            Log.info("[OrderKnight] PlayModeSwitch->Order: stepping in behind")
 
            ResetDissolve(self, entity)
 
            local ui = ECS.getUIComponent(entity)
            ui.layer         = self.layerBack
            ui.colorMultiply = { r=1, g=1, b=1, a=1 }
            ECS.setUIComponent(entity, ui)
 
            Event.emitLocal("OnReadyForChaosDissolve", {})
        end)
 
        -- Chaos finished dissolving
        Event.subscribe("OnChaosDissolveDone", function(_)
            Log.info("[OrderKnight] ChaosDissolveDone: moving to front")
            local ui = ECS.getUIComponent(entity)
            ui.layer = self.layerFront
            ECS.setUIComponent(entity, ui)
        end)
 
        --Chaos tells us to dissolve
        Event.subscribe("OnReadyForOrderDissolve", function(_)
            -- Force-finish any previous dissolve cleanly before starting new one.
            ForceFinishDissolve(self, entity)
 
            Log.info("[OrderKnight] OnReadyForOrderDissolve: starting BURN_DISSOLVE")
 
            ResetDissolve(self, entity)
            self.isDissolving  = true
            self.dissolveTimer = 0.0
 
            local effect = ECS.getObjectEffectComponent(entity)
            effect.effectType     = ECS.EffectType.BURN_DISSOLVE
            effect.effectDuration = self.dissolveDuration
            effect.loop           = false
            ECS.setObjectEffectComponent(entity, effect)
 
            Log.info("[OrderKnight] BURN_DISSOLVE armed: effectType=" .. tostring(effect.effectType))
        end)
    end,
 
    
    Start = function(self, entity)
        local ui = ECS.getUIComponent(entity)
        ui.layer         = self.layerFront
        ui.colorMultiply = { r=1, g=1, b=1, a=1 }
        ECS.setUIComponent(entity, ui)
 
        ResetDissolve(self, entity)
 
        Log.info("[OrderKnight] Start: layer=" .. tostring(self.layerFront) .. " alpha=1")
    end,
 
    
    Update = function(self, entity)
        if not self.isDissolving then return end
 
        self.dissolveTimer = self.dissolveTimer + Time.getDeltaTime()
 
        -- Overflow guard
        if self.dissolveTimer > self.dissolveDuration + 0.1 then
            Log.info("[OrderKnight] Timer overflow, forcing reset")
            ResetDissolve(self, entity)
            return
        end
 
        if self.dissolveTimer >= self.dissolveDuration then
            Log.info("[OrderKnight] Dissolve finished: hiding self, firing OnOrderDissolveDone")
 
            ResetDissolve(self, entity)
 
            local ui = ECS.getUIComponent(entity)
            ui.layer         = self.layerBack
            ui.colorMultiply = { r=1, g=1, b=1, a=0 }
            ECS.setUIComponent(entity, ui)
 
            -- Natural finish  so Chaos can come to front
            Event.emitLocal("OnOrderDissolveDone", {})
        end
    end,
})
 
return class