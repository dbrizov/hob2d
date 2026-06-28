#include <filesystem>

#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"

namespace hob {
    void LuaScriptSystem::dump_component_schemas() {
        const std::filesystem::path out_path =
            PathUtils::get_engine_root() / "scripts" / "component_schemas.generated.lua";

        if (!m_impl->component_schemas.write_to_file(out_path)) {
            log::lua.error("LuaScriptSystem::dump_component_schemas: failed to write '{}'", out_path.string());
        }
    }

    void LuaScriptSystem::dump_path_schemas() {
        const std::filesystem::path out_path = PathUtils::get_engine_root() / "scripts" / "path_schemas.generated.lua";

        if (!m_impl->path_schemas.write_to_file(out_path)) {
            log::lua.error("LuaScriptSystem::dump_path_schemas: failed to write '{}'", out_path.string());
        }
    }

    void LuaScriptSystem::dump_factory_schemas() {
        const std::filesystem::path out_path =
            PathUtils::get_engine_root() / "scripts" / "factory_schemas.generated.lua";

        if (!m_impl->factory_schemas.write_to_file(out_path)) {
            log::lua.error("LuaScriptSystem::dump_factory_schemas: failed to write '{}'", out_path.string());
        }
    }
} // namespace hob
