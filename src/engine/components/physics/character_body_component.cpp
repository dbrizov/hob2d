#include "character_body_component.h"

#include <box2d/box2d.h>

#include "capsule_collider_component.h"
#include "engine/core/assert.h"
#include "engine/core/engine.h"
#include "engine/core/systems/physics.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"
#include "rigidbody_component.h"

namespace hob {
    CharacterBodyComponent::CharacterBodyComponent(Entity& entity)
        : Component(entity) {
        m_rigidbody = entity.add_component<RigidbodyComponent>();
        m_rigidbody->set_body_type(BodyType::Kinematic);
        m_rigidbody->set_fixed_rotation(true);

        m_capsule_collider = entity.add_component<CapsuleColliderComponent>();
    }

    std::string CharacterBodyComponent::to_string() const {
        return "CharacterBodyComponent";
    }

    Capsule CharacterBodyComponent::get_capsule() const {
        return m_capsule_collider->get_capsule();
    }

    void CharacterBodyComponent::set_capsule(const Capsule& capsule) {
        m_capsule_collider->set_capsule(capsule);
    }

    uint64_t CharacterBodyComponent::get_collision_layer() const {
        return m_capsule_collider->get_collision_layer();
    }

    void CharacterBodyComponent::set_collision_layer(uint64_t collision_layer) {
        m_capsule_collider->set_collision_layer(collision_layer);
    }

    uint64_t CharacterBodyComponent::get_collision_mask() const {
        return m_capsule_collider->get_collision_mask();
    }

    void CharacterBodyComponent::set_collision_mask(uint64_t collision_mask) {
        m_capsule_collider->set_collision_mask(collision_mask);
    }

    uint64_t CharacterBodyComponent::get_solver_ignore_mask() const {
        return m_solver_ignore_mask;
    }

    void CharacterBodyComponent::set_solver_ignore_mask(uint64_t solver_ignore_mask) {
        m_solver_ignore_mask = solver_ignore_mask;
    }

    void CharacterBodyComponent::move_and_slide(const Vector2& velocity, float delta_time) {
        const Vector2 delta_pos = velocity * delta_time;
        if (delta_pos.length_sqr() < EPSILON) {
            b2Body_SetLinearVelocity(m_rigidbody->get_body_id(), b2Vec2_zero);
            return;
        }

        // Solver data
        b2QueryFilter collision_filter = b2DefaultQueryFilter();
        collision_filter.categoryBits = get_collision_layer();
        collision_filter.maskBits = get_collision_mask() & ~m_solver_ignore_mask;

        const Capsule local_capsule = m_capsule_collider->get_scaled_capsule();

        const b2BodyId body_id = m_rigidbody->get_body_id();
        const b2WorldId world_id = get_engine().get_physics().get_physics_world().get_id();

        const b2Vec2 b2_start_pos = b2Body_GetPosition(body_id);
        const b2Rot b2_rot = b2Body_GetRotation(body_id);
        const float radians = Physics::b2Rot_to_radians(b2_rot);

        b2Vec2 b2_current_pos = b2_start_pos;
        const b2Vec2 b2_delta_pos = Physics::vec2_to_b2Vec2(delta_pos);
        const b2Vec2 b2_target_pos = b2Add(b2_current_pos, b2_delta_pos);

        // Solver iterations
        for (int i = 0; i < SOLVER_MAX_ITERATIONS; ++i) {
            m_solver_planes_count = 0;
            const b2Capsule mover = make_world_capsule(local_capsule, Physics::b2Vec2_to_vec2(b2_current_pos), radians);

            // 1) Gather planes at current position
            b2World_CollideMover(world_id, &mover, collision_filter, plane_result_callback, this);

            // 2) Solve for the "best" move toward the target given those planes
            const b2Vec2 b2_desired_delta = b2Sub(b2_target_pos, b2_current_pos);
            const b2PlaneSolverResult b2_solver_result =
                b2SolvePlanes(b2_desired_delta, m_solver_planes, m_solver_planes_count);
            const b2Vec2 b2_solver_translation = b2_solver_result.translation;

            // 3) Cast to make that translation continuous (no tunneling)
            const float fraction = b2World_CastMover(world_id, &mover, b2_solver_translation, collision_filter);
            const b2Vec2 b2_delta = b2MulSV(fraction, b2_solver_translation);

            b2_current_pos = b2Add(b2_current_pos, b2_delta);

            if (b2Dot(b2_delta, b2_delta) < SOLVER_DISTANCE_TOLERANCE * SOLVER_DISTANCE_TOLERANCE) {
                break;
            }
        }

        const b2Vec2 achieved_delta = b2Sub(b2_current_pos, b2_start_pos);
        const b2Vec2 achieved_velocity = b2MulSV(1.0f / delta_time, achieved_delta);
        b2Body_SetLinearVelocity(body_id, achieved_velocity);
    }

    Vector2 CharacterBodyComponent::get_velocity() const {
        return m_rigidbody->get_velocity();
    }

    void CharacterBodyComponent::set_velocity(const Vector2& velocity) {
        m_rigidbody->set_velocity(velocity);
    }

    Vector2 CharacterBodyComponent::get_position() const {
        return m_rigidbody->get_position();
    }

    void CharacterBodyComponent::set_position(const Vector2& position) {
        m_rigidbody->set_position(position);
    }

    float CharacterBodyComponent::get_rotation() const {
        return m_rigidbody->get_rotation();
    }

    void CharacterBodyComponent::set_rotation(float radians) {
        m_rigidbody->set_rotation(radians);
    }

    b2Capsule CharacterBodyComponent::make_world_capsule(const Capsule& local_capsule,
                                                         const Vector2& position,
                                                         float radians) {
        const Vector2 c1_world = Vector2::rotate_around(position + local_capsule.center_a, position, radians);
        const Vector2 c2_world = Vector2::rotate_around(position + local_capsule.center_b, position, radians);

        b2Capsule b2_capsule;
        b2_capsule.center1 = Physics::vec2_to_b2Vec2(c1_world);
        b2_capsule.center2 = Physics::vec2_to_b2Vec2(c2_world);
        b2_capsule.radius = local_capsule.radius;

        return b2_capsule;
    }

    bool CharacterBodyComponent::plane_result_callback(b2ShapeId other_shape_id,
                                                       const b2PlaneResult* plane_result,
                                                       void* context) {
        CharacterBodyComponent* self = static_cast<CharacterBodyComponent*>(context);
        HOB_ASSERT(self != nullptr, "Null context for place_result_callback");

        if (self->m_solver_planes_count >= SOLVER_PLANES_CAPACITY) {
            return false; // stop searching
        }

        const bool plane_hit = plane_result != nullptr && plane_result->hit;
        if (!plane_hit) {
            return true; // no hit - keep searching
        }

        const bool other_shape_is_sensor = b2Shape_IsSensor(other_shape_id);
        if (other_shape_is_sensor) {
            return true; // overlap with sensor/trigger - keep searching
        }

        // Check for collision with self.
        // If the collision filters are set correctly, we won't even get to this point.
        // This only prevents collision with self when using default collision filters.
        const b2BodyId self_body_id = self->m_rigidbody->get_body_id();
        const b2BodyId other_body_id = b2Shape_GetBody(other_shape_id);
        if (self_body_id.index1 == other_body_id.index1 && self_body_id.generation == other_body_id.generation) {
            return true; // ignore self - keep searching
        }

        // TODO optional UserData for pushing objects
        const float push_limit = MAX_FLOAT;
        const bool clip_velocity = true;

        self->m_solver_planes[self->m_solver_planes_count] = {plane_result->plane, push_limit, 0.0f, clip_velocity};
        self->m_solver_planes_count += 1;

        return true; // keep searching
    }
} // namespace hob
