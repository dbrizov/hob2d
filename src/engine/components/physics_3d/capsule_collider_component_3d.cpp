#include "capsule_collider_component_3d.h"

#include <algorithm>
#include <cmath>

#include <box3d/box3d.h>

#include "engine/components/transform_component_3d.h"
#include "engine/core/debug.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"

namespace hob {
    CapsuleColliderComponent3D::CapsuleColliderComponent3D(Entity& entity)
        : ColliderComponent3D(entity) {}

    std::string CapsuleColliderComponent3D::to_string() const {
        return "CapsuleColliderComponent3D";
    }

    Vector3 CapsuleColliderComponent3D::get_center() const {
        return m_center;
    }

    void CapsuleColliderComponent3D::set_center(const Vector3& center) {
        if (m_center == center) {
            return;
        }

        m_center = center;
        on_geometry_changed();
    }

    float CapsuleColliderComponent3D::get_radius() const {
        return m_radius;
    }

    void CapsuleColliderComponent3D::set_radius(float radius) {
        if (m_radius == radius) {
            return;
        }

        m_radius = radius;
        on_geometry_changed();
    }

    float CapsuleColliderComponent3D::get_height() const {
        return m_height;
    }

    void CapsuleColliderComponent3D::set_height(float height) {
        if (m_height == height) {
            return;
        }

        m_height = height;
        on_geometry_changed();
    }

    b3Capsule CapsuleColliderComponent3D::get_scaled_capsule() const {
        return make_capsule(get_world_scale());
    }

    b3ShapeId CapsuleColliderComponent3D::create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) {
        const b3Capsule capsule = make_capsule(scale);
        return b3CreateCapsuleShape(get_body_id(), &shape_def, &capsule);
    }

    void CapsuleColliderComponent3D::update_geometry(const Vector3& scale) {
        const b3Capsule capsule = make_capsule(scale);
        b3Shape_SetCapsule(get_shape_id(), &capsule);
    }

    void CapsuleColliderComponent3D::debug_draw_shape(const Color& color) const {
        const TransformComponent3D* transform = get_entity().get_transform_3d();
        const b3Capsule capsule = get_scaled_capsule();
        const Vector3 a =
            transform->get_position() + transform->get_rotation().rotate(Physics3D::b3Vec3_to_vec3(capsule.center1));
        const Vector3 b =
            transform->get_position() + transform->get_rotation().rotate(Physics3D::b3Vec3_to_vec3(capsule.center2));
        debug::draw_capsule_3d(a, b, capsule.radius, color);
    }

    b3Capsule CapsuleColliderComponent3D::make_capsule(const Vector3& scale) const {
        const float radial_scale = std::max(std::abs(scale.x), std::abs(scale.z));
        const float radius = m_radius * radial_scale;
        const float height = m_height * std::abs(scale.y);
        const float half_segment = std::max(height * 0.5f - radius, 0.0f);
        const Vector3 center = Vector3::scale(m_center, scale);

        b3Capsule capsule;
        capsule.center1 = Physics3D::vec3_to_b3Vec3(center - Vector3(0.0f, half_segment, 0.0f));
        capsule.center2 = Physics3D::vec3_to_b3Vec3(center + Vector3(0.0f, half_segment, 0.0f));
        capsule.radius = radius;
        return capsule;
    }
} // namespace hob
