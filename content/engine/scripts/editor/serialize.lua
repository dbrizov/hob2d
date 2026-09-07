-- Editor serialize: the definition-table-to-Lua-source side of the Editor.* contract.

---@class Editor
_G.Editor = _G.Editor or {}

local INDENT = "    "
local LINE_BUDGET = 120
local FLOAT_FORMAT = "%.7g"

local SHAPES = {
    [FieldType.VECTOR2] = { ctor = "Vector2", fields = { "x", "y" } },
    [FieldType.COLOR] = { ctor = "Color", fields = { "r", "g", "b", "a" } },
    [FieldType.AABB] = {
        ctor = "AABB",
        fields = { "center", "extents" },
        types = { center = { type = FieldType.VECTOR2 }, extents = { type = FieldType.VECTOR2 } },
    },
    [FieldType.CAPSULE] = {
        ctor = "Capsule",
        fields = { "center_a", "center_b", "radius" },
        types = { center_a = { type = FieldType.VECTOR2 }, center_b = { type = FieldType.VECTOR2 } },
    },
    [FieldType.CIRCLE] = {
        ctor = "Circle",
        fields = { "center", "radius" },
        types = { center = { type = FieldType.VECTOR2 } },
    },
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
    error("Editor.serialize: " .. path .. " " .. message, 0)
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

local function get_prefab_section(prefab_name, key)
    local prefab = _G.__entity_prefab_registry[prefab_name]
    if prefab == nil then
        return nil
    end

    local section = prefab[key]
    if type(section) ~= "table" then
        return nil
    end

    return section
end

local function get_lua_baseline(prefab_name, class_name, field)
    local sections = Editor.get_definition_sections(DefRegistry.ENTITIES, prefab_name)
    for _, section in ipairs(sections or {}) do
        if section.is_lua and section.name == class_name then
            for _, row in ipairs(section.fields) do
                if row.name == field then
                    return row.value
                end
            end
        end
    end

    return nil
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
    local types = shape.types or {}

    local args = {}
    for index, field in ipairs(shape.fields) do
        args[index] = serialize_value(value[field], types[field], path .. "." .. field, depth, 0)
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

local function get_pose_baseline(prefab_name, field)
    local transform = get_prefab_section(prefab_name, TransformKey.SECTION)
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

local function serialize_pose(pose, path, depth, prefix, prefab_name)
    local parts = {}
    for _, field in ipairs(ordered_keys(pose, POSE_ORDER)) do
        local baseline = get_pose_baseline(prefab_name, field)
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

local function serialize_cpp_sections(sections_by_key, path, depth, prefix, prefab_name)
    local schemas = _G.__component_schemas

    local parts = {}
    for _, key in ipairs(ordered_keys(sections_by_key, schemas.__order)) do
        local schema = schemas[key]

        local has_elidable_fields = schema ~= nil and schema.map_setter == nil
        local prefab_section = has_elidable_fields and get_prefab_section(prefab_name, key) or nil
        local defaults = has_elidable_fields and __get_component_defaults(key) or nil

        local section = serialize_cpp_section(sections_by_key[key], schema, path .. "." .. key,
            depth + 1, field_prefix(key), prefab_section, defaults)
        if section ~= nil then
            parts[#parts + 1] = key .. " = " .. section
        end
    end

    return wrap_fields(parts, depth, prefix)
end

local function serialize_lua_sections(sections_by_class, path, depth, prefix, prefab_name)
    local parts = {}
    for _, class_name in ipairs(sorted_keys(sections_by_class)) do
        local section_path = path .. "." .. class_name
        local section = sections_by_class[class_name]
        if type(section) ~= "table" then
            fail(section_path, "is not a table")
        end

        local fields = {}
        for _, field in ipairs(sorted_keys(section)) do
            local baseline = prefab_name ~= nil and get_lua_baseline(prefab_name, class_name, field) or nil
            if baseline == nil or not is_baseline(section[field], baseline) then
                local field_meta = Editor.get_lua_field_annotation(class_name, field)
                fields[#fields + 1] = field .. " = " ..
                    serialize_value(section[field], field_meta, section_path .. "." .. field, depth + 2,
                        field_prefix(field))
            end
        end

        local text = wrap_fields(fields, depth + 1, field_prefix(class_name))
        if text ~= nil then
            parts[#parts + 1] = class_name .. " = " .. text
        end
    end

    return wrap_fields(parts, depth, prefix)
end

local function append_overrides(parts, inst, key, serialize_section, path, depth, prefab_name)
    local overrides = inst[key]
    if type(overrides) ~= "table" then
        return
    end

    local section = serialize_section(overrides, path .. "." .. key, depth, field_prefix(key), prefab_name)
    if section ~= nil then
        parts[#parts + 1] = key .. " = " .. section
    end
end

local function serialize_instance(inst, path, depth, prefix)
    if type(inst.prefab) ~= "string" then
        fail(path, "does not name a prefab")
    end

    local prefab_name = inst.prefab
    local parts = { "prefab = " .. DefRegistry.ENTITIES .. "." .. prefab_name }

    append_overrides(parts, inst, SceneKey.POSE_OVERRIDES, serialize_pose, path, depth + 1, prefab_name)
    append_overrides(parts, inst, SceneKey.CPP_OVERRIDES, serialize_cpp_sections, path, depth + 1, prefab_name)
    append_overrides(parts, inst, SceneKey.LUA_OVERRIDES, serialize_lua_sections, path, depth + 1, prefab_name)

    return wrap_fields(parts, depth, prefix)
end

local EMPTY_SCENE = { entities = {} }

local function serialize_scene_def(def, name)
    local path = "DefineScene." .. name
    local lines = { path .. " = {" }

    if #def.entities == 0 then
        lines[#lines + 1] = INDENT .. "entities = {},"
    else
        lines[#lines + 1] = INDENT .. "entities = {"

        for index, inst in ipairs(def.entities) do
            lines[#lines + 1] = indent_of(2) ..
                serialize_instance(inst, path .. ".entities[" .. index .. "]", 2, 0) .. ","
        end

        lines[#lines + 1] = INDENT .. "},"
    end

    lines[#lines + 1] = "}"

    return table.concat(lines, "\n") .. "\n"
end

local PREFAB_ROOT_ORDER = { PrefabKey.TICKING, "name" }

local function serialize_lua_component_list(class_names, path, depth, prefix)
    local parts = {}
    for index, class_name in ipairs(class_names) do
        if type(class_name) ~= "string" then
            fail(path .. "[" .. index .. "]", "does not name a component class")
        end

        parts[#parts + 1] = DefRegistry.COMPONENTS .. "." .. class_name
    end

    return wrap_fields(parts, depth, prefix) or "{}"
end

local function serialize_prefab_section(section, key, path, depth, prefix)
    local schema = _G.__component_schemas[key]
    if schema == nil or schema.map_setter then
        return serialize_value(section, nil, path, depth, prefix)
    end

    return serialize_cpp_section(section, schema, path, depth, prefix, nil, __get_component_defaults(key)) or "{}"
end

local EMPTY_PREFAB = {}

local function serialize_prefab_def(def, name)
    local path = "DefineEntity." .. name
    local lines = { path .. " = {" }
    local emitted = {}

    local function append(key, text)
        lines[#lines + 1] = INDENT .. key .. " = " .. text .. ","
        emitted[key] = true
    end

    local function serialize_verbatim(key)
        return serialize_value(def[key], nil, path .. "." .. key, 1, field_prefix(key))
    end

    for _, key in ipairs(PREFAB_ROOT_ORDER) do
        if def[key] ~= nil then
            append(key, serialize_verbatim(key))
        end
    end

    for _, key in ipairs(_G.__component_schemas.__order) do
        local section = def[key]
        if section ~= nil then
            append(key, serialize_prefab_section(section, key, path .. "." .. key, 1, field_prefix(key)))
        end
    end

    local lua_components = def[PrefabKey.LUA_COMPONENTS]
    if lua_components ~= nil then
        local key = PrefabKey.LUA_COMPONENTS
        append(key, serialize_lua_component_list(lua_components, path .. "." .. key, 1, field_prefix(key)))
    end

    local lua_fields = def[PrefabKey.LUA_FIELDS]
    if lua_fields ~= nil then
        local key = PrefabKey.LUA_FIELDS
        append(key, serialize_lua_sections(lua_fields, path .. "." .. key, 1, field_prefix(key), nil) or "{}")
    end

    for _, key in ipairs(sorted_keys(def, emitted)) do
        append(key, serialize_verbatim(key))
    end

    if #lines == 1 then
        return path .. " = {}\n"
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

---@param name string
---@param as_name string|nil the name to declare it under; defaults to `name`
---@return string
function Editor.serialize_prefab(name, as_name)
    local def = _G.__entity_prefab_registry[name]
    if def == nil then
        error("Editor.serialize_prefab: prefab '" .. tostring(name) .. "' is not registered", 0)
    end

    return serialize_prefab_def(def, as_name or name)
end

---@param name string
---@return string
function Editor.serialize_new_prefab(name)
    return serialize_prefab_def(EMPTY_PREFAB, name)
end
