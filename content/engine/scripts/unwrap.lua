-- An explicit "no value" a table can hold, since assigning nil erases the key instead.
_G.None = setmetatable({}, {
    __tostring = function() return "None" end,
    __unwrap = function() return nil end,
})

-- Unwraps any deferred DefineX reference (Textures.X, Materials.X, AnimationClips.X, ...)
---@param value any
---@return any
function _G.unwrap_def(value)
    local mt = getmetatable(value)
    if mt ~= nil and mt.__unwrap ~= nil then
        return mt.__unwrap(value)
    end

    if type(value) == "table" and mt == nil then
        local out = {}
        for k, v in pairs(value) do
            out[k] = unwrap_def(v)
        end
        return out
    end

    return value
end
