#include <filesystem>

#include "engine/core/debug.h"
#include "engine/core/path_utils.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"

namespace hob {
    void LuaScriptSystem::dump_component_schemas() {
        const std::filesystem::path out_path =
            PathUtils::get_root_path() / "scripts" / "engine" / "component_schemas.generated.lua";

        if (!m_impl->component_schemas.write_to_file(out_path)) {
            debug::log_error("LuaScriptSystem::dump_component_schemas: failed to write '{}'", out_path.string());
        }
    }

    void LuaScriptSystem::dump_path_schemas() {
        const std::filesystem::path out_path =
            PathUtils::get_root_path() / "scripts" / "engine" / "path_schemas.generated.lua";

        if (!m_impl->path_schemas.write_to_file(out_path)) {
            debug::log_error("LuaScriptSystem::dump_path_schemas: failed to write '{}'", out_path.string());
        }
    }

    void LuaScriptSystem::dump_factory_schemas() {
        const std::filesystem::path out_path =
            PathUtils::get_root_path() / "scripts" / "engine" / "factory_schemas.generated.lua";

        if (!m_impl->factory_schemas.write_to_file(out_path)) {
            debug::log_error("LuaScriptSystem::dump_factory_schemas: failed to write '{}'", out_path.string());
        }
    }
} // namespace hob
