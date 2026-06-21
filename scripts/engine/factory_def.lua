-- Generic factory-type registry.
--
-- Drives DefineMaterial / DefineAnimationClip (and any future DefineX where X is a
-- C++ usertype bound with a `factory_ctor` that takes a single config table).
-- The per-type field list lives in factory_schemas.generated.lua, which is dumped
-- from C++ by LuaScriptSystem::dump_factory_schemas(). To add a new factory type,
-- call bind_factory_schema(...) next to its bind_usertype<T>() and the Lua side
-- picks it up on the next run; no edits to this file are needed.
--
-- Usage (same contract as DefineAsset / DefineTexture / DefineShader):
--   DefineMaterial.Outline = { outline_width = 2.0, outline_color = Color(1,1,1,1) }
--   sprite = { material = Materials.Outline }
--
-- `Materials.Name` / `AnimationClips.Name` return deferred refs. The actual C++
-- object is constructed lazily on first unwrap_def(...) call and cached.
-- Define calls can live in any file in any load order.

-- Per-registry list of declared alias names, populated as `DefineX.Foo = { ... }` runs.
-- Read by C++ (LuaScriptSystem::dump_factory_aliases_meta) after bootstrap completes to emit
-- scripts/engine/meta/factory_aliases_meta.generated.lua so editors get autocomplete on
-- `Materials.Foo`, `AnimationClips.Foo`, etc.
_G.__factory_alias_names = _G.__factory_alias_names or {}

-- Per-registry cache-clear callbacks, invoked by __clear_factory_caches() on hot reload so
-- redefined factory objects (e.g. DefineMaterial) rebuild lazily from their updated defs.
_G.__factory_cache_clearers = {}

local function install_factory_registry(registry_name, schema)
    local defs = {}
    local built = {}

    _G.__factory_cache_clearers[#_G.__factory_cache_clearers + 1] = function()
        for name in pairs(built) do
            built[name] = nil
        end
    end

    local names = {}
    local seen = {}
    _G.__factory_alias_names[registry_name] = names

    local function build(name)
        local def = defs[name]
        if not def then
            Log.error(schema.lua_type .. " '" .. name .. "' is not defined")
            return nil
        end

        local cfg = {}
        for _, field in ipairs(schema.fields) do
            cfg[field.name] = unwrap_def(def[field.name])
        end

        local ctor = _G[schema.lua_type]
        if ctor == nil then
            Log.error("Factory type '" .. schema.lua_type .. "' is not bound in Lua")
            return nil
        end

        local obj = ctor(cfg)
        built[name] = obj
        return obj
    end

    local ref_mt = {
        __tostring = function(self)
            return schema.lua_type .. "(" .. self.__name .. ")"
        end,
        __unwrap = function(self)
            return built[self.__name] or build(self.__name)
        end,
    }

    _G[schema.define] = setmetatable({}, {
        __newindex = function(_, name, def)
            if type(def) ~= "table" then
                Log.error(schema.define .. "." .. tostring(name) .. " must be assigned a table")
                return
            end

            defs[name] = def

            if not seen[name] then
                seen[name] = true
                names[#names + 1] = name
            end
        end,
    })

    _G[registry_name] = setmetatable({}, {
        __index = function(t, name)
            local wrapper = setmetatable({ __name = name }, ref_mt)
            rawset(t, name, wrapper)
            return wrapper
        end,
    })
end

-- Drop every cached factory object so redefined defs (materials, animation clips, ...) rebuild.
function _G.__clear_factory_caches()
    for _, clear in ipairs(_G.__factory_cache_clearers) do
        clear()
    end
end

function _G.__install_factory_registries()
    local schemas = _G.__factory_schemas
    if schemas == nil then
        Log.error(
            "__install_factory_registries: __factory_schemas is missing (did factory_schemas.generated.lua run?)")
        return
    end

    for registry_name, schema in pairs(schemas) do
        install_factory_registry(registry_name, schema)
    end
end
