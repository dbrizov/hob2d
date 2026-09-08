_G.Editor = _G.Editor or {}

local scene_state = _G.__editor_scene_state or {
    name = nil,
    is_dirty = false,
    instance_count = 0,
    next_instance_id = 1,
    instance_id_by_index = {},
    instance_id_by_entity_id = {},
    entity_id_by_instance_id = {},
    entity_def_by_instance_id = {},
}
_G.__editor_scene_state = scene_state

local function alloc_instance_id()
    local instance_id = scene_state.next_instance_id
    scene_state.next_instance_id = instance_id + 1

    return instance_id
end

---@return string[]
function Editor.get_scene_names()
    local names = {}
    for name in pairs(_G.__scene_registry) do
        names[#names + 1] = name
    end

    table.sort(names)

    return names
end

---@return string|nil
function Editor.get_current_scene()
    return scene_state.name
end

---@return boolean
function Editor.is_scene_dirty()
    return scene_state.is_dirty
end

function Editor.mark_scene_dirty()
    scene_state.is_dirty = true
end

function Editor.mark_scene_saved()
    scene_state.is_dirty = false
end

---@param name string
---@return string|nil
function Editor.get_scene_file(name)
    return __get_def_source(DefRegistry.SCENES, name)
end

---@param name string
---@return string|nil reason, nil when the scene can be written back to its file
function Editor.get_scene_save_error(name)
    return Editor.get_definition_save_error(DefRegistry.SCENES, name, _G.__scene_registry[name] ~= nil)
end

---@param path string
---@return string|nil
function Editor.get_scene_name_for_file(path)
    return __scene_name_from_file(path)
end

---@param path string
---@return string|nil reason, nil when a scene may be created at this path
function Editor.get_scene_create_error(path)
    local name = __scene_name_from_file(path)
    local is_registered = name ~= nil and _G.__scene_registry[name] ~= nil

    return Editor.get_definition_create_error(DefRegistry.SCENES, path, FileExtension.SCENE, is_registered)
end

local function get_or_create(owner, key)
    local existing = owner[key]
    if existing == nil then
        existing = {}
        owner[key] = existing
    end

    return existing
end

local function set_pose_field(inst, field, value)
    if field ~= TransformKey.POSITION and field ~= TransformKey.ROTATION and field ~= TransformKey.SCALE then
        return nil
    end

    local pose = get_or_create(inst, SceneKey.POSE_OVERRIDES)

    if field == TransformKey.ROTATION then
        local rotation_deg = value * Math.RAD_TO_DEG
        local changed = pose[TransformKey.ROTATION_DEG] ~= rotation_deg
        pose[TransformKey.ROTATION_DEG] = rotation_deg
        return changed
    end

    local changed = pose[field] ~= value
    pose[field] = value

    return changed
end

local function set_pose_field_3d(inst, field, value)
    if field ~= TransformKey.POSITION and field ~= TransformKey.ROTATION and field ~= TransformKey.SCALE then
        return nil
    end

    local pose = get_or_create(inst, SceneKey.POSE_OVERRIDES)
    local pose_field = field == TransformKey.ROTATION and TransformKey.ROTATION_DEG or field
    local changed = pose[pose_field] ~= value
    pose[pose_field] = value

    return changed
end

local function set_override_field(inst, overrides_key, component_key, field, value)
    local section = get_or_create(get_or_create(inst, overrides_key), component_key)

    local stored = value
    if stored == nil then
        stored = None
    end

    local changed = section[field] ~= stored
    section[field] = stored

    return changed
end

---@return string|nil
function Editor.get_scene_space()
    local name = scene_state.name
    return name ~= nil and Scene.get_space(name) or nil
end

function Editor.set_instance_field(entity_id, component_key, field, value)
    local instance_id = scene_state.instance_id_by_entity_id[entity_id]
    local inst = instance_id and scene_state.entity_def_by_instance_id[instance_id]
    if inst == nil then
        return
    end

    local changed
    if component_key == TransformKey.SECTION then
        changed = set_pose_field(inst, field, value)
    elseif component_key == TransformKey3D.SECTION then
        changed = set_pose_field_3d(inst, field, value)
    end

    if changed == nil then
        changed = set_override_field(inst, SceneKey.CPP_OVERRIDES, component_key, field, value)
    end

    if changed then
        Editor.mark_scene_dirty()
    end
end

function Editor.set_lua_instance_field(entity_id, class_name, field, value)
    local instance_id = scene_state.instance_id_by_entity_id[entity_id]
    local inst = instance_id and scene_state.entity_def_by_instance_id[instance_id]
    if inst == nil then
        return
    end

    if set_override_field(inst, SceneKey.LUA_OVERRIDES, class_name, field, value) then
        Editor.mark_scene_dirty()
    end
end

local function get_instance_of_entity(entity_id)
    local instance_id = scene_state.instance_id_by_entity_id[entity_id]
    return instance_id and scene_state.entity_def_by_instance_id[instance_id] or nil
end

---@param entity_id integer
---@param component_key string
---@param field string
---@param is_lua boolean
---@return any the override in the setter's domain (radians for rotation); nil when not overridden
function Editor.get_instance_override(entity_id, component_key, field, is_lua)
    local inst = get_instance_of_entity(entity_id)
    if not Editor.is_instance_field_overridden(inst, component_key, field, is_lua) then
        return nil
    end

    if is_lua then
        return inst[SceneKey.LUA_OVERRIDES][component_key][field]
    end

    if component_key == TransformKey.SECTION or component_key == TransformKey3D.SECTION then
        local pose = inst[SceneKey.POSE_OVERRIDES]
        if field == TransformKey.ROTATION and pose ~= nil and pose[TransformKey.ROTATION_DEG] ~= nil then
            local rotation_deg = pose[TransformKey.ROTATION_DEG]
            return component_key == TransformKey.SECTION and rotation_deg * Math.DEG_TO_RAD or rotation_deg
        end

        if pose ~= nil and pose[field] ~= nil then
            return pose[field]
        end
    end

    return inst[SceneKey.CPP_OVERRIDES][component_key][field]
end

local function remove_section_field(inst, overrides_key, component_key, field)
    local overrides = inst[overrides_key]
    local section = overrides ~= nil and overrides[component_key] or nil
    if section == nil or section[field] == nil then
        return false
    end

    section[field] = nil
    if next(section) == nil then
        overrides[component_key] = nil
    end
    if next(overrides) == nil then
        inst[overrides_key] = nil
    end

    return true
end

---@param entity_id integer
---@param component_key string
---@param field string
---@param is_lua boolean
---@return boolean whether an override was removed; the live entity then takes the prefab's value
function Editor.clear_instance_field(entity_id, component_key, field, is_lua)
    local inst = get_instance_of_entity(entity_id)
    if inst == nil then
        return false
    end

    local removed = false
    if is_lua then
        removed = remove_section_field(inst, SceneKey.LUA_OVERRIDES, component_key, field)
    else
        if component_key == TransformKey.SECTION or component_key == TransformKey3D.SECTION then
            local pose = inst[SceneKey.POSE_OVERRIDES]
            local pose_field = field == TransformKey.ROTATION and TransformKey.ROTATION_DEG or field
            if pose ~= nil and pose[pose_field] ~= nil then
                pose[pose_field] = nil
                if next(pose) == nil then
                    inst[SceneKey.POSE_OVERRIDES] = nil
                end
                removed = true
            end
        end

        if not removed then
            removed = remove_section_field(inst, SceneKey.CPP_OVERRIDES, component_key, field)
        end
    end

    if not removed then
        return false
    end

    Editor.mark_scene_dirty()
    Editor.apply_prefab_field_to_entity(entity_id, component_key, field, is_lua)

    return true
end

function Editor.clear_world()
    scene_state.instance_count = 0
    scene_state.instance_id_by_index = {}
    scene_state.instance_id_by_entity_id = {}
    scene_state.entity_id_by_instance_id = {}
    scene_state.entity_def_by_instance_id = {}

    EntitySpawner.clear()
end

function Editor.load_scene()
    local name = scene_state.name
    if name == nil then
        return
    end

    local scene_def = _G.__scene_registry[name]
    if scene_def == nil then
        Log.error("Editor.load_scene: scene '" .. tostring(name) .. "' is no longer registered")
        scene_state.name = nil
        return
    end

    local spawned = Scene.load(name)
    if spawned == nil then
        return
    end

    scene_state.instance_count = #scene_def.entities

    for _, entry in ipairs(spawned) do
        local inst = scene_def.entities[entry.index]
        local instance_id = inst.__instance_id or alloc_instance_id()
        local entity_id = entry.entity:get_id()

        inst.__instance_id = instance_id
        scene_state.instance_id_by_index[entry.index] = instance_id
        scene_state.instance_id_by_entity_id[entity_id] = instance_id
        scene_state.entity_id_by_instance_id[instance_id] = entity_id
        scene_state.entity_def_by_instance_id[instance_id] = inst
    end
end

---@param name string
---@return boolean
function Editor.open_scene(name)
    if _G.__scene_registry[name] == nil then
        Log.error("Editor.open_scene: scene '" .. tostring(name) .. "' is not registered")
        scene_state.name = nil
        return false
    end

    Editor.clear_world()
    scene_state.name = name
    scene_state.is_dirty = false
    Editor.load_scene()

    return true
end

function Editor.reload_scene()
    Editor.clear_world()
    Editor.load_scene()
end

---@param instance_id integer
---@return integer|nil
function Editor.get_entity_id(instance_id)
    return scene_state.entity_id_by_instance_id[instance_id]
end

---@param entity_id integer
---@return integer|nil
function Editor.get_instance_id(entity_id)
    return scene_state.instance_id_by_entity_id[entity_id]
end

---@param instance_id integer
---@return table|nil
function Editor.get_instance_def(instance_id)
    return scene_state.entity_def_by_instance_id[instance_id]
end

local function get_open_scene_def()
    local name = scene_state.name
    return name ~= nil and _G.__scene_registry[name] or nil
end

local function reindex_instances(scene_def)
    local by_index = {}
    for index, inst in ipairs(scene_def.entities) do
        by_index[index] = inst.__instance_id
    end

    scene_state.instance_id_by_index = by_index
    scene_state.instance_count = #scene_def.entities
end

local function find_instance_index(scene_def, inst)
    for index, candidate in ipairs(scene_def.entities) do
        if candidate == inst then
            return index
        end
    end

    return nil
end

-- Destroying a spawn request that has not resolved yet fires no destroyed handler, so the
-- scene instance map cannot be left to the callback that normally clears it.
local function destroy_instance_entity(instance_id)
    local entity_id = scene_state.entity_id_by_instance_id[instance_id]
    if entity_id == nil then
        return
    end

    _G.__scene_instance_by_entity_id[entity_id] = nil
    EntitySpawner.destroy_entity(EntitySpawner.get_entity(entity_id))
    scene_state.instance_id_by_entity_id[entity_id] = nil
    scene_state.entity_id_by_instance_id[instance_id] = nil
end

local function bind_instance_entity(instance_id, entity_id)
    scene_state.instance_id_by_entity_id[entity_id] = instance_id
    scene_state.entity_id_by_instance_id[instance_id] = entity_id
end

---@param prefab_name string
---@param position Vector2|Vector3
---@return table
function Editor.create_instance_def(prefab_name, position)
    return {
        prefab = prefab_name,
        [SceneKey.POSE_OVERRIDES] = { [TransformKey.POSITION] = position },
    }
end

---@param instance_id integer
---@return table|nil
function Editor.copy_instance_def(instance_id)
    local inst = scene_state.entity_def_by_instance_id[instance_id]
    if inst == nil then
        return nil
    end

    return Editor.copy_def_table(inst)
end

---@param instance_id integer
---@param prefab_name string
---@return table|nil a copy of the instance def pointing at the prefab, with its overrides dropped
function Editor.repoint_instance_def(instance_id, prefab_name)
    local copy = Editor.copy_instance_def(instance_id)
    if copy == nil then
        return nil
    end

    copy.prefab = prefab_name
    copy[SceneKey.CPP_OVERRIDES] = nil
    copy[SceneKey.LUA_OVERRIDES] = nil

    return copy
end

---@param instance_id integer
---@return integer|nil
function Editor.get_instance_index(instance_id)
    local scene_def = get_open_scene_def()
    local inst = scene_state.entity_def_by_instance_id[instance_id]
    if scene_def == nil or inst == nil then
        return nil
    end

    return find_instance_index(scene_def, inst)
end

---@param inst table
---@param index integer|nil where to insert it; appends when omitted
---@return integer|nil instance_id
function Editor.add_instance(inst, index)
    local scene_def = get_open_scene_def()
    if scene_def == nil then
        Log.error("Editor.add_instance: no scene is open")
        return nil
    end

    local entity = Scene.spawn_instance(inst, __resolve_def_space(scene_def))
    if entity == nil then
        return nil
    end

    local instance_id = inst.__instance_id or alloc_instance_id()

    inst.__instance_id = instance_id
    table.insert(scene_def.entities, index or (#scene_def.entities + 1), inst)

    bind_instance_entity(instance_id, entity:get_id())
    scene_state.entity_def_by_instance_id[instance_id] = inst

    reindex_instances(scene_def)
    Editor.mark_scene_dirty()

    return instance_id
end

---@param instance_id integer
---@return integer|nil the index it was removed from
function Editor.remove_instance(instance_id)
    local scene_def = get_open_scene_def()
    local inst = scene_state.entity_def_by_instance_id[instance_id]
    if scene_def == nil or inst == nil then
        return nil
    end

    local index = find_instance_index(scene_def, inst)
    if index == nil then
        return nil
    end

    table.remove(scene_def.entities, index)

    destroy_instance_entity(instance_id)
    scene_state.entity_def_by_instance_id[instance_id] = nil

    reindex_instances(scene_def)
    Editor.mark_scene_dirty()

    return index
end

---@param prefab_name string
function Editor.respawn_prefab_instances(prefab_name)
    local scene_def = get_open_scene_def()
    local scene_space = scene_def and __resolve_def_space(scene_def) or SpaceKey.SPACE_2D
    for instance_id, inst in pairs(scene_state.entity_def_by_instance_id) do
        if inst.prefab == prefab_name then
            destroy_instance_entity(instance_id)

            local entity = Scene.spawn_instance(inst, scene_space)
            if entity ~= nil then
                bind_instance_entity(instance_id, entity:get_id())
            end
        end
    end
end

-- Re-points the orphaned defs at the reloaded document and pushes them to the live entities;
-- false means the caller must respawn instead.
---@return boolean
function Editor.rebind_instance_defs()
    local name = scene_state.name
    if name == nil then
        return true
    end

    local scene_def = _G.__scene_registry[name]
    if scene_def == nil then
        return false
    end

    local fresh = scene_def.entities
    if #fresh ~= scene_state.instance_count then
        return false
    end

    for index, inst in ipairs(fresh) do
        local instance_id = scene_state.instance_id_by_index[index]
        local old = instance_id and scene_state.entity_def_by_instance_id[instance_id]
        if old == nil or old.prefab ~= inst.prefab then
            return false
        end
    end

    for index, inst in ipairs(fresh) do
        local instance_id = scene_state.instance_id_by_index[index]
        local old = scene_state.entity_def_by_instance_id[instance_id]

        if scene_state.is_dirty then
            inst[SceneKey.POSE_OVERRIDES] = old[SceneKey.POSE_OVERRIDES]
            inst[SceneKey.CPP_OVERRIDES] = old[SceneKey.CPP_OVERRIDES]
            inst[SceneKey.LUA_OVERRIDES] = old[SceneKey.LUA_OVERRIDES]
        end

        inst.__instance_id = instance_id
        scene_state.entity_def_by_instance_id[instance_id] = inst

        local entity_id = scene_state.entity_id_by_instance_id[instance_id]
        if entity_id then
            _G.__scene_instance_by_entity_id[entity_id] = inst
        end
    end

    __reapply_scene_overrides_to_spawned_entities()

    return true
end
