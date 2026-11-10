Event.define("OnChangePhase", {})
Event.define("OnEndActionButtonPressed", {})

Event.define("FocusOrderGrid", {})
Event.define("FocusChaosGrid", {})

Event.define("OnUnitSelected", {
    isOrder = "boolean",
    unitType = "integer",
    gridX = "integer",
    gridY = "integer"
})

Event.define("ReplayUnitMovement", {
    unitType = "integer",
    fromX = "integer",
    fromY = "integer",
    toX = "integer",
    toY = "integer"
})

Event.define("ReplayComplete", {})

Event.define("TryMoveUnit", {
    unitType = "integer",
    gridX = "integer",
    gridY = "integer"
})

Event.define("ShowUnitMovement", {
    unitType = "integer",
    gridX = "integer",
    gridY = "integer",
    range = "integer"
})

Event.define("OnTileSelected", {
    isOrder = "boolean",
    gridX = "integer",
    gridY = "integer"
})

Event.define("TryMoveCommand", {
    isOrder = "boolean",
    unitType = "integer",
    gridX = "integer",
    gridY = "integer"
})

Event.define("OnQueueAbility", 
{
    abilityType = "integer",
    isTargetOrder = "boolean",
    targetGridX = "integer",
    targetGridY = "integer"
})

Event.define("OnUnitDeployed",
{
    isOrder = "boolean",
    unitType = "integer"
})

Event.define("UpdateGridTextures", {
    playerId = "integer"
})

-- Grid query events (synchronous callbacks via response events)
Event.define("QueryGridID", {
    isOrder = "boolean"
})

Event.define("QueryGridIDResponse", {
    gridID = "integer"
})

Event.define("QueryUnitFlags", {
    isOrder = "boolean"
})

Event.define("QueryUnitFlagsResponse", {
    unitFlags = "integer"
})

Event.define("QueryTileData", {
    isOrder = "boolean",
    gridX = "integer",
    gridY = "integer"
})

Event.define("QueryTileDataResponse", {
    health = "integer",
    occupied = "boolean",
    unitType = "integer",
    color = "integer"
})

Event.define("SetUnitFlag", {
    isOrder = "boolean",
    flag = "integer"
})