--[[************************************************************************
  \file       Events.lua
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       13 November 2025
  \version    1.0
  \brief
    Global Game Events Registry for Order N Chaos
  \details
    Defines the core events for gameplay use
  \dependencies
    Event bindings
  \see
    GGLuaBindings, GGEventBus
  \history
    - 1.0 (13 November 2025): Initial test script for Lua integration.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
************************************************************************--]]

Event.define("OnClick", 
{
    worldX = "number",
    worldY = "number"
})

Event.define("OnDrag", 
{
    worldX = "number",
    worldY = "number"
})

Event.define("OnDrop",
{
    worldX = "number",
    worldY = "number"
})