#pragma once

#include "collider_component_3d.h"
#include "engine/math/aabb3.h"

namespace hob {
    class BoxColliderComponent3D : public ColliderComponent3D {
        AABB3 m_box = AABB3(Vector3::zero(), Vector3(0.5f, 0.5f, 0.5f));

    public:
        explicit BoxColliderComponent3D(Entity& entity);

        std::string to_string() const override;

        AABB3 get_box() const;
        void set_box(const AABB3& box);

        AABB3 get_scaled_box() const;

    protected:
        b3ShapeId create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) override;
        void update_geometry(const Vector3& scale) override;
        void debug_draw_shape(const Color& color) const override;

    private:
        static AABB3 scale_box(const AABB3& local, const Vector3& scale);
    };
} // namespace hob
