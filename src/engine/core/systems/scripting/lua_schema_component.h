#pragma once

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include <sol/sol.hpp>

#include "engine/core/logging.h"
#include "engine/core/space.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "lua_meta.h"
#include "lua_type_names.h"

namespace hob {
    class Entity;

    // One authorable prefab field: its name plus the component getter/setter it maps to.
    struct LuaComponentSchemaField {
        std::string name; // Prefab field, e.g. "body_type"
        std::string get_method; // Component method, e.g. "get_body_type"
        std::string set_method; // Component method, e.g. "set_body_type"
        std::string type; // Value semantics, e.g. "angle" (radians). Empty means it is inferred from the Lua value.
        std::string enum_name; // Lua global holding the entries, e.g. "BodyType". For `enum` and `bitmask`.
        float min = 0.0f; // Valid range. min == max means unbounded.
        float max = 0.0f; // MAX_FLOAT / MIN_FLOAT mark an open end.
        bool reapply_on_hot_reload = true;
        bool hide_in_inspector = false;
    };

    struct LuaComponentSchemaInfo {
        std::string key; // Prefab section key, e.g. "rigidbody"
        std::string add_method; // Entity method, e.g. "add_rigidbody"
        std::string get_method; // Entity method, e.g. "get_rigidbody"
        std::vector<LuaComponentSchemaField> fields;

        // "Map" component: the whole prefab section is a table of arbitrary keys applied wholesale to
        // `map_setter` (e.g. `sockets = { gun_left = Vector2(), ... }`), instead of being iterated as named fields.
        // When set, `fields` is unused.
        std::string map_setter;

        // Which entity space the component belongs to. Empty means it fits both.
        std::optional<Space> space;
    };

    class LuaComponentSchemaRegistry {
        std::vector<LuaComponentSchemaInfo> m_schemas;

    public:
        void add_schema(std::string key,
                        std::string add_method,
                        std::string get_method,
                        std::vector<LuaComponentSchemaField> fields,
                        std::optional<Space> space);

        void add_map_schema(std::string key,
                            std::string add_method,
                            std::string get_method,
                            std::string map_setter,
                            std::optional<Space> space);

        bool write_to_file(const std::filesystem::path& full_path) const;
    };

    // Registers a C++ component type as authorable from a Lua prefab. One call
    // does three things keyed by a single `add_method` string:
    //   1. Adds `entity:<add_method>()` to the already-bound Entity usertype.
    //   2. Records that method in the meta registry for IDE autocomplete.
    //   3. Records the schema entry (prefab `key` + fields) for the prefab applier.
    // bind_entity() must have run first so the Entity usertype exists.
    inline bool entity_accepts_space(const Entity& entity, std::optional<Space> space, const char* add_method) {
        if (space.has_value() && entity.get_space() != *space) {
            log::lua.error("{}: entity {} is a {} entity, the component is {}",
                           add_method,
                           entity.get_id(),
                           space_to_key(entity.get_space()),
                           space_to_key(*space));
            return false;
        }

        return true;
    }

    template<typename T>
    void bind_component_schema(sol::state& lua,
                               LuaMetaRegistry& meta,
                               LuaComponentSchemaRegistry& schemas,
                               const char* key,
                               const char* add_method,
                               const char* get_method,
                               std::initializer_list<LuaComponentSchemaField> fields,
                               std::optional<Space> space = Space::Space2D) {
        const char* entity_lua_name = LuaTypeName<EntityRef>::value;
        sol::table entity_ut = lua[entity_lua_name];
        entity_ut[add_method] = [space, add_method](const EntityRef& r) -> T* {
            Entity* e = r.resolve();
            if (e == nullptr || !entity_accepts_space(*e, space, add_method)) {
                return nullptr;
            }

            return e->add_component<T>();
        };

        if (LuaUsertypeInfo* entity_info = meta.find_usertype(entity_lua_name)) {
            LuaMethodInfo info;
            info.name = add_method;
            info.ret = meta_detail::lua_name<T*>();
            info.is_static = false;
            entity_info->methods.push_back(std::move(info));
        }

        schemas.add_schema(
            key, add_method, get_method, std::vector<LuaComponentSchemaField>(fields.begin(), fields.end()), space);
    }

    // Overload for components that are always present on every entity of their space (e.g. TransformComponent).
    template<typename T>
    void bind_component_schema(LuaComponentSchemaRegistry& schemas,
                               const char* key,
                               const char* existing_method,
                               std::initializer_list<LuaComponentSchemaField> fields,
                               std::optional<Space> space = Space::Space2D) {
        // Always-present component: `existing_method` is its getter, so add == get.
        schemas.add_schema(key,
                           existing_method,
                           existing_method,
                           std::vector<LuaComponentSchemaField>(fields.begin(), fields.end()),
                           space);
    }

    // Registers a "map" component whose prefab section is a table of arbitrary keys (e.g.
    // `sockets = { gun_left = Vector2(), ... }`) passed wholesale to `set_method`, instead of fixed named fields.
    // Synthesizes `entity:<add_method>()`; `get_method` must be bound separately on the Entity usertype.
    template<typename T>
    void bind_component_schema_map(sol::state& lua,
                                   LuaMetaRegistry& meta,
                                   LuaComponentSchemaRegistry& schemas,
                                   const char* key,
                                   const char* add_method,
                                   const char* get_method,
                                   const char* set_method,
                                   std::optional<Space> space = Space::Space2D) {
        const char* entity_lua_name = LuaTypeName<EntityRef>::value;
        sol::table entity_ut = lua[entity_lua_name];
        entity_ut[add_method] = [space, add_method](const EntityRef& r) -> T* {
            Entity* e = r.resolve();
            if (e == nullptr || !entity_accepts_space(*e, space, add_method)) {
                return nullptr;
            }

            return e->add_component<T>();
        };

        if (LuaUsertypeInfo* entity_info = meta.find_usertype(entity_lua_name)) {
            LuaMethodInfo info;
            info.name = add_method;
            info.ret = meta_detail::lua_name<T*>();
            info.is_static = false;
            entity_info->methods.push_back(std::move(info));
        }

        schemas.add_map_schema(key, add_method, get_method, set_method, space);
    }
} // namespace hob
