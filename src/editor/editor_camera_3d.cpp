#include "editor_camera_3d.h"

#include <algorithm>
#include <cmath>

#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/constants.h"
#include "engine/math/projection.h"

namespace hob::editor {
    namespace {
        constexpr float LOOK_DEGREES_PER_PIXEL = 0.15f;
        constexpr float MAX_PITCH_DEG = 89.0f;
        constexpr float FAST_MULTIPLIER = 4.0f;
        constexpr float PAN_METERS_PER_PIXEL_AT_UNIT_SPEED = 0.002f;
        constexpr float DOLLY_METERS_PER_WHEEL_AT_UNIT_SPEED = 0.25f;
        constexpr float SPEED_STEP = 1.25f;
        constexpr float MIN_FLY_SPEED = 0.25f;
        constexpr float MAX_FLY_SPEED = 200.0f;
        constexpr float FOCUS_FIT_FACTOR = 1.4f;
        constexpr float FOCUS_MIN_DISTANCE = 1.0f;

        float aspect_of(const Vector2& size) {
            return size.y > 0.0f ? size.x / size.y : 1.0f;
        }

        projection::Viewport to_viewport(const EditorSceneRect& rect) {
            return projection::Viewport{rect.top_left, rect.size};
        }
    } // namespace

    Quaternion EditorCamera3D::get_rotation() const {
        return Quaternion::from_euler_deg(Vector3(pitch_deg, yaw_deg, 0.0f));
    }

    Vector3 EditorCamera3D::get_forward() const {
        return get_rotation().get_forward();
    }

    Matrix4x4 EditorCamera3D::build_view_matrix() const {
        return Matrix4x4::trs(position, get_rotation(), Vector3::one()).inverse_affine();
    }

    Matrix4x4 EditorCamera3D::build_view_projection(const Vector2& target_size) const {
        return Renderer::perspective_offscreen(fov_deg * DEG_TO_RAD, aspect_of(target_size), near_plane, far_plane) *
               build_view_matrix();
    }

    Matrix4x4 EditorCamera3D::build_display_projection(const Vector2& target_size) const {
        return Matrix4x4::perspective_lh(fov_deg * DEG_TO_RAD, aspect_of(target_size), near_plane, far_plane);
    }

    bool EditorCamera3D::world_to_screen(const Vector3& world_pos,
                                         const EditorSceneRect& scene_rect,
                                         Vector2& out) const {
        return projection::project_point(
            build_display_projection(scene_rect.size) * build_view_matrix(), world_pos, to_viewport(scene_rect), out);
    }

    bool EditorCamera3D::project_segment(const Vector3& world_a,
                                         const Vector3& world_b,
                                         const EditorSceneRect& scene_rect,
                                         Vector2& out_a,
                                         Vector2& out_b) const {
        return projection::project_segment(build_view_matrix(),
                                           build_display_projection(scene_rect.size),
                                           near_plane,
                                           world_a,
                                           world_b,
                                           to_viewport(scene_rect),
                                           out_a,
                                           out_b);
    }

    Ray EditorCamera3D::screen_to_ray(const Vector2& screen_pos, const EditorSceneRect& scene_rect) const {
        return projection::screen_to_ray(position,
                                         get_rotation(),
                                         fov_deg * DEG_TO_RAD,
                                         aspect_of(scene_rect.size),
                                         screen_pos,
                                         to_viewport(scene_rect));
    }

    void EditorCamera3D::look_by_pixel_delta(const Vector2& pixel_delta) {
        yaw_deg += pixel_delta.x * LOOK_DEGREES_PER_PIXEL;
        pitch_deg = std::clamp(pitch_deg + pixel_delta.y * LOOK_DEGREES_PER_PIXEL, -MAX_PITCH_DEG, MAX_PITCH_DEG);
    }

    void EditorCamera3D::fly(const Vector3& local_direction, float delta_time, bool fast) {
        if (local_direction == Vector3::zero()) {
            return;
        }

        const float speed = fly_speed * (fast ? FAST_MULTIPLIER : 1.0f);
        position += get_rotation().rotate(local_direction.normalized()) * (speed * delta_time);
    }

    void EditorCamera3D::pan_by_pixel_delta(const Vector2& pixel_delta, const EditorSceneRect& scene_rect) {
        (void)scene_rect;
        const Quaternion rotation = get_rotation();
        const float meters_per_pixel = PAN_METERS_PER_PIXEL_AT_UNIT_SPEED * fly_speed;
        position -= rotation.get_right() * (pixel_delta.x * meters_per_pixel);
        position += rotation.get_up() * (pixel_delta.y * meters_per_pixel);
    }

    void EditorCamera3D::dolly(float wheel) {
        position += get_forward() * (wheel * DOLLY_METERS_PER_WHEEL_AT_UNIT_SPEED * fly_speed);
    }

    void EditorCamera3D::adjust_speed(float wheel) {
        const float factor = wheel > 0.0f ? SPEED_STEP : 1.0f / SPEED_STEP;
        fly_speed = std::clamp(fly_speed * factor, MIN_FLY_SPEED, MAX_FLY_SPEED);
    }

    void EditorCamera3D::focus_on(const AABB3& world_bounds) {
        const float radius = std::max(world_bounds.extents.length(), FOCUS_MIN_DISTANCE * 0.5f);
        const float distance =
            std::max(radius / std::tan(fov_deg * DEG_TO_RAD * 0.5f) * FOCUS_FIT_FACTOR, FOCUS_MIN_DISTANCE);
        position = world_bounds.center - get_forward() * distance;
    }
} // namespace hob::editor
