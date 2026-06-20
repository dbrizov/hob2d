#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sol {
    class state;
}

namespace hob {
    class Engine;
    class Console;
    struct LuaScriptSystemImpl;

    class LuaScriptSystem {
        Engine& m_engine;
        std::unique_ptr<LuaScriptSystemImpl> m_impl;

    public:
        explicit LuaScriptSystem(Engine& engine);
        ~LuaScriptSystem();

        LuaScriptSystem(const LuaScriptSystem&) = delete;
        LuaScriptSystem& operator=(const LuaScriptSystem&) = delete;

        sol::state& get_lua();

        bool hot_reload();

    private:
        void refresh_lua_component_hook_caches();

        bool run_file(const std::filesystem::path& relative_path);
        bool run_folder(const std::filesystem::path& relative_path, const std::vector<std::string>& excludes = {});
        bool run_bootstrap();

        void register_cvars(Console& console);

        void register_bindings();

        void bind_math();
        void bind_entity();
        void bind_components();
        void bind_camera();
        void bind_renderer();
        void bind_timer();
        void bind_input();
        void bind_ui();
        void bind_physics();
        void bind_entity_spawner();
        void bind_scripts();
        void bind_assets();
        void bind_debug();

        void dump_component_schemas();
        void dump_path_schemas();
        void dump_factory_schemas();

        void dump_bindings_meta();
        void dump_path_schemas_meta();
        void dump_path_aliases_meta();
        void dump_factory_schemas_meta();
        void dump_factory_aliases_meta();
        void dump_entity_registry_meta();
        void dump_component_registry_meta();
    };
} // namespace hob
