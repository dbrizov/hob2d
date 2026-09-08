#include "character_body_component_3d.h"

#include <cmath>

#include <box3d/box3d.h>

#include "capsule_collider_component_3d.h"
#include "engine/core/assert.h"
#include "engine/core/engine.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"
#include "rigidbody_component_3d.h"

namespace hob {
    CharacterBodyComponent3D::CharacterBodyComponent3D(Entity& entity)
        : Component(entity) {
        m_rigidbody = entity.add_component<RigidbodyComponent3D>();
        m_rigidbody->set_body_type(BodyType::Kinematic);
        m_rigidbody->set_motion_locks(static_cast<uint64_t>(MotionLock::Angular));

        m_capsule_collider = entity.add_component<CapsuleColliderComponent3D>();
        m_capsule_collider->set_radius(0.4f);
        m_capsule_collider->set_height(1.8f);
        m_capsule_collider->set_center(Vector3(0.0f, 0.9f, 0.0f));
    }

    std::string CharacterBodyComponent3D::to_string() const {
        return "CharacterBodyComponent3D";
    }

    Vector3 CharacterBodyComponent3D::get_center() const {
        return m_capsule_collider->get_center();
    }

    void CharacterBodyComponent3D::set_center(const Vector3& center) {
        m_capsule_collider->set_center(center);
    }

    float CharacterBodyComponent3D::get_radius() const {
        return m_capsule_collider->get_radius();
    }

    void CharacterBodyComponent3D::set_radius(float radius) {
        m_capsule_collider->set_radius(radius);
    }

    float CharacterBodyComponent3D::get_height() const {
        return m_capsule_collider->get_height();
    }

    void CharacterBodyComponent3D::set_height(float height) {
        m_capsule_collider->set_height(height);
    }

    uint64_t CharacterBodyComponent3D::get_collision_layer() const {
        return m_capsule_collider->get_collision_layer();
    }

    void CharacterBodyComponent3D::set_collision_layer(uint64_t collision_layer) {
        m_capsule_collider->set_collision_layer(collision_layer);
    }

    uint64_t CharacterBodyComponent3D::get_collision_mask() const {
        return m_capsule_collider->get_collision_mask();
    }

    void CharacterBodyComponent3D::set_collision_mask(uint64_t collision_mask) {
        m_capsule_collider->set_collision_mask(collision_mask);
    }

    uint64_t CharacterBodyComponent3D::get_solver_ignore_mask() const {
        return m_solver_ignore_mask;
    }

    void CharacterBodyComponent3D::set_solver_ignore_mask(uint64_t solver_ignore_mask) {
        m_solver_ignore_mask = solver_ignore_mask;
    }

    float CharacterBodyComponent3D::get_max_slope_deg() const {
        return m_max_slope_deg;
    }

    void CharacterBodyComponent3D::set_max_slope_deg(float degrees) {
        m_max_slope_deg = degrees;
    }

    void CharacterBodyComponent3D::move_and_slide(const Vector3& velocity, float delta_time) {
        const b3BodyId body_id = m_rigidbody->get_body_id();
        if (!b3Body_IsValid(body_id) || delta_time <= 0.0f) {
            return;
        }

        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = get_collision_layer();
        filter.maskBits = get_collision_mask() & ~m_solver_ignore_mask;

        const b3Capsule mover = m_capsule_collider->get_scaled_capsule();
        const b3WorldId world_id = get_engine().get_physics_3d().get_physics_world().get_id();

        const Vector3 start_position = Physics3D::b3Vec3_to_vec3(b3Body_GetPosition(body_id));
        const Vector3 target_position = start_position + velocity * delta_time;
        Vector3 current_position = start_position;

        for (int32_t i = 0; i < SOLVER_MAX_ITERATIONS; ++i) {
            gather_planes(current_position, mover, filter);

            const Vector3 desired_delta = target_position - current_position;
            const b3PlaneSolverResult solved =
                b3SolvePlanes(Physics3D::vec3_to_b3Vec3(desired_delta), m_solver_planes, m_solver_planes_count);
            const Vector3 solved_delta = Physics3D::b3Vec3_to_vec3(solved.delta);

            const float fraction = b3World_CastMover(world_id,
                                                     Physics3D::vec3_to_b3Vec3(current_position),
                                                     &mover,
                                                     Physics3D::vec3_to_b3Vec3(solved_delta),
                                                     filter,
                                                     nullptr,
                                                     nullptr);
            const Vector3 delta = solved_delta * fraction;
            current_position += delta;

            if (delta.length_sqr() < SOLVER_DISTANCE_TOLERANCE * SOLVER_DISTANCE_TOLERANCE) {
                break;
            }
        }

        gather_planes(current_position, mover, filter);
        push_dynamic_bodies(velocity);
        update_floor_state();

        const Vector3 achieved_velocity = (current_position - start_position) / delta_time;
        b3Body_SetLinearVelocity(body_id, Physics3D::vec3_to_b3Vec3(achieved_velocity));
    }

    bool CharacterBodyComponent3D::is_on_floor() const {
        return m_on_floor;
    }

    Vector3 CharacterBodyComponent3D::get_floor_normal() const {
        return m_floor_normal;
    }

    Vector3 CharacterBodyComponent3D::get_velocity() const {
        return m_rigidbody->get_velocity();
    }

    void CharacterBodyComponent3D::set_velocity(const Vector3& velocity) {
        m_rigidbody->set_velocity(velocity);
    }

    Vector3 CharacterBodyComponent3D::get_position() const {
        return m_rigidbody->get_position();
    }

    void CharacterBodyComponent3D::set_position(const Vector3& position) {
        m_rigidbody->set_position(position);
    }

    Quaternion CharacterBodyComponent3D::get_rotation() const {
        return m_rigidbody->get_rotation();
    }

    void CharacterBodyComponent3D::set_rotation(const Quaternion& rotation) {
        m_rigidbody->set_rotation(rotation);
    }

    void CharacterBodyComponent3D::gather_planes(const Vector3& origin,
                                                 const b3Capsule& mover,
                                                 const b3QueryFilter& filter) {
        m_solver_origin = origin;
        m_solver_planes_count = 0;
        const b3WorldId world_id = get_engine().get_physics_3d().get_physics_world().get_id();
        b3World_CollideMover(world_id, Physics3D::vec3_to_b3Vec3(origin), &mover, filter, plane_result_callback, this);
    }

    void CharacterBodyComponent3D::push_dynamic_bodies(const Vector3& mover_velocity) {
        for (int32_t i = 0; i < m_solver_planes_count; ++i) {
            const b3BodyId body_id = b3Shape_GetBody(m_solver_plane_contacts[i].shape_id);
            if (b3Body_GetType(body_id) != b3_dynamicBody) {
                continue;
            }

            const b3Vec3 point = m_solver_plane_contacts[i].point;
            const b3Vec3 normal = b3Neg(m_solver_planes[i].plane.normal);
            const b3Vec3 r = b3SubPos(point, b3Body_GetWorldCenterOfMass(body_id));
            const b3Vec3 rn = b3Cross(r, normal);
            const float k_normal = b3Body_GetInverseMass(body_id) +
                                   b3Dot(rn, b3MulMV(b3Body_GetWorldInverseRotationalInertia(body_id), rn));
            if (k_normal <= 0.0f) {
                continue;
            }

            const b3Vec3 contact_velocity =
                b3Add(b3Body_GetLinearVelocity(body_id), b3Cross(b3Body_GetAngularVelocity(body_id), r));
            const float normal_velocity =
                b3Dot(b3Sub(contact_velocity, Physics3D::vec3_to_b3Vec3(mover_velocity)), normal);
            const float impulse = std::fmax(-normal_velocity / k_normal, 0.0f);
            if (impulse > 0.0f) {
                b3Body_ApplyLinearImpulse(body_id, b3MulSV(impulse, normal), point, true);
            }
        }
    }

    float CharacterBodyComponent3D::get_min_floor_dot() const {
        return std::cos(m_max_slope_deg * DEG_TO_RAD);
    }

    b3Plane CharacterBodyComponent3D::make_solver_plane(const b3Plane& plane) const {
        const bool is_steep_slope = plane.normal.y > 0.0f && plane.normal.y < get_min_floor_dot();
        if (!is_steep_slope) {
            return plane;
        }

        const b3Vec3 wall_normal = {plane.normal.x, 0.0f, plane.normal.z};
        if (b3LengthSquared(wall_normal) < 1.0e-6f) {
            return plane;
        }

        return {b3Normalize(wall_normal), plane.offset};
    }

    void CharacterBodyComponent3D::update_floor_state() {
        const float min_floor_dot = get_min_floor_dot();
        m_on_floor = false;
        for (int32_t i = 0; i < m_solver_planes_count; ++i) {
            const Vector3 normal = Physics3D::b3Vec3_to_vec3(m_solver_planes[i].plane.normal);
            if (Vector3::dot(normal, Vector3::up()) >= min_floor_dot) {
                m_on_floor = true;
                m_floor_normal = normal;
                return;
            }
        }
    }

    bool CharacterBodyComponent3D::plane_result_callback(b3ShapeId other_shape_id,
                                                         const b3PlaneResult* plane_result,
                                                         int32_t plane_count,
                                                         void* context) {
        auto* self = static_cast<CharacterBodyComponent3D*>(context);
        HOB_ASSERT(self != nullptr, "Null context for plane_result_callback");

        if (b3Shape_IsSensor(other_shape_id)) {
            return true;
        }

        const b3BodyId self_body_id = self->m_rigidbody->get_body_id();
        const b3BodyId other_body_id = b3Shape_GetBody(other_shape_id);
        if (self_body_id.index1 == other_body_id.index1 && self_body_id.generation == other_body_id.generation) {
            return true;
        }

        for (int32_t i = 0; i < plane_count; ++i) {
            if (self->m_solver_planes_count >= SOLVER_PLANES_CAPACITY) {
                return false;
            }

            const int32_t index = self->m_solver_planes_count;
            self->m_solver_planes[index] = {self->make_solver_plane(plane_result[i].plane), MAX_FLOAT, 0.0f, true};
            self->m_solver_plane_contacts[index] = {
                b3OffsetPos(Physics3D::vec3_to_b3Vec3(self->m_solver_origin), plane_result[i].point), other_shape_id};
            self->m_solver_planes_count += 1;
        }

        return true;
    }
} // namespace hob
