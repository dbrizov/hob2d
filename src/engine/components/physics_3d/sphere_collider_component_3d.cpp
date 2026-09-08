#include "sphere_collider_component_3d.h"

#include <algorithm>
#include <cmath>

#include <box3d/box3d.h>

#include "engine/components/transform_component_3d.h"
#include "engine/core/debug.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"

namespace hob {
    SphereColliderComponent3D::SphereColliderComponent3D(Entity& entity)
        : ColliderComponent3D(entity) {}

    std::string SphereColliderComponent3D::to_string() const {
        return "SphereColliderComponent3D";
    }

    Vector3 SphereColliderComponent3D::get_center() const {
        return m_center;
    }

    void SphereColliderComponent3D::set_center(const Vector3& center) {
        if (m_center == center) {
            return;
        }

        m_center = center;
        on_geometry_changed();
    }

    float SphereColliderComponent3D::get_radius() const {
        return m_radius;
    }

    void SphereColliderComponent3D::set_radius(float radius) {
        if (m_radius == radius) {
            return;
        }

        m_radius = radius;
        on_geometry_changed();
    }

    float SphereColliderComponent3D::get_scaled_radius() const {
        return scale_radius(m_radius, get_world_scale());
    }

    b3ShapeId SphereColliderComponent3D::create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) {
        const b3Sphere sphere{Physics3D::vec3_to_b3Vec3(Vector3::scale(m_center, scale)),
                              scale_radius(m_radius, scale)};
        return b3CreateSphereShape(get_body_id(), &shape_def, &sphere);
    }

    void SphereColliderComponent3D::update_geometry(const Vector3& scale) {
        const b3Sphere sphere{Physics3D::vec3_to_b3Vec3(Vector3::scale(m_center, scale)),
                              scale_radius(m_radius, scale)};
        b3Shape_SetSphere(get_shape_id(), &sphere);
    }

    void SphereColliderComponent3D::debug_draw_shape(const Color& color) const {
        const TransformComponent3D* transform = get_entity().get_transform_3d();
        const Vector3 scaled_center = Vector3::scale(m_center, get_world_scale());
        const Vector3 world_center = transform->get_position() + transform->get_rotation().rotate(scaled_center);
        debug::draw_sphere_3d(world_center, get_scaled_radius(), color);
    }

    float SphereColliderComponent3D::scale_radius(float radius, const Vector3& scale) {
        return radius * std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)});
    }
} // namespace hob
