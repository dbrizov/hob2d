#pragma once

#include <box3d/id.h>
#include <box3d/types.h>

#include "engine/components/component.h"
#include "engine/core/systems/physics/collision_layer.h"
#include "engine/math/vector3.h"

namespace hob {
    struct Color;

    class ColliderComponent3D : public Component {
        b3ShapeId m_shape_id = b3_nullShapeId;
        float m_density = 1.0f;
        float m_friction = 0.6f;
        float m_bounciness = 0.0f;
        uint64_t m_collision_layer = static_cast<uint64_t>(CollisionLayer::Default);
        uint64_t m_collision_mask = ~0ull;
        bool m_is_trigger = false;

    public:
        explicit ColliderComponent3D(Entity& entity);

        void enter_world() override;
        void exit_world() override;
        void debug_draw_tick(float delta_time) override;

        std::string to_string() const override;

        b3BodyId get_body_id() const;
        b3ShapeId get_shape_id() const;

        float get_density() const;
        void set_density(float density);

        float get_friction() const;
        void set_friction(float friction);

        float get_bounciness() const;
        void set_bounciness(float bounciness);

        uint64_t get_collision_layer() const;
        void set_collision_layer(uint64_t collision_layer);

        uint64_t get_collision_mask() const;
        void set_collision_mask(uint64_t collision_mask);

        bool is_trigger() const;
        void set_trigger(bool trigger);

        void on_geometry_changed();

    protected:
        virtual b3ShapeId create_geometry(const b3ShapeDef& shape_def, const Vector3& scale) = 0;
        virtual void update_geometry(const Vector3& scale) = 0;
        virtual void debug_draw_shape(const Color& color) const = 0;

        Vector3 get_world_scale() const;

    private:
        void build_shape();
    };
} // namespace hob
