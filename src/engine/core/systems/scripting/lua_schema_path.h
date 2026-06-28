#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace hob {
    struct LuaPathSchemaInfo {
        std::string define_name; // Lua-side define entry point, e.g. "DefineTexture"
        std::string registry_name; // Lua-side registry table, e.g. "Textures"
        std::string type_label; // Used in error messages, e.g. "Texture"
    };

    class LuaPathSchemaRegistry {
        std::vector<LuaPathSchemaInfo> m_schemas;

    public:
        void add_schema(LuaPathSchemaInfo info);

        const std::vector<LuaPathSchemaInfo>& get_schemas() const;

        bool write_to_file(const std::filesystem::path& full_path) const;
        bool write_meta_to_file(const std::filesystem::path& full_path) const;
    };

    inline void bind_path_schema(LuaPathSchemaRegistry& schemas,
                                 const char* define_name,
                                 const char* registry_name,
                                 const char* type_label) {
        schemas.add_schema({define_name, registry_name, type_label});
    }
} // namespace hob
