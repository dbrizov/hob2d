#pragma once

#include <string>
#include <vector>

#include <box3d/id.h>
#include <box3d/math_functions.h>

#include "engine/math/quaternion.h"
#include "engine/math/vector3.h"

namespace hob {
    struct PhysicsConfig3D;
    class Console;
    class Entity;
    class ColliderComponent3D;
    class RigidbodyComponent3D;

    struct RaycastHit3D {
        ColliderComponent3D* collider = nullptr;
        Vector3 point;
        Vector3 normal;
        float distance = 0.0f;
        bool hit = false;

        std::string to_string() const;
    };

    class PhysicsWorld3D {
        b3WorldId m_id;

    public:
        explicit PhysicsWorld3D(const Vector3& gravity);
        ~PhysicsWorld3D();

        void tick(float fixed_delta_time, uint32_t sub_steps);

        b3WorldId get_id() const;
    };

    class Physics3D {
        PhysicsWorld3D m_physics_world;
        float m_accumulator;
        float m_fixed_delta_time;
        uint32_t m_sub_steps_per_tick;
        float m_interpolation_fraction;
        bool m_interpolation_enabled;

    public:
        bool cvar_show_colliders = true;

        explicit Physics3D(const PhysicsConfig3D& physics_config);
        ~Physics3D();

        Physics3D(const Physics3D&) = delete;
        Physics3D& operator=(const Physics3D&) = delete;

        Physics3D(Physics3D&&) = delete;
        Physics3D& operator=(Physics3D&&) = delete;

        void register_cvars(Console& console);

        void tick(float frame_delta_time, const std::vector<RigidbodyComponent3D*>& rigidbodies);

        const PhysicsWorld3D& get_physics_world() const;

        float get_fixed_delta_time() const;
        float get_interpolation_fraction() const;

        RaycastHit3D raycast(const Vector3& origin,
                             const Vector3& direction,
                             float distance,
                             uint64_t layer_mask = ~0ull) const;

        std::vector<RaycastHit3D> raycast_all(const Vector3& origin,
                                              const Vector3& direction,
                                              float distance,
                                              uint64_t layer_mask = ~0ull) const;

        static Vector3 b3Vec3_to_vec3(const b3Vec3& vec) {
            return Vector3(vec.x, vec.y, vec.z);
        }

        static b3Vec3 vec3_to_b3Vec3(const Vector3& vec) {
            return b3Vec3{vec.x, vec.y, vec.z};
        }

        static Quaternion b3Quat_to_quat(const b3Quat& quat) {
            return Quaternion(quat.v.x, quat.v.y, quat.v.z, quat.s);
        }

        static b3Quat quat_to_b3Quat(const Quaternion& quat) {
            return b3Quat{b3Vec3{quat.x, quat.y, quat.z}, quat.w};
        }

    private:
        void dispatch_collision_events() const;
        void dispatch_trigger_events() const;

        static float delta_time_from_ticks(uint32_t ticks_per_second);
    };
} // namespace hob
