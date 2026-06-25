// clang-format off
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
// clang-format on

#include <algorithm>
#include <string>
#include <vector>

#include "engine/components/lua_script_component.h"
#include "engine/core/assert.h"
#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/console.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        int lua_panic_handler(lua_State* L) {
            const char* message = lua_tostring(L, -1);
            log::sol2.error("panic: {}", message ? message : "unknown error");
            return 0;
        }

        void lua_warn_handler(void* ud, const char* message, int tocont) {
            (void)ud;
            static std::string buffer;
            buffer += message;
            if (!tocont) {
                log::sol2.info("{}", buffer);
                buffer.clear();
            }
        }

        void install_lua_log_redirects(sol::state& lua) {
            lua_State* L = lua.lua_state();
            lua_atpanic(L, lua_panic_handler);
            lua_setwarnf(L, lua_warn_handler, nullptr);

            lua["print"] = [](sol::this_state ts, sol::variadic_args args) {
                sol::state_view sv(ts);
                const sol::protected_function tostring = sv["tostring"];
                std::string out;
                bool first = true;
                for (auto v : args) {
                    sol::protected_function_result r = tostring(sol::object(v));
                    const std::string piece = r.valid() ? r.get<std::string>() : "<tostring failed>";
                    if (!first) {
                        out += '\t';
                    }
                    out += piece;
                    first = false;
                }

                log::sol2.info("{}", out);
            };
        }
    } // namespace

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

        install_lua_log_redirects(lua);

        // Make `require` find modules in scripts/engine/lib (e.g. vendored lldebugger).
        const std::string lib_path = (PathUtils::get_root_path() / "scripts" / "engine" / "lib" / "?.lua").string();
        sol::table package = lua["package"];
        package["path"] = lib_path + ";" + package["path"].get<std::string>();

        register_bindings();

        // Schema files are consumed by the Lua bootstrap, so they must be written first.
        dump_component_schemas();
        dump_path_schemas();
        dump_factory_schemas();

        const bool bootstrap_succeeded = run_bootstrap();
        HOB_CHECK(bootstrap_succeeded, "Lua bootstrap failed");

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
            refresh_lua_component_class_caches();
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

        const std::filesystem::path scripts_root = PathUtils::get_root_path() / "scripts";

        std::filesystem::file_time_type newest = std::filesystem::file_time_type::min();
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(scripts_root, ec)) {
            if (ec) {
                return;
            }

            if (!entry.is_regular_file() || entry.path().extension() != ".lua") {
                continue;
            }

            const std::filesystem::file_time_type t = entry.last_write_time(ec);
            if (!ec && t > newest) {
                newest = t;
            }
        }

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

    void LuaScriptSystem::refresh_lua_component_class_caches() {
        std::vector<Entity*> entities;
        m_engine.get_entity_spawner().get_entities(entities);

        for (Entity* entity : entities) {
            const std::vector<LuaScriptComponent*> components = entity->get_components<LuaScriptComponent>();
            for (LuaScriptComponent* component : components) {
                component->refresh_class_cache();
            }

            // Priorities may have changed during refresh; re-sort so execution order stays correct.
            if (!components.empty()) {
                entity->sort_components();
            }
        }
    }

    bool LuaScriptSystem::run_file(const std::filesystem::path& relative_path) {
        const std::filesystem::path full_path = PathUtils::get_root_path() / relative_path;

        auto result = m_impl->lua.safe_script_file(full_path.string(), sol::script_pass_on_error);
        if (!result.valid()) {
            const sol::error err = result;
            log::sol2.error("Lua error in {}: {}", full_path.string(), err.what());
            return false;
        }

        return true;
    }

    bool LuaScriptSystem::run_folder(const std::filesystem::path& relative_path,
                                     const std::vector<std::string>& excludes) {
        const std::filesystem::path root = PathUtils::get_root_path() / relative_path;
        if (!std::filesystem::exists(root)) {
            log::lua.error("LuaScriptSystem::run_folder: '{}' does not exist", root.string());
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
                log::sol2.error("Lua error in {}: {}", file.string(), err.what());
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
        bind_logging();
    }
} // namespace hob
