#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "lua_type_names.h"

namespace hob {
    struct LuaFactoryFieldInfo {
        std::string name;
    };

    struct LuaFactorySchemaInfo {
        std::string define_name; // Lua-side define entry point, e.g. "DefineMaterial"
        std::string registry_name; // Lua-side registry table, e.g. "Materials"
        std::string lua_type; // Bound usertype name invoked as a factory, e.g. "Material"
        std::vector<LuaFactoryFieldInfo> fields;
    };

    class LuaFactorySchemaRegistry {
        std::vector<LuaFactorySchemaInfo> m_schemas;

    public:
        void add_schema(LuaFactorySchemaInfo info);

        const std::vector<LuaFactorySchemaInfo>& get_schemas() const;

        bool write_to_file(const std::filesystem::path& full_path) const;
        bool write_meta_to_file(const std::filesystem::path& full_path) const;
    };

    // Records that a usertype `T` bound with a `factory_ctor` accepting a single config table
    // is authorable via a Lua-side `DefineX.Name = { ... }` registry. The Lua type name is
    // pulled from `LuaTypeName<T>`; the corresponding bind_usertype<T>(...).factory_ctor(...)
    // call must already exist (this function only records metadata).
    template<typename T>
    void bind_factory_schema(LuaFactorySchemaRegistry& schemas,
                             const char* define_name,
                             const char* registry_name,
                             std::initializer_list<const char*> fields) {
        LuaFactorySchemaInfo info;
        info.define_name = define_name;
        info.registry_name = registry_name;
        info.lua_type = LuaTypeName<T>::value;
        info.fields.reserve(fields.size());
        for (const auto& f : fields) {
            info.fields.push_back({f});
        }

        schemas.add_schema(std::move(info));
    }
} // namespace hob
