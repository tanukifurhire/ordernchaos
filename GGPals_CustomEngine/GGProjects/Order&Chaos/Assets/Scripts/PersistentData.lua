local ALLOWED = {
    ["number"] = true,
    ["string"] = true,
    ["boolean"] = true,
}

-- Use a global backing table so data survives script re-execution on scene change
local _data = rawget(_G, "__persistent_data")
if not _data then
    _data = {}
    rawset(_G, "__persistent_data", _data)
end

local function isValidValue(v)
    local vtype = type(v)
    if ALLOWED[vtype] then return true end
    if vtype == "table" then
        for k, val in pairs(v) do
            if not isValidValue(val) then return false end
            if not isValidValue(k) then return false end -- keys too
        end
        return true
    end
    return false
end

Persistent = setmetatable({}, {
    __newindex = function(t, k, v)
        if v == nil then
            _data[k] = nil
            return
        end
        local vtype = type(v)
        assert(isValidValue(v),
            string.format(
                "Persistent: Cannot store '%s' (key: '%s'), only non lightuserdata/userdata types allowed",
                vtype,
                tostring(k)
            )
        )
        _data[k] = v
    end,
    __index = function(t, k)
        return _data[k]
    end
})