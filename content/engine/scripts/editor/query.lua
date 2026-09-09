-- Editor query: the read side of the Editor.* contract.

---@class Editor
Editor = Editor or {}

-- ---------------------------------------------------------------------------------------------
-- Editor field types
-- ---------------------------------------------------------------------------------------------

local ASSET_FACTORY_TYPES = {
    Textures = FieldType.TEXTURE,
    Materials = FieldType.MATERIAL,
    AnimationClips = FieldType.ANIMATION_CLIP,
    AudioClips = FieldType.AUDIO_CLIP,
}

-- Metatable identity -> field type
local usertype_types = nil

local function get_asset_factory_metatable(factory_name)
    local asset_names = __asset_names[factory_name]
    if asset_names == nil or asset_names[1] == nil then
        return nil
    end

    local ok, object = pcall(unwrap_def, _G[factory_name][asset_names[1]])
    if ok and type(object) == "userdata" then
        return getmetatable(object)
    end

    return nil
end

local function ensure_usertype_types()
    if usertype_types ~= nil then
        return
    end

    usertype_types = {
        [getmetatable(Vector2())] = FieldType.VECTOR2,
        [getmetatable(Color())] = FieldType.COLOR,
    }

    for factory_name, field_type in pairs(ASSET_FACTORY_TYPES) do
        local mt = get_asset_factory_metatable(factory_name)
        if mt ~= nil then
            usertype_types[mt] = field_type
        end
    end
end

local function get_field_type_from_value(value)
    local t = type(value)
    if t == "number" then
        return math.type(value) == "integer" and FieldType.INT or FieldType.FLOAT
    elseif t == "boolean" then
        return FieldType.BOOL
    elseif t == "string" then
        return FieldType.STRING
    elseif t == "userdata" then
        ensure_usertype_types()
        local field_type = usertype_types[getmetatable(value)]
        if field_type ~= nil then
            return field_type
        end
    end

    return FieldType.OTHER
end

-- ---------------------------------------------------------------------------------------------
-- C++ components
-- ---------------------------------------------------------------------------------------------

local function get_field_meta(schema, field)
    local types = schema.types
    return types ~= nil and types[field] or nil
end

local function get_schema_field_order(schema)
    if schema.__order ~= nil then
        return schema.__order
    end

    local names = {}
    for field in pairs(schema.getters) do
        names[#names + 1] = field
    end
    table.sort(names)

    return names
end

local function append_schema_fields(fields, component, schema)
    for _, field in ipairs(get_schema_field_order(schema)) do
        local getter = schema.getters[field]
        local setter = schema.setters[field]
        local field_meta = get_field_meta(schema, field)
        local is_hidden = field_meta ~= nil and field_meta.hidden == true
        if getter ~= nil and setter ~= nil and not is_hidden then
            local ok, value = pcall(component[getter], component)

            if ok then
                local row = {}

                if field_meta ~= nil then
                    for key, meta_value in pairs(field_meta) do
                        row[key] = meta_value
                    end
                end

                row.name = field
                row.value = value
                row.type = (field_meta ~= nil and field_meta.type) or get_field_type_from_value(value)

                fields[#fields + 1] = row
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------
-- Lua components
-- ---------------------------------------------------------------------------------------------

local HIDDEN_LUA_FIELDS = {
    entity = true,
    class_name = true,
    priority = true,
    new = true,
}

local EMPTY_ANNOTATIONS = { entries = {}, by_name = {} }

local annotations_cache = setmetatable({}, { __mode = "k" })

local function get_editor_annotations(class)
    if class == nil then
        return EMPTY_ANNOTATIONS
    end

    local cached = annotations_cache[class]
    if cached ~= nil then
        return cached
    end

    local entries = rawget(class, "__editor")
    if type(entries) ~= "table" then
        cached = EMPTY_ANNOTATIONS
    else
        local by_name = {}
        for _, entry in ipairs(entries) do
            by_name[entry.name] = entry
        end
        cached = { entries = entries, by_name = by_name }
    end

    annotations_cache[class] = cached

    return cached
end

---@param class table|nil
---@param key any
---@param value any
---@return boolean
function Editor.is_editable_lua_field(class, key, value)
    if type(key) ~= "string" or key:sub(1, 1) == "_" or HIDDEN_LUA_FIELDS[key] or type(value) == "function" then
        return false
    end

    local annotation = get_editor_annotations(class).by_name[key]

    return annotation == nil or annotation.hidden ~= true
end

---@param class_name string
---@param field string
---@return table|nil
function Editor.get_lua_field_annotation(class_name, field)
    return get_editor_annotations(__component_registry[class_name]).by_name[field]
end

local is_lua_component_field = Editor.is_editable_lua_field

-- A Lua table cannot report the order its keys were assigned in, so recover it from the source:
-- debug.getinfo gives init()'s file and line span, and the fields are whatever it assigns to self.
-- Weak-keyed, so the class tables a hot reload discards take their cached order with them.
local field_order_cache = setmetatable({}, { __mode = "k" })

local function scan_init_field_order(class)
    local init = rawget(class, "init")
    if type(init) ~= "function" then
        return {}
    end

    local info = debug.getinfo(init, "S")
    -- "@path" means a file chunk. Anything else (precompiled, packed, a string chunk) has no
    -- source to read, and the alphabetical fallback takes over.
    if info == nil or info.source:sub(1, 1) ~= "@" then
        return {}
    end

    local file = io.open(info.source:sub(2), "r")
    if file == nil then
        return {}
    end

    local order = {}
    local seen = {}
    local line_no = 0

    for line in file:lines() do
        line_no = line_no + 1
        if line_no > info.linedefined and line_no < info.lastlinedefined then
            local field = line:match("^%s*self%.([%w_]+)%s*=")
            if field ~= nil and not seen[field] then
                seen[field] = true
                order[#order + 1] = field
            end
        end
    end

    file:close()

    return order
end

local function get_declared_field_order(class)
    local cached = field_order_cache[class]
    if cached == nil then
        cached = scan_init_field_order(class)
        field_order_cache[class] = cached
    end

    return cached
end

local function to_lua_field_row(name, value, annotation)
    local row = {}

    if annotation ~= nil then
        for key, meta_value in pairs(annotation) do
            row[key] = meta_value
        end

        if row.min ~= nil and row.max == nil then
            row.max = Math.MAX_FLOAT
        elseif row.max ~= nil and row.min == nil then
            row.min = -Math.MAX_FLOAT
        end
    end

    row.name = name
    row.value = value
    row.type = (annotation ~= nil and annotation.type) or get_field_type_from_value(value)

    return row
end

local function get_lua_component_fields(comp_instance)
    local class = getmetatable(comp_instance)
    local annotations = get_editor_annotations(class)

    local names = {}
    local present = {}

    local function gather(source)
        for key, value in pairs(source) do
            if present[key] == nil and is_lua_component_field(class, key, value) then
                present[key] = true
                names[#names + 1] = key
            end
        end
    end

    gather(comp_instance)

    if class ~= nil then
        gather(class)
    end

    table.sort(names)

    local ordered = {}
    local taken = {}

    for _, entry in ipairs(annotations.entries) do
        local name = entry.name
        if taken[name] == nil and is_lua_component_field(class, name, comp_instance[name]) then
            taken[name] = true
            ordered[#ordered + 1] = name
        end
    end

    if class ~= nil then
        for _, name in ipairs(get_declared_field_order(class)) do
            if present[name] ~= nil and taken[name] == nil then
                taken[name] = true
                ordered[#ordered + 1] = name
            end
        end
    end

    for _, name in ipairs(names) do
        if taken[name] == nil then
            ordered[#ordered + 1] = name
        end
    end

    local fields = {}
    for _, name in ipairs(ordered) do
        fields[#fields + 1] = to_lua_field_row(name, comp_instance[name], annotations.by_name[name])
    end

    return fields
end

-- ---------------------------------------------------------------------------------------------
-- Enums
-- ---------------------------------------------------------------------------------------------

-- Serves both `enum` and `bitmask`.
---@param name string
---@return table|nil
function Editor.get_enum_entries(name)
    local source = _G[name]
    if type(source) ~= "table" then
        return nil
    end

    local entries = {}
    for key, value in pairs(source) do
        if type(key) == "string" and math.type(value) == "integer" then
            entries[#entries + 1] = { name = key, value = value }
        end
    end

    -- Sorting by value recovers declaration order for a C++ enum and bit order for a flag table.
    table.sort(entries, function(a, b)
        if a.value ~= b.value then
            return a.value < b.value
        end

        return a.name < b.name
    end)

    return entries
end

-- ---------------------------------------------------------------------------------------------
-- Assets
-- ---------------------------------------------------------------------------------------------

---@param factory_name string
---@return table|nil
function Editor.get_asset_entries(factory_name)
    local asset_names = __asset_names[factory_name]
    local asset_defs = __asset_defs[factory_name]
    if asset_names == nil or asset_defs == nil then
        return nil
    end

    local entries = {}
    for _, asset_name in ipairs(asset_names) do
        if asset_defs[asset_name] ~= nil then
            entries[#entries + 1] = { name = asset_name }
        end
    end

    return entries
end

---@param factory_name string
---@param object any
---@return string|nil
function Editor.get_asset_name(factory_name, object)
    if type(object) ~= "userdata" or object.get_name == nil then
        return nil
    end

    local asset_defs = __asset_defs[factory_name]
    if asset_defs == nil then
        return nil
    end

    local asset_name = object:get_name()
    if asset_name == nil or asset_defs[asset_name] == nil then
        return nil
    end

    return asset_name
end

---@param factory_name string
---@param asset_name string
---@return any
function Editor.get_asset_ref(factory_name, asset_name)
    local asset_factory_table = _G[factory_name]
    if asset_factory_table == nil then
        return nil
    end

    return asset_factory_table[asset_name]
end

-- ---------------------------------------------------------------------------------------------
-- Definition catalogue
-- ---------------------------------------------------------------------------------------------

local CATALOGUE_REGISTRIES = {
    DefRegistry.SCENES,
    DefRegistry.ENTITIES,
    DefRegistry.TEXTURES,
    DefRegistry.MATERIALS,
    DefRegistry.SHADERS,
    DefRegistry.ANIMATION_CLIPS,
    DefRegistry.AUDIO_CLIPS,
}

local DEFINITION_REGISTRY_TABLE = {
    [DefRegistry.SCENES] = function() return __scene_registry end,
    [DefRegistry.ENTITIES] = function() return __entity_prefab_registry end,
}

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

local function get_definition_names(registry)
    local get_table = DEFINITION_REGISTRY_TABLE[registry]
    local source = get_table ~= nil and get_table() or __asset_defs[registry]

    return source ~= nil and sorted_string_keys(source) or {}
end

local function count_defs_per_file()
    local counts = {}
    for _, by_name in pairs(__def_sources) do
        for _, path in pairs(by_name) do
            counts[path] = (counts[path] or 0) + 1
        end
    end

    return counts
end

---@return table
function Editor.get_definitions()
    local def_count_by_file = count_defs_per_file()
    local rows = {}

    for _, registry in ipairs(CATALOGUE_REGISTRIES) do
        for _, name in ipairs(get_definition_names(registry)) do
            local file = __get_def_source(registry, name)
            rows[#rows + 1] = {
                registry = registry,
                name = name,
                file = file,
                read_only = file == nil or def_count_by_file[file] ~= 1,
            }
        end
    end

    return rows
end

---@param factory_name string
---@param asset_name string
---@return any
function Editor.build_asset(factory_name, asset_name)
    local asset_factory_table = _G[factory_name]
    if asset_factory_table == nil then
        return nil
    end

    return unwrap_def(asset_factory_table[asset_name])
end

-- ---------------------------------------------------------------------------------------------
-- Definition documents
-- ---------------------------------------------------------------------------------------------

local function get_definition_table(registry, name)
    local get_table = DEFINITION_REGISTRY_TABLE[registry]
    if get_table ~= nil then
        return get_table()[name]
    end

    local asset_defs = __asset_defs[registry]
    return asset_defs ~= nil and asset_defs[name] or nil
end

-- A deferred reference (Textures.X, Shaders.X) is a table whose metatable names its registry.
local function is_asset_ref(value)
    local mt = getmetatable(value)
    return mt ~= nil and mt.__registry ~= nil
end

local function describe_item(value)
    if type(value) ~= "table" or is_asset_ref(value) then
        return tostring(value)
    end

    return nil
end

local function describe_table(value)
    local parts = {}
    for _, item in ipairs(value) do
        local text = describe_item(item)
        if text == nil then
            return #value .. " items"
        end

        parts[#parts + 1] = text
    end

    if #parts > 0 then
        return table.concat(parts, ", ")
    end

    return "{" .. table.concat(sorted_string_keys(value), ", ") .. "}"
end

local function get_field_meta(schema, field)
    return schema ~= nil and schema.types ~= nil and schema.types[field] or nil
end

local function to_definition_field(key, value, field_meta)
    if value == None then
        value = nil
    end

    if type(value) == "table" then
        if is_asset_ref(value) then
            return { name = key, value = value, type = FieldType.OTHER }
        end

        return { name = key, value = describe_table(value), type = FieldType.OTHER }
    end

    local row = {}

    if field_meta ~= nil then
        for meta_key, meta_value in pairs(field_meta) do
            row[meta_key] = meta_value
        end
    end

    row.name = key
    row.value = value
    row.type = (field_meta ~= nil and field_meta.type) or get_field_type_from_value(value)

    return row
end

local function to_definition_fields(source, schema)
    local fields = {}
    for _, key in ipairs(sorted_string_keys(source)) do
        fields[#fields + 1] = to_definition_field(key, source[key], get_field_meta(schema, key))
    end

    return fields
end

-- An array is a value; a map is a section, which is what makes a definition read as its parts.
local function is_section(value)
    return type(value) == "table" and not is_asset_ref(value) and #value == 0
end

-- ---------------------------------------------------------------------------------------------
-- Prefab documents
-- ---------------------------------------------------------------------------------------------

-- Cleared for free by a hot reload, which re-runs this file and with it the local.
local prefab_sections_cache = {}

-- A prefab declares a subset of what it produces: every entity has a transform, a component
-- constructor can add siblings, and a Lua component only has fields once init() has run. Spawning
-- one and reading it back is the only source for that, and it is what makes this panel agree with
-- the Hierarchy field for field. The probe resolves synchronously and never enters play.
local function is_removable_prefab_section(def, section)
    if section.is_lua then
        for _, class_name in ipairs(def[PrefabKey.LUA_COMPONENTS] or {}) do
            if class_name == section.name then
                return true
            end
        end

        return false
    end

    return section.name ~= TransformKey.SECTION and def[section.name] ~= nil
end

local function build_prefab_sections(name, def)
    local probe = EntitySpawner.spawn_entity(name)
    if probe == nil then
        return {}
    end

    local sections = Editor.get_components(probe:get_id()) or {}
    EntitySpawner.destroy_entity(probe)

    for _, section in ipairs(sections) do
        section.removable = is_removable_prefab_section(def, section)
    end

    local schemas = __component_schemas
    local root_fields = {}

    for _, key in ipairs(sorted_string_keys(def)) do
        if key ~= PrefabKey.LUA_COMPONENTS and key ~= PrefabKey.LUA_FIELDS and schemas[key] == nil then
            root_fields[#root_fields + 1] = to_definition_field(key, def[key], nil)
        end
    end

    if #root_fields > 0 then
        table.insert(sections, 1, { name = name, is_lua = false, fields = root_fields })
    end

    return sections
end

---@param name string
function Editor.invalidate_prefab_sections(name)
    prefab_sections_cache[name] = nil
end

local function get_prefab_sections(name, def)
    local cached = prefab_sections_cache[name]
    if cached == nil then
        cached = build_prefab_sections(name, def)
        prefab_sections_cache[name] = cached
    end

    return cached
end

local function get_asset_sections(name, def)
    local sections = {}
    local root_fields = {}

    for _, key in ipairs(sorted_string_keys(def)) do
        if is_section(def[key]) then
            sections[#sections + 1] = { name = key, is_lua = false, fields = to_definition_fields(def[key], nil) }
        else
            root_fields[#root_fields + 1] = to_definition_field(key, def[key], nil)
        end
    end

    if #root_fields > 0 then
        table.insert(sections, 1, { name = name, is_lua = false, fields = root_fields })
    end

    return sections
end

---@param registry string
---@param name string
---@return table|nil
function Editor.get_definition_sections(registry, name)
    local def = get_definition_table(registry, name)
    if type(def) ~= "table" then
        return nil
    end

    if registry == DefRegistry.ENTITIES then
        return get_prefab_sections(name, def)
    end

    return get_asset_sections(name, def)
end

-- ---------------------------------------------------------------------------------------------
-- Public query
-- ---------------------------------------------------------------------------------------------

---@param entity_id integer
---@return table|nil
function Editor.get_components(entity_id)
    local entity = EntitySpawner.get_entity(entity_id)
    if not entity:is_valid() then
        return nil
    end

    local schemas = __component_schemas
    local inst = __scene_instance_by_entity_id[entity_id]
    local out = {}

    local function mark_overridden(section)
        for _, row in ipairs(section.fields) do
            if Editor.is_instance_field_overridden(inst, section.name, row.name, section.is_lua) then
                row.overridden = true
            end
        end

        return section
    end

    for _, key in ipairs(schemas.__order) do
        local schema = schemas[key]
        local component = entity[schema.get](entity)
        if component ~= nil then
            local fields = {}
            append_schema_fields(fields, component, schema)
            out[#out + 1] = mark_overridden({
                name = key,
                is_lua = false,
                fields = fields
            })
        end
    end

    for _, instance in ipairs(entity:get_lua_components()) do
        out[#out + 1] = mark_overridden({
            name = instance.class_name or "?",
            is_lua = true,
            fields = get_lua_component_fields(instance),
        })
    end

    return out
end
