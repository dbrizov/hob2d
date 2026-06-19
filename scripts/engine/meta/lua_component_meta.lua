---@meta
-- Hand-written annotations for Lua-side constructs that are NOT bound from
-- C++. C++ bindings are auto-generated into engine.generated.lua by
-- hob::LuaScriptSystem::dump_meta(); do not duplicate them here.

-- Every component declared with DefineComponent.Foo = { ... } should be
-- annotated as `---@class Foo : LuaComponent` so that self.entity and the
-- lifecycle hooks (init/enter_play/tick/...) are typed correctly.
---@class LuaComponent
---@field entity Entity
---@field class_name string  # Name of the DefineComponent class (set automatically on instantiation).
---@field priority integer?  # Priority execution order for this component type. Defaults to 0 (CP_DEFAULT). Set on the class table (e.g. `Player.priority = -50`), NOT per-instance.
local LuaComponent = {}

--- Called once when the component instance is created (before enter_play).
function LuaComponent:init() end

--- Called when the entity enters play.
function LuaComponent:enter_play() end

--- Called when the entity exits play.
function LuaComponent:exit_play() end

--- Called every frame.
---@param delta_time number
function LuaComponent:tick(delta_time) end

--- Called every fixed physics step. Drive movement here.
---@param fixed_delta_time number
function LuaComponent:physics_tick(fixed_delta_time) end

--- Called every frame after physics_tick. Ideal for camera control that needs post-physics transforms.
---@param delta_time number
function LuaComponent:late_tick(delta_time) end

--- Called every frame after all ticks, intended for debug visualization (Debug.draw_line / Debug.draw_circle).
---@param delta_time number
function LuaComponent:debug_draw_tick(delta_time) end

--- Called when a collider begins colliding with another (solid contact).
---@param other ColliderComponent
function LuaComponent:on_collision_enter(other) end

--- Called when a collider stops colliding with another.
---@param other ColliderComponent
function LuaComponent:on_collision_exit(other) end

--- Called when a collider enters a trigger volume.
---@param other ColliderComponent
function LuaComponent:on_trigger_enter(other) end

--- Called when a collider leaves a trigger volume.
---@param other ColliderComponent
function LuaComponent:on_trigger_exit(other) end

--- Called after a Lua hot reload, once per live instance, right after its metatable
--- has been re-pointed at the rebuilt class. Per-instance state is preserved (it lives
--- on the instance table); use this hook to migrate state when a field's shape changed
--- or to re-apply spawn-time data (e.g. swap a sprite material from the entity prefab).
--- init() is NOT re-run on reload.
function LuaComponent:on_hot_reload() end
