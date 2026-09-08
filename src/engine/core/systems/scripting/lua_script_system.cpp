// clang-format off
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
// clang-format on

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "engine/components/lua_script_component.h"
#include "engine/core/assert.h"
#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/engine_hooks.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/console.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        constexpr std::string_view LUA_SOURCE_EXTENSIONS[] = {file_extension::LUA,
                                                              file_extension::SCENE,
                                                              file_extension::PREFAB,
                                                              file_extension::MATERIAL,
                                                              file_extension::ANIMATION_CLIP,
                                                              file_extension::SHADER,
                                                              file_extension::META,
                                                              file_extension::MESH};

        bool is_lua_source_file(const std::filesystem::directory_entry& entry) {
            if (!entry.is_regular_file()) {
                return false;
            }

            const std::string extension = entry.path().extension().string();
            for (const std::string_view candidate : LUA_SOURCE_EXTENSIONS) {
                if (extension == candidate) {
                    return true;
                }
            }

            return false;
        }
    } // namespace

    LuaScriptSystem::LuaScriptSystem(Engine& engine, bool run_project_main_on_boot)
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

        install_log_redirects();
        install_entity_lifetime_handlers();

        // Make `require` find modules in the engine's scripts/lib (e.g. vendored lldebugger).
        const std::string lib_path = (PathUtils::get_engine_scripts_root() / "lib" / "?.lua").string();
        sol::table package = lua["package"];
        package["path"] = lib_path + ";" + package["path"].get<std::string>();

        register_bindings();

        // Schema files are consumed by the Lua bootstrap, so they must be written first.
        dump_component_schemas();
        dump_asset_factory_schemas();

        if (!run_project_main_on_boot) {
            lua["__defer_project_main"] = true;
        }

        const bool bootstrap_succeeded = run_bootstrap();
        HOB_CHECK(bootstrap_succeeded, "Lua bootstrap failed");

#if defined(HOB_EDITOR) || !defined(NDEBUG)
        // Meta files are LuaCATS-only (no runtime effect),
        // so they're written after bootstrap which runs all user-defined scripts.
        dump_bindings_meta();
        dump_asset_factory_schemas_meta();
        dump_asset_names_meta();
        dump_shader_params_meta();
        dump_entity_registry_meta();
        dump_component_registry_meta();
        dump_scene_registry_meta();
#endif

        log::lua.info("LuaScriptSystem::Initialise");
    }

    LuaScriptSystem::~LuaScriptSystem() {
        EntitySpawner& spawner = m_engine.get_entity_spawner();
        spawner.set_entity_spawned_handler(nullptr);
        spawner.set_entity_destroyed_handler(nullptr);
        spawner.set_entities_cleared_handler(nullptr);

        log::lua.info("LuaScriptSystem::Shutdown");
    }

    sol::state& LuaScriptSystem::get_lua() {
        return m_impl->lua;
    }

    bool LuaScriptSystem::hot_reload() {
        const bool success = run_engine_file("scripts/hot_reload.lua");
        if (success) {
            refresh_lua_component_class_caches();

            EngineHooks* hooks = m_engine.get_hooks();
            if (hooks != nullptr) {
                hooks->on_lua_hot_reloaded();
            }

            debug::print(Color::white(), 5.0f, true, "[Lua] hot reload");
        }
        else {
            log::lua.error("hot reload failed");
        }

        return success;
    }

    void LuaScriptSystem::poll_hot_reload(float delta_time) {
        constexpr float POLL_INTERVAL = 0.5f;
        m_script_watch_accumulator += delta_time;
        if (m_script_watch_accumulator < POLL_INTERVAL) {
            return;
        }
        m_script_watch_accumulator = 0.0f;

        const std::filesystem::file_time_type newest = scan_newest_script_write_time();

        // First poll just records the baseline; never reload on startup.
        if (!m_has_script_write_baseline) {
            m_last_script_write_time = newest;
            m_has_script_write_baseline = true;
            return;
        }

        if (newest > m_last_script_write_time) {
            m_last_script_write_time = newest;
            hot_reload();
        }
    }

    void LuaScriptSystem::rebaseline_script_watch() {
        m_last_script_write_time = scan_newest_script_write_time();
        m_has_script_write_baseline = true;
        m_script_watch_accumulator = 0.0f;
    }

    std::filesystem::file_time_type LuaScriptSystem::scan_newest_script_write_time() const {
        std::filesystem::file_time_type newest = std::filesystem::file_time_type::min();
        std::error_code ec;

        auto scan_root = [&](const std::filesystem::path& root) {
            ec.clear();
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
                if (ec) {
                    break;
                }

                if (!is_lua_source_file(entry)) {
                    continue;
                }

                const std::filesystem::file_time_type t = entry.last_write_time(ec);
                if (!ec && t > newest) {
                    newest = t;
                }
            }
        };

        scan_root(PathUtils::get_engine_scripts_root());
        for (const auto& root : PathUtils::get_project_definition_roots()) {
            scan_root(root);
        }

        return newest;
    }

    bool LuaScriptSystem::run_file(const std::filesystem::path& full_path) {
        auto result = m_impl->lua.safe_script_file(full_path.string(), sol::script_pass_on_error);
        if (!result.valid()) {
            const sol::error err = result;
            log::sol2.error("Lua error in {}: {}", full_path.string(), err.what());
            return false;
        }

        return true;
    }

    bool LuaScriptSystem::run_file(const std::filesystem::path& base, const std::filesystem::path& relative_path) {
        return run_file(base / relative_path);
    }

    bool LuaScriptSystem::run_folder(const std::filesystem::path& base,
                                     const std::filesystem::path& relative_path,
                                     const std::vector<std::string>& excludes) {
        const std::filesystem::path root = base / relative_path;
        if (!std::filesystem::exists(root)) {
            log::lua.error("LuaScriptSystem::run_folder_in: '{}' does not exist", root.string());
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
            if (!is_lua_source_file(entry)) {
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
                log::sol2.error("Lua error in {}: {}", file.string(), err.what());
                all_ok = false;
            }
        }

        return all_ok;
    }

    bool LuaScriptSystem::run_engine_file(const std::filesystem::path& relative_path) {
        return run_file(PathUtils::get_engine_root(), relative_path);
    }

    bool LuaScriptSystem::run_engine_folder(const std::filesystem::path& relative_path,
                                            const std::vector<std::string>& excludes) {
        return run_folder(PathUtils::get_engine_root(), relative_path, excludes);
    }

    bool LuaScriptSystem::run_project_file(const std::filesystem::path& relative_path) {
        return run_file(PathUtils::get_project_root(), relative_path);
    }

    bool LuaScriptSystem::run_project_folder(const std::filesystem::path& relative_path,
                                             const std::vector<std::string>& excludes) {
        // Unlike an engine folder, a project folder is allowed not to exist
        if (!std::filesystem::exists(PathUtils::get_project_root() / relative_path)) {
            return true;
        }

        return run_folder(PathUtils::get_project_root(), relative_path, excludes);
    }

    bool LuaScriptSystem::run_bootstrap() {
        return run_engine_file("scripts/bootstrap.lua");
    }

    bool LuaScriptSystem::run_project_main() {
        return run_project_file("scripts/main.lua");
    }

    void LuaScriptSystem::refresh_lua_component_class_caches() {
        m_engine.get_entity_spawner().for_each_entity([](Entity* entity) {
            bool refreshed_any = false;
            entity->for_each_component<LuaScriptComponent>([&refreshed_any](LuaScriptComponent* component) {
                component->refresh_class_cache();
                refreshed_any = true;
            });

            // Priorities may have changed during refresh; re-sort so execution order stays correct.
            if (refreshed_any) {
                entity->sort_components();
            }
        });
    }

    void LuaScriptSystem::register_bindings() {
        bind_schema();
        bind_asset();
        bind_math();
        bind_entity();
        bind_components();
        bind_camera();
        bind_renderer();
        bind_timer();
        bind_input();
        bind_ui();
        bind_physics();
        bind_audio();
        bind_entity_spawner();
        bind_scripts();
        bind_debug();
        bind_logging();
    }

    void LuaScriptSystem::register_cvars(Console& console) {
        console.register_command("l_reload", "Hot-reload Lua scripts", [this](CommandArgs) {
            hot_reload();
        });

        console.register_command(
            "l_defs", "List every definition and the file that declared it", [this, &console](CommandArgs) {
                const sol::object def_sources = m_impl->lua["__def_sources"];
                if (!def_sources.is<sol::table>()) {
                    log::lua.error("__def_sources is not installed");
                    return;
                }

                std::vector<std::string> lines;
                def_sources.as<sol::table>().for_each(
                    [&lines](const sol::object& registry, const sol::object& by_name) {
                        if (!registry.is<std::string>() || !by_name.is<sol::table>()) {
                            return;
                        }

                        const std::string registry_name = registry.as<std::string>();
                        by_name.as<sol::table>().for_each([&](const sol::object& name, const sol::object& path) {
                            if (name.is<std::string>() && path.is<std::string>()) {
                                lines.push_back(std::format(
                                    "{}.{} -> {}", registry_name, name.as<std::string>(), path.as<std::string>()));
                            }
                        });
                    });

                std::sort(lines.begin(), lines.end());
                for (const std::string& line : lines) {
                    log::lua.info("{}", line);
                    console.log("{}", line);
                }

                log::lua.info("{} definitions", lines.size());
                console.log("{} definitions", lines.size());
            });
    }
} // namespace hob
