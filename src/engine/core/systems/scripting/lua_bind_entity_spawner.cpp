#include <utility>

#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/space.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::install_entity_lifetime_handlers() {
        EntitySpawner& spawner = m_engine.get_entity_spawner();

        const auto dispatch = [this](const char* name, auto&&... args) {
            const sol::protected_function dispatcher = m_impl->lua[name];
            if (!dispatcher.valid()) {
                return;
            }

            const sol::protected_function_result result = dispatcher(std::forward<decltype(args)>(args)...);
            if (!result.valid()) {
                const sol::error err = result;
                log::sol2.error("{}: {}", name, err.what());
            }
        };

        spawner.set_entity_spawned_handler([dispatch](EntityId id) {
            dispatch("__on_entity_spawned", id);
        });

        spawner.set_entity_destroyed_handler([dispatch](EntityId id) {
            dispatch("__on_entity_destroyed", id);
        });

        spawner.set_entities_cleared_handler([dispatch]() {
            dispatch("__on_entities_cleared");
        });
    }

    void LuaScriptSystem::bind_entity_spawner() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        EntitySpawner& spawner = m_engine.get_entity_spawner();

        bind_table(lua, meta, "EntitySpawner")
            .func_sig(
                "spawn_entity",
                [&spawner](sol::optional<std::string> space_key) {
                    const Space space = space_from_key(space_key.value_or("")).value_or(Space::Space2D);
                    return EntityRef(spawner.spawn_entity(space).get_id(), spawner);
                },
                "(space: string?): Entity")
            .func("destroy_entity",
                  [&spawner](const EntityRef& r) {
                      spawner.destroy_entity(r.get_id());
                  },
                  {"entity"})
            .func("clear",
                  [&spawner]() {
                      spawner.clear();
                  })
            .func("get_entity",
                  [&spawner](EntityId id) {
                      return EntityRef(id, spawner);
                  },
                  {"id"})
            .func_sig(
                "for_each_entity",
                [&spawner](const sol::protected_function& func,
                           const sol::optional<sol::protected_function>& until_predicate) {
                    const auto visit = [&func, &spawner](Entity* entity) {
                        const sol::protected_function_result result = func(EntityRef(entity->get_id(), spawner));
                        if (!result.valid()) {
                            const sol::error err = result;
                            log::sol2.error("Lua error in EntitySpawner.for_each_entity: {}", err.what());
                        }
                    };

                    if (!until_predicate) {
                        spawner.for_each_entity(visit);
                        return;
                    }

                    spawner.for_each_entity(visit, [&until_predicate, &spawner](Entity* entity) {
                        const sol::protected_function_result result =
                            (*until_predicate)(EntityRef(entity->get_id(), spawner));
                        if (!result.valid()) {
                            const sol::error err = result;
                            log::sol2.error("Lua error in EntitySpawner.for_each_entity until_predicate: {}",
                                            err.what());
                            return true;
                        }

                        const sol::optional<bool> stop = result;
                        return stop.value_or(false);
                    });
                },
                "(func: fun(entity: Entity), until_predicate: (fun(entity: Entity): boolean)?)");
    }
} // namespace hob
