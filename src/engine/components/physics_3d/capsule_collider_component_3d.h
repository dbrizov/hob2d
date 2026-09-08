#pragma once

#include <box3d/types.h>

#include "collider_component_3d.h"

namespace hob {
    class CapsuleColliderComponent3D : public ColliderComponent3D {
        Vector3 m_center;
        float m_radius = 0.5f;
        float m_height = 2.0f;

    public:
        explicit CapsuleColliderComponent3D(Entity& entity);

        std::string to_string() const override;

        Vector3 get_center() const;
        void set_center(const Vector3& center);

        float get_radius() const;
        void set_radius(float radius);

        float get_height() const;
        void set_height(float height);

        b3Capsule get_scaled_capsule() const;

    protected:
        b3ShapeId create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) override;
        void update_geometry(const Vector3& scale) override;
        void debug_draw_shape(const Color& color) const override;

    private:
        b3Capsule make_capsule(const Vector3& scale) const;
    };
} // namespace hob
