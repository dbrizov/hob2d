#pragma once

#include <filesystem>

namespace hob {
    class PathUtils {
    public:
        // Engine content (framework Lua + builtin assets) that ships with the binary.
        static std::filesystem::path get_engine_root();
        static std::filesystem::path get_engine_assets_path();

        // Active game project (Lua + assets + config), chosen at launch.
        static std::filesystem::path resolve_project_root(int argc, char* argv[]);
        static void set_project_root(const std::filesystem::path& project_root);
        static std::filesystem::path get_project_root();
        static std::filesystem::path get_project_assets_path();

        // Resolve an asset by relative path: the project's assets win, engine assets are the fallback.
        static std::filesystem::path resolve_asset_path(const std::filesystem::path& relative);

        static std::filesystem::path get_engine_config_path();
        static std::filesystem::path get_input_config_path();
        static std::filesystem::path get_log_path();
    };
} // namespace hob
