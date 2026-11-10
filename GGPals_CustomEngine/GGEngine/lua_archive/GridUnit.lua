-- Test Lua ECS integration

local TestUnit = {
    -- Unit stats
    Health = 0,
    MaxHealth = 1,
    MovementRange = 2,
    AttackDamage = 3,
    
    -- Grid positioning
    GridX = 4,
    GridY = 5,
    GridID = 6,
    
    -- Unit state
    HasMoved = 7,
    HasActed = 8
}

function TestUnit.OnInstantiate(scriptInstance)
    Log.info("=== TestUnit.OnInstantiate ===")
    Log.info("Calling for Script Instance " .. scriptInstance)

    -- Initialize unit stats
    Vars.setInt(scriptInstance, TestUnit.Health, "Health", 1)
    Vars.setInt(scriptInstance, TestUnit.MaxHealth, "MaxHealth", 1)
    Vars.setInt(scriptInstance, TestUnit.MovementRange, "MovementRange", 3)

    -- Initialize grid position (default to 0,0 on Order grid)
    Vars.setInt(scriptInstance, TestUnit.GridX, "GridX", 0)
    Vars.setInt(scriptInstance, TestUnit.GridY, "GridY", 0)
    Vars.setInt(scriptInstance, TestUnit.GridID, "GridID", Grid.getLeftGrid())
    
    -- Initialize turn state
    Vars.setInt(scriptInstance, TestUnit.HasMoved, "HasMoved", 0) -- false
    Vars.setInt(scriptInstance, TestUnit.HasActed, "HasActed", 0) -- false

    Log.info("Unit stats initialized: Health=" .. Vars.getInt(scriptInstance, TestUnit.Health) .. 
             ", MovementRange=" .. Vars.getInt(scriptInstance, TestUnit.MovementRange) ..
             ", AttackDamage=" .. Vars.getInt(scriptInstance, TestUnit.AttackDamage))
end

function TestUnit.Start(entityHandle, scriptInstance)
    Log.info("=== TestUnit.Start ===")
    Log.info("Spawning unit for Entity Handle " .. tostring(entityHandle))
    
    -- Get spawn position from script variables
    local gridX = Vars.getInt(scriptInstance, TestUnit.GridX)
    local gridY = Vars.getInt(scriptInstance, TestUnit.GridY)
    local gridID = Vars.getInt(scriptInstance, TestUnit.GridID)

    Event.subscribe("OnClick", 
      function(params)
        Log.info("OnClickHandler " .. params.worldX .. ", " .. params.worldY)
        local gridPos = Grid.worldToGrid(gridID, params.worldX, params.worldY)
        TestUnit.MoveToPosition(scriptInstance, entityHandle, gridPos.x, gridPos.y)
      end
    )
    
    Log.info("Spawning unit at grid position (" .. gridX .. "," .. gridY .. ") on grid " .. gridID)
    
    -- Convert grid position to world coordinates
    local worldPos = Grid.gridToWorld(gridID, gridX, gridY)
    if not worldPos then
        Log.error("Invalid grid position: (" .. gridX .. "," .. gridY .. ")")
        return
    end
    
    Log.info("World position: (" .. worldPos.x .. "," .. worldPos.y .. ")")
    
    -- Verify the tile is valid and passable
    local tile = Grid.getTile(gridID, gridX, gridY)
    if not tile then
        Log.error("No tile found at position (" .. gridX .. "," .. gridY .. ")")
        return
    end
    
    if not tile.isPassable then
        Log.warning("Unit spawned on non-passable tile at (" .. gridX .. "," .. gridY .. ")")
    end
    
    -- Mark tile as occupied (you might need to add this to your grid system)
    -- For now, we'll just reveal the tile
    Grid.revealTile(gridID, gridX, gridY)

    TestUnit.MoveToPosition(scriptInstance, entityHandle, gridX, gridY)
    
    Log.info("Unit successfully spawned and positioned")
end

function TestUnit.Update(entityHandle, scriptInstance)
    
end

function TestUnit.MoveToPosition(scriptInstance, entityHandle, targetX, targetY)
    local currentX = Vars.getInt(scriptInstance, TestUnit.GridX)
    local currentY = Vars.getInt(scriptInstance, TestUnit.GridY)
    local gridID = Vars.getInt(scriptInstance, TestUnit.GridID)
    local movementRange = Vars.getInt(scriptInstance, TestUnit.MovementRange)
    local hasMoved = Vars.getInt(scriptInstance, TestUnit.HasMoved)
    
    -- Validate movement
    local moveResult = Grid.validateMovement(gridID, currentX, currentY, targetX, targetY, movementRange)
    
    if not moveResult.canMove then
        Log.warning("Invalid movement: " .. moveResult.reason)
        return false
    end
    
    Log.info("Moving unit from (" .. currentX .. "," .. currentY .. ") to (" .. targetX .. "," .. targetY .. ")")
    Log.info("Path length: " .. #moveResult.path .. ", Algorithm: " .. moveResult.algorithmUsed)
    
    -- Update grid position in script variables
    Vars.setInt(scriptInstance, TestUnit.GridX, "GridX", targetX)
    Vars.setInt(scriptInstance, TestUnit.GridY, "GridY", targetY)
    Vars.setInt(scriptInstance, TestUnit.HasMoved, "HasMoved", 1) -- true
    
    -- Update world transform
    local newWorldPos = Grid.gridToWorld(gridID, targetX, targetY)
    
    Log.info("Resolved new world pos: " .. newWorldPos.x .. ", " .. newWorldPos.y)

    if newWorldPos then
        local transform = ECS.getTransform(entityHandle)
        if transform then
            transform.x = newWorldPos.x
            transform.y = newWorldPos.y
            ECS.setTransform(entityHandle, transform)
        end
    end
    
    -- Clear old highlights and highlight new position
    Grid.highlightTiles(gridID, {}, true) -- Clear all highlights
    Grid.highlightTiles(gridID, {{targetX, targetY}}, false) -- Highlight new position
    
    Log.info("Unit movement completed successfully")
    return true
end

return TestUnit
