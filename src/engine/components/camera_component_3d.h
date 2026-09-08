#pragma once

#include "component.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/ray.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"

namespace hob {
    class CameraComponent3D : public Component {
        float m_fov_deg = 60.0f;
        float m_near_plane = 0.1f;
        float m_far_plane = 1000.0f;

    public:
        explicit CameraComponent3D(Entity& entity);

        void enter_world() override;
        void exit_world() override;

        std::string to_string() const override;

        float get_fov_deg() const;
        void set_fov_deg(float value);

        float get_near_plane() const;
        void set_near_plane(float value);

        float get_far_plane() const;
        void set_far_plane(float value);

        Vector3 get_position() const;

        Matrix4x4 build_view_matrix() const;
        Matrix4x4 build_view_projection(const Vector2& target_size) const;
        Matrix4x4 build_display_view_projection(const Vector2& target_size) const;
        Matrix4x4 build_view_projection() const;

        bool world_to_screen(const Vector3& world_pos, Vector2& out_screen_pos) const;
        bool project_segment(const Vector3& world_a, const Vector3& world_b, Vector2& out_a, Vector2& out_b) const;
        Ray screen_to_ray(const Vector2& screen_pos) const;
    };
} // namespace hob
