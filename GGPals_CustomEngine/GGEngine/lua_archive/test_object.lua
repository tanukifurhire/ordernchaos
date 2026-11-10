-- Test Lua ECS integration

local PlayerScript = {
    Health = 0
}

function PlayerScript.OnInstantiate(scriptInstance)
    Log.info("=== PlayerScript.OnInstantiate ===")
    Log.info("Calling for Script Instance " .. scriptInstance)

    Vars.setInt(scriptInstance, PlayerScript.Health, "Health", 100)

    local health = Vars.getInt(scriptInstance, PlayerScript.Health)

    Log.info("Check value of health through getInt: " .. health)
end

function PlayerScript.Start(entityHandle, scriptInstance)
    Log.info("=== PlayerScript.Start ===")
    Log.info("Calling for Entity Handle " .. tostring(entityHandle))
end

function PlayerScript.Update(entityHandle, scriptInstance)
end

return PlayerScript