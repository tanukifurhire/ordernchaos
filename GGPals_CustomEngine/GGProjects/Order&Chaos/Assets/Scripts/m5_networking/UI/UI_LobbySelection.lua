--[[
    UI_LobbySelection.lua

    The server browser.

    Two sources of rooms, chosen at runtime:

      Steam    Steam.requestLobbyList() -> SteamCB_OnLobbyList
               Works across the internet with no port forwarding. What players
               get.

      Direct   ServerRPC_RequestRoomList over an already-open connection.
               Needs a reachable host. Kept behind useSteam = false as the
               debug path and as the kill-switch: if Steam misbehaves near a
               deadline, flip this and ship the LAN path.

    The two layers are NOT the same thing and it is worth being clear about it:

      A Steam lobby is matchmaking. It exists to let a client discover the
      host's SteamID and open a P2P connection to it.

      A room is the game's own concept, created with ServerRPC_CreateRoom over
      an established connection, and it is what actually holds factions,
      readiness and arsenals.

    Joining a Steam lobby gets you connected to the host. The existing room
    flow then runs on top of that connection, unchanged - exactly as it does
    over LAN today.
]]

UI_LobbySelection_instance = nil

local class = DefineType("LobbySelection", {
    Serializable = {
        roomListContainer = 0,
        maxVisibleRooms = 4,

        -- Matchmake through Steam. Set false to fall back to the direct path.
        useSteam = false
    },

    rooms = {},
    roomEntities = {},
    steamLobbies = {},
    isConnected = false,

    -- Set when the player picks a lobby from the browser. Joining a lobby only
    -- gets you connected to the host; the room still has to be joined
    -- afterwards. Rather than making that a second click, the intent is
    -- remembered here and replayed once the host's real room list arrives.
    pendingAutoJoin = false,

    Init = function(self, entity)
        UI_LobbySelection_instance = self

        -- Default, not override. This used to assign ORDER unconditionally,
        -- which threw away the faction the player picked on the main menu and
        -- made the scene's own arrows fight whichever Init happened to run
        -- last - FactionIconDisplay reads this same value to decide which icon
        -- to show, so whether the icon agreed with what got sent to the server
        -- came down to entity order.
        Persistent.sessionFaction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER

        Event.subscribe("OnJoinRoomSuccess", function(params)
            Log.info("[LobbySelection] Joined room, navigating to lobby room")
            Event.emitLocal("RequestSceneTransition", {
                scene = "M5LobbyRoomScene"
            })
        end)

        Event.subscribe("OnJoinRoomFailed", function(params)
            Log.warning("[LobbySelection] Join failed: " .. params.reason)
        end)

        Event.subscribe("OnDisconnectedFromServer", function()
            Log.warning("[LobbySelection] Disconnected from server")
            Event.emitLocal("RequestSceneTransition", {
                scene = "M5ModeSelectScene"
            })
        end)

        -- Steam could not get us into a lobby, or got us into one we could not
        -- connect through. Either way there is nothing to show here.
        Event.subscribe("OnSteamLobbyFailed", function(params)
            Log.warning("[LobbySelection] Steam lobby failed: " .. tostring(params.reason))
            Event.emitLocal("RequestSceneTransition", {
                scene = "M5ModeSelectScene"
            })
        end)

        -- One-click join.
        --
        -- The list shows hosts before you are connected and rooms after, so
        -- clicking Join on a lobby used to leave you looking at a freshly
        -- repopulated list needing a second click on what looked like the same
        -- row. Replay the intent instead: when the real room list lands and we
        -- are connected, join straight away.
        Event.subscribe("OnRoomListReceived", function(params)
            if not self.pendingAutoJoin then return end
            if not NetworkedGameClient_instance then return end
            if not NetworkedGameClient_instance.connected then return end

            for _, room in ipairs(params.rooms or {}) do
                -- steamLobbyId marks a host row; a real room has none.
                if not room.steamLobbyId and not (room.orderJoined and room.chaosJoined) then
                    self.pendingAutoJoin = false

                    local playerName = Persistent.sessionPlayerName or "Player"
                    if playerName == "" then playerName = "Player" end

                    -- Take the free faction rather than the preferred one when
                    -- the host already occupies it, matching what the Join
                    -- button does manually.
                    local faction = Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
                    if faction == CONSTANTS.ORDER_PLAYER and room.orderJoined then
                        faction = CONSTANTS.CHAOS_PLAYER
                    elseif faction == CONSTANTS.CHAOS_PLAYER and room.chaosJoined then
                        faction = CONSTANTS.ORDER_PLAYER
                    end

                    Log.info("[LobbySelection] Auto-joining room " .. room.roomID
                        .. " after entering the lobby")
                    NetworkedGameClient_instance:JoinRoom(playerName, room.roomID, faction)
                    return
                end
            end
        end)

        -- A failed lobby join must not leave the intent armed, or it would fire
        -- on some unrelated room list later.
        Event.subscribe("OnSteamLobbyFailed", function(params)
            self.pendingAutoJoin = false
        end)

        Log.info("[LobbySelection] Initialized")

        Event.subscribe("OnClientConnected", function(params)
            -- Direct path only. On the Steam path the connection is opened by
            -- SteamCB_OnLobbyEntered after a lobby is picked, so there is
            -- nothing to list at connect time.
            if Persistent.connected and not self.useSteam then
                NetworkedGameClient_instance:RequestRoomList()
            end
        end)
    end,

    Start = function(self, entity)
        if self.useSteam then
            self:RefreshRoomList()
        end
    end,

    --[[
        Asks whichever source is active for a fresh list.
        Both are asynchronous; the answer arrives on a later frame.
    ]]
    RefreshRoomList = function(self)
        if self.useSteam then
            if not Steam.isAvailable() then
                Log.warning("[LobbySelection] Steam is not available; cannot list lobbies")
                return
            end

            Log.info("[LobbySelection] Requesting the Steam lobby list...")
            Steam.requestLobbyList()
            return
        end

        if NetworkedGameClient_instance and Persistent.connected then
            NetworkedGameClient_instance:RequestRoomList()
        else
            Log.warning("[LobbySelection] Not connected; cannot refresh the room list")
        end
    end,

    --[[
        Receives the Steam lobby list from SteamCB_OnLobbyList.

        lobbies  array of { id, hostSteamId, name, players, maxPlayers }
                 ids are strings and must stay strings - see
                 SteamNetworkCallbacks.lua for why.

        Already filtered by the engine to this game and this build version, so
        every row is joinable in principle.
    ]]
    PopulateSteamLobbies = function(self, lobbies)
        self.steamLobbies = lobbies or {}

        Log.info("[LobbySelection] " .. #self.steamLobbies .. " Steam lobby/lobbies available")

        -- Reuse the existing room list to show hosts.
        --
        -- The two are different things - a Steam lobby is a HOST you have not
        -- connected to yet, a room is something that exists ON a host you are
        -- already connected to - but before connecting there are no rooms to
        -- show, so the same widget can present hosts without any conflict.
        -- Once connected, ClientRPC_RoomList overwrites this with real rooms.
        --
        -- steamLobbyId is what tells a RoomEntry which of the two it is. It is
        -- a string, and must stay one: a lobby ID is 64-bit and a Lua number
        -- would silently mangle it.
        local entries = {}

        -- Never list the lobby we are already in. Steam returns it like any
        -- other, and it is the one row in the browser that cannot do anything:
        -- clicking it calls Steam.joinLobby on a lobby we are a member of, and
        -- for the owner SteamCB_OnLobbyEntered correctly refuses to connect a
        -- client to its own host. From the player's side that is a room that
        -- swallows clicks and will not go away - which is exactly what a host
        -- saw after a match ended.
        local ownLobbyId = Steam.isAvailable() and Steam.getLobbyId() or "0"

        for index, lobby in ipairs(self.steamLobbies) do
            if lobby.id ~= ownLobbyId then
                table.insert(entries, {
                    roomID       = index,
                    name         = lobby.name .. "  (" .. lobby.players .. "/" .. lobby.maxPlayers .. ")",
                    orderJoined  = lobby.orderTaken or false,
                    chaosJoined  = lobby.chaosTaken or false,
                    status       = (lobby.orderTaken and lobby.chaosTaken) and "Full" or "Open",
                    steamLobbyId = lobby.id
                })
            end
        end

        Event.emitLocal("OnRoomListReceived", { rooms = entries })
    end,

    --[[
        Joins one row of the Steam list.

        index  1-based position in the list last shown to the player.

        Asynchronous: SteamCB_OnLobbyEntered does the actual connecting once
        Steam confirms we are in the lobby.
    ]]
    JoinSteamLobby = function(self, index)
        local lobby = self.steamLobbies[index]

        if not lobby then
            Log.warning("[LobbySelection] No Steam lobby at index " .. tostring(index))
            return
        end

        if lobby.players >= lobby.maxPlayers then
            Log.warning("[LobbySelection] Lobby '" .. lobby.name .. "' is full")
            return
        end

        Log.info("[LobbySelection] Joining Steam lobby " .. lobby.id .. " (" .. lobby.name .. ")")
        Steam.joinLobby(lobby.id)
    end,

    --[[
        Hosts a new Steam lobby. SteamCB_OnLobbyCreated brings the transport up.
    ]]
    HostSteamLobby = function(self, maxPlayers)
        if not Steam.isAvailable() then
            Log.warning("[LobbySelection] Steam is not available; cannot host")
            return
        end

        Steam.createLobby(maxPlayers or 4, Steam.getPersonaName() .. "'s game")
    end,

    --[[
        Opens the Steam overlay invite dialog for the lobby we are in.
        Does nothing outside a lobby, or if the overlay is disabled.
    ]]
    OpenInviteOverlay = function(self)
        Steam.openInviteOverlay()
    end
})

return class
