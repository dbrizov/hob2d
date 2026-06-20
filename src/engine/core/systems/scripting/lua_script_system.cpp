// clang-format off
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
// clang-format on

#include <algorithm>
#include <string>
#include <vector>

#include "engine/components/lua_script_component.h"
#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/console.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"

namespace hob {
    LuaScriptSystem::LuaScriptSystem(Engine& engine)
        : m_engine(engine)
        , m_impl(std::make_unique<LuaScriptSystemImpl>()) {
        sol::state& lua = m_impl->lua;
        lua.open_libraries(sol::lib::base,
                           sol::lib::string,
                           sol::lib::math,
                           sol::lib::table,
                           sol::lib::io,
                           sol::lib::os,
                           sol::lib::package,
                           sol::lib::coroutine,
                           sol::lib::debug);

        // Make `require` find modules in scripts/engine/lib (e.g. vendored lldebugger).
        const std::string lib_path = (PathUtils::get_root_path() / "scripts" / "engine" / "lib" / "?.lua").string();
        sol::table package = lua["package"];
        package["path"] = lib_path + ";" + package["path"].get<std::string>();

        register_cvars(m_engine.get_console());

        register_bindings();

        // Schema files are consumed by the Lua bootstrap, so they must be written first.
        dump_component_schemas();
        dump_path_schemas();
        dump_factory_schemas();

        run_bootstrap();

#ifndef NDEBUG
        // Meta files are LuaCATS-only (no runtime effect),
        // so they're written after bootstrap which runs all user-defined scripts.
        dump_bindings_meta();
        dump_path_schemas_meta();
        dump_path_aliases_meta();
        dump_factory_schemas_meta();
        dump_factory_aliases_meta();
        dump_entity_registry_meta();
        dump_component_registry_meta();
#endif
    }

    LuaScriptSystem::~LuaScriptSystem() = default;

    sol::state& LuaScriptSystem::get_lua() {
        return m_impl->lua;
    }

    bool LuaScriptSystem::hot_reload() {
        const bool success = run_file("scripts/engine/hot_reload.lua");
        if (success) {
            refresh_lua_component_hook_caches();
            debug::print("[Lua] hot reload");
        }
        else {
            debug::log_error("[Lua] hot reload failed");
        }

        return success;
    }

    void LuaScriptSystem::refresh_lua_component_hook_caches() {
        std::vector<Entity*> entities;
        m_engine.get_entity_spawner().get_entities(entities);

        for (Entity* entity : entities) {
            for (LuaScriptComponent* component : entity->get_components<LuaScriptComponent>()) {
                component->refresh_hook_cache();
            }
        }
    }

    bool LuaScriptSystem::run_file(const std::filesystem::path& relative_path) {
        const std::filesystem::path full_path = PathUtils::get_root_path() / relative_path;

        auto result = m_impl->lua.safe_script_file(full_path.string(), sol::script_pass_on_error);
        if (!result.valid()) {
            const sol::error err = result;
            debug::log_error("Lua error in {}: {}", full_path.string(), err.what());
            return false;
        }

        return true;
    }

    bool LuaScriptSystem::run_folder(const std::filesystem::path& relative_path,
                                     const std::vector<std::string>& excludes) {
        const std::filesystem::path root = PathUtils::get_root_path() / relative_path;
        if (!std::filesystem::exists(root)) {
            debug::log_error("LuaScriptSystem::run_folder: '{}' does not exist", root.string());
            return false;
        }

        auto is_excluded = [&](const std::filesystem::path& p) {
            const std::filesystem::path rel = std::filesystem::relative(p, root);
            for (const auto& part : rel) {
                const std::string s = part.string();
                for (const auto& name : excludes) {
                    if (s == name) {
                        return true;
                    }
                }
            }

            return false;
        };

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".lua") {
                continue;
            }

            if (is_excluded(entry.path())) {
                continue;
            }

            files.push_back(entry.path());
        }

        std::sort(files.begin(), files.end());

        bool all_ok = true;
        for (const auto& file : files) {
            auto result = m_impl->lua.safe_script_file(file.string(), sol::script_pass_on_error);
            if (!result.valid()) {
                const sol::error err = result;
                debug::log_error("Lua error in {}: {}", file.string(), err.what());
                all_ok = false;
            }
        }

        return all_ok;
    }

    bool LuaScriptSystem::run_bootstrap() {
        return run_file("scripts/engine/bootstrap.lua");
    }

    void LuaScriptSystem::register_cvars(Console& console) {
        console.register_command("l_reload", "Hot-reload Lua scripts", [this](CommandArgs) {
            hot_reload();
        });
    }

    void LuaScriptSystem::register_bindings() {
        bind_math();
        bind_entity();
        bind_components();
        bind_camera();
        bind_renderer();
        bind_timer();
        bind_input();
        bind_ui();
        bind_physics();
        bind_entity_spawner();
        bind_scripts();
        bind_assets();
        bind_debug();
    }
} // namespace hob
