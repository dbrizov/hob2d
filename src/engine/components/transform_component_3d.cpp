#include "transform_component_3d.h"

#include <cmath>

#include "engine/components/physics_3d/collider_component_3d.h"
#include "engine/core/logging.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"

namespace hob {
    namespace {
        Vector3 divide_scale(const Vector3& scale, const Vector3& by) {
            return Vector3((std::abs(by.x) > EPSILON) ? (scale.x / by.x) : scale.x,
                           (std::abs(by.y) > EPSILON) ? (scale.y / by.y) : scale.y,
                           (std::abs(by.z) > EPSILON) ? (scale.z / by.z) : scale.z);
        }
    } // namespace

    TransformComponent3D::TransformComponent3D(Entity& entity)
        : Component(entity) {
        rebuild_local_matrix();
    }

    void TransformComponent3D::exit_world() {
        detach_from_hierarchy();
    }

    std::string TransformComponent3D::to_string() const {
        return "TransformComponent3D";
    }

    Vector3 TransformComponent3D::get_position() const {
        return (m_parent != nullptr) ? get_world_matrix().get_translation() : m_local_position;
    }

    void TransformComponent3D::set_position(const Vector3& position) {
        if (m_parent != nullptr) {
            set_local_position(m_parent->get_world_matrix().inverse_affine().transform_point(position));
        }
        else {
            set_local_position(position);
        }
    }

    Quaternion TransformComponent3D::get_rotation() const {
        return (m_parent != nullptr) ? m_parent->get_rotation() * m_local_rotation : m_local_rotation;
    }

    void TransformComponent3D::set_rotation(const Quaternion& rotation) {
        if (m_parent != nullptr) {
            set_local_rotation(m_parent->get_rotation().inverse() * rotation);
        }
        else {
            set_local_rotation(rotation);
        }
    }

    Vector3 TransformComponent3D::get_euler_deg() const {
        return get_rotation().to_euler_deg();
    }

    void TransformComponent3D::set_euler_deg(const Vector3& euler_deg) {
        set_rotation(Quaternion::from_euler_deg(euler_deg));
    }

    Vector3 TransformComponent3D::get_lossy_scale() const {
        return (m_parent != nullptr) ? get_world_matrix().get_scale() : m_local_scale;
    }

    Vector3 TransformComponent3D::get_local_position() const {
        return m_local_position;
    }

    void TransformComponent3D::set_local_position(const Vector3& position) {
        if (m_local_position == position) {
            return;
        }

        m_local_position = position;
        rebuild_local_matrix();
        mark_world_matrix_dirty();
    }

    Quaternion TransformComponent3D::get_local_rotation() const {
        return m_local_rotation;
    }

    void TransformComponent3D::set_local_rotation(const Quaternion& rotation) {
        const Quaternion normalized = rotation.normalized();
        if (m_local_rotation == normalized) {
            return;
        }

        m_local_rotation = normalized;
        rebuild_local_matrix();
        mark_world_matrix_dirty();
    }

    Vector3 TransformComponent3D::get_local_euler_deg() const {
        return m_local_rotation.to_euler_deg();
    }

    void TransformComponent3D::set_local_euler_deg(const Vector3& euler_deg) {
        set_local_rotation(Quaternion::from_euler_deg(euler_deg));
    }

    Vector3 TransformComponent3D::get_local_scale() const {
        return m_local_scale;
    }

    void TransformComponent3D::set_local_scale(const Vector3& scale) {
        if (m_local_scale == scale) {
            return;
        }

        m_local_scale = scale;
        rebuild_local_matrix();
        mark_world_matrix_dirty();
        notify_scale_changed();
    }

    Vector3 TransformComponent3D::get_forward() const {
        return get_rotation().get_forward();
    }

    Vector3 TransformComponent3D::get_right() const {
        return get_rotation().get_right();
    }

    Vector3 TransformComponent3D::get_up() const {
        return get_rotation().get_up();
    }

    const Matrix4x4& TransformComponent3D::get_world_matrix() const {
        if (m_world_matrix_dirty) {
            m_world_matrix = (m_parent != nullptr) ? m_parent->get_world_matrix() * m_local_matrix : m_local_matrix;
            m_world_matrix_dirty = false;
        }

        return m_world_matrix;
    }

    Matrix4x4 TransformComponent3D::get_prev_world_matrix() const {
        const Matrix4x4 prev_local = Matrix4x4::trs(m_prev_local_position, m_prev_local_rotation, m_local_scale);
        return (m_parent != nullptr) ? m_parent->get_prev_world_matrix() * prev_local : prev_local;
    }

    Matrix4x4 TransformComponent3D::get_interpolated_world_matrix(float fraction) const {
        const Matrix4x4 local = Matrix4x4::trs(Vector3::lerp(m_prev_local_position, m_local_position, fraction),
                                               Quaternion::slerp(m_prev_local_rotation, m_local_rotation, fraction),
                                               m_local_scale);
        return (m_parent != nullptr) ? m_parent->get_interpolated_world_matrix(fraction) * local : local;
    }

    const Matrix4x4& TransformComponent3D::get_local_matrix() const {
        return m_local_matrix;
    }

    TransformComponent3D* TransformComponent3D::get_parent() const {
        return m_parent;
    }

    void TransformComponent3D::set_parent(TransformComponent3D* parent, bool keep_world_transform) {
        if (parent == m_parent) {
            return;
        }

        if (parent != nullptr && is_ancestor_of(parent)) {
            log::engine.error("TransformComponent3D::set_parent: cannot create a cyclic hierarchy (entity_id = {})",
                              get_entity().get_id());
            return;
        }

        const Vector3 world_position = get_position();
        const Quaternion world_rotation = get_rotation();
        const Vector3 old_world_scale = get_lossy_scale();

        if (m_parent != nullptr) {
            std::erase(m_parent->m_children, this);
        }

        m_parent = parent;

        if (m_parent != nullptr) {
            m_parent->m_children.push_back(this);
        }

        if (keep_world_transform) {
            if (m_parent != nullptr) {
                m_local_position = m_parent->get_world_matrix().inverse_affine().transform_point(world_position);
                m_local_rotation = m_parent->get_rotation().inverse() * world_rotation;
                m_local_scale = divide_scale(old_world_scale, m_parent->get_lossy_scale());
            }
            else {
                m_local_position = world_position;
                m_local_rotation = world_rotation;
                m_local_scale = old_world_scale;
            }

            rebuild_local_matrix();
        }

        mark_world_matrix_dirty();
        set_prev_local_pose(m_local_position, m_local_rotation);

        if (get_lossy_scale() != old_world_scale) {
            notify_scale_changed();
        }
    }

    const std::vector<TransformComponent3D*>& TransformComponent3D::get_children() const {
        return m_children;
    }

    bool TransformComponent3D::get_interpolate_physics() const {
        return m_interpolate_physics;
    }

    void TransformComponent3D::set_interpolate_physics(bool value) {
        if (m_interpolate_physics == value) {
            return;
        }

        m_interpolate_physics = value;
        if (value) {
            set_prev_local_pose(m_local_position, m_local_rotation);
        }
    }

    bool TransformComponent3D::consume_render_dirty() {
        const bool was_dirty = m_render_dirty;
        m_render_dirty = false;
        return was_dirty;
    }

    void TransformComponent3D::mark_world_matrix_dirty() {
        m_world_matrix_dirty = true;
        m_render_dirty = true;
        for (TransformComponent3D* child : m_children) {
            child->mark_world_matrix_dirty();
        }
    }

    void TransformComponent3D::rebuild_local_matrix() {
        m_local_matrix = Matrix4x4::trs(m_local_position, m_local_rotation, m_local_scale);

        if (!get_entity().is_in_play()) {
            set_prev_local_pose(m_local_position, m_local_rotation);
        }
    }

    void TransformComponent3D::set_prev_local_pose(const Vector3& position, const Quaternion& rotation) {
        m_prev_local_position = position;
        m_prev_local_rotation = rotation;
    }

    void TransformComponent3D::notify_scale_changed() {
        get_entity().for_each_component<ColliderComponent3D>([](ColliderComponent3D* collider) {
            collider->on_geometry_changed();
        });
    }

    bool TransformComponent3D::is_ancestor_of(const TransformComponent3D* node) const {
        const TransformComponent3D* current = node;
        while (current != nullptr) {
            if (current == this) {
                return true;
            }

            current = current->m_parent;
        }

        return false;
    }

    void TransformComponent3D::detach_from_hierarchy() {
        set_parent(nullptr, false);

        const std::vector<TransformComponent3D*> children = m_children;
        for (TransformComponent3D* child : children) {
            child->set_parent(nullptr, false);
        }
    }
} // namespace hob
