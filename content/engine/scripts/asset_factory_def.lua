-- Generic asset factory.

__asset_names = {}
__asset_defs = {}

-- One entry per installed factory: these four tables are closure locals, so only a closure can reset them.
local asset_def_clearers = {}

function __clear_asset_factory_defs()
    for _, clear in ipairs(asset_def_clearers) do
        clear()
    end
end

local function install_asset_factory(factory_name, schema)
    local asset_defs = {}
    local asset_names = {}
    local built_assets = {}
    local seen = {}
    __asset_names[factory_name] = asset_names
    __asset_defs[factory_name] = asset_defs

    asset_def_clearers[#asset_def_clearers + 1] = function()
        asset_defs = {}
        asset_names = {}
        built_assets = {}
        seen = {}
        __asset_names[factory_name] = asset_names
        __asset_defs[factory_name] = asset_defs
    end

    local function build(asset_name)
        local asset_def = asset_defs[asset_name]
        if asset_def == nil then
            Log.error(schema.lua_type .. " '" .. asset_name .. "' is not defined")
            return nil
        end

        local cfg = {}
        for key, value in pairs(asset_def) do
            cfg[key] = unwrap_def(value)
        end

        local ctor = _G[schema.lua_type]
        if ctor == nil then
            Log.error("Factory type '" .. schema.lua_type .. "' is not bound in Lua")
            return nil
        end

        local obj = ctor(cfg)
        if obj ~= nil then
            if obj.set_name ~= nil then
                obj:set_name(asset_name)
            else
                Log.error(schema.lua_type .. " does not derive from Asset, so '" .. asset_name ..
                    "' has no identity; the editor cannot resolve it back to its definition")
            end
        end
        built_assets[asset_name] = obj
        return obj
    end

    local ref_mt = {
        __registry = factory_name,
        __tostring = function(self)
            return schema.lua_type .. "(" .. self.__name .. ")"
        end,
        __unwrap = function(self)
            return built_assets[self.__name] or build(self.__name)
        end,
    }

    _G[schema.define] = setmetatable({}, {
        __newindex = function(_, asset_name, asset_def)
            if type(asset_def) == "string" then
                asset_def = { path = asset_def }
            elseif type(asset_def) ~= "table" then
                Log.error(schema.define .. "." .. tostring(asset_name) .. " must be assigned a table or a path string")
                return
            end

            if not __record_def_source(factory_name, asset_name) then
                return
            end

            asset_defs[asset_name] = asset_def

            if seen[asset_name] == nil then
                seen[asset_name] = true
                asset_names[#asset_names + 1] = asset_name
            end
        end,
    })

    _G[factory_name] = setmetatable({}, {
        __index = function(t, asset_name)
            local asset_ref = setmetatable({ __name = asset_name }, ref_mt)
            rawset(t, asset_name, asset_ref)
            return asset_ref
        end,
    })
end

-- Eagerly build every declared shader so its GPU pipeline compiles at load, not on the gameplay hot path.
-- Only shaders are warmed: materials are a cheap CPU param buffer (no compile).
function __warmup_shaders()
    local asset_names = __asset_names["Shaders"]
    if asset_names ~= nil and Shaders ~= nil then
        for _, asset_name in ipairs(asset_names) do
            unwrap_def(Shaders[asset_name])
        end
    end
end

function __install_asset_factories()
    local schemas = __asset_factory_schemas
    if schemas == nil then
        Log.error(
            "__install_asset_factories: __asset_factory_schemas is missing (did asset_factory_schemas.generated.lua run?)")
        return
    end

    for factory_name, schema in pairs(schemas) do
        install_asset_factory(factory_name, schema)
    end
end
