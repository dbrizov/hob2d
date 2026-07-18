#include "camera_component.h"

#include <format>

#include "engine/core/engine.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/entity/entity.h"
#include "transform_component.h"

namespace hob {
    CameraComponent::CameraComponent(Entity& entity)
        : Component(entity) {}

    void CameraComponent::enter_play() {
        get_engine().set_active_camera(this);
    }

    void CameraComponent::exit_play() {
        get_engine().clear_active_camera(this);
    }

    std::string CameraComponent::to_string() const {
        return std::format("CameraComponent(entity_id = {})", get_entity().get_id());
    }

    float CameraComponent::get_pixels_per_meter() const {
        return m_pixels_per_meter;
    }

    void CameraComponent::set_pixels_per_meter(float value) {
        m_pixels_per_meter = value;
        if (!m_base_captured) {
            m_base_pixels_per_meter = value;
            m_base_captured = true;
        }
    }

    float CameraComponent::get_zoom() const {
        return m_pixels_per_meter / m_base_pixels_per_meter;
    }

    void CameraComponent::set_zoom(float multiplier) {
        set_pixels_per_meter(m_base_pixels_per_meter * multiplier);
    }

    Matrix4x4 CameraComponent::build_view_projection() const {
        const Renderer& renderer = get_engine().get_renderer();
        const Vector2 logical_size = renderer.get_logical_size();
        const float w = logical_size.x;
        const float h = logical_size.y;

        const Vector2 camera_position = get_entity().get_transform()->get_position();
        const float ppm = m_pixels_per_meter;

        // World meters -> logical pixels (matches world_to_screen): scale by ppm, flip y, recenter on the camera.
        // Column-major, translation in m[12]/m[13].
        Matrix4x4 world_to_pixels = Matrix4x4::identity();
        world_to_pixels.m[0] = ppm;
        world_to_pixels.m[5] = -ppm;
        world_to_pixels.m[12] = w * 0.5f - ppm * camera_position.x;
        world_to_pixels.m[13] = h * 0.5f + ppm * camera_position.y;

        // Compose with the offscreen ortho (logical pixels -> NDC, y-down) to get world -> NDC.
        return Renderer::ortho_top_left(w, h) * world_to_pixels;
    }

    Vector2 CameraComponent::world_to_screen(const Vector2& world_pos) const {
        TransformComponent* transform = get_entity().get_transform();
        const Vector2 camera_pos = transform->get_position();
        const Vector2 screen_pos = world_to_screen(world_pos, camera_pos);

        return screen_pos;
    }

    Vector2 CameraComponent::world_to_screen(const Vector2& world_pos, const Vector2& camera_pos) const {
        const Vector2 delta_meters = world_pos - camera_pos;
        Vector2 delta_pixels = delta_meters * m_pixels_per_meter;
        delta_pixels.y = -delta_pixels.y; // Flip Y: screen positive Y goes down.

        const Vector2 half_size = get_engine().get_renderer().get_logical_size() * 0.5f;
        const Vector2 screen_pos = Vector2(delta_pixels.x + half_size.x, delta_pixels.y + half_size.y);

        return screen_pos;
    }

    Vector2 CameraComponent::screen_to_world(const Vector2& screen_pos) const {
        TransformComponent* transform = get_entity().get_transform();
        const Vector2 camera_pos = transform->get_position();
        const Vector2 world_pos = screen_to_world(screen_pos, camera_pos);

        return world_pos;
    }

    Vector2 CameraComponent::screen_to_world(const Vector2& screen_pos, const Vector2& camera_pos) const {
        const Vector2 half_size = get_engine().get_renderer().get_logical_size() * 0.5f;
        Vector2 delta_pixels = Vector2(screen_pos.x - half_size.x, screen_pos.y - half_size.y);
        delta_pixels.y = -delta_pixels.y; // Undo Y flip: world positive Y goes up.

        const Vector2 delta_meters = delta_pixels / m_pixels_per_meter;
        const Vector2 world_pos = camera_pos + delta_meters;

        return world_pos;
    }
} // namespace hob
