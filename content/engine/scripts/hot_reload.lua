-- Lua hot reload. Running this file IS the reload: it rebuilds every component
-- class from the current source files and re-points all live instances at the
-- rebuilt classes, preserving per-instance state.
--
-- This file is imperative top-level code, so it is excluded from the normal
-- bootstrap load (see bootstrap.lua) and is only executed when C++ explicitly
-- runs it via LuaScriptSystem::reload().
--
-- State survives for free: per-instance state lives as raw fields on the instance
-- table, while methods/defaults live on the class table reached via the metatable.
-- We only swap the metatable; we do NOT re-run init().

-- 1. Wipe registries so definitions rebuild cleanly.
_G.__component_registry = {}
_G.__component_pending = {}

-- 2a. Re-run the same definition files bootstrap uses, then re-finalize.
__load_project_definitions()

-- 2b. Drop cached factory objects (materials, animation clips, ...) so redefined defs
--     rebuild from their updated config on next unwrap.
__clear_factory_caches()

-- 2c. Re-warm shaders so pipelines stay built (rebuilds hit the strong shader cache) instead of
--     lazily recompiling the first time a reloaded material references them.
__warmup_shaders()

-- 3. Re-point every live component instance at its rebuilt class.
--    Optional on_hot_reload() lets a component migrate state when a field's shape changed.
--    Weak-keyed set, so iterate keys: `for inst in pairs(...)`.
for inst in pairs(_G.__live_component_instances) do
    local class = _G.__component_registry[inst.class_name]
    if class then
        setmetatable(inst, class)
        if type(inst.on_hot_reload) == "function" then
            inst:on_hot_reload()
        end
    end
end

-- 4. Push changed prefab data onto already-spawned entities (setters only; no re-add).
__reapply_prefabs_to_spawned_entities()

-- 5. Force a full GC so the previous generation of factory objects (materials, etc.) — now only
--    held by unreachable Lua userdata — releases its C++ refs immediately instead of lingering.
collectgarbage("collect")
