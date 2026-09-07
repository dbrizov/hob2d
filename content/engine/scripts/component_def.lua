-- DefineComponent: Class declaration for Lua components.

_G.__component_registry = {}
_G.__component_pending = {}

_G.__live_component_instances = setmetatable({}, { __mode = "k" })

function _G.__clear_component_defs()
    _G.__component_registry = {}
    _G.__component_pending = {}
end

local function is_valid_editor_annotation(name, annotations)
    if type(annotations) ~= "table" then
        Log.error("DefineComponent." .. name .. ": __editor must be an array of { name = ... } entries")
        return false
    end

    for key, entry in pairs(annotations) do
        if math.type(key) ~= "integer" or type(entry) ~= "table" or type(entry.name) ~= "string" then
            Log.error("DefineComponent." .. name .. ": __editor[" .. tostring(key) ..
                "] must be a table with a string 'name'")
            return false
        end
    end

    return true
end

---@class DefineComponent
_G.DefineComponent = setmetatable({}, {
    __newindex = function(_, name, def)
        if type(def) ~= "table" then
            Log.error("DefineComponent." .. tostring(name) .. " must be assigned a table")
            return
        end

        if not __record_def_source(DefRegistry.COMPONENTS, name) then
            return
        end

        local class = {}
        for k, v in pairs(def) do
            if k ~= "__parent" then
                class[k] = v
            end
        end

        if class.__editor ~= nil and not is_valid_editor_annotation(name, class.__editor) then
            class.__editor = nil
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

-- `Components.Foo` evaluates to the component name string `"Foo"`.
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

    -- C++ sets self.entity and calls init() after this returns; do not invoke init() here.
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
