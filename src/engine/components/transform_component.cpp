#include "transform_component.h"

#include <cmath>
#include <format>

#include "engine/components/physics/collider_component.h"
#include "engine/core/logging.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"

namespace hob {
    TransformComponent::TransformComponent(Entity& entity)
        : Component(entity) {
        rebuild_local_matrix();
    }

    void TransformComponent::exit_play() {
        detach_from_hierarchy();
    }

    std::string TransformComponent::to_string() const {
        return std::format("TransformComponent(entity_id = {})", get_entity().get_id());
    }

    Vector2 TransformComponent::get_position() const {
        return (m_parent != nullptr) ? get_world_matrix().origin : m_local_position;
    }

    void TransformComponent::set_position(const Vector2& position) {
        if (m_parent != nullptr) {
            set_local_position(m_parent->get_world_matrix().inverse().transform_point(position));
        }
        else {
            set_local_position(position);
        }
    }

    float TransformComponent::get_rotation() const {
        return (m_parent != nullptr) ? get_world_matrix().get_rotation() : m_local_rotation;
    }

    void TransformComponent::set_rotation(float radians) {
        const float parent_rotation = (m_parent != nullptr) ? m_parent->get_world_matrix().get_rotation() : 0.0f;
        set_local_rotation(radians - parent_rotation);
    }

    Vector2 TransformComponent::get_scale() const {
        return (m_parent != nullptr) ? get_world_matrix().get_scale() : m_local_scale;
    }

    void TransformComponent::set_scale(const Vector2& scale) {
        if (m_parent == nullptr) {
            set_local_scale(scale);
            return;
        }

        // Lossy when the parent has non-uniform scale combined with child rotation (shear).
        const Vector2 parent_scale = m_parent->get_world_matrix().get_scale();
        const float sx = (std::abs(parent_scale.x) > EPSILON) ? (scale.x / parent_scale.x) : scale.x;
        const float sy = (std::abs(parent_scale.y) > EPSILON) ? (scale.y / parent_scale.y) : scale.y;
        set_local_scale(Vector2(sx, sy));
    }

    Vector2 TransformComponent::get_local_position() const {
        return m_local_position;
    }

    void TransformComponent::set_local_position(const Vector2& position) {
        if (m_local_position == position) {
            return;
        }

        m_local_position = position;
        m_local_matrix.origin = m_local_position;

        if (!get_entity().is_in_play()) {
            // The entity isn't spawned yet. Match local matrices to prevent initial Physics interpolation.
            // Only the origin changed here, so copying the whole matrix is equivalent to rebuilding it.
            set_prev_local_matrix(m_local_matrix);
        }

        mark_world_matrix_dirty();
    }

    float TransformComponent::get_local_rotation() const {
        return m_local_rotation;
    }

    void TransformComponent::set_local_rotation(float radians) {
        if (m_local_rotation == radians) {
            return;
        }

        m_local_rotation = radians;
        rebuild_local_matrix();
        mark_world_matrix_dirty();
    }

    Vector2 TransformComponent::get_local_scale() const {
        return m_local_scale;
    }

    void TransformComponent::set_local_scale(const Vector2& scale) {
        if (m_local_scale == scale) {
            return;
        }

        m_local_scale = scale;
        rebuild_local_matrix();
        mark_world_matrix_dirty();

        // Scale feeds into the physics shape geometry; have each collider re-sync.
        for (ColliderComponent* collider : get_entity().get_components<ColliderComponent>()) {
            collider->on_geometry_changed();
        }
    }

    const Matrix2x3& TransformComponent::get_world_matrix() const {
        if (m_world_matrix_dirty) {
            m_world_matrix = (m_parent != nullptr) ? m_parent->get_world_matrix() * m_local_matrix : m_local_matrix;
            m_world_matrix_dirty = false;
        }

        return m_world_matrix;
    }

    Matrix2x3 TransformComponent::get_prev_world_matrix() const {
        return (m_parent != nullptr) ? m_parent->get_prev_world_matrix() * m_prev_local_matrix : m_prev_local_matrix;
    }

    const Matrix2x3& TransformComponent::get_local_matrix() const {
        return m_local_matrix;
    }

    const Matrix2x3& TransformComponent::get_prev_local_matrix() const {
        return m_prev_local_matrix;
    }

    TransformComponent* TransformComponent::get_parent() const {
        return m_parent;
    }

    void TransformComponent::set_parent(TransformComponent* parent, bool keep_world_transform) {
        if (parent == m_parent) {
            return;
        }

        if (parent != nullptr && is_ancestor_of(parent)) {
            // 'parent' is this node or one of its descendants -> would create a cycle.
            log::engine.error("TransformComponent::set_parent: cannot create a cyclic hierarchy (entity_id = {})",
                              get_entity().get_id());
            return;
        }

        // Capture the current world transform before relinking; consumed only when keeping world transform.
        const Matrix2x3 world = get_world_matrix();

        if (m_parent != nullptr) {
            std::erase(m_parent->m_children, this);
        }

        m_parent = parent;

        if (m_parent != nullptr) {
            m_parent->m_children.push_back(this);
        }

        const Vector2 old_world_scale = world.get_scale();

        if (keep_world_transform) {
            // Back-solve local against the new parent in one pass (vs. set_position/rotation/scale,
            // which would re-fetch the parent world, rebuild, and dirty the subtree three times).
            if (m_parent != nullptr) {
                const Matrix2x3 parent_world = m_parent->get_world_matrix();
                const Vector2 parent_scale = parent_world.get_scale();

                m_local_position = parent_world.inverse().transform_point(world.origin);
                m_local_rotation = world.get_rotation() - parent_world.get_rotation();
                m_local_scale = Vector2(
                    (std::abs(parent_scale.x) > EPSILON) ? (old_world_scale.x / parent_scale.x) : old_world_scale.x,
                    (std::abs(parent_scale.y) > EPSILON) ? (old_world_scale.y / parent_scale.y) : old_world_scale.y);
            }
            else {
                m_local_position = world.origin;
                m_local_rotation = world.get_rotation();
                m_local_scale = old_world_scale;
            }

            rebuild_local_matrix();
        }

        mark_world_matrix_dirty();

        // Resync prev to the new parent frame so the next render doesn't interpolate across the reparent
        // (a non-physics child's prev is never otherwise refreshed -> it would teleport every frame).
        set_prev_local_matrix(m_local_matrix);

        // Colliders use WORLD scale, which keep_world_transform preserves; resync only if it changed.
        if (get_world_matrix().get_scale() != old_world_scale) {
            for (ColliderComponent* collider : get_entity().get_components<ColliderComponent>()) {
                collider->on_geometry_changed();
            }
        }
    }

    const std::vector<TransformComponent*>& TransformComponent::get_children() const {
        return m_children;
    }

    bool TransformComponent::get_interpolate_physics() const {
        return m_interpolate_physics;
    }

    void TransformComponent::set_interpolate_physics(bool value) {
        if (m_interpolate_physics == value) {
            return;
        }

        m_interpolate_physics = value;
        if (value) {
            // Bring prev back in sync so the first render after re-enabling doesn't lerp from a stale matrix.
            set_prev_local_matrix(m_local_matrix);
        }
    }

    void TransformComponent::rebuild_local_matrix() {
        const float cos = std::cos(m_local_rotation);
        const float sin = std::sin(m_local_rotation);

        m_local_matrix.x = Vector2(cos * m_local_scale.x, sin * m_local_scale.x);
        m_local_matrix.y = Vector2(-sin * m_local_scale.y, cos * m_local_scale.y);
        m_local_matrix.origin = m_local_position;

        if (!get_entity().is_in_play()) {
            // The entity isn't spawned yet. Match local matrices to prevent initial Physics interpolation.
            set_prev_local_matrix(m_local_matrix);
        }
    }

    void TransformComponent::set_prev_local_matrix(const Matrix2x3& prev_local_matrix) {
        m_prev_local_matrix = prev_local_matrix;
    }

    bool TransformComponent::consume_render_dirty() {
        const bool was_dirty = m_render_dirty;
        m_render_dirty = false;
        return was_dirty;
    }

    void TransformComponent::mark_world_matrix_dirty() {
        m_world_matrix_dirty = true;
        m_render_dirty = true;
        for (TransformComponent* child : m_children) {
            child->mark_world_matrix_dirty();
        }
    }

    bool TransformComponent::is_ancestor_of(const TransformComponent* node) const {
        const TransformComponent* current = node;
        while (current != nullptr) {
            if (current == this) {
                return true;
            }

            current = current->m_parent;
        }

        return false;
    }

    void TransformComponent::detach_from_hierarchy() {
        set_parent(nullptr, false);

        // Orphan the children. Copy first because set_parent() mutates m_children as each child detaches.
        const std::vector<TransformComponent*> children = m_children;
        for (TransformComponent* child : children) {
            child->set_parent(nullptr, false);
        }
    }
} // namespace hob
