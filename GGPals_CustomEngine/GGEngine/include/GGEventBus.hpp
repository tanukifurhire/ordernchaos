/*************************************************************************
  \file       GGEventBus.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       1 November 2025
  \version    1.0
  \brief
    Type-safe event bus for Lua gameplay scripts with network awareness.
  \details
    Provides validated event emission and subscription with schema enforcement,
    network/local separation, and debug logging. Designed for turn-based
    networked gameplay with deterministic execution. Uses schema validation
    to catch type errors at runtime before they cause desyncs. Separates
    local-only events (animations, UI) from networked events (game state).
  \dependencies
    <string>, <vector>, <unordered_map>, lua.h, GGECSInterface.hpp, GGLogger.hpp
  \see
    GGLuaBindings, GGLuaVM, GGLuaDebug
  \history
    - 1.0 (1 November 2025): Initial version with schema validation.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "GGECSInterface.hpp"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

/*************************************************************************
  \enum ParamType
  \brief
    Supported parameter types for event schemas.
  \details
    Defines the types that can be validated when passing parameters
    through the event system. INTEGER is separate from NUMBER to
    enforce grid coordinates are whole numbers in turn-based games.
*************************************************************************/
enum class ParamType : uint8_t
{
    NUMBER,    // Lua number (double)
    STRING,    // Lua string
    BOOLEAN,   // Lua boolean
    ENTITY,    // Entity handle (validated)
    INTEGER    // Subset of NUMBER, no decimals
};

/*************************************************************************
  \struct ParamDef
  \brief
    Definition of a single event parameter.
  \details
    Describes one parameter in an event schema including its name,
    type, and whether it's required. Used for runtime validation
    before event emission.
*************************************************************************/
struct ParamDef
{
    std::string name;
    ParamType type = ParamType::STRING;
    bool required = false;
};

/*************************************************************************
  \struct EventSchema
  \brief
    Complete schema definition for an event type.
  \details
    Contains all parameter definitions and metadata for an event.
    The isNetworked flag determines if this event needs network
    synchronization. The eventId is a hashed version of the name
    for efficient network transmission.
*************************************************************************/
struct EventSchema
{
    std::vector<ParamDef> params;
    bool isNetworked = false;  // Initialize to false
    uint32_t eventId = 0;  // Initialize to 0
    std::string description;  // For debug/documentation
};

/*************************************************************************
  \class GGEventBus
  \brief
    Central event routing system with type safety and validation.
  \details
    Manages event schemas, subscriptions, and emission with runtime
    validation. Separates local events (client-only) from networked
    events (synchronized). Provides debug logging for development.

    Usage pattern:
    1. Define event schemas during initialization
    2. Scripts subscribe to events with callbacks
    3. Scripts emit events with validated parameters
    4. Event bus routes to all subscribers

  \warning
    Not thread-safe. Use only from main thread or synchronize access.
*************************************************************************/
class GGEventBus
{
private:
    // Event name -> schema definition
    std::unordered_map<std::string, EventSchema> schemas;

    // Event name -> list of Lua function references
    std::unordered_map<std::string, std::vector<int>> subscribers;

    // Subscription ID counter for unsubscribe
    uint32_t nextSubscriptionId = 1;

    // ID -> subscription info for cleanup
    struct SubscriptionInfo
    {
        std::string eventName;
        int luaRef = 0;  // Initialize to 0
    };
    std::unordered_map<uint32_t, SubscriptionInfo> subscriptions;

    // Debug mode flag
    bool debugMode = false;

    // Network event callback
    std::function<void(const std::string&, lua_State*, int)> networkHandler;

public:
    /*************************************************************************
      \brief
        Registers an event schema for validation.
      \param[in] name
        Unique event identifier string.
      \param[in] schema
        Schema definition with parameter types.
      \details
        Must be called before events can be emitted. Usually called
        during initialization phase. Attempting to register same event
        twice will log warning and ignore second registration.
      \par Example
      \code
        EventSchema shotSchema;
        shotSchema.params = {{"x", ParamType::INTEGER, true},
                            {"y", ParamType::INTEGER, true}};
        shotSchema.isNetworked = true;
        eventBus.RegisterEvent("FireShot", shotSchema);
      \endcode
    *************************************************************************/
    void RegisterEvent(const std::string& name, const EventSchema& schema);

    /*************************************************************************
      \brief
        Subscribe to an event with a Lua callback.
      \param[in] L
        Lua state containing the callback function.
      \param[in] event
        Event name to subscribe to.
      \param[in] funcIdx
        Stack index of Lua function to call.
      \return
        Subscription ID for later unsubscribe.
      \details
        Stores a reference to the Lua function in the registry.
        The function will be called with event parameters when
        the event is emitted. Returns ID that can be used to
        unsubscribe later.
      \warning
        Caller must ensure Lua function remains valid.
    *************************************************************************/
    uint32_t Subscribe(lua_State* L, const std::string& event, int funcIdx);

    /*************************************************************************
      \brief
        Unsubscribe from an event.
      \param[in] L
        Lua state.
      \param[in] subscriptionId
        ID returned from Subscribe().
      \details
        Removes subscription and releases Lua function reference.
        Safe to call with invalid ID (logs warning).
    *************************************************************************/
    void Unsubscribe(lua_State* L, uint32_t subscriptionId);

    /*************************************************************************
      \brief
        Emit a local-only event (not networked).
      \param[in] L
        Lua state.
      \param[in] event
        Event name.
      \param[in] tableIdx
        Stack index of parameter table.
      \details
        Validates parameters against schema if defined, then calls
        all subscribed callbacks. Local events are not sent over
        network. Used for UI, animations, sounds, etc.
      \par Lua Usage
      \code
        Event.emitLocal("PlaySound", {sound = "explosion.wav"})
      \endcode
    *************************************************************************/
    void EmitLocal(lua_State* L, const std::string& event, int tableIdx);

    /*************************************************************************
      \brief
        Emit a networked event (synchronized).
      \param[in] L
        Lua state.
      \param[in] event
        Event name.
      \param[in] tableIdx
        Stack index of parameter table.
      \details
        Validates parameters, ensures event is marked as networked
        in schema, then both executes locally and sends to network
        handler if configured. Used for game state changes.
      \par Lua Usage
      \code
        Event.emitNetworked("FireShot", {x = 5, y = 3})
      \endcode
      \warning
        Event must be registered with isNetworked = true.
    *************************************************************************/
    void EmitNetworked(lua_State* L, const std::string& event, int tableIdx);

    /*************************************************************************
      \brief
        Validate parameters against event schema.
      \param[in] L
        Lua state.
      \param[in] event
        Event name.
      \param[in] tableIdx
        Stack index of parameter table.
      \return
        True if validation passes or no schema defined.
      \details
        Checks that all required parameters exist and have correct
        types. Logs detailed errors on validation failure. Returns
        true if event has no schema (allows flexibility during
        prototyping).
    *************************************************************************/
    bool ValidateParams(lua_State* L, const std::string& event, int tableIdx);

    /*************************************************************************
      \brief
        Set network event handler callback.
      \param[in] handler
        Function to call for networked events.
      \details
        The handler receives event name, Lua state, and parameter
        table index. It's responsible for serialization and network
        transmission.
    *************************************************************************/
    void SetNetworkHandler(std::function<void(const std::string&, lua_State*, int)> handler);

    /*************************************************************************
      \brief
        Enable or disable debug logging.
      \param[in] enabled
        True to enable verbose logging.
      \details
        When enabled, logs all event emissions, subscriptions, and
        validation results. Useful during development, should be
        disabled for production.
    *************************************************************************/
    void SetDebugLogging(bool enabled) { debugMode = enabled; }

    /*************************************************************************
      \brief
        Clear all subscriptions for cleanup.
      \param[in] L
        Lua state for releasing references.
      \details
        Releases all Lua function references and clears subscription
        maps. Call before destroying event bus or Lua state.
    *************************************************************************/
    void ClearAllSubscriptions(lua_State* L);

    /*************************************************************************
      \brief
        Get list of all registered events (for debug UI).
      \return
        Vector of registered event names.
    *************************************************************************/
    std::vector<std::string> GetRegisteredEvents() const;

private:
    /*************************************************************************
      \brief
        Hash string to uint32 for network efficiency.
      \param[in] str
        String to hash.
      \return
        32-bit hash value.
    *************************************************************************/
    uint32_t HashString(const std::string& str) const;

    /*************************************************************************
      \brief
        Call Lua function with event parameters.
      \param[in] L
        Lua state.
      \param[in] funcRef
        Lua registry reference to function.
      \param[in] event
        Event name for error reporting.
      \param[in] tableIdx
        Stack index of parameter table.
    *************************************************************************/
    void CallLuaCallback(lua_State* L, int funcRef, const std::string& event, int tableIdx);
};