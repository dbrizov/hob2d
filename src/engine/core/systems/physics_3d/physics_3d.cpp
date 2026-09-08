#include "physics_3d.h"

#include <format>

#include <box3d/box3d.h>

#include "engine/components/physics_3d/collider_component_3d.h"
#include "engine/components/physics_3d/rigidbody_component_3d.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/assert.h"
#include "engine/core/engine_config.h"
#include "engine/core/logging.h"
#include "engine/core/systems/console.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        int box3d_assert_handler(const char* condition, const char* file_name, int line_number) {
            log::physics.error("Box3D assertion failed: {} ({}:{})", condition, file_name, line_number);
            return 1;
        }

        ColliderComponent3D* collider_of(b3ShapeId shape_id) {
            return static_cast<ColliderComponent3D*>(b3Shape_GetUserData(shape_id));
        }
    } // namespace

    std::string RaycastHit3D::to_string() const {
        return std::format("RaycastHit3D(hit = {}, point = {}, normal = {}, distance = {:.3f})",
                           hit,
                           point.to_string(),
                           normal.to_string(),
                           distance);
    }

    PhysicsWorld3D::PhysicsWorld3D(const Vector3& gravity) {
        b3SetAssertFcn(box3d_assert_handler);

        b3WorldDef world_def = b3DefaultWorldDef();
        world_def.gravity = Physics3D::vec3_to_b3Vec3(gravity);

        m_id = b3CreateWorld(&world_def);
        HOB_CHECK(b3World_IsValid(m_id), "Failed to create the Box3D world");
    }

    PhysicsWorld3D::~PhysicsWorld3D() {
        if (b3World_IsValid(m_id)) {
            b3DestroyWorld(m_id);
        }
    }

    void PhysicsWorld3D::tick(float fixed_delta_time, uint32_t sub_steps) {
        b3World_Step(m_id, fixed_delta_time, static_cast<int32_t>(sub_steps));
    }

    b3WorldId PhysicsWorld3D::get_id() const {
        return m_id;
    }

    Physics3D::Physics3D(const PhysicsConfig3D& physics_config)
        : m_physics_world(physics_config.gravity)
        , m_accumulator(0.0f)
        , m_fixed_delta_time(delta_time_from_ticks(physics_config.ticks_per_second))
        , m_sub_steps_per_tick(physics_config.sub_steps_per_tick)
        , m_interpolation_fraction(0.0f)
        , m_interpolation_enabled(physics_config.interpolation_enabled) {
        log::physics.info("Physics3D::Initialise (gravity {}, {} ticks/s, {} sub-steps, interpolation {})",
                          physics_config.gravity.to_string(),
                          physics_config.ticks_per_second,
                          physics_config.sub_steps_per_tick,
                          physics_config.interpolation_enabled ? "on" : "off");
    }

    Physics3D::~Physics3D() {
        log::physics.info("Physics3D::Shutdown");
    }

    void Physics3D::register_cvars(Console& console) {
        console.register_cvar("p3_show_colliders",
                              "Draw 3D collider shapes",
                              cvar_show_colliders ? "1" : "0",
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  cvar_show_colliders = cvar.bool_value();
                              });

        console.register_command("p3_stats", "Print Box3D world counters", [this](CommandArgs) {
            const b3Counters counters = b3World_GetCounters(m_physics_world.get_id());
            log::physics.info("Box3D: {} bodies, {} shapes, {} contacts, {} islands",
                              counters.bodyCount,
                              counters.shapeCount,
                              counters.contactCount,
                              counters.islandCount);
        });
    }

    void Physics3D::tick(float frame_delta_time, const std::vector<RigidbodyComponent3D*>& rigidbodies) {
        m_accumulator += frame_delta_time;
        while (m_accumulator >= m_fixed_delta_time) {
            if (m_interpolation_enabled) {
                for (RigidbodyComponent3D* rigidbody : rigidbodies) {
                    TransformComponent3D* transform = rigidbody->get_entity().get_transform_3d();
                    if (transform->get_interpolate_physics()) {
                        transform->set_prev_local_pose(transform->get_local_position(),
                                                       transform->get_local_rotation());
                    }
                }
            }

            for (RigidbodyComponent3D* rigidbody : rigidbodies) {
                Entity& entity = rigidbody->get_entity();
                if (entity.is_ticking()) {
                    entity.physics_tick(m_fixed_delta_time);
                }
            }

            m_physics_world.tick(m_fixed_delta_time, m_sub_steps_per_tick);

            dispatch_collision_events();
            dispatch_trigger_events();

            for (RigidbodyComponent3D* rigidbody : rigidbodies) {
                TransformComponent3D* transform = rigidbody->get_entity().get_transform_3d();
                transform->set_position(b3Vec3_to_vec3(b3Body_GetPosition(rigidbody->get_body_id())));
                transform->set_rotation(b3Quat_to_quat(b3Body_GetRotation(rigidbody->get_body_id())));
            }

            m_accumulator -= m_fixed_delta_time;
        }

        m_interpolation_fraction = m_interpolation_enabled ? (m_accumulator / m_fixed_delta_time) : 1.0f;
    }

    const PhysicsWorld3D& Physics3D::get_physics_world() const {
        return m_physics_world;
    }

    float Physics3D::get_fixed_delta_time() const {
        return m_fixed_delta_time;
    }

    float Physics3D::get_interpolation_fraction() const {
        return m_interpolation_fraction;
    }

    RaycastHit3D Physics3D::raycast(const Vector3& origin,
                                    const Vector3& direction,
                                    float distance,
                                    uint64_t layer_mask) const {
        RaycastHit3D result;

        const float direction_length = direction.length();
        if (distance <= 0.0f || direction_length <= 0.0f) {
            return result;
        }

        const Vector3 translation = direction * (distance / direction_length);

        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = ~0ull;
        filter.maskBits = layer_mask;

        const b3RayResult ray = b3World_CastRayClosest(
            m_physics_world.get_id(), vec3_to_b3Vec3(origin), vec3_to_b3Vec3(translation), filter);
        if (!ray.hit || !b3Shape_IsValid(ray.shapeId)) {
            return result;
        }

        ColliderComponent3D* collider = collider_of(ray.shapeId);
        if (collider == nullptr) {
            return result;
        }

        result.collider = collider;
        result.point = b3Vec3_to_vec3(ray.point);
        result.normal = b3Vec3_to_vec3(ray.normal);
        result.distance = ray.fraction * distance;
        result.hit = true;
        return result;
    }

    std::vector<RaycastHit3D> Physics3D::raycast_all(const Vector3& origin,
                                                     const Vector3& direction,
                                                     float distance,
                                                     uint64_t layer_mask) const {
        std::vector<RaycastHit3D> hits;

        const float direction_length = direction.length();
        if (distance <= 0.0f || direction_length <= 0.0f) {
            return hits;
        }

        const Vector3 translation = direction * (distance / direction_length);

        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = ~0ull;
        filter.maskBits = layer_mask;

        struct Context {
            std::vector<RaycastHit3D>* hits;
            float distance;
        } context{&hits, distance};

        const auto callback = [](b3ShapeId shape_id,
                                 b3Pos point,
                                 b3Vec3 normal,
                                 float fraction,
                                 uint64_t,
                                 int32_t,
                                 int32_t,
                                 void* user_context) -> float {
            Context* ctx = static_cast<Context*>(user_context);
            if (fraction <= 0.0f || !b3Shape_IsValid(shape_id)) {
                return 1.0f;
            }

            ColliderComponent3D* collider = collider_of(shape_id);
            if (collider != nullptr) {
                ctx->hits->push_back(RaycastHit3D{collider,
                                                  Physics3D::b3Vec3_to_vec3(point),
                                                  Physics3D::b3Vec3_to_vec3(normal),
                                                  fraction * ctx->distance,
                                                  true});
            }

            return 1.0f;
        };

        b3World_CastRay(
            m_physics_world.get_id(), vec3_to_b3Vec3(origin), vec3_to_b3Vec3(translation), filter, callback, &context);

        return hits;
    }

    void Physics3D::dispatch_collision_events() const {
        const b3ContactEvents contact_events = b3World_GetContactEvents(m_physics_world.get_id());

        for (int32_t i = 0; i < contact_events.beginCount; ++i) {
            const b3ContactBeginTouchEvent& ev = contact_events.beginEvents[i];
            if (!b3Shape_IsValid(ev.shapeIdA) || !b3Shape_IsValid(ev.shapeIdB)) {
                continue;
            }

            ColliderComponent3D* collider_a = collider_of(ev.shapeIdA);
            ColliderComponent3D* collider_b = collider_of(ev.shapeIdB);
            collider_a->get_entity().on_collision_enter_3d(collider_b);
            collider_b->get_entity().on_collision_enter_3d(collider_a);
        }

        for (int32_t i = 0; i < contact_events.endCount; ++i) {
            const b3ContactEndTouchEvent& ev = contact_events.endEvents[i];
            if (!b3Shape_IsValid(ev.shapeIdA) || !b3Shape_IsValid(ev.shapeIdB)) {
                continue;
            }

            ColliderComponent3D* collider_a = collider_of(ev.shapeIdA);
            ColliderComponent3D* collider_b = collider_of(ev.shapeIdB);
            collider_a->get_entity().on_collision_exit_3d(collider_b);
            collider_b->get_entity().on_collision_exit_3d(collider_a);
        }
    }

    void Physics3D::dispatch_trigger_events() const {
        const b3SensorEvents sensor_events = b3World_GetSensorEvents(m_physics_world.get_id());

        for (int32_t i = 0; i < sensor_events.beginCount; ++i) {
            const b3SensorBeginTouchEvent& ev = sensor_events.beginEvents[i];
            if (!b3Shape_IsValid(ev.sensorShapeId) || !b3Shape_IsValid(ev.visitorShapeId)) {
                continue;
            }

            ColliderComponent3D* trigger = collider_of(ev.sensorShapeId);
            ColliderComponent3D* visitor = collider_of(ev.visitorShapeId);
            trigger->get_entity().on_trigger_enter_3d(visitor);
            visitor->get_entity().on_trigger_enter_3d(trigger);
        }

        for (int32_t i = 0; i < sensor_events.endCount; ++i) {
            const b3SensorEndTouchEvent& ev = sensor_events.endEvents[i];
            if (!b3Shape_IsValid(ev.sensorShapeId) || !b3Shape_IsValid(ev.visitorShapeId)) {
                continue;
            }

            ColliderComponent3D* trigger = collider_of(ev.sensorShapeId);
            ColliderComponent3D* visitor = collider_of(ev.visitorShapeId);
            trigger->get_entity().on_trigger_exit_3d(visitor);
            visitor->get_entity().on_trigger_exit_3d(trigger);
        }
    }

    float Physics3D::delta_time_from_ticks(uint32_t ticks_per_second) {
        HOB_ASSERT(ticks_per_second > 0, "Division by zero");
        return 1.0f / static_cast<float>(ticks_per_second);
    }
} // namespace hob
