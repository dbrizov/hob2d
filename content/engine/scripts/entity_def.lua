-- DefineEntity: Prefab declaration for entities.

_G.__entity_prefab_registry = {}
_G.__entity_prefab_name_by_id = {}

on_entity_destroyed(function(entity_id)
    _G.__entity_prefab_name_by_id[entity_id] = nil
end)

on_entities_cleared(function()
    _G.__entity_prefab_name_by_id = {}
end)

function _G.__clear_entity_defs()
    _G.__entity_prefab_registry = {}
end

local component_defaults_cache = {}

function _G.__clear_component_defaults()
    component_defaults_cache = {}
end

---@class DefineEntity
_G.DefineEntity = setmetatable({}, {
    __newindex = function(_, name, def)
        if type(def) ~= "table" then
            Log.error("DefineEntity." .. tostring(name) .. " must be assigned a table")
            return
        end

        if not __record_def_source(DefRegistry.ENTITIES, name) then
            return
        end

        _G.__entity_prefab_registry[name] = def
    end,
    __index = function(_, name)
        return _G.__entity_prefab_registry[name]
    end,
})

-- `Entities.Foo` evaluates to the prefab name string `"Foo"`.
---@class Entities
_G.Entities = setmetatable({}, {
    __index = function(_, name) return name end,
})

function _G.__call_component_setter(component, setter, value)
    if type(setter) == "string" then
        component[setter](component, value)
    else
        setter(component, value)
    end
end

local call_setter = _G.__call_component_setter

---@type fun(): Entity
local spawn_entity_c = EntitySpawner.spawn_entity
local destroy_entity_c = EntitySpawner.destroy_entity

local function should_reapply_field(schema, field)
    local flags = schema.reapply_on_hot_reload
    return flags == nil or flags[field] ~= false
end

local function resolve_ticking(prefab)
    local ticking = prefab[PrefabKey.TICKING]
    if ticking == nil then
        return false
    end
    return ticking
end

---@param entity Entity
---@param fields_by_class table
---@param context string
function _G.__apply_lua_fields(entity, fields_by_class, context)
    for class_name, fields in pairs(fields_by_class) do
        local instance = entity:get_lua_component(class_name)
        if instance == nil then
            Log.error(context .. ": entity has no '" .. tostring(class_name) .. "' lua component")
        else
            for field, value in pairs(fields) do
                instance[field] = unwrap_def(value)
            end
        end
    end
end

local function apply_lua_fields(entity, prefab)
    local lua_fields = prefab[PrefabKey.LUA_FIELDS]
    if lua_fields then
        __apply_lua_fields(entity, lua_fields, "Prefab lua_fields")
    end
end

local function for_each_section(entity, prefab, accessor, fn)
    local schemas = _G.__component_schemas
    for _, key in ipairs(schemas.__order) do
        local section = prefab[key]
        if section ~= nil then
            local schema = schemas[key]
            local component = entity[schema[accessor]](entity)
            if component ~= nil then
                fn(key, schema, section, component)
            end
        end
    end
end

local function apply_setters(component, section, setters)
    for prop, value in pairs(section) do
        local setter = setters[prop]
        if setter == nil then
            Log.error("Unknown prefab property '" .. tostring(prop) .. "' for component")
        else
            call_setter(component, setter, unwrap_def(value))
        end
    end
end

local function apply_prefab(entity, prefab)
    entity:set_ticking(resolve_ticking(prefab))

    if prefab.name then
        entity:set_name(prefab.name)
    end

    for_each_section(entity, prefab, "add", function(_, schema, section, component)
        if schema.map_setter then
            call_setter(component, schema.map_setter, unwrap_def(section))
        else
            apply_setters(component, section, schema.setters)
        end
    end)

    local lua_components = prefab[PrefabKey.LUA_COMPONENTS]
    if lua_components then
        for _, entry in ipairs(lua_components) do
            entity:add_lua_component(entry)
        end
    end

    apply_lua_fields(entity, prefab)
end

---@param section table|nil
---@param field string
---@param defaults table
---@return any
function _G.__resolve_prefab_field_value(section, field, defaults)
    local value = section ~= nil and section[field] or nil
    if value ~= nil then
        return unwrap_def(value)
    end

    return unwrap_def(defaults[field])
end

local resolve_field_value = _G.__resolve_prefab_field_value

-- The probe never enters play, so its spawn and destroy both resolve synchronously
-- and leave the live entity list untouched, which is what makes this callable from inside for_each_entity.
---@param key string
---@return table
function _G.__get_component_defaults(key)
    local cached = component_defaults_cache[key]
    if cached then
        return cached
    end

    local defaults = {}
    local schema = _G.__component_schemas[key]

    -- A map_setter section (sockets) has no getters to read.
    if schema ~= nil and schema.getters ~= nil then
        local probe = spawn_entity_c()
        local component = probe[schema.add](probe)
        if component ~= nil then
            for field, getter in pairs(schema.getters) do
                local value = component[getter](component)
                if value == nil then
                    value = None
                end
                defaults[field] = value
            end
        end

        destroy_entity_c(probe)
    end

    component_defaults_cache[key] = defaults

    return defaults
end

local function reapply_prefab(entity, prefab)
    entity:set_ticking(resolve_ticking(prefab))

    for_each_section(entity, prefab, "get", function(key, schema, section, component)
        if schema.map_setter then
            call_setter(component, schema.map_setter, unwrap_def(section))
        else
            local defaults = __get_component_defaults(key)
            for field, setter in pairs(schema.setters) do
                if should_reapply_field(schema, field) then
                    call_setter(component, setter, resolve_field_value(section, field, defaults))
                end
            end
        end
    end)

    apply_lua_fields(entity, prefab)
end

function _G.__reapply_prefabs_to_spawned_entities()
    local live = {}

    EntitySpawner.for_each_entity(function(entity)
        local id = entity:get_id()
        local name = _G.__entity_prefab_name_by_id[id]
        if name then
            live[id] = name
            local prefab = _G.__entity_prefab_registry[name]
            if prefab then
                reapply_prefab(entity, prefab)
            end
        end
    end)

    _G.__entity_prefab_name_by_id = live
end

---@param prefab_name string
---@param position? Vector2
---@param rotation_deg? number
---@param scale? Vector2
---@return Entity|nil
EntitySpawner.spawn_entity = function(prefab_name, position, rotation_deg, scale)
    local prefab = _G.__entity_prefab_registry[prefab_name]
    if not prefab then
        Log.error("EntitySpawner.spawn_entity: prefab '" .. prefab_name .. "' is not registered")
        return nil
    end

    local entity = spawn_entity_c()
    entity:set_prefab_name(prefab_name)

    apply_prefab(entity, prefab)
    _G.__entity_prefab_name_by_id[entity:get_id()] = prefab_name

    local transform = entity:get_transform()
    if position ~= nil then
        transform:set_position(position)
    end
    if rotation_deg ~= nil then
        transform:set_rotation(rotation_deg * Math.DEG_TO_RAD)
    end
    if scale ~= nil then
        transform:set_scale(scale)
    end

    return entity
end
