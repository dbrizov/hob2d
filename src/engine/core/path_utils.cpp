#include "path_utils.h"

#include <cstdlib>
#include <cstring>

#include "engine/core/assert.h"

// Default project baked in by CMake (see HOB_PROJECT). Fallback keeps editor/clangd builds happy.
#ifndef HOB_DEFAULT_PROJECT
#define HOB_DEFAULT_PROJECT "sandbox"
#endif

namespace hob {
    namespace {
        std::filesystem::path s_project_root;

        // Root directory that holds the content/ tree (content/engine, content/projects) and hob2d.log.
        std::filesystem::path root_dir() {
#ifndef NDEBUG
            // (DEBUG) the repo root, derived from this file's location.
            const std::filesystem::path source_file_path = __FILE__;
            return source_file_path
                .parent_path() // src/engine/core
                .parent_path() // src/engine
                .parent_path() // src
                .parent_path(); // repo root
#else
            // (RELEASE) the executable directory, where content/ is synced.
            return std::filesystem::current_path();
#endif
        }
    } // namespace

    std::filesystem::path PathUtils::get_engine_root() {
        return root_dir() / "content" / "engine";
    }

    std::filesystem::path PathUtils::get_engine_assets_path() {
        return get_engine_root() / "assets";
    }

    std::filesystem::path PathUtils::resolve_project_root(int argc, char* argv[]) {
        std::string project;

        // 1) --project <path>
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::strcmp(argv[i], "--project") == 0) {
                project = argv[i + 1];
                break;
            }
        }

        // 2) HOB_PROJECT env var
        if (project.empty()) {
            if (const char* env = std::getenv("HOB_PROJECT")) {
                project = env;
            }
        }

        // 3) compile-time default
        if (project.empty()) {
            project = HOB_DEFAULT_PROJECT;
        }

        std::filesystem::path project_path = project;
        if (project_path.is_relative()) {
            const bool is_bare_name = project.find('/') == std::string::npos && project.find('\\') == std::string::npos;
            project_path =
                is_bare_name ? (root_dir() / "content" / "projects" / project_path) : (root_dir() / project_path);
        }

        return project_path.lexically_normal();
    }

    void PathUtils::set_project_root(const std::filesystem::path& project_root) {
        s_project_root = project_root;
    }

    std::filesystem::path PathUtils::get_project_root() {
        HOB_CHECK(!s_project_root.empty(), "project root requested before set_project_root() was called");
        return s_project_root;
    }

    std::filesystem::path PathUtils::get_project_assets_path() {
        return get_project_root() / "assets";
    }

    std::filesystem::path PathUtils::resolve_asset_path(const std::filesystem::path& relative_path) {
        std::filesystem::path project_path = get_project_assets_path() / relative_path;
        if (std::filesystem::exists(project_path)) {
            return project_path;
        }
        return get_engine_assets_path() / relative_path;
    }

    std::filesystem::path PathUtils::get_engine_config_path() {
        return get_project_root() / "config" / "engine_config.json";
    }

    std::filesystem::path PathUtils::get_input_config_path() {
        return get_project_root() / "config" / "input_config.json";
    }

    std::filesystem::path PathUtils::get_log_path() {
        return root_dir() / "hob2d.log";
    }
} // namespace hob
