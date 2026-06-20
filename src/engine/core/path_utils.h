#pragma once

#include <filesystem>

namespace hob {
    class PathUtils {
    public:
        static std::filesystem::path get_root_path();
        static std::filesystem::path get_assets_root_path();
        static std::filesystem::path get_engine_config_path();
        static std::filesystem::path get_input_config_path();
        static std::filesystem::path get_log_path();
    };
} // namespace hob
