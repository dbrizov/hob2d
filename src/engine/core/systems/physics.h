#pragma once

#include <cmath>
#include <vector>

#include <box2d/box2d.h>

#include "engine/math/capsule.h"
#include "engine/math/vector2.h"

namespace hob {
    struct PhysicsConfig;
    class Console;
    class Entity;
    class ColliderComponent;
    class RigidbodyComponent;

    struct RaycastHit {
        ColliderComponent* collider = nullptr;
        Vector2 point;
        Vector2 normal;
        float distance = 0.0f;
        bool hit = false;
    };

    class PhysicsWorld {
        b2WorldId m_id;

    public:
        explicit PhysicsWorld(const Vector2& gravity);
        ~PhysicsWorld();

        void tick(float fixed_delta_time, uint32_t sub_steps = 4);

        b2WorldId get_id() const;
    };

    class Physics {
        PhysicsWorld m_physics_world;
        float m_accumulator;
        float m_fixed_delta_time;
        uint32_t m_sub_steps_per_tick;
        float m_interpolation_fraction;
        bool m_interpolation_enabled;

    public:
        bool cvar_debug_draw = true;

        Physics(const PhysicsConfig& physics_config, Console& console);

        void tick(float frame_delta_time, const std::vector<RigidbodyComponent*>& rigidbodies);

        const PhysicsWorld& get_physics_world() const;

        float get_fixed_delta_time() const;
        float get_interpolation_fraction() const;

        RaycastHit raycast(const Vector2& origin,
                           const Vector2& direction,
                           float distance,
                           uint64_t layer_mask = ~0ull) const;

        std::vector<RaycastHit> raycast_all(const Vector2& origin,
                                            const Vector2& direction,
                                            float distance,
                                            uint64_t layer_mask = ~0ull) const;

        static Vector2 b2Vec2_to_vec2(const b2Vec2& vec) {
            return Vector2(vec.x, vec.y);
        }

        static b2Vec2 vec2_to_b2Vec2(const Vector2& vec) {
            return b2Vec2{vec.x, vec.y};
        }

        static float b2Rot_to_radians(const b2Rot& rot) {
            return std::atan2(rot.s, rot.c);
        }

        static b2Rot radians_to_b2Rot(float radians) {
            return b2MakeRot(radians);
        }

        static b2Capsule capsule_to_b2Capsule(const Capsule& capsule) {
            const Vector2 center_a = capsule.center_a;
            Vector2 center_b = capsule.center_b;

            // Box2D normalizes the axis (center2 - center1).
            // A zero-length axis (i.e. a circle) yields NaN normals.
            // Treat a degenerate capsule as a circle by nudging the second endpoint up a hair.
            const float min_axis = 0.01f;
            if ((center_b - center_a).length_sqr() < min_axis * min_axis) {
                center_b = center_a + Vector2::up() * min_axis;
            }

            b2Capsule b2_capsule;
            b2_capsule.center1 = vec2_to_b2Vec2(center_a);
            b2_capsule.center2 = vec2_to_b2Vec2(center_b);
            b2_capsule.radius = capsule.radius;

            return b2_capsule;
        }

    private:
        void dispatch_collision_events() const;
        void dispatch_trigger_events() const;

        static float delta_time_from_ticks(uint32_t ticks_per_second);

        void register_cvars(Console& console);
    };
} // namespace hob
