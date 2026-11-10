/*************************************************************************
  \file       GGEventBus.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
    Implementation of type-safe event bus for Lua gameplay scripts.
  \details
    Implements event validation, subscription management, and routing
    with comprehensive error checking and debug logging.
  \dependencies
    GGEventBus.hpp, lua.h
  \see
    GGEventBus.hpp
  \history
    - 1.0 (1 November 2025): Initial implementation.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGEventBus.hpp"
#include "GGLogger.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

/*************************************************************************
  \brief
    Registers an event schema for runtime validation.
  \param[in] name
    Unique event name.
  \param[in] schema
    Schema definition with parameter types.
  \details
    Stores schema for later validation. Computes hash of event name
    for network efficiency. Logs warning if event already registered.
*************************************************************************/
void GGEventBus::RegisterEvent(const std::string& name, const EventSchema& schema)
{
    if (schemas.find(name) != schemas.end())
    {
        //std::cout << "WARNING: [EventBus] Event '" << name << "' already registered, ignoring" << std::endl;
        return;
    }

    EventSchema newSchema = schema;
    newSchema.eventId = HashString(name);
    schemas[name] = newSchema;

    if (debugMode)
    {
        std::stringstream ss;
        ss << "[EventBus] Registered event '" << name << "' with "
            << schema.params.size() << " parameters";
        if (schema.isNetworked)
            ss << " (NETWORKED)";
        GGLogger::Info(ss.str());
    }
}

/*************************************************************************
  \brief
    Subscribe to an event with a Lua callback function.
  \param[in] L
    Lua state.
  \param[in] event
    Event name to subscribe to.
  \param[in] funcIdx
    Stack index of callback function.
  \return
    Subscription ID for unsubscribe.
  \details
    Creates a reference to the Lua function in the registry to prevent
    garbage collection. Stores mapping for later cleanup.
*************************************************************************/
uint32_t GGEventBus::Subscribe(lua_State* L, const std::string& event, int funcIdx)
{
    // Make absolute index
    if (funcIdx < 0)
        funcIdx = lua_gettop(L) + funcIdx + 1;

    // Verify it's a function
    if (!lua_isfunction(L, funcIdx))
    {
        luaL_error(L, "Expected function for event subscription");
        return 0;
    }

    // Create reference to function
    lua_pushvalue(L, funcIdx);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // Store subscription
    subscribers[event].push_back(ref);

    // Track for unsubscribe
    uint32_t id = nextSubscriptionId++;
    subscriptions[id] = { event, ref };

    if (debugMode)
    {
        GGLogger::Info("[EventBus] Subscribed to '" + event
            + "' (ID: " + std::to_string(id) + ")");
    }

    return id;
}

/*************************************************************************
  \brief
    Unsubscribe from an event using subscription ID.
  \param[in] L
    Lua state.
  \param[in] subscriptionId
    ID returned from Subscribe().
  \details
    Releases Lua function reference and removes from subscriber list.
    Safe to call with invalid ID.
*************************************************************************/
void GGEventBus::Unsubscribe(lua_State* L, uint32_t subscriptionId)
{
    auto it = subscriptions.find(subscriptionId);
    if (it == subscriptions.end())
    {
        GGLogger::Warning("[EventBus] Invalid subscription ID: "
            + std::to_string(subscriptionId));
        return;
    }

    // Remove from subscribers list
    auto& subs = subscribers[it->second.eventName];
    subs.erase(std::remove(subs.begin(), subs.end(), it->second.luaRef),
        subs.end());

    // Release Lua reference
    luaL_unref(L, LUA_REGISTRYINDEX, it->second.luaRef);

    // Remove subscription record
    subscriptions.erase(it);

    if (debugMode)
    {
        GGLogger::Info("[EventBus] Unsubscribed ID: " + std::to_string(subscriptionId));
    }
}

/*************************************************************************
  \brief
    Emit a local-only event.
  \param[in] L
    Lua state.
  \param[in] event
    Event name.
  \param[in] tableIdx
    Stack index of parameter table.
  \details
    Validates parameters if schema exists, then calls all subscribers.
    Does not trigger network synchronization.
*************************************************************************/
void GGEventBus::EmitLocal(lua_State* L, const std::string& event, int tableIdx)
{
    // Validate if schema exists
    if (schemas.find(event) != schemas.end())
    {
        if (!ValidateParams(L, event, tableIdx))
        {
            GGLogger::Error("[EventBus] Event parameter validation failed for '" + event + "'");
            luaL_error(L, "Event parameter validation failed");
            return;
        }

        // Check if trying to emit networked event as local
        if (schemas[event].isNetworked)
        {
            GGLogger::Warning("[EventBus] Emitting networked event '" + event
                + "' as local - intentional?");
        }
    }
    else
    {
        //GGLogger::Warning("[EventBus] Emitting undefined event '" + event + "' - no schema validation");
    }

    // Call all subscribers
    auto it = subscribers.find(event);
    if (it == subscribers.end() || it->second.empty())
    {
        if (debugMode)
        {
            GGLogger::Info("[EventBus] No subscribers for event '" + event + "'");
        }
        return;
    }

    if (debugMode)
    {
        GGLogger::Info("[EventBus] Emitting local event '" + event
            + "' to " + std::to_string(it->second.size()) + " subscribers");
    }

    // Call each subscriber
    for (int funcRef : it->second)
    {
        CallLuaCallback(L, funcRef, event, tableIdx);
    }
}

/*************************************************************************
  \brief
    Emit a networked event.
  \param[in] L
    Lua state.
  \param[in] event
    Event name.
  \param[in] tableIdx
    Stack index of parameter table.
  \details
    Validates parameters, ensures event is networked, executes locally,
    and forwards to network handler if configured.
*************************************************************************/
void GGEventBus::EmitNetworked(lua_State* L, const std::string& event, int tableIdx)
{
    // Must have schema for networked events
    auto schemaIt = schemas.find(event);
    if (schemaIt == schemas.end())
    {
        GGLogger::Error("[EventBus] Cannot emit networked event '" + event + "' without schema");
        luaL_error(L, "Cannot emit networked event '%s' without schema", event.c_str());
        return;
    }

    // Must be marked as networked
    if (!schemaIt->second.isNetworked)
    {
        GGLogger::Error("[EventBus] Event '" + event + "' is not marked as networked");
        luaL_error(L, "Event '%s' is not marked as networked", event.c_str());
        return;
    }

    // Validate parameters
    if (!ValidateParams(L, event, tableIdx))
    {
        GGLogger::Error("[EventBus] Event parameter validation failed for '" + event + "'");
        luaL_error(L, "Event parameter validation failed");
        return;
    }

    if (debugMode)
    {
        GGLogger::Info("[EventBus] Emitting networked event '" + event + "'");
    }

    // Send to network handler if configured
    if (networkHandler)
    {
        networkHandler(event, L, tableIdx);
    }

    // Also execute locally
    auto it = subscribers.find(event);
    if (it != subscribers.end())
    {
        for (int funcRef : it->second)
        {
            CallLuaCallback(L, funcRef, event, tableIdx);
        }
    }
}

/*************************************************************************
  \brief
    Validate event parameters against schema.
  \param[in] L
    Lua state.
  \param[in] event
    Event name.
  \param[in] tableIdx
    Stack index of parameter table.
  \return
    True if valid or no schema.
  \details
    Checks each parameter for correct type and presence of required
    parameters. Logs specific validation errors.
*************************************************************************/
bool GGEventBus::ValidateParams(lua_State* L, const std::string& event, int tableIdx)
{
    auto it = schemas.find(event);
    if (it == schemas.end())
        return true;  // No schema = no validation

    const EventSchema& schema = it->second;

    // Make absolute index
    if (tableIdx < 0)
        tableIdx = lua_gettop(L) + tableIdx + 1;

    // Check it's a table
    if (!lua_istable(L, tableIdx))
    {
        GGLogger::Error("[EventBus] Event parameters must be a table");
        return false;
    }

    // Validate each parameter
    for (const ParamDef& param : schema.params)
    {
        lua_getfield(L, tableIdx, param.name.c_str());

        bool hasValue = !lua_isnil(L, -1);

        if (!hasValue && param.required)
        {
            GGLogger::Error("[EventBus] Missing required parameter '"
                + param.name + "' for event '" + event + "'");
            lua_pop(L, 1);
            return false;
        }

        if (hasValue)
        {
            bool validType = false;

            switch (param.type)
            {
            case ParamType::NUMBER:
                validType = lua_isnumber(L, -1);
                break;

            case ParamType::INTEGER:
                if (lua_isnumber(L, -1))
                {
                    double val = lua_tonumber(L, -1);
                    validType = (val == static_cast<int>(val));
                    if (!validType)
                    {
                        GGLogger::Warning("[EventBus] Parameter '" + param.name
                            + "' should be integer, got decimal");
                    }
                }
                break;

            case ParamType::STRING:
                validType = lua_isstring(L, -1);
                break;

            case ParamType::BOOLEAN:
                validType = lua_isboolean(L, -1);
                break;

            case ParamType::ENTITY:
                validType = lua_islightuserdata(L, -1);
                break;
            }

            if (!validType)
            {
                GGLogger::Error("[EventBus] Parameter '" + param.name
                    + "' has wrong type for event '" + event + "'");
                lua_pop(L, 1);
                return false;
            }
        }

        lua_pop(L, 1);
    }

    return true;
}

/*************************************************************************
  \brief
    Set callback for networked event transmission.
  \param[in] handler
    Function to handle network events.
*************************************************************************/
void GGEventBus::SetNetworkHandler(std::function<void(const std::string&, lua_State*, int)> handler)
{
    networkHandler = handler;
    if (debugMode)
    {
        GGLogger::Info("[EventBus] Network handler configured");
    }
}

/*************************************************************************
  \brief
    Clear all subscriptions and release Lua references.
  \param[in] L
    Lua state.
  \details
    Releases all Lua function references to prevent leaks. Call before
    destroying event bus or Lua state.
*************************************************************************/
void GGEventBus::ClearAllSubscriptions(lua_State* L)
{
    // Release all Lua references
    for (const auto& [id, info] : subscriptions)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, info.luaRef);
    }

    subscriptions.clear();
    subscribers.clear();

    if (debugMode)
    {
        GGLogger::Info("[EventBus] Cleared all subscriptions");
    }
}

/*************************************************************************
  \brief
    Get list of registered event names for debugging.
  \return
    Vector of event names.
*************************************************************************/
std::vector<std::string> GGEventBus::GetRegisteredEvents() const
{
    std::vector<std::string> events;
    events.reserve(schemas.size());

    for (const auto& [name, schema] : schemas)
    {
        events.push_back(name);
    }

    return events;
}

/*************************************************************************
  \brief
    Hash string to 32-bit integer for network efficiency.
  \param[in] str
    String to hash.
  \return
    32-bit hash value.
  \details
    Uses FNV-1a hash algorithm for good distribution and speed.
*************************************************************************/
uint32_t GGEventBus::HashString(const std::string& str) const
{
    uint32_t hash = 2166136261u;
    for (char c : str)
    {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

/*************************************************************************
  \brief
    Call a Lua callback function with event parameters.
  \param[in] L
    Lua state.
  \param[in] funcRef
    Registry reference to function.
  \param[in] event
    Event name for error reporting.
  \param[in] tableIdx
    Stack index of parameter table.
  \details
    Pushes function and parameters onto stack, calls function with
    protected call for error handling. Logs any errors that occur.
*************************************************************************/
void GGEventBus::CallLuaCallback(lua_State* L, int funcRef,
    const std::string& event, int tableIdx)
{
    // Push function
    lua_rawgeti(L, LUA_REGISTRYINDEX, funcRef);

    // Push parameter table
    lua_pushvalue(L, tableIdx);

    // Call with error handling
    int result = lua_pcall(L, 1, 0, 0);

    if (result != LUA_OK)
    {
        const char* error = lua_tostring(L, -1);
        GGLogger::Error("[EventBus] Error in callback for event '" + event
            + "': " + (error ? error : "unknown error"));
        lua_pop(L, 1);  // Pop error message
    }
}