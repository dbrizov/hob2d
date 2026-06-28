#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include <sol/sol.hpp>

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
    };

    struct LuaComponentSchemaInfo {
        std::string key; // Prefab section key, e.g. "rigidbody"
        std::string add_method; // Entity method, e.g. "add_rigidbody"
        std::string get_method; // Entity method, e.g. "get_rigidbody"
        std::vector<LuaComponentSchemaField> fields;
    };

    class LuaComponentSchemaRegistry {
        std::vector<LuaComponentSchemaInfo> m_schemas;

    public:
        void add_schema(std::string key,
                        std::string add_method,
                        std::string get_method,
                        std::vector<LuaComponentSchemaField> fields);

        bool write_to_file(const std::filesystem::path& full_path) const;
    };

    // Registers a C++ component type as authorable from a Lua prefab. One call
    // does three things keyed by a single `add_method` string:
    //   1. Adds `entity:<add_method>()` to the already-bound Entity usertype.
    //   2. Records that method in the meta registry for IDE autocomplete.
    //   3. Records the schema entry (prefab `key` + fields) for the prefab applier.
    // bind_entity() must have run first so the Entity usertype exists.
    template<typename T>
    void bind_component_schema(sol::state& lua,
                               LuaMetaRegistry& meta,
                               LuaComponentSchemaRegistry& schemas,
                               const char* key,
                               const char* add_method,
                               const char* get_method,
                               std::initializer_list<LuaComponentSchemaField> fields) {
        const char* entity_lua_name = LuaTypeName<EntityRef>::value;
        sol::table entity_ut = lua[entity_lua_name];
        entity_ut[add_method] = [](const EntityRef& r) -> T* {
            Entity* e = r.resolve();
            return e ? e->add_component<T>() : nullptr;
        };

        if (LuaUsertypeInfo* entity_info = meta.find_usertype(entity_lua_name)) {
            LuaMethodInfo info;
            info.name = add_method;
            info.ret = meta_detail::lua_name<T*>();
            info.is_static = false;
            entity_info->methods.push_back(std::move(info));
        }

        schemas.add_schema(
            key, add_method, get_method, std::vector<LuaComponentSchemaField>(fields.begin(), fields.end()));
    }

    // Overload for components that are always present on every entity. No `add_X` is
    // synthesized, so Lua cannot construct a duplicate; dispatch goes through `existing_method`
    // (e.g. "get_transform") which must already be bound on the Entity usertype.
    template<typename T>
    void bind_component_schema(LuaComponentSchemaRegistry& schemas,
                               const char* key,
                               const char* existing_method,
                               std::initializer_list<LuaComponentSchemaField> fields) {
        // Always-present component: `existing_method` is its getter, so add == get.
        schemas.add_schema(
            key, existing_method, existing_method, std::vector<LuaComponentSchemaField>(fields.begin(), fields.end()));
    }
} // namespace hob
