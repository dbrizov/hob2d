#include <vector>

#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_entity_spawner() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        EntitySpawner& spawner = m_engine.get_entity_spawner();

        bind_table(lua, meta, "EntitySpawner")
            .func("spawn_entity_c",
                  [&spawner]() {
                      return EntityRef(spawner.spawn_entity().get_id(), spawner);
                  })
            .func("destroy_entity_c",
                  [&spawner](const EntityRef& r) {
                      spawner.destroy_entity(r.get_id());
                  },
                  {"entity"})
            .func("get_entity",
                  [&spawner](EntityId id) {
                      return EntityRef(id, spawner);
                  },
                  {"id"})
            .func_sig(
                "for_each",
                [&spawner](const sol::function& fn) {
                    std::vector<Entity*> entities;
                    spawner.get_entities(entities);
                    for (Entity* e : entities) {
                        fn(EntityRef(e->get_id(), spawner));
                    }
                },
                "(fn: fun(entity: Entity))");
    }
} // namespace hob
