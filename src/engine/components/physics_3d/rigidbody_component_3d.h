#pragma once

#include <limits>

#include <box3d/id.h>

#include "engine/components/component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/math/quaternion.h"
#include "engine/math/vector3.h"

namespace hob {
    using RigidbodyIndex3D = uint32_t;
    constexpr RigidbodyIndex3D INVALID_RIGIDBODY_INDEX_3D = std::numeric_limits<RigidbodyIndex3D>::max();

    enum class MotionLock : uint64_t {
        None = 0,
        LinearX = 1 << 0,
        LinearY = 1 << 1,
        LinearZ = 1 << 2,
        AngularX = 1 << 3,
        AngularY = 1 << 4,
        AngularZ = 1 << 5,
        Angular = AngularX | AngularY | AngularZ,
    };

    class RigidbodyComponent3D : public Component {
        friend class EntitySpawner;

        b3BodyId m_body_id = b3_nullBodyId;
        BodyType m_body_type = BodyType::Static;
        uint64_t m_motion_locks = static_cast<uint64_t>(MotionLock::None);
        float m_gravity_scale = 1.0f;
        float m_linear_damping = 0.0f;
        float m_angular_damping = 0.05f;

        RigidbodyIndex3D m_rigidbody_index = INVALID_RIGIDBODY_INDEX_3D;

    public:
        explicit RigidbodyComponent3D(Entity& entity);

        void enter_world() override;
        void exit_world() override;

        std::string to_string() const override;

        b3BodyId get_body_id() const;

        bool has_body() const;
        bool is_awake() const;

        BodyType get_body_type() const;
        void set_body_type(BodyType body_type);

        uint64_t get_motion_locks() const;
        void set_motion_locks(uint64_t motion_locks);

        float get_gravity_scale() const;
        void set_gravity_scale(float gravity_scale);

        float get_linear_damping() const;
        void set_linear_damping(float damping);

        float get_angular_damping() const;
        void set_angular_damping(float damping);

        Vector3 get_velocity() const;
        void set_velocity(const Vector3& velocity);

        Vector3 get_angular_velocity() const;
        void set_angular_velocity(const Vector3& angular_velocity);

        Vector3 get_position() const;
        void set_position(const Vector3& position);

        Quaternion get_rotation() const;
        void set_rotation(const Quaternion& rotation);

        void apply_force(const Vector3& force);
        void apply_impulse(const Vector3& impulse);
        void apply_torque(const Vector3& torque);
    };
} // namespace hob
