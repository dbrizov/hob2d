DefineComponent.Player = {
    __parent = Components.Character,
}
---@class Player : Character
local Player = Player

function Player:init()
    self.speed = 7.0
    self.camera_follow_speed = 10.0
    self.movement_input = Vector2.zero()
    self.aim_input = Vector2.zero()
    self.x_axis_id = nil
    self.y_axis_id = nil
    self.aim_x_axis_id = nil
    self.aim_y_axis_id = nil
    self.fire_action_id = nil
    self.slow_motion_action_id = nil

    self.camera_zoom_time = 0.0
    self.camera_min_zoom = 0.5
    self.camera_max_zoom = 2.0
end

function Player:enter_play()
    local input = self.entity:get_input()

    self.x_axis_id = input:bind_axis("horizontal", function(axis)
        self.movement_input.x = axis
    end)

    self.y_axis_id = input:bind_axis("vertical", function(axis)
        self.movement_input.y = axis
    end)

    self.aim_x_axis_id = input:bind_axis("aim_x", function(axis)
        self.aim_input.x = axis
    end)

    self.aim_y_axis_id = input:bind_axis("aim_y", function(axis)
        self.aim_input.y = axis
    end)

    self.fire_action_id = input:bind_action("fire", InputEventType.Pressed, function()
        -- Debug.print("fire")
    end)

    self.slow_motion_action_id = input:bind_action("slow_motion", InputEventType.Pressed, function()
        local new_scale = Timer.get_time_scale() < 1.0 and 1.0 or 0.2
        Timer.set_time_scale(new_scale)
    end)
end

function Player:exit_play()
    local input = self.entity:get_input()
    input:unbind_axis("horizontal", self.x_axis_id)
    input:unbind_axis("vertical", self.y_axis_id)
    input:unbind_axis("aim_x", self.aim_x_axis_id)
    input:unbind_axis("aim_y", self.aim_y_axis_id)
    input:unbind_action("fire", self.fire_action_id)
    input:unbind_action("slow_motion", self.slow_motion_action_id)
end

function Player:physics_tick(fixed_delta_time)
    self:move(self.movement_input, fixed_delta_time)
end

function Player:late_tick(delta_time)
    self:update_animation()

    local position = self.entity:get_transform():get_position()
    self:update_camera_position(position, delta_time)
    self:update_rotation(delta_time)

    -- self.camera_zoom_time = self.camera_zoom_time + delta_time
    -- local t = (math.sin(self.camera_zoom_time) + 1.0) * 0.5
    -- local zoom = Math.lerp(self.camera_min_zoom, self.camera_max_zoom, t)
    -- Camera.set_zoom(zoom)
end

function Player:debug_draw_tick(delta_time)
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

function Player:update_animation()
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

function Player:update_camera_position(target_position, delta_time)
    local current_position = Camera.get_position()
    local new_position = Vector2.lerp(current_position, target_position, delta_time * self.camera_follow_speed)
    Camera.set_position(new_position)
end

function Player:update_rotation(delta_time)
    local character_body = self.entity:get_character_body()

    -- Twin-stick aiming: the right stick rotates the character toward its direction.
    -- It already passes through the engine deadzone, so any non-zero value means it's in use.
    if self.aim_input:length_sqr() > 0.0 then
        local radians = math.atan(self.aim_input.y, self.aim_input.x)
        character_body:set_rotation(radians)
        return
    end

    -- Fall back to aiming at the mouse cursor.
    local mouse_screen = Input.get_mouse_screen_position()
    local mouse_world = Camera.screen_to_world(mouse_screen)
    local player_pos = character_body:get_position()
    local direction = mouse_world - player_pos

    local radians = math.atan(direction.y, direction.x)
    character_body:set_rotation(radians)
end
