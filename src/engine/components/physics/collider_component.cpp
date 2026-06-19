#include "collider_component.h"

#include <format>

#include <box2d/box2d.h>

#include "engine/components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/entity/entity.h"
#include "rigidbody_component.h"

namespace hob {
    ColliderComponent::ColliderComponent(Entity& entity)
        : Component(entity) {}

    void ColliderComponent::enter_play() {
        const RigidbodyComponent* rigidbody = get_entity().get_rigidbody();
        assert(rigidbody != nullptr && rigidbody->has_body() && "Collider requires a Rigidbody to function");

        build_shape();
    }

    void ColliderComponent::exit_play() {
        if (b2Shape_IsValid(m_shape_id)) {
            b2DestroyShape(m_shape_id, false);
            m_shape_id = b2_nullShapeId;
        }
    }

    void ColliderComponent::debug_draw_tick(float delta_time) {
        Color color;
        if (m_is_trigger) {
            color = Color::cyan();
        }
        else {
            const BodyType body_type = get_entity().get_rigidbody()->get_body_type();
            switch (body_type) {
                case BodyType::Static:
                    color = Color::orange();
                    break;
                case BodyType::Dynamic:
                    color = Color::green();
                    break;
                case BodyType::Kinematic:
                    color = Color::yellow();
                    break;
            }
        }

        if (get_engine().get_physics().cvar_debug_draw) {
            debug_draw_shape(color, get_entity().get_transform()->get_scale());
        }
    }

    std::string ColliderComponent::to_string() const {
        return std::format("ColliderComponent(entity_id = {})", get_entity().get_id());
    }

    b2BodyId ColliderComponent::get_body_id() const {
        const b2BodyId body_id = get_entity().get_rigidbody()->get_body_id();
        return body_id;
    }

    b2ShapeId ColliderComponent::get_shape_id() const {
        return m_shape_id;
    }

    float ColliderComponent::get_density() const {
        return m_density;
    }

    void ColliderComponent::set_density(float density) {
        if (m_density == density) {
            return;
        }

        m_density = density;
        if (b2Shape_IsValid(m_shape_id)) {
            b2Shape_SetDensity(m_shape_id, m_density, true); // true: re-derive the body's mass
        }
    }

    float ColliderComponent::get_friction() const {
        return m_friction;
    }

    void ColliderComponent::set_friction(float friction) {
        if (m_friction == friction) {
            return;
        }

        m_friction = friction;
        if (b2Shape_IsValid(m_shape_id)) {
            b2Shape_SetFriction(m_shape_id, m_friction);
        }
    }

    float ColliderComponent::get_bounciness() const {
        return m_bounciness;
    }

    void ColliderComponent::set_bounciness(float bounciness) {
        if (m_bounciness == bounciness) {
            return;
        }

        m_bounciness = bounciness;
        if (b2Shape_IsValid(m_shape_id)) {
            b2Shape_SetRestitution(m_shape_id, m_bounciness);
        }
    }

    uint64_t ColliderComponent::get_collision_layer() const {
        return m_collision_layer;
    }

    void ColliderComponent::set_collision_layer(uint64_t collision_layer) {
        if (m_collision_layer == collision_layer) {
            return;
        }

        m_collision_layer = collision_layer;
        if (b2Shape_IsValid(m_shape_id)) {
            b2Filter filter = b2Shape_GetFilter(m_shape_id);
            filter.categoryBits = m_collision_layer;
            b2Shape_SetFilter(m_shape_id, filter);
        }
    }

    uint64_t ColliderComponent::get_collision_mask() const {
        return m_collision_mask;
    }

    void ColliderComponent::set_collision_mask(uint64_t collision_mask) {
        if (m_collision_mask == collision_mask) {
            return;
        }

        m_collision_mask = collision_mask;
        if (b2Shape_IsValid(m_shape_id)) {
            b2Filter filter = b2Shape_GetFilter(m_shape_id);
            filter.maskBits = m_collision_mask;
            b2Shape_SetFilter(m_shape_id, filter);
        }
    }

    bool ColliderComponent::is_trigger() const {
        return m_is_trigger;
    }

    void ColliderComponent::set_trigger(bool trigger) {
        if (m_is_trigger == trigger) {
            return;
        }

        m_is_trigger = trigger;
        if (b2Shape_IsValid(m_shape_id)) {
            build_shape(); // b2Shape.isSensor is fixed at shape creation; Box2D has no in-place setter, so rebuild.
        }
    }

    void ColliderComponent::on_geometry_changed() {
        if (!b2Shape_IsValid(m_shape_id)) {
            return;
        }

        update_geometry(get_entity().get_transform()->get_scale());
        b2Body_ApplyMassFromShapes(get_body_id()); // geometry changed -> area changed -> re-derive mass
    }

    void ColliderComponent::build_shape() {
        if (b2Shape_IsValid(m_shape_id)) {
            b2DestroyShape(m_shape_id, false);
            m_shape_id = b2_nullShapeId;
        }

        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = m_density;
        shape_def.material.friction = m_friction;
        shape_def.material.restitution = m_bounciness;
        shape_def.filter.categoryBits = m_collision_layer;
        shape_def.filter.maskBits = m_collision_mask;
        shape_def.isSensor = m_is_trigger;

        m_shape_id = create_geometry(shape_def, get_entity().get_transform()->get_scale());

        b2Shape_SetUserData(m_shape_id, this);
        b2Shape_EnableSensorEvents(m_shape_id, true);
        b2Shape_EnableContactEvents(m_shape_id, !m_is_trigger);
    }
} // namespace hob
