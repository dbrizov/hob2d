#pragma once

#include <box3d/id.h>
#include <box3d/types.h>

#include "engine/components/component.h"
#include "engine/math/quaternion.h"
#include "engine/math/vector3.h"

namespace hob {
    class CapsuleColliderComponent3D;
    class RigidbodyComponent3D;

    class CharacterBodyComponent3D : public Component {
        RigidbodyComponent3D* m_rigidbody = nullptr;
        CapsuleColliderComponent3D* m_capsule_collider = nullptr;

        static constexpr int32_t SOLVER_MAX_ITERATIONS = 4;
        static constexpr float SOLVER_DISTANCE_TOLERANCE = 0.005f;
        static constexpr int32_t SOLVER_PLANES_CAPACITY = 16;
        struct SolverPlaneContact {
            b3Vec3 point;
            b3ShapeId shape_id;
        };
        Vector3 m_solver_origin;
        int32_t m_solver_planes_count = 0;
        b3CollisionPlane m_solver_planes[SOLVER_PLANES_CAPACITY] = {};
        SolverPlaneContact m_solver_plane_contacts[SOLVER_PLANES_CAPACITY] = {};

        uint64_t m_solver_ignore_mask = 0u;
        float m_max_slope_deg = 50.0f;
        bool m_on_floor = false;
        Vector3 m_floor_normal = Vector3::up();

    public:
        explicit CharacterBodyComponent3D(Entity& entity);

        std::string to_string() const override;

        Vector3 get_center() const;
        void set_center(const Vector3& center);

        float get_radius() const;
        void set_radius(float radius);

        float get_height() const;
        void set_height(float height);

        uint64_t get_collision_layer() const;
        void set_collision_layer(uint64_t collision_layer);

        uint64_t get_collision_mask() const;
        void set_collision_mask(uint64_t collision_mask);

        uint64_t get_solver_ignore_mask() const;
        void set_solver_ignore_mask(uint64_t solver_ignore_mask);

        float get_max_slope_deg() const;
        void set_max_slope_deg(float degrees);

        void move_and_slide(const Vector3& velocity, float delta_time);

        bool is_on_floor() const;
        Vector3 get_floor_normal() const;

        Vector3 get_velocity() const;
        void set_velocity(const Vector3& velocity);

        Vector3 get_position() const;
        void set_position(const Vector3& position);

        Quaternion get_rotation() const;
        void set_rotation(const Quaternion& rotation);

    private:
        void gather_planes(const Vector3& origin, const b3Capsule& mover, const b3QueryFilter& filter);
        void push_dynamic_bodies(const Vector3& mover_velocity);
        void update_floor_state();
        float get_min_floor_dot() const;
        b3Plane make_solver_plane(const b3Plane& plane) const;

        static bool plane_result_callback(b3ShapeId other_shape_id,
                                          const b3PlaneResult* plane_result,
                                          int32_t plane_count,
                                          void* context);
    };
} // namespace hob
