#pragma once

#include <vector>

#include "component.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/quaternion.h"
#include "engine/math/vector3.h"

namespace hob {
    class TransformComponent3D : public Component {
        friend class Physics3D;
        friend class EntitySpawner;

        Vector3 m_local_position;
        Quaternion m_local_rotation = Quaternion::identity();
        Vector3 m_local_scale = Vector3::one();

        Matrix4x4 m_local_matrix = Matrix4x4::identity();
        Vector3 m_prev_local_position;
        Quaternion m_prev_local_rotation = Quaternion::identity();

        mutable Matrix4x4 m_world_matrix = Matrix4x4::identity();
        mutable bool m_world_matrix_dirty = true;
        bool m_render_dirty = true;

        TransformComponent3D* m_parent = nullptr;
        std::vector<TransformComponent3D*> m_children;

        bool m_interpolate_physics = false;

    public:
        explicit TransformComponent3D(Entity& entity);

        void exit_world() override;

        std::string to_string() const override;

        Vector3 get_position() const;
        void set_position(const Vector3& position);

        Quaternion get_rotation() const;
        void set_rotation(const Quaternion& rotation);

        Vector3 get_euler_deg() const;
        void set_euler_deg(const Vector3& euler_deg);

        Vector3 get_lossy_scale() const;

        Vector3 get_local_position() const;
        void set_local_position(const Vector3& position);

        Quaternion get_local_rotation() const;
        void set_local_rotation(const Quaternion& rotation);

        Vector3 get_local_euler_deg() const;
        void set_local_euler_deg(const Vector3& euler_deg);

        Vector3 get_local_scale() const;
        void set_local_scale(const Vector3& scale);

        Vector3 get_forward() const;
        Vector3 get_right() const;
        Vector3 get_up() const;

        const Matrix4x4& get_world_matrix() const;
        Matrix4x4 get_prev_world_matrix() const;
        Matrix4x4 get_interpolated_world_matrix(float fraction) const;
        const Matrix4x4& get_local_matrix() const;

        TransformComponent3D* get_parent() const;
        void set_parent(TransformComponent3D* parent, bool keep_world_transform = true);
        const std::vector<TransformComponent3D*>& get_children() const;

        bool get_interpolate_physics() const;
        void set_interpolate_physics(bool value);

        bool consume_render_dirty();

    private:
        void mark_world_matrix_dirty();
        void rebuild_local_matrix();
        void set_prev_local_pose(const Vector3& position, const Quaternion& rotation);
        void notify_scale_changed();
        bool is_ancestor_of(const TransformComponent3D* node) const;
        void detach_from_hierarchy();
    };
} // namespace hob
