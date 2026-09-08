#include "box_collider_component_3d.h"

#include <box3d/box3d.h>
#include <box3d/collision.h>

#include "engine/components/transform_component_3d.h"
#include "engine/core/debug.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        b3BoxHull make_hull(const AABB3& box) {
            return b3MakeOffsetBoxHull(
                box.extents.x, box.extents.y, box.extents.z, Physics3D::vec3_to_b3Vec3(box.center));
        }
    } // namespace

    BoxColliderComponent3D::BoxColliderComponent3D(Entity& entity)
        : ColliderComponent3D(entity) {}

    std::string BoxColliderComponent3D::to_string() const {
        return "BoxColliderComponent3D";
    }

    AABB3 BoxColliderComponent3D::get_box() const {
        return m_box;
    }

    void BoxColliderComponent3D::set_box(const AABB3& box) {
        if (m_box == box) {
            return;
        }

        m_box = box;
        on_geometry_changed();
    }

    AABB3 BoxColliderComponent3D::get_scaled_box() const {
        return scale_box(m_box, get_world_scale());
    }

    b3ShapeId BoxColliderComponent3D::create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) {
        const b3BoxHull hull = make_hull(scale_box(m_box, scale));
        return b3CreateHullShape(get_body_id(), &shape_def, &hull.base);
    }

    void BoxColliderComponent3D::update_geometry(const Vector3& scale) {
        const b3BoxHull hull = make_hull(scale_box(m_box, scale));
        b3Shape_SetHull(get_shape_id(), &hull.base);
    }

    void BoxColliderComponent3D::debug_draw_shape(const Color& color) const {
        const TransformComponent3D* transform = get_entity().get_transform_3d();
        const Matrix4x4 body_matrix =
            Matrix4x4::trs(transform->get_position(), transform->get_rotation(), Vector3::one());
        debug::draw_aabb3(get_scaled_box(), body_matrix, color);
    }

    AABB3 BoxColliderComponent3D::scale_box(const AABB3& local, const Vector3& scale) {
        return AABB3(Vector3::scale(local.center, scale), Vector3::scale(local.extents, Vector3::abs(scale)));
    }
} // namespace hob
