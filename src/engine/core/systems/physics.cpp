#include "physics.h"

#include <cassert>
#include <cmath>

#include "console.h"
#include "engine/components/physics/collider_component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/debug.h"
#include "engine/core/engine_config.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        int box2d_assert_handler(const char* condition, const char* file_name, int line_number) {
            debug::log_error("Box2D assertion: {} ({}:{})", condition, file_name, line_number);
            return 1;
        }
    } // namespace

    PhysicsWorld::PhysicsWorld(const Vector2& gravity)
        : m_id(b2_nullWorldId) {
        b2SetAssertFcn(box2d_assert_handler);

        b2WorldDef world_def = b2DefaultWorldDef();
        world_def.gravity = Physics::vec2_to_b2Vec2(gravity);
        m_id = b2CreateWorld(&world_def);
    }

    PhysicsWorld::~PhysicsWorld() {
        if (b2World_IsValid(m_id)) {
            b2DestroyWorld(m_id);
        }

        m_id = b2_nullWorldId;
    }

    void PhysicsWorld::tick(float fixed_delta_time, uint32_t sub_steps) {
        b2World_Step(m_id, fixed_delta_time, static_cast<int>(sub_steps));
    }

    b2WorldId PhysicsWorld::get_id() const {
        return m_id;
    }

    Physics::Physics(const PhysicsConfig& physics_config, Console& console)
        : m_physics_world(physics_config.gravity)
        , m_accumulator(0.0f)
        , m_fixed_delta_time(delta_time_from_ticks(physics_config.ticks_per_second))
        , m_sub_steps_per_tick(physics_config.sub_steps_per_tick)
        , m_interpolation_fraction(0.0f)
        , m_interpolation_enabled(physics_config.interpolation_enabled) {
        register_cvars(console);
    }

    void Physics::tick(float frame_delta_time, const std::vector<RigidbodyComponent*>& rigidbodies) {
        m_accumulator += frame_delta_time;
        while (m_accumulator >= m_fixed_delta_time) {
            // Save previous (start-of-step) state for Physics interpolation.
            // Runs for every simulated body, independent of is_ticking, so interpolation tracks it.
            if (m_interpolation_enabled) {
                for (RigidbodyComponent* rigidbody : rigidbodies) {
                    TransformComponent* transform = rigidbody->get_entity().get_transform();
                    if (transform->get_interpolate_physics()) {
                        transform->set_prev_local_matrix(transform->get_local_matrix());
                    }
                }
            }

            // Let components apply forces / set kinematic velocities; physics_tick() is a gameplay
            // callback, so it honors is_ticking (the body itself still simulates below regardless).
            for (RigidbodyComponent* rigidbody : rigidbodies) {
                Entity& entity = rigidbody->get_entity();
                if (entity.is_ticking()) {
                    entity.physics_tick(m_fixed_delta_time);
                }
            }

            m_physics_world.tick(m_fixed_delta_time, m_sub_steps_per_tick);

            dispatch_collision_events();
            dispatch_trigger_events();

            // Sync transforms from Box2D for every simulated body, independent of is_ticking,
            // so the transform always reflects what the body actually did this step.
            for (RigidbodyComponent* rigidbody : rigidbodies) {
                const b2Vec2 b2_position = b2Body_GetPosition(rigidbody->get_body_id());
                const b2Rot b2_rotation = b2Body_GetRotation(rigidbody->get_body_id());

                const Vector2 position = b2Vec2_to_vec2(b2_position);
                const float radians = b2Rot_to_radians(b2_rotation);

                TransformComponent* transform = rigidbody->get_entity().get_transform();
                transform->set_position(position);
                transform->set_rotation(radians);
            }

            m_accumulator -= m_fixed_delta_time;
        }

        m_interpolation_fraction = m_interpolation_enabled ? (m_accumulator / m_fixed_delta_time) : 1.0f;
    }

    const PhysicsWorld& Physics::get_physics_world() const {
        return m_physics_world;
    }

    float Physics::get_fixed_delta_time() const {
        return m_fixed_delta_time;
    }

    float Physics::get_interpolation_fraction() const {
        return m_interpolation_fraction;
    }

    RaycastHit Physics::raycast(const Vector2& origin,
                                const Vector2& direction,
                                float distance,
                                uint64_t layer_mask) const {
        RaycastHit result;

        if (distance <= 0.0f) {
            return result;
        }

        const float dir_length = direction.length();
        if (dir_length <= 0.0f) {
            return result;
        }

        const Vector2 translation = direction * (distance / dir_length);

        b2QueryFilter filter = b2DefaultQueryFilter();
        filter.maskBits = layer_mask;

        const b2RayResult ray = b2World_CastRayClosest(
            m_physics_world.get_id(), vec2_to_b2Vec2(origin), vec2_to_b2Vec2(translation), filter);

        if (!ray.hit) {
            return result;
        }

        if (!b2Shape_IsValid(ray.shapeId)) {
            return result;
        }

        auto* collider = static_cast<ColliderComponent*>(b2Shape_GetUserData(ray.shapeId));
        if (collider == nullptr) {
            return result;
        }

        result.collider = collider;
        result.point = b2Vec2_to_vec2(ray.point);
        result.normal = b2Vec2_to_vec2(ray.normal);
        result.distance = ray.fraction * distance;
        result.hit = true;

        return result;
    }

    std::vector<RaycastHit> Physics::raycast_all(const Vector2& origin,
                                                 const Vector2& direction,
                                                 float distance,
                                                 uint64_t layer_mask) const {
        std::vector<RaycastHit> hits;

        if (distance <= 0.0f) {
            return hits;
        }

        const float dir_length = direction.length();
        if (dir_length <= 0.0f) {
            return hits;
        }

        const Vector2 translation = direction * (distance / dir_length);

        b2QueryFilter filter = b2DefaultQueryFilter();
        filter.maskBits = layer_mask;

        struct CallbackContext {
            std::vector<RaycastHit>* hits;
            float distance;
        };

        CallbackContext ctx{&hits, distance};

        auto callback = [](b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* user_data) -> float {
            auto* c = static_cast<CallbackContext*>(user_data);

            if (!b2Shape_IsValid(shape_id)) {
                return 1.0f; // continue
            }

            // Skip initial-overlap hits (ray origin is inside this shape).
            // b2World_CastRayClosest already does this; mirror it for consistency.
            if (fraction <= 0.0f) {
                return 1.0f; // continue
            }

            auto* collider = static_cast<ColliderComponent*>(b2Shape_GetUserData(shape_id));
            if (collider == nullptr) {
                return 1.0f; // continue
            }

            RaycastHit hit;
            hit.collider = collider;
            hit.point = b2Vec2_to_vec2(point);
            hit.normal = b2Vec2_to_vec2(normal);
            hit.distance = fraction * c->distance;
            hit.hit = true;
            c->hits->push_back(hit);

            return 1.0f; // continue past this hit
        };

        b2World_CastRay(
            m_physics_world.get_id(), vec2_to_b2Vec2(origin), vec2_to_b2Vec2(translation), filter, callback, &ctx);

        return hits;
    }

    void Physics::dispatch_collision_events() const {
        const b2ContactEvents contact_events = b2World_GetContactEvents(m_physics_world.get_id());

        // Dispatch on_collision_enter
        for (int i = 0; i < contact_events.beginCount; ++i) {
            const b2ContactBeginTouchEvent& ev = contact_events.beginEvents[i];

            if (!b2Shape_IsValid(ev.shapeIdA) || !b2Shape_IsValid(ev.shapeIdB)) {
                continue;
            }

            const auto* collider_a = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.shapeIdA));
            const auto* collider_b = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.shapeIdB));

            Entity& entity_a = collider_a->get_entity();
            Entity& entity_b = collider_b->get_entity();

            entity_a.on_collision_enter(collider_b);
            entity_b.on_collision_enter(collider_a);
        }

        // Dispatch on_collision_exit
        for (int i = 0; i < contact_events.endCount; ++i) {
            const b2ContactEndTouchEvent& ev = contact_events.endEvents[i];

            if (!b2Shape_IsValid(ev.shapeIdA) || !b2Shape_IsValid(ev.shapeIdB)) {
                continue;
            }

            const auto* collider_a = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.shapeIdA));
            const auto* collider_b = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.shapeIdB));

            Entity& entity_a = collider_a->get_entity();
            Entity& entity_b = collider_b->get_entity();

            entity_a.on_collision_exit(collider_b);
            entity_b.on_collision_exit(collider_a);
        }
    }

    void Physics::dispatch_trigger_events() const {
        const b2SensorEvents sensor_events = b2World_GetSensorEvents(m_physics_world.get_id());

        // Dispatch on_trigger_enter
        for (int i = 0; i < sensor_events.beginCount; ++i) {
            const b2SensorBeginTouchEvent& ev = sensor_events.beginEvents[i];

            if (!b2Shape_IsValid(ev.sensorShapeId) || !b2Shape_IsValid(ev.visitorShapeId)) {
                continue;
            }

            const auto* trigger_collider = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.sensorShapeId));
            const auto* visitor_collider = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.visitorShapeId));

            Entity& trigger_entity = trigger_collider->get_entity();
            Entity& visitor_entity = visitor_collider->get_entity();

            trigger_entity.on_trigger_enter(visitor_collider);
            visitor_entity.on_trigger_enter(trigger_collider);
        }

        // Dispatch on_trigger_exit
        for (int i = 0; i < sensor_events.endCount; ++i) {
            const b2SensorEndTouchEvent& ev = sensor_events.endEvents[i];

            if (!b2Shape_IsValid(ev.sensorShapeId) || !b2Shape_IsValid(ev.visitorShapeId)) {
                continue;
            }

            const auto* trigger_collider = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.sensorShapeId));
            const auto* visitor_collider = static_cast<ColliderComponent*>(b2Shape_GetUserData(ev.visitorShapeId));

            Entity& trigger_entity = trigger_collider->get_entity();
            Entity& visitor_entity = visitor_collider->get_entity();

            trigger_entity.on_trigger_exit(visitor_collider);
            visitor_entity.on_trigger_exit(trigger_collider);
        }
    }

    float Physics::delta_time_from_ticks(uint32_t ticks_per_second) {
        assert(ticks_per_second > 0 && "Division by zero");
        return 1.0f / static_cast<float>(ticks_per_second);
    }

    void Physics::register_cvars(Console& console) {
        console.register_cvar("p_show_colliders",
                              "Show physics colliders",
                              "1",
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  cvar_debug_draw = cvar.bool_value();
                              });
    }
} // namespace hob
