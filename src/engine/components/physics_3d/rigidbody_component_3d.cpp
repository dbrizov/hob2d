#include "rigidbody_component_3d.h"

#include <box3d/box3d.h>

#include "engine/components/transform_component_3d.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        b3MotionLocks to_b3_motion_locks(uint64_t locks) {
            b3MotionLocks out{};
            out.linearX = (locks & static_cast<uint64_t>(MotionLock::LinearX)) != 0;
            out.linearY = (locks & static_cast<uint64_t>(MotionLock::LinearY)) != 0;
            out.linearZ = (locks & static_cast<uint64_t>(MotionLock::LinearZ)) != 0;
            out.angularX = (locks & static_cast<uint64_t>(MotionLock::AngularX)) != 0;
            out.angularY = (locks & static_cast<uint64_t>(MotionLock::AngularY)) != 0;
            out.angularZ = (locks & static_cast<uint64_t>(MotionLock::AngularZ)) != 0;
            return out;
        }

        b3BodyType to_b3_body_type(BodyType type) {
            switch (type) {
                case BodyType::Dynamic:
                    return b3_dynamicBody;
                case BodyType::Kinematic:
                    return b3_kinematicBody;
                default:
                    return b3_staticBody;
            }
        }
    } // namespace

    RigidbodyComponent3D::RigidbodyComponent3D(Entity& entity)
        : Component(entity) {}

    void RigidbodyComponent3D::enter_world() {
        const TransformComponent3D* transform = get_entity().get_transform_3d();

        if (m_body_type == BodyType::Dynamic) {
            if (const TransformComponent3D* parent = transform->get_parent()) {
                const RigidbodyComponent3D* parent_body = parent->get_entity().get_rigidbody_3d();
                const bool parent_is_static =
                    parent_body != nullptr && parent_body->get_body_type() == BodyType::Static;
                if (!parent_is_static) {
                    log::physics.error("RigidbodyComponent3D: dynamic body (entity_id = {}) is parented under a "
                                       "non-static transform; parent motion will not drive the simulated body.",
                                       get_entity().get_id());
                }
            }
        }

        b3BodyDef body_def = b3DefaultBodyDef();
        body_def.type = to_b3_body_type(m_body_type);
        body_def.position = Physics3D::vec3_to_b3Vec3(transform->get_position());
        body_def.rotation = Physics3D::quat_to_b3Quat(transform->get_rotation());
        body_def.motionLocks = to_b3_motion_locks(m_motion_locks);
        body_def.gravityScale = m_gravity_scale;
        body_def.linearDamping = m_linear_damping;
        body_def.angularDamping = m_angular_damping;

        const PhysicsWorld3D& physics_world = get_engine().get_physics_3d().get_physics_world();
        m_body_id = b3CreateBody(physics_world.get_id(), &body_def);
        b3Body_SetUserData(m_body_id, this);

        if (m_body_type != BodyType::Static) {
            get_engine().get_entity_spawner().register_simulated_rigidbody_3d(this);
        }
    }

    void RigidbodyComponent3D::exit_world() {
        if (m_body_type != BodyType::Static) {
            get_engine().get_entity_spawner().unregister_simulated_rigidbody_3d(this);
        }

        if (b3Body_IsValid(m_body_id)) {
            b3DestroyBody(m_body_id);
            m_body_id = b3_nullBodyId;
        }
    }

    std::string RigidbodyComponent3D::to_string() const {
        return "RigidbodyComponent3D";
    }

    b3BodyId RigidbodyComponent3D::get_body_id() const {
        return m_body_id;
    }

    bool RigidbodyComponent3D::has_body() const {
        return b3Body_IsValid(m_body_id);
    }

    bool RigidbodyComponent3D::is_awake() const {
        return has_body() && b3Body_IsAwake(m_body_id);
    }

    BodyType RigidbodyComponent3D::get_body_type() const {
        return m_body_type;
    }

    void RigidbodyComponent3D::set_body_type(BodyType body_type) {
        m_body_type = body_type;
    }

    uint64_t RigidbodyComponent3D::get_motion_locks() const {
        return m_motion_locks;
    }

    void RigidbodyComponent3D::set_motion_locks(uint64_t motion_locks) {
        m_motion_locks = motion_locks;
        if (has_body()) {
            b3Body_SetMotionLocks(m_body_id, to_b3_motion_locks(m_motion_locks));
        }
    }

    float RigidbodyComponent3D::get_gravity_scale() const {
        return m_gravity_scale;
    }

    void RigidbodyComponent3D::set_gravity_scale(float gravity_scale) {
        m_gravity_scale = gravity_scale;
        if (has_body()) {
            b3Body_SetGravityScale(m_body_id, gravity_scale);
        }
    }

    float RigidbodyComponent3D::get_linear_damping() const {
        return m_linear_damping;
    }

    void RigidbodyComponent3D::set_linear_damping(float damping) {
        m_linear_damping = damping;
        if (has_body()) {
            b3Body_SetLinearDamping(m_body_id, damping);
        }
    }

    float RigidbodyComponent3D::get_angular_damping() const {
        return m_angular_damping;
    }

    void RigidbodyComponent3D::set_angular_damping(float damping) {
        m_angular_damping = damping;
        if (has_body()) {
            b3Body_SetAngularDamping(m_body_id, damping);
        }
    }

    Vector3 RigidbodyComponent3D::get_velocity() const {
        return has_body() ? Physics3D::b3Vec3_to_vec3(b3Body_GetLinearVelocity(m_body_id)) : Vector3::zero();
    }

    void RigidbodyComponent3D::set_velocity(const Vector3& velocity) {
        if (has_body()) {
            b3Body_SetLinearVelocity(m_body_id, Physics3D::vec3_to_b3Vec3(velocity));
        }
    }

    Vector3 RigidbodyComponent3D::get_angular_velocity() const {
        return has_body() ? Physics3D::b3Vec3_to_vec3(b3Body_GetAngularVelocity(m_body_id)) : Vector3::zero();
    }

    void RigidbodyComponent3D::set_angular_velocity(const Vector3& angular_velocity) {
        if (has_body()) {
            b3Body_SetAngularVelocity(m_body_id, Physics3D::vec3_to_b3Vec3(angular_velocity));
        }
    }

    Vector3 RigidbodyComponent3D::get_position() const {
        return has_body() ? Physics3D::b3Vec3_to_vec3(b3Body_GetPosition(m_body_id)) : Vector3::zero();
    }

    void RigidbodyComponent3D::set_position(const Vector3& position) {
        if (has_body()) {
            b3Body_SetTransform(m_body_id, Physics3D::vec3_to_b3Vec3(position), b3Body_GetRotation(m_body_id));
        }
    }

    Quaternion RigidbodyComponent3D::get_rotation() const {
        return has_body() ? Physics3D::b3Quat_to_quat(b3Body_GetRotation(m_body_id)) : Quaternion::identity();
    }

    void RigidbodyComponent3D::set_rotation(const Quaternion& rotation) {
        if (has_body()) {
            b3Body_SetTransform(m_body_id, b3Body_GetPosition(m_body_id), Physics3D::quat_to_b3Quat(rotation));
        }
    }

    void RigidbodyComponent3D::apply_force(const Vector3& force) {
        if (has_body()) {
            b3Body_ApplyForceToCenter(m_body_id, Physics3D::vec3_to_b3Vec3(force), true);
        }
    }

    void RigidbodyComponent3D::apply_impulse(const Vector3& impulse) {
        if (has_body()) {
            b3Body_ApplyLinearImpulseToCenter(m_body_id, Physics3D::vec3_to_b3Vec3(impulse), true);
        }
    }

    void RigidbodyComponent3D::apply_torque(const Vector3& torque) {
        if (has_body()) {
            b3Body_ApplyTorque(m_body_id, Physics3D::vec3_to_b3Vec3(torque), true);
        }
    }
} // namespace hob
