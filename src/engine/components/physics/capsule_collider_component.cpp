#include "capsule_collider_component.h"

#include <cmath>

#include <box2d/types.h>

#include "engine/components/transform_component.h"
#include "engine/core/debug.h"
#include "engine/core/systems/physics.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"

namespace hob {
    CapsuleColliderComponent::CapsuleColliderComponent(Entity& entity)
        : ColliderComponent(entity) {}

    std::string CapsuleColliderComponent::to_string() const {
        return "CapsuleColliderComponent";
    }

    Capsule CapsuleColliderComponent::get_capsule() const {
        return m_capsule;
    }

    void CapsuleColliderComponent::set_capsule(const Capsule& capsule) {
        if (m_capsule == capsule) {
            return;
        }

        m_capsule = capsule;
        on_geometry_changed();
    }

    Capsule CapsuleColliderComponent::get_scaled_capsule() const {
        return scale_capsule(m_capsule, get_entity().get_transform()->get_scale());
    }

    b2ShapeId CapsuleColliderComponent::create_geometry(const b2ShapeDef& shape_def, const Vector2& scale) {
        const Capsule scaled_capsule = scale_capsule(m_capsule, scale);
        const b2Capsule b2_capsule = Physics::capsule_to_b2Capsule(scaled_capsule);
        return b2CreateCapsuleShape(get_body_id(), &shape_def, &b2_capsule);
    }

    void CapsuleColliderComponent::update_geometry(const Vector2& scale) {
        const Capsule scaled_capsule = scale_capsule(m_capsule, scale);
        const b2Capsule b2_capsule = Physics::capsule_to_b2Capsule(scaled_capsule);
        b2Shape_SetCapsule(get_shape_id(), &b2_capsule);
    }

    void CapsuleColliderComponent::debug_draw_shape(const Color& color, const Vector2& scale) const {
        const TransformComponent* transform = get_entity().get_transform();
        const Vector2 position = transform->get_position();
        const float radians = transform->get_rotation();

        const Capsule scaled = scale_capsule(m_capsule, scale);

        const Vector2 c1_world = Vector2::rotate_around(position + scaled.center_a, position, radians);
        const Vector2 c2_world = Vector2::rotate_around(position + scaled.center_b, position, radians);

        Vector2 axis = (c2_world - c1_world);
        if (axis.length_sqr() > EPSILON) {
            axis = axis.normalized();
        }
        else {
            axis = Vector2::up(); // Arbitrary if it's basically a circle
        }

        const Vector2 perp(-axis.y, axis.x);

        const Vector2 p1 = c1_world + perp * scaled.radius;
        const Vector2 p2 = c2_world + perp * scaled.radius;
        const Vector2 p3 = c2_world - perp * scaled.radius;
        const Vector2 p4 = c1_world - perp * scaled.radius;

        debug::draw_line(p1, p2, color);
        debug::draw_line(p3, p4, color);

        debug::draw_circle(c1_world, scaled.radius, color);
        debug::draw_circle(c2_world, scaled.radius, color);
    }

    Capsule CapsuleColliderComponent::scale_capsule(const Capsule& local, const Vector2& scale) {
        const Vector2 center_a(local.center_a.x * scale.x, local.center_a.y * scale.y);
        const Vector2 center_b(local.center_b.x * scale.x, local.center_b.y * scale.y);
        const float radius = local.radius * std::max(std::abs(scale.x), std::abs(scale.y));

        return Capsule(center_a, center_b, radius);
    }
} // namespace hob
