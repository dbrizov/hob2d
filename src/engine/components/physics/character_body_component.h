#pragma once

#include <box2d/collision.h>
#include <box2d/id.h>

#include "engine/components/component.h"
#include "engine/math/vector2.h"

namespace hob {
    struct Capsule;
    class CapsuleColliderComponent;
    class RigidbodyComponent;

    class CharacterBodyComponent : public Component {
        RigidbodyComponent* m_rigidbody = nullptr;
        CapsuleColliderComponent* m_capsule_collider = nullptr;

        static constexpr int SOLVER_MAX_ITERATIONS = 4;
        static constexpr float SOLVER_DISTANCE_TOLERANCE = 0.01f;
        static constexpr int SOLVER_PLANES_CAPACITY = 8;
        int m_solver_planes_count = 0;
        b2CollisionPlane m_solver_planes[SOLVER_PLANES_CAPACITY] = {};

        // - We want the character to receive collision/trigger events from other colliders/triggers,
        // but also to ignore the collisions with these colliders/triggers during movement.
        // - We do this by including these colliders/triggers in the capsule's m_collision_mask,
        // but we ignore them during movement via the m_solver_ignore_mask.
        uint64_t m_solver_ignore_mask = 0u;

    public:
        explicit CharacterBodyComponent(Entity& entity);

        std::string to_string() const override;

        Capsule get_capsule() const;
        void set_capsule(const Capsule& capsule);

        uint64_t get_collision_layer() const;
        void set_collision_layer(uint64_t collision_layer);

        uint64_t get_collision_mask() const;
        void set_collision_mask(uint64_t collision_mask);

        uint64_t get_solver_ignore_mask() const;
        void set_solver_ignore_mask(uint64_t solver_ignore_mask);

        void move_and_slide(const Vector2& velocity, float delta_time);

        Vector2 get_velocity() const;
        void set_velocity(const Vector2& velocity);

        Vector2 get_position() const;
        void set_position(const Vector2& position);

        float get_rotation() const;
        void set_rotation(float radians);

    private:
        static b2Capsule make_world_capsule(const Capsule& local_capsule, const Vector2& position, float radians);

        static bool plane_result_callback(b2ShapeId other_shape_id, const b2PlaneResult* plane_result, void* context);
    };
} // namespace hob
