#include "engine/core/assert.h"
#include "engine/core/engine.h"
#include "engine/core/engine_config.h"
#include "engine/core/path_utils.h"

int main(int argc, char* argv[]) {
    const std::filesystem::path project_root = hob::PathUtils::resolve_project_root(argc, argv);
    HOB_CHECK(std::filesystem::exists(project_root / "scripts" / "main.lua"),
              "no game project found at '{}' (expected scripts/main.lua); pass --project <path>",
              project_root.string());
    hob::PathUtils::set_project_root(project_root);

    const hob::EngineConfig config(hob::PathUtils::get_engine_config_path());
    hob::Engine engine(config);

    engine.run();
    return 0;
}
