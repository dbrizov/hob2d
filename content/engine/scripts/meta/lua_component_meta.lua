---@meta

---@class EditorFieldAnnotation
---@field name string
---@field type string?  # A FieldType value, e.g. "float" or "enum". Inferred from the value when omitted.
---@field enum string?  # Global table holding the entries, for "enum" and "bitmask".
---@field min number?   # Lower bound. Either bound may be given alone.
---@field max number?   # Upper bound.
---@field hidden boolean?  # Keeps the field out of the Inspector, like a `_` prefix does.

---@class LuaComponent
---@field entity Entity
---@field class_name string  # Name of the DefineComponent class (set automatically on instantiation).
---@field priority integer?  # Priority execution order for this component type. Defaults to 0 (CP_DEFAULT). Set on the class table (e.g. `Player.priority = -50`), NOT per-instance.
---@field __editor EditorFieldAnnotation[]?  # Set on the class table, in the DefineComponent literal.
local LuaComponent = {}

function LuaComponent:init() end

function LuaComponent:enter_play() end

function LuaComponent:exit_play() end

---@param delta_time number
function LuaComponent:tick(delta_time) end

---@param fixed_delta_time number
function LuaComponent:physics_tick(fixed_delta_time) end

---@param delta_time number
function LuaComponent:late_tick(delta_time) end

---@param delta_time number
function LuaComponent:debug_draw_tick(delta_time) end

---@param other ColliderComponent
function LuaComponent:on_collision_enter(other) end

---@param other ColliderComponent
function LuaComponent:on_collision_exit(other) end

---@param other ColliderComponent
function LuaComponent:on_trigger_enter(other) end

---@param other ColliderComponent
function LuaComponent:on_trigger_exit(other) end

function LuaComponent:on_hot_reload() end
