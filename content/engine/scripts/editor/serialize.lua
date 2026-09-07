-- Editor serialize: the definition-table-to-Lua-source side of the Editor.* contract.

---@class Editor
_G.Editor = _G.Editor or {}

local INDENT = "    "
local LINE_BUDGET = 140
local FLOAT_FORMAT = "%.7g"

local SHAPES = {
    [FieldType.VECTOR2] = { ctor = "Vector2", fields = { "x", "y" } },
    [FieldType.COLOR] = { ctor = "Color", fields = { "r", "g", "b", "a" } },
    [FieldType.AABB] = { ctor = "AABB", fields = { "center", "extents" } },
    [FieldType.CAPSULE] = { ctor = "Capsule", fields = { "center_a", "center_b", "radius" } },
    [FieldType.CIRCLE] = { ctor = "Circle", fields = { "center", "radius" } },
}

local POSE_ORDER = { TransformKey.POSITION, TransformKey.ROTATION_DEG, TransformKey.SCALE }
local POSE_TYPES = { [TransformKey.ROTATION_DEG] = { type = FieldType.FLOAT } }

local shape_by_metatable = nil

local function ensure_shape_metatables()
    if shape_by_metatable ~= nil then
        return
    end

    shape_by_metatable = {
        [getmetatable(Vector2())] = SHAPES[FieldType.VECTOR2],
        [getmetatable(Color())] = SHAPES[FieldType.COLOR],
        [getmetatable(AABB())] = SHAPES[FieldType.AABB],
        [getmetatable(Capsule())] = SHAPES[FieldType.CAPSULE],
        [getmetatable(Circle())] = SHAPES[FieldType.CIRCLE],
    }
end

local function fail(path, message)
    error("Editor.serialize_scene: " .. path .. " " .. message, 0)
end

-- Lua dispatches __eq off the left operand whenever both sides are userdata, and sol2's __eq raises
-- when it cannot convert the right one, so mismatched shapes must not reach it.
local function is_baseline(value, baseline)
    if value == baseline then
        return true
    end

    if type(value) == "userdata" then
        return type(baseline) == "userdata" and getmetatable(value) == getmetatable(baseline) and value == baseline
    end

    -- Integers stay exact: approx_equal narrows to float, which would collapse adjacent int64 bitmasks.
    if type(value) == "number" and type(baseline) == "number" and
        (math.type(value) == "float" or math.type(baseline) == "float") then
        return Math.approx_equal(value, baseline)
    end

    return false
end

local function get_prefab_section(prefab, key)
    if prefab == nil then
        return nil
    end

    local section = prefab[key]
    if type(section) ~= "table" then
        return nil
    end

    return section
end

local function resolve_baseline(prefab_section, defaults, field)
    if prefab_section ~= nil and prefab_section[field] ~= nil then
        return prefab_section[field]
    end

    if defaults ~= nil then
        return defaults[field]
    end

    return nil
end

local function format_number(value, path)
    if math.type(value) == "integer" then
        return tostring(value)
    end

    if value ~= value then
        fail(path, "is NaN, which has no Lua literal")
    end

    if value == math.huge then
        return "math.huge"
    end

    if value == -math.huge then
        return "-math.huge"
    end

    if value == 0.0 then
        return "0.0"
    end

    local text = string.format(FLOAT_FORMAT, value)
    if not text:find("[.eE]") then
        text = text .. ".0"
    end

    return text
end

local function format_enum(value, enum_name)
    local entries = Editor.get_enum_entries(enum_name)
    if entries == nil then
        return nil
    end

    for _, entry in ipairs(entries) do
        if entry.value == value then
            return enum_name .. "." .. entry.name
        end
    end

    return nil
end

local function format_bitmask(value, enum_name)
    local entries = Editor.get_enum_entries(enum_name)
    if entries == nil then
        return nil
    end

    local remaining = value
    local names = {}
    for _, entry in ipairs(entries) do
        if entry.value ~= 0 and (remaining & entry.value) == entry.value then
            names[#names + 1] = enum_name .. "." .. entry.name
            remaining = remaining & ~entry.value
        end
    end

    if remaining ~= 0 or #names == 0 then
        return nil
    end

    return table.concat(names, " | ")
end

local function format_named_number(value, field_meta, path)
    local enum_name = field_meta and field_meta.enum
    if enum_name ~= nil and math.type(value) == "integer" then
        local text = nil
        if field_meta.type == FieldType.ENUM then
            text = format_enum(value, enum_name)
        elseif field_meta.type == FieldType.BITMASK then
            text = format_bitmask(value, enum_name)
        end

        if text ~= nil then
            return text
        end
    end

    local declared = field_meta and field_meta.type
    if declared == FieldType.FLOAT or declared == FieldType.ANGLE then
        value = value + 0.0
    end

    return format_number(value, path)
end

local function sorted_keys(source, skip)
    local keys = {}
    for key in pairs(source) do
        if type(key) == "string" and key:sub(1, 2) ~= "__" and (skip == nil or skip[key] == nil) then
            keys[#keys + 1] = key
        end
    end
    table.sort(keys)

    return keys
end

local function ordered_keys(source, order)
    local keys = {}
    local emitted = {}

    if order ~= nil then
        for _, key in ipairs(order) do
            if source[key] ~= nil then
                keys[#keys + 1] = key
                emitted[key] = true
            end
        end
    end

    for _, key in ipairs(sorted_keys(source, emitted)) do
        keys[#keys + 1] = key
    end

    return keys
end

local function indent_of(depth)
    return string.rep(INDENT, depth)
end

local function wrap_fields(parts, depth, prefix)
    if #parts == 0 then
        return nil
    end

    local inline = "{ " .. table.concat(parts, ", ") .. " }"
    if #INDENT * depth + prefix + #inline + 1 <= LINE_BUDGET and not inline:find("\n", 1, true) then
        return inline
    end

    local inner = indent_of(depth + 1)
    local separator = ",\n" .. inner

    return "{\n" .. inner .. table.concat(parts, separator) .. ",\n" .. indent_of(depth) .. "}"
end

local function field_prefix(key)
    return #key + 3
end

local serialize_value

local function serialize_shape(shape, value, path, depth)
    local args = {}
    for index, field in ipairs(shape.fields) do
        args[index] = serialize_value(value[field], nil, path .. "." .. field, depth, 0)
    end

    return shape.ctor .. "(" .. table.concat(args, ", ") .. ")"
end

local function serialize_plain_table(value, path, depth, prefix)
    local parts = {}

    for index = 1, #value do
        parts[#parts + 1] = serialize_value(value[index], nil, path .. "[" .. index .. "]", depth + 1, 0)
    end

    for _, key in ipairs(sorted_keys(value)) do
        parts[#parts + 1] = key .. " = " ..
            serialize_value(value[key], nil, path .. "." .. key, depth + 1, field_prefix(key))
    end

    return wrap_fields(parts, depth, prefix) or "{}"
end

serialize_value = function(value, field_meta, path, depth, prefix)
    if value == None then
        return "None"
    end

    local value_type = type(value)

    if value_type == "number" then
        return format_named_number(value, field_meta, path)
    end

    if value_type == "boolean" then
        return tostring(value)
    end

    if value_type == "string" then
        return string.format("%q", value)
    end

    if value_type == "table" then
        local mt = getmetatable(value)
        if mt == nil then
            return serialize_plain_table(value, path, depth, prefix)
        end

        local registry = rawget(mt, "__registry")
        local asset_name = rawget(value, "__name")
        if registry == nil or asset_name == nil then
            fail(path, "holds a table whose metatable is not a declared asset registry")
        end

        return registry .. "." .. asset_name
    end

    if value_type == "userdata" then
        ensure_shape_metatables()

        local shape = SHAPES[field_meta and field_meta.type] or shape_by_metatable[getmetatable(value)]
        if shape == nil then
            fail(path, "holds '" .. tostring(value) ..
                "', which is neither a declared asset nor a value with a Lua literal")
        end

        return serialize_shape(shape, value, path, depth)
    end

    fail(path, "holds a " .. value_type .. " value, which cannot be written to a scene file")
end

local function get_pose_baseline(prefab, field)
    local transform = get_prefab_section(prefab, TransformKey.SECTION)
    local defaults = __get_component_defaults(TransformKey.SECTION)

    if field == TransformKey.ROTATION_DEG then
        local radians = resolve_baseline(transform, defaults, TransformKey.ROTATION)
        return type(radians) == "number" and radians * Math.RAD_TO_DEG or nil
    end

    if field == TransformKey.SCALE then
        return resolve_baseline(transform, defaults, TransformKey.SCALE)
    end

    return nil
end

local function serialize_pose_overrides(pose, path, depth, prefix, prefab)
    local parts = {}
    for _, field in ipairs(ordered_keys(pose, POSE_ORDER)) do
        local baseline = get_pose_baseline(prefab, field)
        if baseline == nil or not is_baseline(pose[field], baseline) then
            parts[#parts + 1] = field .. " = " ..
                serialize_value(pose[field], POSE_TYPES[field], path .. "." .. field, depth + 1, field_prefix(field))
        end
    end

    return wrap_fields(parts, depth, prefix)
end

local function serialize_cpp_section(section, schema, path, depth, prefix, prefab_section, defaults)
    if type(section) ~= "table" then
        fail(path, "is not a table")
    end

    local types = schema and schema.types

    local parts = {}
    for _, field in ipairs(ordered_keys(section, schema and schema.__order)) do
        local baseline = resolve_baseline(prefab_section, defaults, field)
        if baseline == nil or not is_baseline(section[field], baseline) then
            local field_meta = types and types[field]
            parts[#parts + 1] = field .. " = " ..
                serialize_value(section[field], field_meta, path .. "." .. field, depth + 1, field_prefix(field))
        end
    end

    return wrap_fields(parts, depth, prefix)
end

local function serialize_cpp_overrides(cpp_overrides, path, depth, prefix, prefab)
    local schemas = _G.__component_schemas

    local parts = {}
    for _, key in ipairs(ordered_keys(cpp_overrides, schemas.__order)) do
        local schema = schemas[key]

        local has_elidable_fields = schema ~= nil and schema.map_setter == nil
        local prefab_section = has_elidable_fields and get_prefab_section(prefab, key) or nil
        local defaults = has_elidable_fields and __get_component_defaults(key) or nil

        local section = serialize_cpp_section(cpp_overrides[key], schema, path .. "." .. key,
            depth + 1, field_prefix(key), prefab_section, defaults)
        if section ~= nil then
            parts[#parts + 1] = key .. " = " .. section
        end
    end

    return wrap_fields(parts, depth, prefix)
end

local function serialize_lua_overrides(lua_overrides, path, depth, prefix)
    local parts = {}
    for _, class_name in ipairs(sorted_keys(lua_overrides)) do
        local section_path = path .. "." .. class_name
        local section = lua_overrides[class_name]
        if type(section) ~= "table" then
            fail(section_path, "is not a table")
        end

        local fields = {}
        for _, field in ipairs(sorted_keys(section)) do
            local field_meta = Editor.get_lua_field_annotation(class_name, field)
            fields[#fields + 1] = field .. " = " ..
                serialize_value(section[field], field_meta, section_path .. "." .. field, depth + 2, field_prefix(field))
        end

        local text = wrap_fields(fields, depth + 1, field_prefix(class_name))
        if text ~= nil then
            parts[#parts + 1] = class_name .. " = " .. text
        end
    end

    return wrap_fields(parts, depth, prefix)
end

local function append_overrides(parts, inst, key, serialize_section, path, depth, prefab)
    local overrides = inst[key]
    if type(overrides) ~= "table" then
        return
    end

    local section = serialize_section(overrides, path .. "." .. key, depth, field_prefix(key), prefab)
    if section ~= nil then
        parts[#parts + 1] = key .. " = " .. section
    end
end

local function serialize_instance(inst, path, depth, prefix)
    if type(inst.prefab) ~= "string" then
        fail(path, "does not name a prefab")
    end

    local prefab = _G.__entity_prefab_registry[inst.prefab]
    local parts = { "prefab = " .. DefRegistry.ENTITIES .. "." .. inst.prefab }

    append_overrides(parts, inst, SceneKey.POSE_OVERRIDES, serialize_pose_overrides, path, depth + 1, prefab)
    append_overrides(parts, inst, SceneKey.CPP_OVERRIDES, serialize_cpp_overrides, path, depth + 1, prefab)
    append_overrides(parts, inst, SceneKey.LUA_OVERRIDES, serialize_lua_overrides, path, depth + 1, prefab)

    return wrap_fields(parts, depth, prefix)
end

local EMPTY_SCENE = { entities = {} }

local function serialize_scene_def(def, name)
    local lines = { "DefineScene." .. name .. " = {" }

    if #def.entities == 0 then
        lines[#lines + 1] = INDENT .. "entities = {},"
    else
        lines[#lines + 1] = INDENT .. "entities = {"

        for index, inst in ipairs(def.entities) do
            lines[#lines + 1] = indent_of(2) .. serialize_instance(inst, "entities[" .. index .. "]", 2, 0) .. ","
        end

        lines[#lines + 1] = INDENT .. "},"
    end

    lines[#lines + 1] = "}"

    return table.concat(lines, "\n") .. "\n"
end

---@param name string
---@param as_name string|nil the name to declare it under; defaults to `name`
---@return string
function Editor.serialize_scene(name, as_name)
    local def = _G.__scene_registry[name]
    if def == nil then
        error("Editor.serialize_scene: scene '" .. tostring(name) .. "' is not registered", 0)
    end

    return serialize_scene_def(def, as_name or name)
end

---@param name string
---@return string
function Editor.serialize_new_scene(name)
    return serialize_scene_def(EMPTY_SCENE, name)
end
