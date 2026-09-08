#include "engine/components/physics/collider_component.h"
#include "engine/components/physics_3d/collider_component_3d.h"
#include "engine/components/physics_3d/rigidbody_component_3d.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/physics/collision_layer.h"
#include "engine/core/systems/physics/physics.h"
#include "engine/core/systems/physics_3d/physics_3d.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_physics() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        Physics& physics = m_engine.get_physics();
        Physics3D& physics_3d = m_engine.get_physics_3d();

        bind_enum<CollisionLayer>(lua,
                                  meta,
                                  {
                                      {"None", CollisionLayer::None},
                                      {"Default", CollisionLayer::Default},
                                  });

        bind_usertype<RaycastHit>(lua, meta)
            .field("collider", &RaycastHit::collider)
            .field("point", &RaycastHit::point)
            .field("normal", &RaycastHit::normal)
            .field("distance", &RaycastHit::distance)
            .field("hit", &RaycastHit::hit)
            .op_tostring(&RaycastHit::to_string)
            .op_concat(&RaycastHit::to_string)
            .property_sig(
                "entity",
                [](const RaycastHit& h, const sol::this_state ts) -> sol::object {
                    const sol::state_view sv(ts);
                    if (h.collider == nullptr) {
                        return sol::lua_nil;
                    }

                    return sol::make_object(
                        sv,
                        EntityRef(h.collider->get_entity().get_id(), h.collider->get_engine().get_entity_spawner()));
                },
                "Entity?");

        // Masks/layers are uint64_t (matching Box2D), but sol2 can't push values above
        // MAX_INT64, so reinterpret to/from int64_t at the Lua boundary (bit pattern is preserved).
        bind_table(lua, meta, "Physics")
            .func("raycast",
                  [&physics](const Vector2& origin,
                             const Vector2& direction,
                             float distance,
                             sol::optional<int64_t> layer_mask) {
                      const uint64_t mask = layer_mask ? static_cast<uint64_t>(*layer_mask) : ~0ull;
                      return physics.raycast(origin, direction, distance, mask);
                  },
                  {"origin", "direction", "distance", "layer_mask"})
            .func("raycast_all",
                  [&physics](const Vector2& origin,
                             const Vector2& direction,
                             float distance,
                             sol::optional<int64_t> layer_mask) {
                      const uint64_t mask = layer_mask ? static_cast<uint64_t>(*layer_mask) : ~0ull;
                      return physics.raycast_all(origin, direction, distance, mask);
                  },
                  {"origin", "direction", "distance", "layer_mask"});

        bind_enum<MotionLock>(lua,
                              meta,
                              {
                                  {"None", MotionLock::None},
                                  {"LinearX", MotionLock::LinearX},
                                  {"LinearY", MotionLock::LinearY},
                                  {"LinearZ", MotionLock::LinearZ},
                                  {"AngularX", MotionLock::AngularX},
                                  {"AngularY", MotionLock::AngularY},
                                  {"AngularZ", MotionLock::AngularZ},
                                  {"Angular", MotionLock::Angular},
                              });

        bind_usertype<RaycastHit3D>(lua, meta)
            .field("collider", &RaycastHit3D::collider)
            .field("point", &RaycastHit3D::point)
            .field("normal", &RaycastHit3D::normal)
            .field("distance", &RaycastHit3D::distance)
            .field("hit", &RaycastHit3D::hit)
            .property_sig(
                "entity",
                [](const RaycastHit3D& h, const sol::this_state ts) -> sol::object {
                    const sol::state_view sv(ts);
                    if (h.collider == nullptr) {
                        return sol::lua_nil;
                    }

                    return sol::make_object(
                        sv,
                        EntityRef(h.collider->get_entity().get_id(), h.collider->get_engine().get_entity_spawner()));
                },
                "Entity?")
            .op_tostring(&RaycastHit3D::to_string)
            .op_concat(&RaycastHit3D::to_string);

        bind_table(lua, meta, "Physics3D")
            .func_sig(
                "raycast",
                [&physics_3d](
                    const Vector3& origin, const Vector3& direction, float distance, sol::optional<int64_t> mask) {
                    const uint64_t layer_mask = mask ? static_cast<uint64_t>(*mask) : ~0ull;
                    return physics_3d.raycast(origin, direction, distance, layer_mask);
                },
                "(origin: Vector3, direction: Vector3, distance: number, layer_mask: integer?): RaycastHit3D")
            .func_sig(
                "raycast_all",
                [&physics_3d](
                    const Vector3& origin, const Vector3& direction, float distance, sol::optional<int64_t> mask) {
                    const uint64_t layer_mask = mask ? static_cast<uint64_t>(*mask) : ~0ull;
                    return physics_3d.raycast_all(origin, direction, distance, layer_mask);
                },
                "(origin: Vector3, direction: Vector3, distance: number, layer_mask: integer?): RaycastHit3D[]");
    }
} // namespace hob
