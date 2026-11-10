--[[************************************************************************
  \file       test_event_driven.lua
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       01 November 2025
  \version    1.0
  \brief
    Event-driven test script for Lua-ECS integration validation.
  \details
    Tests the core Lua bindings including entity creation, component
    management, input event routing, and debug utilities. Implements
    Start() for initialization and event callbacks for keyboard and
    mouse input to validate the event-driven architecture.
  \dependencies
    ECS bindings, Input bindings, Debug utilities (Log, Profile, Debug)
  \see
    GGLuaBindings, GGLuaInput, GGLuaDebug, GGEngine
  \history
    - 1.0 (01 November 2025): Initial test script for Lua integration.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
************************************************************************--]]

-- Called once when script loads
function Start()
    Log.info("=== Lua Script Started ===")
    
    -- Test debug utilities
    Log.info("Testing debug functions...")
    Log.warning("This is a warning")
    
    -- Test profiling
    Profile.start("initialization")
    
    -- Test ECS
    local player = ECS.createEntity()
    Log.info("Created entity: " .. tostring(player))
    
    -- Add components
    ECS.addTransform(player, 0, 0, 1, 1, 0)
    ECS.addRigidbody(player, 10, 5)
    
    -- Get transform
    local transform = ECS.getTransform(player)
    Log.info("Player position: " .. transform.x .. ", " .. transform.y)
    
    Profile.stop("initialization")
    
    -- Test memory reporting
    local memory = Debug.getMemory()
    Log.info("Lua memory usage: " .. memory .. " KB")
    
    Log.info("=== Initialization Complete ===")
end

-- Called when keys are pressed
function OnKeyPressed(key)
    Log.info("Key pressed: " .. key)
    
    if key == "SPACE" then
        Log.info("SPACE - Jump!")
    elseif key == "ENTER" then
        Log.info("ENTER - Confirm action")
    elseif key == "UP" then
        Log.info("Moving UP")
    elseif key == "DOWN" then
        Log.info("Moving DOWN")
    elseif key == "LEFT" then
        Log.info("Moving LEFT")
    elseif key == "RIGHT" then
        Log.info("Moving RIGHT")
    end
end

-- Called when mouse is clicked
function OnMouseClick(x, y, button)
    Log.info("Mouse clicked at (" .. x .. ", " .. y .. ") with " .. button .. " button")
    
    if button == "LEFT" then
        Log.info("Left click - Select/Fire")
        Event.emitLocal("OnClick", {
            worldX = x,
            worldY = y
        })
    elseif button == "RIGHT" then
        Log.info("Right click - Alt action")
    end
end

function OnMouseDrag(x, y, button)
    if button == "LEFT" then
        Event.emitLocal("OnDrag", {
            worldX = x,
            worldY = y
        })
    end
end

function OnMouseDrop(x, y, button)
    if button == "LEFT" then
        Event.emitLocal("OnDrop", {
            worldX = x,
            worldY = y
        })
    end
end

-- Optional: Continuous update (uncomment in GGEngine.cpp if needed)
function Update(deltaTime)
    -- This won't be called unless you uncomment the Update call in GGEngine.cpp
    -- Log.info("Update with dt: " .. deltaTime)
end

Log.info("Script loaded successfully!")