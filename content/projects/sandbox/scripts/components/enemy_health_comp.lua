DefineComponent.EnemyHealthComponent = {
    __editor = {
        { name = "max_health",  type = "float", min = 1 },
        { name = "health",      type = "float", min = 0 },
        { name = "head_offset", type = "vector2" },
    },
}
---@class EnemyHealthComponent : LuaComponent
local EnemyHealthComponent = EnemyHealthComponent

function EnemyHealthComponent:init()
    self.max_health = 100.0
    self.health = self.max_health
    self.head_offset = Vector2(0.0, 1.6)

    self._doc = nil
    self._root = nil
    self._fill = nil
end

function EnemyHealthComponent:enter_play()
    self._doc = UI.load_document("ui/enemy_healthbar.rml")
    UI.show_document(self._doc)
    self._root = UI.get_element(self._doc, "enemy_healthbar")
    self._fill = UI.get_element(self._doc, "enemy_health_fill")
end

function EnemyHealthComponent:exit_play()
    UI.unload_document(self._doc)
    self._doc = nil
    self._root = nil
    self._fill = nil
end

function EnemyHealthComponent:late_tick(delta_time)
    if self._root == nil then
        return
    end

    local world_pos = self.entity:get_transform():get_position() + self.head_offset
    UI.set_element_position(self._root, UI.world_to_ui(world_pos))
end

function EnemyHealthComponent:set_health(value)
    self.health = math.max(0, math.min(self.max_health, value))
    if self._fill == nil then
        return
    end

    local percent = math.floor(self.health / self.max_health * 100)
    UI.set_element_property(self._fill, "width", string.format("%d%%", percent))
end
