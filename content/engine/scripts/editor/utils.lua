---@class Editor
_G.Editor = _G.Editor or {}

-- Shares userdata leaves and asset refs rather than cloning them: a definition is written by assigning
-- a fresh value into a table, never by mutating the one already there, and cloning an asset ref would
-- drop the metatable that names its registry.
---@param source table
---@return table
function Editor.copy_def_table(source)
    local copy = {}
    for key, value in pairs(source) do
        if type(key) ~= "string" or key:sub(1, 2) ~= "__" then
            if type(value) == "table" and getmetatable(value) == nil then
                copy[key] = Editor.copy_def_table(value)
            else
                copy[key] = value
            end
        end
    end

    return copy
end

---@param registry string
---@param path string
---@param extension string
---@param is_registered boolean
---@return string|nil reason, nil when a definition may be created at this path
function Editor.get_definition_create_error(registry, path, extension, is_registered)
    local name = __def_name_from_file(path, extension)
    if name == nil then
        return "'" .. path .. "' is not a '" .. extension .. "' file"
    end

    if not name:match("^[%a_][%w_]*$") then
        return "'" .. path .. "' derives the name '" .. name .. "', which is not a valid Lua name"
    end

    local owner = __get_def_source(registry, name)
    if owner ~= nil then
        return registry .. "." .. name .. " is already declared in '" .. owner .. "'"
    end

    if is_registered then
        return registry .. "." .. name .. " is already registered"
    end

    return nil
end

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
