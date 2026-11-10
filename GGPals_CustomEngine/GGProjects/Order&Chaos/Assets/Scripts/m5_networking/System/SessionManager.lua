SessionManager_instance = nil

local class = DefineType("SessionManager", {
    Serializable = {
        isNetworked = false
    },

    Init = function(self, entity)
        SessionManager_instance = self
        
        if Persistent.sessionFaction then
            self.isNetworked = Persistent.sessionIsNetworked or false
        end
    end,

    GetFaction = function(self)
        return Persistent.sessionFaction or CONSTANTS.ORDER_PLAYER
    end,

    SetFaction = function(self, faction)
        Persistent.sessionFaction = faction
    end,

    GetPlayerName = function(self)
        return Persistent.sessionPlayerName or "Player"
    end,

    SetPlayerName = function(self, name)
        Persistent.sessionPlayerName = name
    end,

    GetRoomID = function(self)
        return Persistent.sessionRoomID or -1
    end,

    SetRoomID = function(self, roomID)
        Persistent.sessionRoomID = roomID
        self.currentRoomID = roomID
    end,

    GetRoomName = function(self)
        return Persistent.sessionRoomName or "Room"
    end,

    SetRoomName = function(self, name)
        Persistent.sessionRoomName = name
    end,

    GetGameMode = function(self)
        return Persistent.sessionGameMode or "passandplay"
    end,

    SetGameMode = function(self, mode)
        Persistent.sessionGameMode = mode
        if mode == "online" then
            self.isNetworked = true
            Persistent.sessionIsNetworked = true
        else
            self.isNetworked = false
            Persistent.sessionIsNetworked = false
        end
    end,

    SetNetworked = function(self, val)
        self.isNetworked = val
        Persistent.sessionIsNetworked = val
    end,

    IsNetworked = function(self)
        return self.isNetworked
    end
})

return class