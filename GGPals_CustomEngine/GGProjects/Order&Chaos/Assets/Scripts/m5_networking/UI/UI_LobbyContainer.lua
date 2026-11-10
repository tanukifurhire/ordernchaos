local class = DefineType("LobbyContainer", {
    Serializable = {
        scrollSens = 1.0001
    },
    
    scrollAmount = 0.0001,
    scrollAmountMax = 0.0001,

    scrollOffset = 80.0,

    rooms = {},
    refreshRoomList = false,

    Init = function(self, entity)
        Event.subscribe("OnRoomListReceived", function(params)
            -- Handle delete of all children and empty rooms list
            local children = ECS.getChildren(entity)
            Log.info("[LobbyContainer] Received " .. #params.rooms .. " rooms")
            Log.info("[LobbyContainer] " .. #children .. " found!")
            for _, e in ipairs(children) do
                ECS.destroyEntityDeferred(e)
            end
            self.rooms = {}

            -- Handle population of rooms list
            for _, entry in ipairs(params.rooms) do
                table.insert(self.rooms, {
                    roomID = entry.roomID,
                    name = entry.name,
                    orderJoined = entry.orderJoined,
                    chaosJoined = entry.chaosJoined,
                    status = entry.status,
                    -- Present only for Steam lobbies (a host to connect to)
                    -- rather than rooms on a host we already reached.
                    steamLobbyId = entry.steamLobbyId,
                    handled = false
                })

                self.refreshRoomList = true
            end

            
            self.scrollAmountMax = self.scrollOffset * #params.rooms
        end)

        Event.subscribe("OnRoomEntryRequestHandle", function(params)
            
        end)
    end,

    Update = function(self, entity)
        local d = Input.getMouseWheelDelta()

        self.scrollAmount = math.min(self.scrollAmountMax, math.max(0.0001, self.scrollAmount + d * self.scrollSens))
    
        local t = ECS.getTransform(entity)

        t.y = self.scrollAmount

        ECS.setTransform(entity, t)

        if self.refreshRoomList then
            for i, entry in ipairs(self.rooms) do
                local e = Prefab.spawn("RoomEntry", 0.0, - self.scrollOffset * (i - 1), entity)
                local roomSelf = ECS.getScriptInstance(e)

                if roomSelf then
                    roomSelf.roomID = entry.roomID
                    roomSelf.roomName = entry.name
                    roomSelf.orderJoined = entry.orderJoined
                    roomSelf.chaosJoined = entry.chaosJoined
                    roomSelf.roomStatus = entry.status
                    roomSelf.steamLobbyId = entry.steamLobbyId or ""
                    entry.handled = true
                    roomSelf:UpdateDisplay(e)
                end
            end

            self.refreshRoomList = false
        end 
    end,

    RequestRoomList = function(self, entity)
        if NetworkedGameClient_instance then
            NetworkedGameClient_instance:RequestRoomList()
        end
    end
})

return class