#include "collider_component_3d.h"

#include <box3d/box3d.h>

#include "engine/components/transform_component_3d.h"
#include "engine/core/assert.h"
#include "engine/core/engine.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"
#include "engine/math/color.h"
#include "rigidbody_component_3d.h"

namespace hob {
    ColliderComponent3D::ColliderComponent3D(Entity& entity)
        : Component(entity) {}

    void ColliderComponent3D::enter_world() {
        const RigidbodyComponent3D* rigidbody = get_entity().get_rigidbody_3d();
        HOB_ASSERT(rigidbody != nullptr && rigidbody->has_body(), "Collider3D requires a Rigidbody3D to function");

        build_shape();
    }

    void ColliderComponent3D::exit_world() {
        if (b3Shape_IsValid(m_shape_id)) {
            b3DestroyShape(m_shape_id, false);
            m_shape_id = b3_nullShapeId;
        }
    }

    void ColliderComponent3D::debug_draw_tick(float delta_time) {
        (void)delta_time;
        if (!get_engine().get_physics_3d().cvar_show_colliders) {
            return;
        }

        Color color = Color::cyan();
        if (!m_is_trigger) {
            switch (get_entity().get_rigidbody_3d()->get_body_type()) {
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

        debug_draw_shape(color);
    }

    std::string ColliderComponent3D::to_string() const {
        return "ColliderComponent3D";
    }

    b3BodyId ColliderComponent3D::get_body_id() const {
        return get_entity().get_rigidbody_3d()->get_body_id();
    }

    b3ShapeId ColliderComponent3D::get_shape_id() const {
        return m_shape_id;
    }

    float ColliderComponent3D::get_density() const {
        return m_density;
    }

    void ColliderComponent3D::set_density(float density) {
        if (m_density == density) {
            return;
        }

        m_density = density;
        if (b3Shape_IsValid(m_shape_id)) {
            b3Shape_SetDensity(m_shape_id, m_density, true);
        }
    }

    float ColliderComponent3D::get_friction() const {
        return m_friction;
    }

    void ColliderComponent3D::set_friction(float friction) {
        if (m_friction == friction) {
            return;
        }

        m_friction = friction;
        if (b3Shape_IsValid(m_shape_id)) {
            b3Shape_SetFriction(m_shape_id, m_friction);
        }
    }

    float ColliderComponent3D::get_bounciness() const {
        return m_bounciness;
    }

    void ColliderComponent3D::set_bounciness(float bounciness) {
        if (m_bounciness == bounciness) {
            return;
        }

        m_bounciness = bounciness;
        if (b3Shape_IsValid(m_shape_id)) {
            b3Shape_SetRestitution(m_shape_id, m_bounciness);
        }
    }

    uint64_t ColliderComponent3D::get_collision_layer() const {
        return m_collision_layer;
    }

    void ColliderComponent3D::set_collision_layer(uint64_t collision_layer) {
        if (m_collision_layer == collision_layer) {
            return;
        }

        m_collision_layer = collision_layer;
        if (b3Shape_IsValid(m_shape_id)) {
            b3Filter filter = b3Shape_GetFilter(m_shape_id);
            filter.categoryBits = m_collision_layer;
            b3Shape_SetFilter(m_shape_id, filter, true);
        }
    }

    uint64_t ColliderComponent3D::get_collision_mask() const {
        return m_collision_mask;
    }

    void ColliderComponent3D::set_collision_mask(uint64_t collision_mask) {
        if (m_collision_mask == collision_mask) {
            return;
        }

        m_collision_mask = collision_mask;
        if (b3Shape_IsValid(m_shape_id)) {
            b3Filter filter = b3Shape_GetFilter(m_shape_id);
            filter.maskBits = m_collision_mask;
            b3Shape_SetFilter(m_shape_id, filter, true);
        }
    }

    bool ColliderComponent3D::is_trigger() const {
        return m_is_trigger;
    }

    void ColliderComponent3D::set_trigger(bool trigger) {
        if (m_is_trigger == trigger) {
            return;
        }

        m_is_trigger = trigger;
        if (b3Shape_IsValid(m_shape_id)) {
            build_shape();
        }
    }

    void ColliderComponent3D::on_geometry_changed() {
        if (!b3Shape_IsValid(m_shape_id)) {
            return;
        }

        update_geometry(get_world_scale());
        b3Body_ApplyMassFromShapes(get_body_id());
    }

    Vector3 ColliderComponent3D::get_world_scale() const {
        return get_entity().get_transform_3d()->get_lossy_scale();
    }

    void ColliderComponent3D::build_shape() {
        if (b3Shape_IsValid(m_shape_id)) {
            b3DestroyShape(m_shape_id, false);
            m_shape_id = b3_nullShapeId;
        }

        b3ShapeDef shape_def = b3DefaultShapeDef();
        shape_def.density = m_density;
        shape_def.baseMaterial.friction = m_friction;
        shape_def.baseMaterial.restitution = m_bounciness;
        shape_def.filter.categoryBits = m_collision_layer;
        shape_def.filter.maskBits = m_collision_mask;
        shape_def.isSensor = m_is_trigger;
        shape_def.enableSensorEvents = true;
        shape_def.enableContactEvents = !m_is_trigger;

        m_shape_id = create_geometry(shape_def, get_world_scale());

        b3Shape_SetUserData(m_shape_id, this);
    }
} // namespace hob
