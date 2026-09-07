---@class Editor
_G.Editor = _G.Editor or {}

local prefab_state = _G.__editor_prefab_state or {
    dirty = {},
}
_G.__editor_prefab_state = prefab_state

local function get_prefab_def(name)
    local def = _G.__entity_prefab_registry[name]
    if def == nil then
        Log.error("Editor: prefab '" .. tostring(name) .. "' is not registered")
    end

    return def
end

local function get_or_create(owner, key)
    local existing = owner[key]
    if existing == nil then
        existing = {}
        owner[key] = existing
    end

    return existing
end

local function mark_prefab_dirty(name, def)
    prefab_state.dirty[name] = def
    Editor.invalidate_prefab_sections(name)
end

---@param name string
---@return boolean
function Editor.is_prefab_dirty(name)
    return prefab_state.dirty[name] ~= nil
end

---@return string[]
function Editor.get_dirty_prefab_names()
    local names = {}
    for name in pairs(prefab_state.dirty) do
        names[#names + 1] = name
    end

    table.sort(names)

    return names
end

---@param name string
function Editor.mark_prefab_saved(name)
    prefab_state.dirty[name] = nil
end

---@param name string
function Editor.mark_prefab_reverted(name)
    prefab_state.dirty[name] = nil
    Editor.invalidate_prefab_sections(name)
    __reapply_prefabs_to_spawned_entities()
    __reapply_scene_overrides_to_spawned_entities()
end

---@param name string
---@return string|nil
function Editor.get_prefab_file(name)
    return __get_def_source(DefRegistry.ENTITIES, name)
end

---@param name string
---@return string|nil reason, nil when the prefab can be written back to its file
function Editor.get_prefab_save_error(name)
    return Editor.get_definition_save_error(DefRegistry.ENTITIES, name, _G.__entity_prefab_registry[name] ~= nil)
end

local function for_each_instance_of_prefab(name, fn)
    EntitySpawner.for_each_entity(function(entity)
        local entity_id = entity:get_id()
        if _G.__entity_prefab_name_by_id[entity_id] == name then
            fn(entity, _G.__scene_instance_by_entity_id[entity_id])
        end
    end)
end

local function is_cpp_field_overridden(inst, component_key, field)
    if inst == nil then
        return false
    end

    if component_key == TransformKey.SECTION then
        local pose = inst[SceneKey.POSE_OVERRIDES]
        local pose_field = field == TransformKey.ROTATION and TransformKey.ROTATION_DEG or field
        if pose ~= nil and pose[pose_field] ~= nil then
            return true
        end
    end

    local cpp_overrides = inst[SceneKey.CPP_OVERRIDES]
    local section = cpp_overrides ~= nil and cpp_overrides[component_key] or nil

    return section ~= nil and section[field] ~= nil
end

local function is_lua_field_overridden(inst, class_name, field)
    if inst == nil then
        return false
    end

    local lua_overrides = inst[SceneKey.LUA_OVERRIDES]
    local fields = lua_overrides ~= nil and lua_overrides[class_name] or nil

    return fields ~= nil and fields[field] ~= nil
end

local function push_cpp_field(name, def, component_key, field)
    local schema = _G.__component_schemas[component_key]
    local setter = schema.setters[field]
    local defaults = __get_component_defaults(component_key)

    for_each_instance_of_prefab(name, function(entity, inst)
        if not is_cpp_field_overridden(inst, component_key, field) then
            local component = entity[schema.get](entity)
            if component ~= nil then
                __call_component_setter(component, setter,
                    __resolve_prefab_field_value(def[component_key], field, defaults))
            end
        end
    end)
end

local function push_lua_field(name, def, class_name, field)
    local fields = def[PrefabKey.LUA_FIELDS][class_name]

    for_each_instance_of_prefab(name, function(entity, inst)
        if not is_lua_field_overridden(inst, class_name, field) then
            local instance = entity:get_lua_component(class_name)
            if instance ~= nil then
                instance[field] = unwrap_def(fields[field])
            end
        end
    end)
end

local ROOT_FIELD_PUSHERS = {
    [PrefabKey.TICKING] = function(entity, value)
        entity:set_ticking(value == true)
    end,
    name = function(entity, value)
        entity:set_name(value or "")
    end,
}

local function set_root_field(name, def, field, value)
    local push = ROOT_FIELD_PUSHERS[field]
    if push == nil then
        Log.error("Editor.set_prefab_field: '" .. tostring(field) .. "' is not a prefab field")
        return false
    end

    def[field] = value
    mark_prefab_dirty(name, def)

    for_each_instance_of_prefab(name, function(entity)
        push(entity, value)
    end)

    return true
end

local function to_stored(value)
    if value == nil then
        return None
    end

    return value
end

---@param name string
---@param component_key string Schema key, or the prefab name for its root fields
---@param field string
---@param value any
---@return boolean
function Editor.set_prefab_field(name, component_key, field, value)
    local def = get_prefab_def(name)
    if def == nil then
        return false
    end

    local schema = _G.__component_schemas[component_key]
    if schema == nil and component_key == name then
        return set_root_field(name, def, field, value)
    end

    if schema == nil or schema.map_setter then
        Log.error("Editor.set_prefab_field: '" .. tostring(component_key) .. "' has no per-field setters")
        return false
    end

    if schema.setters[field] == nil then
        Log.error("Editor.set_prefab_field: unknown field '" .. tostring(field) .. "' on '" .. component_key .. "'")
        return false
    end

    get_or_create(def, component_key)[field] = to_stored(value)
    mark_prefab_dirty(name, def)
    push_cpp_field(name, def, component_key, field)

    return true
end

local function declares_lua_component(def, class_name)
    for _, entry in ipairs(def[PrefabKey.LUA_COMPONENTS] or {}) do
        if entry == class_name then
            return true
        end
    end

    return false
end

---@param name string
---@param class_name string
---@param field string
---@param value any
---@return boolean
function Editor.set_prefab_lua_field(name, class_name, field, value)
    local def = get_prefab_def(name)
    if def == nil then
        return false
    end

    if not declares_lua_component(def, class_name) then
        Log.error("Editor.set_prefab_lua_field: prefab '" .. name .. "' has no '" .. tostring(class_name) ..
            "' lua component")
        return false
    end

    if not Editor.is_editable_lua_field(_G.__component_registry[class_name], field, value) then
        Log.error("Editor.set_prefab_lua_field: '" .. tostring(field) .. "' is not an editable field")
        return false
    end

    get_or_create(get_or_create(def, PrefabKey.LUA_FIELDS), class_name)[field] = to_stored(value)
    mark_prefab_dirty(name, def)
    push_lua_field(name, def, class_name, field)

    return true
end

local function find_lua_component_index(def, class_name)
    for index, entry in ipairs(def[PrefabKey.LUA_COMPONENTS] or {}) do
        if entry == class_name then
            return index
        end
    end

    return nil
end

local function sorted_string_keys(source)
    local keys = {}
    for key in pairs(source) do
        if type(key) == "string" then
            keys[#keys + 1] = key
        end
    end
    table.sort(keys)

    return keys
end

---@param name string
---@return table rows of { name, is_lua }
function Editor.get_addable_prefab_sections(name)
    local def = get_prefab_def(name)
    if def == nil then
        return {}
    end

    local present = {}
    for _, section in ipairs(Editor.get_definition_sections(DefRegistry.ENTITIES, name) or {}) do
        present[section.name] = true
    end

    local rows = {}
    local schemas = _G.__component_schemas
    for _, key in ipairs(schemas.__order) do
        if schemas[key].map_setter == nil and def[key] == nil and not present[key] then
            rows[#rows + 1] = { name = key, is_lua = false }
        end
    end

    for _, class_name in ipairs(sorted_string_keys(_G.__component_registry)) do
        if find_lua_component_index(def, class_name) == nil then
            rows[#rows + 1] = { name = class_name, is_lua = true }
        end
    end

    return rows
end

---@param name string
---@param key string schema key, or the class name when is_lua
---@param is_lua boolean
---@param removed table|nil what remove_prefab_section returned, to restore it in place
---@return boolean
function Editor.add_prefab_section(name, key, is_lua, removed)
    local def = get_prefab_def(name)
    if def == nil then
        return false
    end

    removed = removed or {}

    if is_lua then
        if _G.__component_registry[key] == nil then
            Log.error("Editor.add_prefab_section: '" .. tostring(key) .. "' is not a component class")
            return false
        end

        if find_lua_component_index(def, key) ~= nil then
            Log.error("Editor.add_prefab_section: prefab '" .. name .. "' already has '" .. key .. "'")
            return false
        end

        local class_names = get_or_create(def, PrefabKey.LUA_COMPONENTS)
        table.insert(class_names, math.min(removed.index or #class_names + 1, #class_names + 1), key)

        if removed.lua_fields ~= nil then
            get_or_create(def, PrefabKey.LUA_FIELDS)[key] = removed.lua_fields
        end
    else
        local schema = _G.__component_schemas[key]
        if schema == nil or schema.map_setter then
            Log.error("Editor.add_prefab_section: '" .. tostring(key) .. "' is not an addable component")
            return false
        end

        if def[key] ~= nil then
            Log.error("Editor.add_prefab_section: prefab '" .. name .. "' already declares '" .. key .. "'")
            return false
        end

        def[key] = removed.section or {}
    end

    mark_prefab_dirty(name, def)

    return true
end

---@param name string
---@param key string
---@param is_lua boolean
---@return table|nil what add_prefab_section needs to restore it: { section } or { index, lua_fields }
function Editor.remove_prefab_section(name, key, is_lua)
    local def = get_prefab_def(name)
    if def == nil then
        return nil
    end

    local removed

    if is_lua then
        local index = find_lua_component_index(def, key)
        if index == nil then
            Log.error("Editor.remove_prefab_section: prefab '" .. name .. "' has no '" .. tostring(key) .. "'")
            return nil
        end

        local class_names = def[PrefabKey.LUA_COMPONENTS]
        table.remove(class_names, index)
        if #class_names == 0 then
            def[PrefabKey.LUA_COMPONENTS] = nil
        end

        local lua_fields = def[PrefabKey.LUA_FIELDS]
        local fields = lua_fields ~= nil and lua_fields[key] or nil
        if lua_fields ~= nil then
            lua_fields[key] = nil
            if next(lua_fields) == nil then
                def[PrefabKey.LUA_FIELDS] = nil
            end
        end

        removed = { index = index, lua_fields = fields }
    else
        if key == TransformKey.SECTION or def[key] == nil or _G.__component_schemas[key] == nil then
            Log.error("Editor.remove_prefab_section: '" .. tostring(key) .. "' is not removable from '" .. name .. "'")
            return nil
        end

        removed = { section = def[key] }
        def[key] = nil
    end

    mark_prefab_dirty(name, def)

    return removed
end

---@param path string
---@return string|nil
function Editor.get_prefab_name_for_file(path)
    return __def_name_from_file(path, FileExtension.PREFAB)
end

---@param path string
---@return string|nil reason, nil when a prefab may be created at this path
function Editor.get_prefab_create_error(path)
    local name = Editor.get_prefab_name_for_file(path)
    local is_registered = name ~= nil and _G.__entity_prefab_registry[name] ~= nil

    return Editor.get_definition_create_error(DefRegistry.ENTITIES, path, FileExtension.PREFAB, is_registered)
end

local function merge_section(section, overrides)
    for field, value in pairs(overrides) do
        if value == None then
            section[field] = nil
        else
            section[field] = value
        end
    end
end

---@param entity_id integer
---@return table|nil the source prefab's definition with the instance's overrides folded in
function Editor.create_prefab_def_from_entity(entity_id)
    local inst = _G.__scene_instance_by_entity_id[entity_id]
    if inst == nil then
        Log.error("Editor.create_prefab_def_from_entity: entity " .. tostring(entity_id) .. " is not a scene instance")
        return nil
    end

    local source = get_prefab_def(inst.prefab)
    if source == nil then
        return nil
    end

    local def = Editor.copy_def_table(source)

    for key, section in pairs(inst[SceneKey.CPP_OVERRIDES] or {}) do
        merge_section(get_or_create(def, key), section)
    end

    for class_name, fields in pairs(inst[SceneKey.LUA_OVERRIDES] or {}) do
        merge_section(get_or_create(get_or_create(def, PrefabKey.LUA_FIELDS), class_name), fields)
    end

    return def
end

function Editor.rebind_prefab_defs()
    local rebound = false

    for name, def in pairs(prefab_state.dirty) do
        if _G.__entity_prefab_registry[name] ~= nil then
            _G.__entity_prefab_registry[name] = def
            rebound = true
        else
            prefab_state.dirty[name] = nil
        end
    end

    if rebound then
        __reapply_prefabs_to_spawned_entities()
    end
end
