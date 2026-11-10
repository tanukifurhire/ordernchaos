SessionManager_instance = nil

local class = DefineType("SessionManager",
{
    Serializable = {
        isNetworked = false
    },

    sessionID = -1,

    Init = function(self, entity)
        if SessionManager_instance == nil then
           SessionManager_instance = self 
        end
    end
})

return class