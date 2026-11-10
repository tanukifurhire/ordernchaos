Event.define("OnAbilitySelected", {
    ability = "integer"
})

Event.define("OnEndPlanningPhase", {})

Event.define("OnResolveAbilities", {})

Event.define("ReplaySupportAbility", {
    unitType = "integer",
    ability = "integer"
})

Event.define("TriggerVFXFinished", {})

Event.define("TriggerOracle1", {
    targetX = "integer",
    targetY = "integer"
})

Event.define("TriggerOracle1Projectile", {})

Event.define("TriggerOracle2", {})

Event.define("TriggerOracle2VFX", {
    gridX = "integer",
    gridY = "integer"
})

Event.define("ApplyShield", {
    unitType = "integer"
})

Event.define("ActivateShieldAfterVFX", {
    unitType = "integer"
})

Event.define("Oracle_Ability2_VFX_Transition", {})

Event.define("Oracle_Ability1_VFX_Transition", {})

Event.define("TriggerDragoon1", {
    targetX = "integer",
    targetY = "integer"
})

Event.define("TriggerDragoon1Projectile", {})

Event.define("Dragoon_Ability1_VFX_Transition", {})

Event.define("TriggerDragoon2", {})

Event.define("TriggerEye1", {})

Event.define("TriggerEye2", {})

Event.define("TriggerImp1", {})

Event.define("TriggerImp2", {
    isOrder = "boolean",
    unitType = "integer",
    fromX = "integer",
    fromY = "integer",
    toX = "integer",
    toY = "integer"
})

Event.define("TriggerImp2Projectile", {})

--[[Event.define("OnForceMoveUnit", {
    ability = "integer",
    unitType = "integer",
    fromX = "integer",
    fromY = "integer",
    toX = "integer",
    toY = "integer"
})]]

Event.define("TriggerTirion1", {})
Event.define("TriggerTirion2", {})
Event.define("TriggerVizom1", {})
Event.define("TriggerVizom2", {})

Event.define("PullUnit", {
    unitType = "integer",
    fromX = "integer",
    fromY = "integer",
    toX = "integer",
    toY = "integer"
})

Event.define("TakeDamage", {
    player = "integer",
    damage = "integer"
})

Event.define("OnHealthChanged", {
    player = "integer",
    curr = "integer",
    max = "integer"
})

Event.define("GainHealth", {
    player = "integer",
    healthAmount = "integer"
})

Event.define("OutOfAPWarning", {})

Event.define("OnAPUpdate", {
    value = "integer"
})

Event.define("HideAP", {})

Event.define("ShowAP", {})

Event.define("OnPause", {})

Event.define("OnResume", {})

Event.define("OnReturnMainMenu", {})

-- for ability queue UI
Event.define("OnAbilityQueued", {
    abilityID = "integer",
    queueIndex = "integer",
    player = "integer"
})

Event.define("OnAbilityResolved", {
    owner = "integer"
}) 

Event.define("AbilityAnimationFinished", {
    player = "number"
})