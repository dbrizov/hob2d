-- Editor apply: the write side of the Editor.* contract.

---@class Editor
Editor = Editor or {}

local function resolve_entity(entity_id)
    local entity = EntitySpawner.get_entity(entity_id)
    if not entity:is_valid() then
        return nil
    end

    return entity
end

--- Write a field of a C++ component through its schema setter.
---@param entity_id integer
---@param component_key string Schema key, e.g. "sprite", "box_collider"
---@param field string
---@param value any
function Editor.set_component_field(entity_id, component_key, field, value)
    local entity = resolve_entity(entity_id)
    if entity == nil then
        return false
    end

    local schema = __component_schemas[component_key]
    if schema == nil then
        Log.error("Editor.set_component_field: unknown component '" .. tostring(component_key) .. "'")
        return false
    end

    local setter = schema.setters[field]
    if setter == nil then
        Log.error("Editor.set_component_field: unknown field '" .. tostring(field) .. "' on '" .. component_key .. "'")
        return false
    end

    local component = entity[schema.get](entity)
    if component == nil then
        return false
    end

    __call_component_setter(component, setter, unwrap_def(value))
    return true
end

--- Write a field of a Lua component.
---@param entity_id integer
---@param class_name string Lua component class, e.g. "Player"
---@param field string
---@param value any
function Editor.set_lua_component_field(entity_id, class_name, field, value)
    local entity = resolve_entity(entity_id)
    if entity == nil then
        return false
    end

    local instance = entity:get_lua_component(class_name)
    if instance == nil then
        Log.error("Editor.set_lua_component_field: no lua component of class '" .. tostring(class_name) .. "'")
        return false
    end

    if not Editor.is_editable_lua_field(getmetatable(instance), field, instance[field]) then
        Log.error("Editor.set_lua_component_field: '" .. tostring(field) .. "' is not an editable field")
        return false
    end

    instance[field] = unwrap_def(value)
    return true
end
