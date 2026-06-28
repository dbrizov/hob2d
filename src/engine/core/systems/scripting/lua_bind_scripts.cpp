#include <string>
#include <vector>

#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_scripts() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;

        auto to_exclude_list = [](const sol::optional<sol::table>& excludes) {
            std::vector<std::string> exclude_list;
            if (excludes) {
                for (const auto& kv : *excludes) {
                    exclude_list.push_back(kv.second.as<std::string>());
                }
            }
            return exclude_list;
        };

        bind_table(lua, meta, "Scripts")
            .func("run_engine_file",
                  [this](const std::string& relative_path) {
                      return run_engine_file(relative_path);
                  },
                  {"relative_path"})
            .func_sig(
                "run_engine_folder",
                [this, to_exclude_list](const std::string& relative_path, const sol::optional<sol::table>& excludes) {
                    return run_engine_folder(relative_path, to_exclude_list(excludes));
                },
                "(relative_path: string, excludes: string[]?): boolean")
            .func("run_project_file",
                  [this](const std::string& relative_path) {
                      return run_project_file(relative_path);
                  },
                  {"relative_path"})
            .func_sig(
                "run_project_folder",
                [this, to_exclude_list](const std::string& relative_path, const sol::optional<sol::table>& excludes) {
                    return run_project_folder(relative_path, to_exclude_list(excludes));
                },
                "(relative_path: string, excludes: string[]?): boolean");
    }
} // namespace hob
