---@class Editor
_G.Editor = _G.Editor or {}

---@param registry string
---@param name string
---@param is_registered boolean
---@return string|nil reason, nil when the definition can be written back to its file
function Editor.get_definition_save_error(registry, name, is_registered)
    if name == nil or not is_registered then
        return registry .. "." .. tostring(name) .. " is not registered"
    end

    local path = __get_def_source(registry, name)
    if path == nil then
        return registry .. "." .. name .. " has no recorded source file"
    end

    local count = __count_defs_in_file(path)
    if count > 1 then
        return "'" .. path .. "' declares " .. count ..
            " definitions, and the editor only writes files holding a single one"
    end

    return nil
end
