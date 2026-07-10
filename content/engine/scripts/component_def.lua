-- DefineComponent: Class declaration for Lua components.
--
-- Components are built in two phases:
--   1. DefineComponent.X = {...}  -- registers a placeholder class table immediately
--                                    so `local X = X` and `function X:foo() end` work.
--   2. finalize_components()      -- called by bootstrap.lua after ALL user scripts have
--                                    loaded. Resolves __parent for every pending
--                                    component, walking the inheritance graph.
--
-- This means DefineComponent calls can appear in ANY file in ANY order;
-- references only need to be resolvable by the time finalize_components() runs.
--
-- Usage:
--   DefineComponent.Player = {
--       speed = 7.0,                          -- scalar default; per-instance via Lua shadowing
--       priority = 1,                         -- priority execution order for this type of component; NOT per-instance data
--       __parent = "Character",               -- optional single-inheritance (must be a registered Component)
--   }
--
--   ---@class Player : Character              -- these 2 lines are a hint to the LuaLS
--   local Player = Player                     -- so that we can see intellisense from the inherited component
--
--   function Player:init() ... end            -- per-instance setup (mutable state goes here)
--       self.speed = 10.0                     -- override the default speed
--   }
--
--   function Player:enter_play() ... end
--   function Player:exit_play() ... end
--   function Player:tick(delta_time) ... end
--   function Player:my_helper() ... end       -- any user method

_G.__component_registry = _G.__component_registry or {}
_G.__component_pending = _G.__component_pending or {}

-- Weak-keyed set of every live component instance, used by hot reload to re-point
-- each instance's metatable at its rebuilt class. Weak keys (not values) so the GC
-- genuinely removes collected instances and the set self-compacts toward the live
-- set. C++ holds each instance strongly (LuaScriptComponent::m_impl->lua_instance),
-- so an entry lives exactly as long as its component.
_G.__live_component_instances = _G.__live_component_instances or setmetatable({}, { __mode = "k" })

--- Assigning `DefineComponent.Foo = { ... }` registers a Lua component class
--- and creates a global `Foo`. The table may contain default fields,
--- and methods (`init`, `enter_play`, `exit_play`, `tick`, ...).
---@class DefineComponent
_G.DefineComponent = setmetatable({}, {
    __newindex = function(_, name, def)
        if type(def) ~= "table" then
            Log.error("DefineComponent." .. tostring(name) .. " must be assigned a table")
            return
        end

        -- Create the class placeholder immediately so that `local X = X` and
        -- subsequent `function X:foo()` calls in the same file work as expected.
        -- Copy non-meta fields from def onto the placeholder so scalar defaults
        -- (e.g. `speed = 7.0`) are visible right away.
        local class = {}
        for k, v in pairs(def) do
            if k ~= "__parent" then
                class[k] = v
            end
        end

        _G.__component_pending[name] = { class = class, def = def }
        _G[name] = class
    end,
    __index = function(_, name)
        local class = _G.__component_registry[name]
        if class then return class end

        local pending = _G.__component_pending[name]
        return pending and pending.class or nil
    end,
})

-- `Components.Foo` evaluates to the component name string `"Foo"`. The registry
-- only exists so editors can autocomplete the name and catch typos via the
-- LuaCATS meta; there is no validation at index time (load order means a
-- referenced component may legitimately be defined later). Unknown names
-- surface as errors at use time (e.g. add_lua_component / get_lua_component).
---@class Components
_G.Components = setmetatable({}, {
    __index = function(_, name) return name end,
})

local function build_class(name)
    if _G.__component_registry[name] then
        return _G.__component_registry[name]
    end

    local pending = _G.__component_pending[name]
    if not pending then
        return nil
    end

    if pending.building then
        Log.error("DefineComponent." .. name .. ": cyclic inheritance detected")
        return pending.class
    end
    pending.building = true

    local class = pending.class
    local def = pending.def

    -- Single parent (must be a registered Component).
    -- Resolve recursively so parents in any file/load-order get built before children.
    -- Subclass-on-class methods (defined via `function X:foo()`) are already on `class`.
    -- Parent entries that collide are skipped (override semantics).
    if def.__parent then
        if type(def.__parent) ~= "string" then
            Log.error("DefineComponent." .. name .. ": __parent must be a string component name")
        else
            build_class(def.__parent)
            local parent = _G.__component_registry[def.__parent]
            if not parent then
                Log.error("DefineComponent." .. name .. ": parent '" .. def.__parent .. "' is not registered")
            else
                for k, v in pairs(parent) do
                    if k ~= "__index" and k ~= "new" then
                        if rawget(class, k) == nil then
                            class[k] = v
                        end
                    end
                end
            end
        end
    end

    class.__index = class

    -- Allocates an instance. The engine sets self.entity and calls init()
    -- from C++ after this returns, so do not invoke init() here.
    function class.new()
        local inst = setmetatable({}, class)
        _G.__live_component_instances[inst] = true -- track for hot reload; weak-keyed, self-prunes on GC
        return inst
    end

    _G.__component_registry[name] = class
    pending.building = false

    return class
end

function _G.__finalize_components()
    local names = {}
    local n = 0
    for name in pairs(_G.__component_pending) do
        n = n + 1
        names[n] = name
    end

    for _, name in ipairs(names) do
        build_class(name)
    end

    _G.__component_pending = {}
end
