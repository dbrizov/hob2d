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

    float CameraComponent::get_screen_pixels_per_meter() const {
        return m_screen_pixels_per_meter;
    }

    void CameraComponent::set_screen_pixels_per_meter(float value) {
        m_screen_pixels_per_meter = value;
        if (!m_base_captured) {
            m_base_screen_pixels_per_meter = value;
            m_base_captured = true;
        }
    }

    float CameraComponent::get_zoom() const {
        return m_screen_pixels_per_meter / m_base_screen_pixels_per_meter;
    }

    void CameraComponent::set_zoom(float multiplier) {
        set_screen_pixels_per_meter(m_base_screen_pixels_per_meter * multiplier);
    }

    Matrix4x4 CameraComponent::build_sprite_view_projection() const {
        const Renderer& renderer = get_engine().get_renderer();
        const Vector2 logical_size = renderer.get_logical_size();
        const float w = logical_size.x;
        const float h = logical_size.y;

        const Vector2 camera_position = get_entity().get_transform()->get_position();
        const float ppm = m_screen_pixels_per_meter;

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

    Vector2 CameraComponent::world_to_screen(const Vector2& world_position) const {
        TransformComponent* transform = get_entity().get_transform();
        const Vector2 camera_position = transform->get_position();
        const Vector2 screen_position = world_to_screen(world_position, camera_position);

        return screen_position;
    }

    Vector2 CameraComponent::world_to_screen(const Vector2& world_position, const Vector2& camera_position) const {
        const Renderer& renderer = get_engine().get_renderer();

        const Vector2 half_size = renderer.get_logical_size() * 0.5f;

        // 1) Calculate world delta relative to camera
        const Vector2 delta_meters = world_position - camera_position;

        // 2) Convert meters to pixels using this camera's scale
        Vector2 delta_pixels = delta_meters * m_screen_pixels_per_meter;

        // 3) Flip Y (because screen positive Y goes down)
        delta_pixels.y = -delta_pixels.y;

        // 4) Calculate screen position
        const Vector2 screen_position = Vector2(delta_pixels.x + half_size.x, delta_pixels.y + half_size.y);

        return screen_position;
    }

    Vector2 CameraComponent::screen_to_world(const Vector2& screen_position) const {
        TransformComponent* transform = get_entity().get_transform();
        const Vector2 camera_position = transform->get_position();
        const Vector2 world_position = screen_to_world(screen_position, camera_position);

        return world_position;
    }

    Vector2 CameraComponent::screen_to_world(const Vector2& screen_position, const Vector2& camera_position) const {
        const Renderer& renderer = get_engine().get_renderer();

        const Vector2 half_size = renderer.get_logical_size() * 0.5f;

        // 1) Move origin from top-left to screen center
        Vector2 delta_pixels = Vector2(screen_position.x - half_size.x, screen_position.y - half_size.y);

        // 2) Undo Y flip (because world positive Y goes up)
        delta_pixels.y = -delta_pixels.y;

        // 3) Convert pixels to meters using this camera's scale
        const Vector2 delta_meters = delta_pixels / m_screen_pixels_per_meter;

        // 4) Calculate world position
        const Vector2 world_position = camera_position + delta_meters;

        return world_position;
    }
} // namespace hob
