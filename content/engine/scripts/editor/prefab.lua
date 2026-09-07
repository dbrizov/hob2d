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
