#pragma once

#include "editor_camera.h"
#include "engine/math/aabb3.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/quaternion.h"
#include "engine/math/ray.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"

namespace hob::editor {
    struct EditorCamera3D {
        Vector3 position = Vector3(0.0f, 5.0f, -10.0f);
        float yaw_deg = 0.0f;
        float pitch_deg = 20.0f;
        float fov_deg = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 2000.0f;
        float fly_speed = 8.0f;

        Quaternion get_rotation() const;
        Vector3 get_forward() const;

        Matrix4x4 build_view_matrix() const;
        Matrix4x4 build_view_projection(const Vector2& target_size) const;
        Matrix4x4 build_display_projection(const Vector2& target_size) const;

        bool world_to_screen(const Vector3& world_pos, const EditorSceneRect& scene_rect, Vector2& out) const;
        bool project_segment(const Vector3& world_a,
                             const Vector3& world_b,
                             const EditorSceneRect& scene_rect,
                             Vector2& out_a,
                             Vector2& out_b) const;
        Ray screen_to_ray(const Vector2& screen_pos, const EditorSceneRect& scene_rect) const;

        void look_by_pixel_delta(const Vector2& pixel_delta);
        void fly(const Vector3& local_direction, float delta_time, bool fast);
        void pan_by_pixel_delta(const Vector2& pixel_delta, const EditorSceneRect& scene_rect);
        void dolly(float wheel);
        void adjust_speed(float wheel);
        void focus_on(const AABB3& world_bounds);
    };
} // namespace hob::editor
