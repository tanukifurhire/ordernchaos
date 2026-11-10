--[[
    RPCRegistry.lua

    OPTIONAL. Explicit RPC handler registration and audit helper.

    ---------------------------------------------------------------------------
    You do not need to attach this script for networking to work.
    ---------------------------------------------------------------------------
    The engine already registers every ServerRPC_* and ClientRPC_* global
    automatically, from C++, inside RPC.startServer() and RPC.connectToServer()
    (see EnsureDefaultRPCHandlersRegistered in RPCRegistry.cpp). That happens
    after all gameplay scripts have loaded and before the socket is live, so it
    cannot miss a handler and cannot race an inbound packet.

    Attach this script to an entity only when you want to do one of the two
    things below. It must run BEFORE RPC.startServer / RPC.connectToServer --
    an explicit registration outranks the automatic sweep, but only if it got
    there first.

    ---------------------------------------------------------------------------
    Background: why a registry exists at all
    ---------------------------------------------------------------------------
    Dispatch used to be lua_getglobal(name) with `name` taken straight off the
    network, which made loadstring, dofile, require and every engine binding
    one packet away from remote invocation. Now the wire name is only a key
    into this registry, and dispatch calls a reference captured at registration
    time. An unregistered name cannot reach _G at all.

    Three things are checked per call, all failing closed:
      - direction     a client cannot drive ClientRPC_* logic on the host
      - arity         argument count must sit inside the declared bounds
      - sender        the clientID argument must match the identity the
                      transport authenticated, so a peer cannot act as
                      another player

    senderArgIndex is the zero-based index of the argument carrying the
    caller's clientID, or -1 if the handler does not take one.
]]

local class = DefineType("RPCRegistry", {

    Start = function(self, entity)

        -- ------------------------------------------------------------------
        -- 1. Tighten a handler's contract
        -- ------------------------------------------------------------------
        -- The automatic sweep registers every handler permissively: 0..32
        -- arguments, senderArgIndex 0 for server-bound. Registering explicitly
        -- here replaces that with a real contract, so malformed calls are
        -- rejected before any handler code runs.
        --
        --   RPC.registerServer(name, fn, minArgs, maxArgs, senderArgIndex)
        --   RPC.registerClient(name, fn, minArgs, maxArgs, senderArgIndex)
        --
        -- Examples, once audited:
        --
        --   RPC.registerServer("ServerRPC_JoinRoom",   ServerRPC_JoinRoom,   4, 4,  0)
        --   RPC.registerServer("ServerRPC_LeaveRoom",  ServerRPC_LeaveRoom,  2, 2,  0)
        --   RPC.registerServer("ServerRPC_SetReady",   ServerRPC_SetReady,   3, 3,  0)
        --   RPC.registerClient("ClientRPC_JoinRoomFailed", ClientRPC_JoinRoomFailed, 1, 1, -1)

        -- ------------------------------------------------------------------
        -- 2. Correct a handler that does NOT take clientID as argument 1
        -- ------------------------------------------------------------------
        -- TODO(team): audit every ServerRPC_* and confirm its senderArgIndex.
        -- The sweep assumes index 0 for all of them. That holds for the
        -- lobby/room handlers in NetworkedGameServer.lua, but needs confirming
        -- for the gameplay ones -- ServerRPC_OnDeployUnit and
        -- ServerRPC_OnSubmitActions in particular take a different shape.
        --
        -- A wrong senderArgIndex fails safe: the call is rejected and logged as
        -- "sender mismatch". If you see that warning for a call you know is
        -- legitimate, register it here with the correct index (or -1).
        --
        --   RPC.registerServer("ServerRPC_OnDeployUnit", ServerRPC_OnDeployUnit, 0, 32, -1)

        -- ------------------------------------------------------------------
        -- 3. Audit surface
        -- ------------------------------------------------------------------
        -- Note: called from Start, this runs before the automatic sweep, so it
        -- lists only explicit registrations. To see the full set, call it after
        -- connecting -- or read the "N handler(s) reachable from the network"
        -- line the engine logs at startServer / connectToServer time.
        for i, name in ipairs(RPC.dumpHandlers()) do
            Log.info("[RPCRegistry] explicit " .. i .. ". " .. name)
        end
    end
})

return class
