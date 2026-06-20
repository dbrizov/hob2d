#include "engine/components/physics/collider_component.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/physics.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "engine/math/vector2.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_physics() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        Physics& physics = m_engine.get_physics();

        bind_usertype<RaycastHit>(lua, meta)
            .field("collider", &RaycastHit::collider)
            .field("point", &RaycastHit::point)
            .field("normal", &RaycastHit::normal)
            .field("distance", &RaycastHit::distance)
            .field("hit", &RaycastHit::hit)
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
    }
} // namespace hob
