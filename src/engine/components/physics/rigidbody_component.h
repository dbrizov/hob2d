#pragma once

#include <limits>

#include <box2d/id.h>

#include "engine/components/component.h"
#include "engine/math/vector2.h"

namespace hob {
    using RigidbodyIndex = uint32_t;
    constexpr RigidbodyIndex INVALID_RIGIDBODY_INDEX = std::numeric_limits<RigidbodyIndex>::max();

    enum class BodyType {
        Static,
        Dynamic,
        Kinematic
    };

    class RigidbodyComponent : public Component {
        friend class EntitySpawner;

        b2BodyId m_body_id = b2_nullBodyId;
        BodyType m_body_type = BodyType::Static;
        bool m_has_fixed_rotation = false;

        RigidbodyIndex m_rigidbody_index = INVALID_RIGIDBODY_INDEX; // Slot in EntitySpawner's rigidbody registry

    public:
        explicit RigidbodyComponent(Entity& entity);

        void enter_play() override;
        void exit_play() override;

        std::string to_string() const override;

        b2BodyId get_body_id() const;

        bool has_body() const;
        bool is_awake() const; // True while Box2D is still simulating the body

        BodyType get_body_type() const;
        void set_body_type(BodyType body_type);

        bool has_fixed_rotation() const;
        void set_fixed_rotation(bool has_fixed_rotation);

        Vector2 get_velocity() const;
        void set_velocity(const Vector2& velocity);

        Vector2 get_position() const;
        void set_position(const Vector2& position);

        float get_rotation() const;
        void set_rotation(float radians);
    };
} // namespace hob
