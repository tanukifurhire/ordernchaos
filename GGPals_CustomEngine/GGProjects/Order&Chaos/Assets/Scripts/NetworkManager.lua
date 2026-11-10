--[[
    NetworkManager.lua

    Brings a session up. Three paths, one of which is new:

      1. Direct host      RPC.startServer()          binds UDP 9081
      2. Direct client    RPC.connectToServer(host)   needs a reachable host
      3. Steam            Steam.createLobby()  or  Steam.requestLobbyList()

    Paths 1 and 2 are unchanged and are still the fast iteration loop: two
    instances on one machine over loopback, no Steam client, no login, instant
    restart. Use them for all gameplay work.

    Path 3 is what players get. Steam supplies NAT traversal and Valve's relay
    network, so neither side needs port forwarding or a DDNS name - which the
    direct path does need, and which is fine for a lab and unacceptable for a
    shipped game.

    Steam is asynchronous throughout. Nothing here blocks; the answers arrive
    later as SteamCB_* globals (see SteamNetworkCallbacks.lua).
]]

NetworkManager_instance = nil

local class = DefineType("NetworkManager", {
    Serializable = {
        isServer = false,
        localPlayerId = 1,

        -- Set true to matchmake through Steam instead of a direct address.
        -- Falls back to the direct path on its own when Steam is unavailable,
        -- so a build with no Steam client still starts.
        useSteam = false,

        -- Only used by the direct path.
        directHost = "onclocal.ddns.net",
        directPort = 9081,

        -- Only used by the Steam path.
        lobbySize = 4
    },

    connected = false,
    transport = "winsock",

    Init = function(self, entity)
        if NetworkManager_instance == nil then
            NetworkManager_instance = self
        end
    end,

    Start = function(self, entity)
        if self.useSteam and NetworkManager.EnsureSteam(self) then
            if self.isServer then
                NetworkManager.StartSteamHost(self, entity)
            else
                NetworkManager.StartSteamClient(self, entity)
            end
            return
        end

        if self.isServer then
            NetworkManager.StartServer(self, entity)
        else
            NetworkManager.StartClient(self, entity)
        end
    end,

    Update = function(self, entity)
    end,

    --[[
        Brings the Steam API up and reports whether the Steam path is usable.

        A false return is not an error: it is the "player is running without
        Steam" case - client closed, not logged in, or a build with no SDK.
        The caller falls back to the direct path rather than failing.
    ]]
    EnsureSteam = function(self)
        if not Steam.isCompiledIn() then
            Log.warning("[NetworkManager] Build has no Steamworks SDK, using the direct path")
            return false
        end

        -- Normally already true: the engine initialises Steam at startup, so
        -- that the overlay hooks, the relay handshake has a head start, and a
        -- friend invite accepted from the main menu is not dropped. The retry
        -- here covers the case where the player started the Steam client after
        -- the game.
        if not Steam.isAvailable() then
            if not Steam.init() then
                Log.warning("[NetworkManager] Steam unavailable (client closed or not logged in), "
                    .. "using the direct path")
                return false
            end
        end

        Log.info("[NetworkManager] Steam ready: " .. Steam.getPersonaName()
            .. " (" .. Steam.getSteamID() .. ")")
        return true
    end,

    StartServer = function(self, entity)
        self.transport = "winsock"

        if RPC.startServer(self.directPort) then
            Log.info("[NetworkManager] Server started successfully")
            self.connected = true
        else
            Log.error("[NetworkManager] Failed to start server")
        end
    end,

    StartClient = function(self, entity)
        self.transport = "winsock"

        if RPC.connectToServer(self.directHost, self.directPort) then
            Log.info("[NetworkManager] Connected to server")
            self.connected = true
        else
            Log.error("[NetworkManager] Failed to connect to server")
        end
    end,

    --[[
        Creates a lobby. The transport does NOT come up here.

        Order matters: the lobby has to exist before hosting starts, because
        the lobby is what advertises the host's SteamID and what the accept
        gate checks membership against. SteamCB_OnLobbyCreated is where
        RPC.startSteamHost() actually runs.
    ]]
    StartSteamHost = function(self, entity)
        self.transport = "steam"

        Log.info("[NetworkManager] Creating a Steam lobby for " .. self.lobbySize .. " players...")
        Steam.createLobby(self.lobbySize, Steam.getPersonaName() .. "'s game")
    end,

    --[[
        Asks for the lobby list. The browser UI fills in from
        SteamCB_OnLobbyList; joining is the player's choice from there.
    ]]
    StartSteamClient = function(self, entity)
        self.transport = "steam"

        Log.info("[NetworkManager] Requesting the Steam lobby list...")
        Steam.requestLobbyList()
    end
})

return class
