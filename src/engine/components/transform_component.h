#pragma once

#include <vector>

#include "component.h"
#include "engine/math/matrix2x3.h"
#include "engine/math/vector2.h"

namespace hob {
    class TransformComponent : public Component {
        friend class Physics;
        friend class EntitySpawner;

        // Authoritative state is local (relative to the parent).
        Vector2 m_local_position;
        float m_local_rotation = 0.0f; // In radians
        Vector2 m_local_scale = Vector2(1.0f, 1.0f);

        Matrix2x3 m_local_matrix;
        Matrix2x3 m_prev_local_matrix; // Used for Physics interpolation

        // World matrix is derived (parent_world * local) and cached with a downward dirty flag.
        mutable Matrix2x3 m_world_matrix;
        mutable bool m_world_matrix_dirty = true;

        // "The world transform changed since the renderer last consumed it" -- lets the
        // world draw pass skip re-resolving a sprite whose transform hasn't moved.
        bool m_render_dirty = true;

        TransformComponent* m_parent = nullptr;
        std::vector<TransformComponent*> m_children;

        bool m_interpolate_physics = false;

    public:
        explicit TransformComponent(Entity& entity);

        void exit_play() override;

        std::string to_string() const override;

        Vector2 get_position() const;
        void set_position(const Vector2& position);

        float get_rotation() const;
        void set_rotation(float radians);

        Vector2 get_scale() const;
        void set_scale(const Vector2& scale);

        Vector2 get_local_position() const;
        void set_local_position(const Vector2& position);

        float get_local_rotation() const;
        void set_local_rotation(float radians);

        Vector2 get_local_scale() const;
        void set_local_scale(const Vector2& scale);

        const Matrix2x3& get_world_matrix() const;
        Matrix2x3 get_prev_world_matrix() const;

        const Matrix2x3& get_local_matrix() const;
        const Matrix2x3& get_prev_local_matrix() const;

        TransformComponent* get_parent() const;
        void set_parent(TransformComponent* parent, bool keep_world_transform = true);
        const std::vector<TransformComponent*>& get_children() const;

        bool get_interpolate_physics() const;
        void set_interpolate_physics(bool value);

        bool consume_render_dirty();

    private:
        void mark_world_matrix_dirty();
        void rebuild_local_matrix();
        void set_prev_local_matrix(const Matrix2x3& prev_local_matrix);
        bool is_ancestor_of(const TransformComponent* node) const;
        void detach_from_hierarchy();
    };
} // namespace hob
