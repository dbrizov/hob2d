#include "box_collider_component.h"

#include <cmath>

#include <box2d/box2d.h>
#include <box2d/collision.h>
#include <box2d/types.h>

#include "engine/components/transform_component.h"
#include "engine/core/debug.h"
#include "engine/entity/entity.h"

namespace hob {
    BoxColliderComponent::BoxColliderComponent(Entity& entity)
        : ColliderComponent(entity) {}

    std::string BoxColliderComponent::to_string() const {
        return "BoxColliderComponent";
    }

    AABB BoxColliderComponent::get_aabb() const {
        return m_aabb;
    }

    void BoxColliderComponent::set_aabb(const AABB& aabb) {
        if (m_aabb == aabb) {
            return;
        }

        m_aabb = aabb;
        on_geometry_changed();
    }

    AABB BoxColliderComponent::get_scaled_aabb() const {
        return scale_aabb(m_aabb, get_entity().get_transform()->get_scale());
    }

    b2ShapeId BoxColliderComponent::create_geometry(const b2ShapeDef& shape_def, const Vector2& scale) {
        const AABB scaled = scale_aabb(m_aabb, scale);
        const b2Polygon box = b2MakeBox(scaled.extents.x, scaled.extents.y);
        return b2CreatePolygonShape(get_body_id(), &shape_def, &box);
    }

    void BoxColliderComponent::update_geometry(const Vector2& scale) {
        const AABB scaled = scale_aabb(m_aabb, scale);
        const b2Polygon box = b2MakeBox(scaled.extents.x, scaled.extents.y);
        b2Shape_SetPolygon(get_shape_id(), &box);
    }

    void BoxColliderComponent::debug_draw_shape(const Color& color, const Vector2& scale) const {
        const TransformComponent* transform = get_entity().get_transform();
        const Vector2 position = transform->get_position();
        const float radians = transform->get_rotation();

        const AABB scaled = scale_aabb(m_aabb, scale);

        Vector2 top_left = position + Vector2::left() * scaled.extents.x + Vector2::up() * scaled.extents.y;
        Vector2 top_right = position + Vector2::right() * scaled.extents.x + Vector2::up() * scaled.extents.y;
        Vector2 bottom_left = position + Vector2::left() * scaled.extents.x + Vector2::down() * scaled.extents.y;
        Vector2 bottom_right = position + Vector2::right() * scaled.extents.x + Vector2::down() * scaled.extents.y;

        top_left = Vector2::rotate_around(top_left, position, radians);
        top_right = Vector2::rotate_around(top_right, position, radians);
        bottom_left = Vector2::rotate_around(bottom_left, position, radians);
        bottom_right = Vector2::rotate_around(bottom_right, position, radians);

        debug::draw_line(top_left, top_right, color);
        debug::draw_line(top_right, bottom_right, color);
        debug::draw_line(bottom_right, bottom_left, color);
        debug::draw_line(bottom_left, top_left, color);
    }

    AABB BoxColliderComponent::scale_aabb(const AABB& local, const Vector2& scale) {
        const Vector2 center(local.center.x * scale.x, local.center.y * scale.y);
        const Vector2 extents(local.extents.x * std::abs(scale.x), local.extents.y * std::abs(scale.y));

        return AABB(center, extents);
    }
} // namespace hob
