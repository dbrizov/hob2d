-- DefineScene: a level as data.

_G.__scene_registry = {}
_G.__scene_instance_by_entity_id = {}

on_entity_destroyed(function(entity_id)
    _G.__scene_instance_by_entity_id[entity_id] = nil
end)

on_entities_cleared(function()
    _G.__scene_instance_by_entity_id = {}
end)

function _G.__clear_scene_defs()
    _G.__scene_registry = {}
end

function _G.__scene_name_from_file(path)
    return __def_name_from_file(path, FileExtension.SCENE)
end

---@class DefineScene
_G.DefineScene = setmetatable({}, {
    __newindex = function(_, name, def)
        if type(def) ~= "table" or type(def.entities) ~= "table" then
            Log.error("DefineScene." .. tostring(name) .. " must be assigned a table with an 'entities' list")
            return
        end

        if not __record_def_source(DefRegistry.SCENES, name) then
            return
        end

        _G.__scene_registry[name] = def
    end,
    __index = function(_, name)
        return _G.__scene_registry[name]
    end,
})

-- `Scenes.Foo` evaluates to the scene name string `"Foo"`.
---@class Scenes
_G.Scenes = setmetatable({}, {
    __index = function(_, name) return name end,
})

_G.Scene = {}

---@param entity Entity
---@param cpp_overrides table
function Scene.apply_cpp_overrides(entity, cpp_overrides)
    local schemas = _G.__component_schemas
    local call_setter = _G.__call_component_setter

    for _, key in ipairs(schemas.__order) do
        local section = cpp_overrides[key]
        if section ~= nil then
            local schema = schemas[key]
            local component = entity[schema.get](entity)
            if component == nil then
                Log.error("Scene override: entity has no '" .. key .. "' component")
            else
                for field, value in pairs(section) do
                    local setter = schema.setters[field]
                    if setter == nil then
                        Log.error("Scene override: unknown field '" .. tostring(field) .. "' for '" .. key .. "'")
                    else
                        call_setter(component, setter, unwrap_def(value))
                    end
                end
            end
        end
    end
end

---@param entity Entity
---@param lua_overrides table
function Scene.apply_lua_overrides(entity, lua_overrides)
    __apply_lua_fields(entity, lua_overrides, "Scene override")
end

local function apply_overrides(entity, inst)
    local cpp_overrides = inst[SceneKey.CPP_OVERRIDES]
    if cpp_overrides ~= nil then
        Scene.apply_cpp_overrides(entity, cpp_overrides)
    end

    local lua_overrides = inst[SceneKey.LUA_OVERRIDES]
    if lua_overrides ~= nil then
        Scene.apply_lua_overrides(entity, lua_overrides)
    end
end

local NO_POSE = {}

---@param inst table
---@return Entity|nil
function Scene.spawn_instance(inst)
    local pose = inst[SceneKey.POSE_OVERRIDES] or NO_POSE
    local entity = EntitySpawner.spawn_entity(inst.prefab, pose[TransformKey.POSITION],
        pose[TransformKey.ROTATION_DEG], pose[TransformKey.SCALE])

    if entity == nil then
        return nil
    end

    apply_overrides(entity, inst)
    _G.__scene_instance_by_entity_id[entity:get_id()] = inst

    return entity
end

---@param name string
---@return { index: integer, entity: Entity }[]|nil
function Scene.load(name)
    local def = _G.__scene_registry[name]
    if def == nil then
        Log.error("Scene.load: scene '" .. tostring(name) .. "' is not registered")
        return nil
    end

    local spawned = {}
    for index, inst in ipairs(def.entities) do
        local entity = Scene.spawn_instance(inst)
        if entity ~= nil then
            spawned[#spawned + 1] = { index = index, entity = entity }
        end
    end

    return spawned
end

function _G.__reapply_scene_overrides_to_spawned_entities()
    local live = {}

    EntitySpawner.for_each_entity(function(entity)
        local id = entity:get_id()
        local inst = _G.__scene_instance_by_entity_id[id]
        if inst ~= nil then
            live[id] = inst
            apply_overrides(entity, inst)
        end
    end)

    _G.__scene_instance_by_entity_id = live
end
