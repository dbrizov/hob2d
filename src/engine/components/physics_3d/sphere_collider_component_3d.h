#pragma once

#include "collider_component_3d.h"

namespace hob {
    class SphereColliderComponent3D : public ColliderComponent3D {
        Vector3 m_center;
        float m_radius = 0.5f;

    public:
        explicit SphereColliderComponent3D(Entity& entity);

        std::string to_string() const override;

        Vector3 get_center() const;
        void set_center(const Vector3& center);

        float get_radius() const;
        void set_radius(float radius);

        float get_scaled_radius() const;

    protected:
        b3ShapeId create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) override;
        void update_geometry(const Vector3& scale) override;
        void debug_draw_shape(const Color& color) const override;

    private:
        static float scale_radius(float radius, const Vector3& scale);
    };
} // namespace hob
