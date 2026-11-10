-- Define all game events upfront
Event.define("GridGenerated", {
    playerId = "number",
    width = "number",
    height = "number",
    originX = "number",
    originY = "number"
})

Event.define("TileCreated", {
    tileEntity = "number",
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("UnitSpawned", {
    unitEntity = "number",
    gridX = "number",
    gridY = "number",
    playerId = "number",
    unitType = "string"
})

Event.define("UnitMoved", {
    fromX = "number",
    fromY = "number",
    toX = "number",
    toY = "number"
})

Event.define("ShowAttackRange", {
    centerX = "number",
    centerY = "number",
    rangeUp = "number",
    rangeDown = "number",
    rangeLeft = "number",
    rangeRight = "number"
})

Event.define("TileHovered", {
    tileEntity = "number",
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("ClearHighlights", {})

Event.define("UnitSelected", {
    unitEntity = "number"
})

Event.define("PhaseChanged", {
    phase = "string",
    player = "number",
    turnNumber = "number"
})

Event.define("EndTurn", {})

Event.define("ExecuteActions", {})

Event.define("UnitDied", {
    unitEntity = "entity",
    unitType = "string",
    owner = "number"
})

Event.define("AbilityUsed", {
    unitEntity = "number",
    targetX = "number",
    targetY = "number",
    abilityType = "string"
})

Event.define("TileRegistered", {
    tileEntity = "number",
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("SetTileHighlight", {
    tileEntity = "number",
    highlighted = "boolean",
    color = "string"
})

Event.define("TileClicked", {
    tileEntity = "number",
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("HighlightValidPlacements", {
    playerId = "number"
})

Event.define("ShowMovementRange", {
    centerX = "number",
    centerY = "number",
    range = "number",
    playerId = "number"
})

Event.define("UnitPlacedOnTile", {
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("UnitRemovedFromTile", {
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("GameEnded", {
    winner = "number"
})

Event.define("TileDestroyed", {
    gridX = "number",
    gridY = "number",
    playerId = "number",
    tileEntity = "number"
})

Event.define("SetTileDestroyed", {
    tileEntity = "number",
    isDestroyed = "boolean"
})

Event.define("UnitAttacked", {
    attackerType = "string",
    targetType = "string",
    damage = "number"
})

Event.define("AbilityTriggered", {
    abilityType = "string",
    gridX = "number",
    gridY = "number",
    playerId = "number"
})

Event.define("OnAbilityQueued", {
    abilityID = "number",
    queueIndex = "number"
})

Event.define("PlayerReadyForTurn", {
    playerId = "number"
})

Event.define("CameraSwap", {})

-- Animation-driven resolution events
Event.define("PlayAbilityAnimations", {
    player = "number"
})

Event.define("AbilityAnimationFinished", {
    player = "number"
})

Event.define("DamagePhaseComplete", {})

Event.define("CheckWinConditions", {})

-- Audio Events
Event.define("PlayUnitMove", {})
Event.define("PlayAbilityCast", {})
Event.define("PlayAbilityImpact", {})
Event.define("PlayTileBreak", {})
Event.define("PlayUnitDeath", {})
Event.define("PlayButtonClick", {})

-- Faction display event (tells UI which faction the local player is)
Event.define("OnGameplayFactionReady", {
    isOrder = "boolean"
})

-- Status effect events
Event.define("OnUnitStatusInflicted", {
    statusID = "number",     -- Ability/passive ID that inflicts the status (matches CONSTANTS.STATUS_PREFABS key)
    ownerUnit = "number",    -- Unit type that applied the status (caster)
    targetUnit = "number",   -- Unit type that received the status
    stackCount = "number",   -- Optional stack count for stackable statuses
    isExpiring = "boolean"   -- Optional flag for "expires at end of this resolution"
})

Event.define("OnUnitStatusRemoved", {
    statusID = "number",     -- Status ID to remove
    targetUnit = "number"    -- Unit type to remove the status from
})

Event.define("Darken_All", {})

Event.define("Lighten_All", {})

Event.define("Tutorial_Set_Unit_Move_Targets", {
    unitType = "number",  -- torso: 0 means any unit
    tiles = "table"       -- array of {x=number, y=number}
})

Event.define("Tutorial_Clear_Unit_Move_Targets", {})

Event.define("Disable_Placement_Tile", {
    gridX = "number",
    gridY = "number"
})

Event.define("Enable_Placement_Tile", {
    gridX = "number",
    gridY = "number"
})

Event.define("Darken_Tile", {
    gridX = "number",
    gridY = "number"
})

Event.define("Lighten_Tile", {
    gridX = "number",
    gridY = "number"
})

Event.define("OnTutorialDisableTimer", {
})

local class = DefineType("GameEvents", {
    Start = function(self, entity)
        Log.info("[GameEvents] All events registered successfully")
    end,
    
    Update = function(self, entity)
        -- Empty update
    end
})

return class
