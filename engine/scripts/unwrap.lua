-- Unwraps any deferred DefineX reference (Assets.X, Materials.X, AnimationClips.X, ...)
-- and recurses into plain tables so nested fields are unwrapped too. Plain values
-- (numbers, strings, usertypes, wrappers without __unwrap) pass through unchanged.
--
-- Protocol: a "deferred reference" is any table whose metatable carries a
-- `__unwrap(self)` function returning the unwrapped value. This is the only API
-- any dispatch site needs; registries don't expose per-type `.resolve` methods.

--- Unwrap any deferred `DefineX` reference (Assets.X, Textures.X, Shaders.X,
--- Materials.X, AnimationClips.X, ...) into its underlying value, recursing into
--- plain tables. Non-deferred values are returned unchanged.
---@param value any
---@return any
function _G.unwrap_def(value)
    local mt = getmetatable(value)
    if mt and mt.__unwrap then
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
