DefineComponent.PlayerComponent = {
    __parent = Components.CharacterComponent,
    __editor = {
        { name = "speed",               type = "float", min = 0 },
        { name = "camera_follow_speed", type = "float", min = 0 },
        { name = "max_health",          type = "float", min = 1 },
        { name = "health",              type = "float", min = 0 },
        { name = "health_regen",        type = "float", min = 0 },
        { name = "fire_damage",         type = "float", min = 0 },
    },
}
---@class PlayerComponent : CharacterComponent
local PlayerComponent = PlayerComponent

function PlayerComponent:init()
    self.speed = 7.0
    self.camera_follow_speed = 10.0
    self.max_health = 100.0
    self.health = self.max_health
    self.health_regen = 12.0 -- per second
    self.fire_damage = 15.0

    self._movement_input = Vector2.zero()
    self._aim_input = Vector2.zero()
    self._x_axis_id = nil
    self._y_axis_id = nil
    self._aim_x_axis_id = nil
    self._aim_y_axis_id = nil
    self._fire_action_id = nil
    self._slow_motion_action_id = nil
    self._hud_model = nil
    self._hud_doc = nil
end

function PlayerComponent:enter_play()
    -- The data model must exist before the document that binds to it loads.
    self._hud_model = UI.create_model("player_hud", {
        health = self.health,
        max_health = self.max_health,
        fill_width = "100%",
    })
    self._hud_doc = UI.load_document("ui/healthbar.rml")
    UI.show_document(self._hud_doc)

    local input = self.entity:get_input()

    self._x_axis_id = input:bind_axis("horizontal", function(axis)
        self._movement_input.x = axis
    end)

    self._y_axis_id = input:bind_axis("vertical", function(axis)
        self._movement_input.y = axis
    end)

    self._aim_x_axis_id = input:bind_axis("aim_x", function(axis)
        self._aim_input.x = axis
    end)

    self._aim_y_axis_id = input:bind_axis("aim_y", function(axis)
        self._aim_input.y = axis
    end)

    self._fire_action_id = input:bind_action("fire", InputEventType.Pressed, function()
        self:set_health(self.health - self.fire_damage)
        self.entity:get_audio():play()

        local mouse_screen = Input.get_mouse_screen_position()
        local mouse_world = Camera.screen_to_world(mouse_screen)
        local player_pos = self.entity:get_transform():get_position()

        local direction = mouse_world - player_pos
        local distance = direction:length()
        local hit = Physics.raycast(player_pos, direction, distance)
        if hit.hit then
            local health_comp = hit.entity:get_lua_component(Components.EnemyHealthComponent)
            if health_comp then
                local new_health = health_comp.health - self.fire_damage
                if new_health <= 0 then
                    EntitySpawner.destroy_entity(hit.entity)
                else
                    health_comp:set_health(new_health)
                end
            end
        end
    end)

    self._slow_motion_action_id = input:bind_action("slow_motion", InputEventType.Pressed, function()
        local new_scale = Timer.get_time_scale() < 1.0 and 1.0 or 0.2
        Timer.set_time_scale(new_scale)
    end)
end

function PlayerComponent:exit_play()
    local input = self.entity:get_input()
    input:unbind_axis("horizontal", self._x_axis_id)
    input:unbind_axis("vertical", self._y_axis_id)
    input:unbind_axis("aim_x", self._aim_x_axis_id)
    input:unbind_axis("aim_y", self._aim_y_axis_id)
    input:unbind_action("fire", self._fire_action_id)
    input:unbind_action("slow_motion", self._slow_motion_action_id)

    -- Unload the document before destroying the model it binds to.
    UI.unload_document(self._hud_doc)
    UI.destroy_model(self._hud_model)
    self._hud_doc = nil
    self._hud_model = nil
end

function PlayerComponent:physics_tick(fixed_delta_time)
    self:move(self._movement_input, fixed_delta_time)
end

function PlayerComponent:late_tick(delta_time)
    self:update_animation()

    if self.health < self.max_health then
        self:set_health(self.health + self.health_regen * delta_time)
    end

    local position = self.entity:get_transform():get_position()
    self:update_camera_position(position, delta_time)
    self:update_rotation(delta_time)
end

function PlayerComponent:debug_draw_tick(delta_time)
    if not Input.is_mouse_over_game_window() then
        return
    end

    local mouse_screen = Input.get_mouse_screen_position()
    local mouse_world = Camera.screen_to_world(mouse_screen)
    local player_pos = self.entity:get_transform():get_position()

    local direction = mouse_world - player_pos
    local distance = direction:length()
    local hit = Physics.raycast(player_pos, direction, distance)

    if hit.hit then
        Debug.draw_line(player_pos, hit.point, Color.red())
        Debug.draw_circle(hit.point, 0.1, Color.red())
    else
        Debug.draw_line(player_pos, mouse_world, Color.green())
    end
end

function PlayerComponent:set_health(value)
    self.health = math.max(0, math.min(self.max_health, value))
    if self._hud_model == nil then
        return
    end

    local percent = math.floor(self.health / self.max_health * 100)
    UI.set(self._hud_model, "health", math.floor(self.health))
    UI.set(self._hud_model, "fill_width", string.format("%d%%", percent))
end

function PlayerComponent:update_animation()
    local animator = self.entity:get_sprite_animator()
    if animator == nil then
        return
    end

    local velocity = self.entity:get_character_body():get_velocity()
    local moving = velocity:length_sqr() > 0.01
    local desired = moving and "run" or "idle"

    if animator:get_current_clip() ~= desired then
        animator:play(desired)
    end
end

function PlayerComponent:update_camera_position(target_position, delta_time)
    local current_position = Camera.get_position()
    local new_position = Vector2.lerp(current_position, target_position, delta_time * self.camera_follow_speed)
    Camera.set_position(new_position)
end

function PlayerComponent:update_rotation(delta_time)
    local character_body = self.entity:get_character_body()

    if self._aim_input:length_sqr() > 0.0 then
        local radians = math.atan(self._aim_input.y, self._aim_input.x)
        character_body:set_rotation(radians)
        return
    end

    if not Input.is_mouse_over_game_window() then
        return
    end

    local mouse_screen = Input.get_mouse_screen_position()
    local mouse_world = Camera.screen_to_world(mouse_screen)
    local player_pos = character_body:get_position()
    local direction = mouse_world - player_pos

    local radians = math.atan(direction.y, direction.x)
    character_body:set_rotation(radians)
end
