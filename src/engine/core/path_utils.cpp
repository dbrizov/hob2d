#include "path_utils.h"

namespace hob {
    std::filesystem::path PathUtils::get_root_path() {
#ifndef NDEBUG
        // (IN DEBUG MODE) - return the root directory of the project
        const std::filesystem::path source_file_path = __FILE__;
        std::filesystem::path project_root_path = source_file_path
                                                      .parent_path() // root/src/engine/core
                                                      .parent_path() // root/src/engine
                                                      .parent_path() // root/src
                                                      .parent_path(); // root

        return project_root_path;
#else
        // (IN RELEASE MODE) - return the current directory of the executable
        std::filesystem::path current_path = std::filesystem::current_path();
        return current_path;
#endif
    }

    std::filesystem::path PathUtils::get_assets_root_path() {
        const std::filesystem::path root_path = get_root_path();
        std::filesystem::path assets_root_path = root_path / "assets";
        return assets_root_path;
    }

    std::filesystem::path PathUtils::get_engine_config_path() {
        const std::filesystem::path root_path = get_root_path();
        std::filesystem::path engine_config_path = root_path / "config" / "engine_config.json";
        return engine_config_path;
    }

    std::filesystem::path PathUtils::get_input_config_path() {
        const std::filesystem::path root_path = get_root_path();
        std::filesystem::path input_config_path = root_path / "config" / "input_config.json";
        return input_config_path;
    }

    std::filesystem::path PathUtils::get_log_path() {
        const std::filesystem::path root_path = get_root_path();
        std::filesystem::path log_path = root_path / "hob2d.log";
        return log_path;
    }
} // namespace hob
