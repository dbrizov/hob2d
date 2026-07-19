#include "rigidbody_component.h"

#include <box2d/box2d.h>

#include "engine//components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"

namespace hob {
    RigidbodyComponent::RigidbodyComponent(Entity& entity)
        : Component(entity) {}

    void RigidbodyComponent::enter_play() {
        const TransformComponent* transform = get_entity().get_transform();

        // Warn when a dynamic body is parented under a moving (non-static) transform: the body is
        // simulated in world space, so the parent's motion won't drive it as a transform hierarchy implies.
        if (m_body_type == BodyType::Dynamic) {
            if (const TransformComponent* parent = transform->get_parent()) {
                const RigidbodyComponent* parent_body = parent->get_entity().get_rigidbody();
                const bool parent_is_static =
                    parent_body != nullptr && parent_body->get_body_type() == BodyType::Static;

                if (!parent_is_static) {
                    log::physics.error(
                        "RigidbodyComponent: dynamic body (entity_id = {}) is parented under a non-static transform; "
                        "parent motion will not drive the simulated body.",
                        get_entity().get_id());
                }
            }
        }

        const Vector2 position = transform->get_position();
        const float radians = transform->get_rotation();

        b2BodyDef body_def = b2DefaultBodyDef();
        body_def.position = Physics::vec2_to_b2Vec2(position);
        body_def.rotation = Physics::radians_to_b2Rot(radians);

        switch (m_body_type) {
            case BodyType::Static:
                body_def.type = b2_staticBody;
                break;
            case BodyType::Dynamic:
                body_def.type = b2_dynamicBody;
                break;
            case BodyType::Kinematic:
                body_def.type = b2_kinematicBody;
                break;
        }

        body_def.fixedRotation = m_has_fixed_rotation;

        const PhysicsWorld& physics_world = get_engine().get_physics().get_physics_world();
        m_body_id = b2CreateBody(physics_world.get_id(), &body_def);

        b2Body_SetUserData(m_body_id, this);

        if (m_body_type != BodyType::Static) {
            get_engine().get_entity_spawner().register_simulated_rigidbody(this);
        }
    }

    void RigidbodyComponent::exit_play() {
        if (m_body_type != BodyType::Static) {
            get_engine().get_entity_spawner().unregister_simulated_rigidbody(this);
        }

        if (b2Body_IsValid(m_body_id)) {
            b2DestroyBody(m_body_id);
            m_body_id = b2_nullBodyId;
        }
    }

    std::string RigidbodyComponent::to_string() const {
        return "RigidbodyComponent";
    }

    b2BodyId RigidbodyComponent::get_body_id() const {
        return m_body_id;
    }

    bool RigidbodyComponent::has_body() const {
        return b2Body_IsValid(m_body_id);
    }

    bool RigidbodyComponent::is_awake() const {
        return b2Body_IsAwake(m_body_id);
    }

    BodyType RigidbodyComponent::get_body_type() const {
        return m_body_type;
    }

    void RigidbodyComponent::set_body_type(BodyType body_type) {
        m_body_type = body_type;
    }

    bool RigidbodyComponent::has_fixed_rotation() const {
        return m_has_fixed_rotation;
    }

    void RigidbodyComponent::set_fixed_rotation(bool has_fixed_rotation) {
        m_has_fixed_rotation = has_fixed_rotation;
    }

    Vector2 RigidbodyComponent::get_velocity() const {
        const b2Vec2 b2_velocity = b2Body_GetLinearVelocity(m_body_id);
        const Vector2 velocity = Physics::b2Vec2_to_vec2(b2_velocity);

        return velocity;
    }

    void RigidbodyComponent::set_velocity(const Vector2& velocity) {
        const b2Vec2 b2_velocity = Physics::vec2_to_b2Vec2(velocity);
        b2Body_SetLinearVelocity(m_body_id, b2_velocity);
    }

    Vector2 RigidbodyComponent::get_position() const {
        const b2Vec2 b2_position = b2Body_GetPosition(m_body_id);
        const Vector2 position = Physics::b2Vec2_to_vec2(b2_position);

        return position;
    }

    void RigidbodyComponent::set_position(const Vector2& position) {
        const b2Vec2 b2_position = Physics::vec2_to_b2Vec2(position);
        const b2Rot b2_rotation = b2Body_GetRotation(m_body_id);
        b2Body_SetTransform(m_body_id, b2_position, b2_rotation);
    }

    float RigidbodyComponent::get_rotation() const {
        const b2Rot b2_rotation = b2Body_GetRotation(m_body_id);
        const float radians = Physics::b2Rot_to_radians(b2_rotation);

        return radians;
    }

    void RigidbodyComponent::set_rotation(float radians) {
        const b2Vec2 b2_position = b2Body_GetPosition(m_body_id);
        const b2Rot b2_rotation = Physics::radians_to_b2Rot(radians);
        b2Body_SetTransform(m_body_id, b2_position, b2_rotation);
    }
} // namespace hob
