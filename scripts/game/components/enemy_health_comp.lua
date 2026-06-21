DefineComponent.EnemyHealthbar = {}
---@class EnemyHealthbar : LuaComponent
local EnemyHealthbar = EnemyHealthbar

function EnemyHealthbar:init()
    self.max_health = 100
    self.health = self.max_health
    self.head_offset = Vector2(0.0, 1.6)
    self.doc = nil
    self.root = nil
    self.fill = nil
end

function EnemyHealthbar:enter_play()
    self.doc = UI.load_document("ui/enemy_healthbar.rml")
    UI.show_document(self.doc)
    self.root = UI.get_element(self.doc, "enemy_healthbar")
    self.fill = UI.get_element(self.doc, "enemy_health_fill")
end

function EnemyHealthbar:exit_play()
    UI.unload_document(self.doc)
    self.doc = nil
    self.root = nil
    self.fill = nil
end

function EnemyHealthbar:late_tick(delta_time)
    if self.root == nil then
        return
    end

    local world_pos = self.entity:get_transform():get_position() + self.head_offset
    UI.set_element_position(self.root, UI.world_to_ui(world_pos))
end

function EnemyHealthbar:set_health(value)
    self.health = math.max(0, math.min(self.max_health, value))
    if self.fill == nil then
        return
    end

    local percent = math.floor(self.health / self.max_health * 100)
    UI.set_element_property(self.fill, "width", string.format("%d%%", percent))
end
